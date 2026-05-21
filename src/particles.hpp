#pragma once
#include "config.hpp"
#include <random>
#include <cmath>

struct Particle {
    float radius;
    vec3 position;
    vec3 velocity;
};

class ParticleGenerator {
    public:
        int numParticles;
        std::vector<Particle> particles; 

        ParticleGenerator(int maxParticles) 
            : numParticles(maxParticles)
            {
                std::mt19937 rng(42);

                std::uniform_real_distribution<float> genUnit(0.f, 1.f);
                std::uniform_real_distribution<float> genAngle(0.f, 6.283185307f);
                std::uniform_real_distribution<float> genY(-0.005f * diskRadius, 0.005f * diskRadius);
                std::uniform_real_distribution<float> genJitter(-0.04f, 0.04f);
                std::uniform_real_distribution<float> genJitterY(-0.005f, 0.005f);
                std::normal_distribution<float> genRadius(0.01f, 0.005f);

                particles.resize(maxParticles);

                for (auto& particle : particles) {
                    float angle = genAngle(rng);
                    float minR = sphereRadius * 3.0f;
                    float maxR = diskRadius;
                    float radial = std::sqrt(genUnit(rng)) * (maxR - minR) + minR;

                    vec3 pos = vec3(
                        radial * std::cos(angle),
                        genY(rng),
                        radial * std::sin(angle)
                    );

                    vec3 tangent = glm::normalize(vec3(-pos.z, 0.0f, pos.x));
                    float orbitSpeed = glm::clamp(std::sqrt(GM / glm::max(radial, sphereRadius * 1.05f)), 0.2f, 2.5f);

                    particle.position = pos;
                    particle.velocity = tangent * orbitSpeed + vec3(genJitter(rng), genJitterY(rng), genJitter(rng));
                    particle.radius = glm::clamp(genRadius(rng), 0.02f, 0.12f);
                }
            }
        Particle* getArrayPtr() {
            return particles.data();
        }
};

class ParticleManager {
    public:
        int numParticles;

        ParticleManager(int maxParticles, Particle* externalArray)
            : numParticles(maxParticles)
            , particleArray(externalArray)
            {}

        ~ParticleManager() = default;
        

        __host__ __device__ const Particle& operator[](int index) const {
            return particleArray[index];
        }

        __host__ std::vector<std::thread> updateParticles(float dt) {
            unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
            unsigned int particlesPerThread = (numParticles + numThreads - 1) / numThreads;
            std::vector<std::thread> threads;
            auto updateRange = [this, dt](int start, int end) {
                for (int i = start; i < end; ++i) {
                    Particle& p = particleArray[i];
                    vec3 toCenter = -p.position;
                    float dist = glm::length(toCenter);
                    vec3 accelDir = glm::normalize(toCenter);
                    float accelMag = GM / (dist * dist + 0.01f);
                    p.velocity += accelDir * accelMag * dt;
                    p.position += p.velocity * dt;
                }
            };

            for (unsigned int t = 0; t < numThreads; ++t) {
                int start = t * particlesPerThread;
                int end = std::min((int)(start + particlesPerThread), numParticles);
                threads.emplace_back(updateRange, start, end);
            }
            return threads;
        }

        __device__ void updateParticlesDevice(float dt, int idx) {
            if (idx >= numParticles) return;

            Particle& p = particleArray[idx];
            vec3 toCenter = -p.position;
            float dist = glm::length(toCenter);
            vec3 accelDir = glm::normalize(toCenter);
            float accelMag = GM / (dist * dist + 0.01f);
            p.velocity += accelDir * accelMag * dt;
            p.position += p.velocity * dt;
        }

    private:
        Particle* particleArray;
        
        // if fall inside, respawn at random position on disk edge with velocity for circular orbit
        __host__ __device__ void handleFallInside(Particle& p) {
            float dist = glm::length(p.position);
            if (dist < sphereRadius) {
                float angle = atan2(p.position.z, p.position.x);
                float respawnRadius = diskRadius * 0.9f;
                p.position = vec3(
                    respawnRadius * std::cos(angle),
                    0.f,
                    respawnRadius * std::sin(angle)
                );
                vec3 tangent = glm::normalize(vec3(-p.position.z, 0.0f, p.position.x));
                float orbitSpeed = glm::clamp(std::sqrt(GM / respawnRadius), 0.2f, 2.5f);
                p.velocity = tangent * orbitSpeed;
            }
        }
};