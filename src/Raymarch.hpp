#pragma once
#include "config.hpp"
#include "particles.hpp"

#ifndef __CUDACC__
#  ifndef __host__
#    define __host__
#  endif
#  ifndef __device__
#    define __device__
#  endif
#endif


class BaseRaymarch {
    public:
        float diskHeight;

        __host__ __device__ BaseRaymarch(const vec3& r_init, const vec3& rayDir,
            float GM, const vec3& sphereCenter, float sphereRadius, float diskRadius)
            : position{r_init}
            , direction{glm::normalize(rayDir)}
            , GM{GM}
            , sphereCenter{sphereCenter}
            , sphereRadius{sphereRadius}
            , diskRadius{diskRadius}
        {
            diskHeight = 0.02f * diskRadius;
            rho = glm::length(r_init);
        }

        __host__ __device__ float y_pos() const {
            return position.y;
        }

        __host__ __device__ float getR() const {
            return r;
        }
        __host__ __device__ vec4 traceRay(ParticleManager* particleManager = nullptr) {

            const float tMax = 25.0f;

            float dt = 0.1f;

            for (int i {0}; i < 256; ++i) {
                CollisionType collision = checkCollision(dt, particleManager);
                
                if (t > tMax) break;
                switch (collision) {
                    case BLACKHOLE:
                        return vec4(0.0f, 0.0f, 0.0f, 1.0f);
                    
                    case DISK: {
                        float speed = glm::clamp(particleSpeed / 2.5f, 0.0f, 1.0f);
                        float brightness = 0.4f + 1.2f * speed;
                        vec3 cool = vec3(1.80f, 0.55f, 0.15f);
                        vec3 hot  = vec3(0.40f, 0.80f, 2.20f);
                        vec3 color = glm::mix(cool, hot, speed);
                        return brightness * vec4(color, 1.0f);
                    }
                    
                    case NONE:
                        break;
                }
            }
            return vec4(glm::normalize(direction), -1.0f);
        }

    protected:
        vec3 position;
        vec3 direction;

        float GM;
        vec3  sphereCenter;
        float sphereRadius;
        float diskRadius;

        float rho;
        float theta;
        float r;
        float r_dot;

        float t {0.0f};
        float nearestParticleDist {1000.0f};

    private:
        float particleSpeed = 0.0f;
        enum CollisionType {
            NONE,
            BLACKHOLE,
            DISK
        };
        __host__ __device__ CollisionType checkCollision(float dt, ParticleManager* particleManager = nullptr) {
            // adaptive dt
            dt = glm::clamp(dt * (rho / sphereRadius), 0.005f, dt * 2.0f);
            // now adapt to distance to nearest particle if in disk region
            if (abs(position.y) < diskHeight && (sqrt(position.x * position.x + position.z * position.z) + 0.01f) < diskRadius) {
                if (particleManager) {
                    dt = glm::min(dt, glm::clamp(nearestParticleDist * 0.5f, 0.001f, dt));
                }
            }
            update(dt);

            // Check collision with black hole (sphere)
            if (rho <= sphereRadius) {
                return BLACKHOLE;
            }

            // Check collision with accretion disk (short cylinder)
            if (abs(position.y) < diskHeight && (sqrt(position.x * position.x + position.z * position.z) + 0.01f) < diskRadius) {
                // Check for collision with particle in accretion disk
                if (particleManager) {
                    nearestParticleDist = 1000.0f;
                    for (int i {0}; i < particleManager->numParticles; ++i) {
                        float dist = glm::length(position - (*particleManager)[i].position) - (*particleManager)[i].radius;
                        if (dist < nearestParticleDist) {
                            nearestParticleDist = dist;
                        }
                        if (dist < 0.01f) {
                            particleSpeed = glm::length((*particleManager)[i].velocity);
                            return DISK;
                        }
                    }
                }
            }

            return NONE;
        }
        __host__ __device__ virtual void update(float dt) = 0;

};

class LightRay : public BaseRaymarch {
    public:

        __host__ __device__ LightRay(
            const vec3& r_init,
            const vec3& rayDir,
            float GM, const vec3& sphereCenter, float sphereRadius, float diskRadius
        );

    private:
        float h;
        float y;

        float v_t;
        float theta_dot;
        float y_dot;

        __host__ __device__ void update(float dt) override;

};

class Schwarzschild : public BaseRaymarch {
    public:
        float b;
        __host__ __device__ Schwarzschild(
            const vec3& r_init,
            const vec3& rayDir,
            float GM, const vec3& sphereCenter, float sphereRadius, float diskRadius
        );


    private:
        float u;
        float phi;

        vec3 radial_dir;
        vec3 tangential_dir;

        int sign;

        __host__ __device__ void update(float dt) override;
};