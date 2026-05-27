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
                std::normal_distribution<float> genRadius(0.0003f, 0.0015f);

                particles.resize(maxParticles);

                for (auto& particle : particles) {
                    float angle = genAngle(rng);
                    float minR = sphereRadius * 2.0f;
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
                    particle.radius = glm::clamp(genRadius(rng), 0.0001f, 0.0004f);
                }
            }
        Particle* getArrayPtr() {
            return particles.data();
        }
};


constexpr int gridWidth = 256;
struct ParticleGrid {
    int* cellHeads;
    int* nextParticles;

    int gridWidth;
    int numCells;
    float cellSize;
    vec3 center;
    vec3 start;

    __host__ ParticleGrid(int numParticles) 
        : gridWidth(::gridWidth)
        , numCells(::gridWidth * ::gridWidth)
        , cellHeads(nullptr)
        , nextParticles(nullptr)
    {
        float offset = 0.1f;
        cellSize = (::diskRadius * 2.0f + offset) / gridWidth;
        cudaMalloc(&cellHeads, numCells * sizeof(int));
        cudaMalloc(&nextParticles, numParticles * sizeof(int));

        cudaMemset(cellHeads, -1, numCells * sizeof(int));
        cudaMemset(nextParticles, -1, numParticles * sizeof(int));

        center = ::sphereCenter;
        start = vec3(
            center.x - diskRadius,
            center.y,
            center.z - diskRadius
        );
    }
    __host__ ~ParticleGrid() {
        cudaFree(cellHeads);
        cudaFree(nextParticles);
    };

#ifdef __CUDACC__
    __device__ void insert(Particle& particle, int particleIdx) {
        vec3& pos {particle.position};
        int x_idx {static_cast<int>(floor((pos.x - start.x) / cellSize))};
        int z_idx {static_cast<int>(floor((pos.z - start.z) / cellSize))};

        if (x_idx < 0 || x_idx >= gridWidth || z_idx < 0 || z_idx >= gridWidth) {
            return;
        }

        int cellIdx {x_idx + z_idx * gridWidth};
        int oldHead = atomicExch(&cellHeads[cellIdx], particleIdx);
        nextParticles[particleIdx] = oldHead;
    };
#endif

    __host__ __device__ int getCellParticles(vec3& pos) {
        int x_idx {static_cast<int>(floor((pos.x - start.x) / cellSize))};
        int z_idx {static_cast<int>(floor((pos.z - start.z) / cellSize))};

        if (x_idx < 0 || x_idx >= gridWidth || z_idx < 0 || z_idx >= gridWidth) {
            return -1;
        }

        int cellIdx {x_idx + z_idx * gridWidth};
        return cellHeads[cellIdx];
    };

    __host__ __device__ int getNextParticle(int particleIdx) {
        return nextParticles[particleIdx];
    };

    
    
};

class ParticleManager {
    public:
        int numParticles;

        ParticleManager(int maxParticles, Particle* externalArray)
            : numParticles(maxParticles)
            , particleArray(externalArray)
            , grid(maxParticles)
            {}

        ~ParticleManager() = default;
        

        __host__ __device__ const Particle& operator[](int index) const {
            return particleArray[index];
        }


        __device__ void updateParticlesDevice(float dt, int idx) {

            Particle& p = particleArray[idx];
            vec3 toCenter = -p.position;
            float dist = glm::length(toCenter);
            vec3 accelDir = glm::normalize(toCenter);
            float accelMag = GM / (dist * dist + 0.01f);
            p.velocity += accelDir * accelMag * dt;

            // artificial transfer of orbital momentum
            for (int i {0}; i < 2; ++i) {
                vec3 ortho = glm::normalize(glm::cross(accelDir, vec3(0.f, 1.f, 0.f)));
                p.velocity += ortho * accelMag * 0.1f * dt;
            }
            p.position += p.velocity * dt;
            handleFallInside(p);
        }
#ifdef __CUDACC__
        __device__ void updateGridDevice(int idx) {
            grid.insert(particleArray[idx], idx);
        }
#endif

        __host__ __device__ int checkCollisions(vec3& rayPos, float& nearestParticleDist) {
            int nearestParticleIdx = -1;
            int rayCellX = static_cast<int>(floor((rayPos.x - grid.start.x) / grid.cellSize));
            int rayCellZ = static_cast<int>(floor((rayPos.z - grid.start.z) / grid.cellSize));
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                for (int offsetZ = -1; offsetZ <= 1; ++offsetZ) {
                    int cellX = rayCellX + offsetX;
                    int cellZ = rayCellZ + offsetZ;
                    if (cellX < 0 || cellX >= grid.gridWidth || cellZ < 0 || cellZ >= grid.gridWidth) continue;
                    int particleIdx = grid.cellHeads[cellX + cellZ * grid.gridWidth];
                    while (particleIdx != -1) {
                        float surfaceDist = glm::length(rayPos - particleArray[particleIdx].position) - particleArray[particleIdx].radius;
                        if (surfaceDist < nearestParticleDist) {
                            nearestParticleDist = surfaceDist;
                            nearestParticleIdx = particleIdx;
                        }
                        particleIdx = grid.getNextParticle(particleIdx);
                    }
                }
            }
            return nearestParticleIdx;
        }

        __host__ void resetGrid() {
            cudaMemset(grid.cellHeads, -1, grid.numCells * sizeof(int));
            cudaMemset(grid.nextParticles, -1, numParticles * sizeof(int));
        }
        #ifdef __CUDACC__
        __device__ void resetGridDevice() {
            int idx = threadIdx.x + blockIdx.x * blockDim.x;
            int stride = blockDim.x * gridDim.x;
            while (idx < grid.numCells) {
                grid.cellHeads[idx] = -1;
                idx += stride;
            }
            idx = threadIdx.x + blockIdx.x * blockDim.x;
            while (idx < numParticles) {
                grid.nextParticles[idx] = -1;
                idx += stride;
            }
        }
        #endif

    private:
        Particle* particleArray;
        ParticleGrid grid;

        
        // if fall inside, respawn at random position on disk edge with velocity for circular orbit
        __host__ __device__ void handleFallInside(Particle& p) {
            float dist = glm::length(p.position);
            if (dist < sphereRadius + 0.01f) {
                float angle = atan2(p.position.z, p.position.x);

                // spawn randomly between 0.85 and 0.95 of disk radius
                float respawnRadius = diskRadius * (0.85f + 0.1f * (static_cast<float>(rand()) / RAND_MAX));
                p.position = vec3(
                    respawnRadius * cosf(angle),
                    0.f,
                    respawnRadius * sinf(angle)
                );
                vec3 tangent = glm::normalize(vec3(-p.position.z, 0.0f, p.position.x));
                float orbitSpeed = glm::clamp(std::sqrt(GM / respawnRadius), 0.2f, 2.5f);
                p.velocity = tangent * orbitSpeed;
            }
        }
        __host__ void initGrid(float cellSize);
};
