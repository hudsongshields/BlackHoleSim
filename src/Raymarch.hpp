#pragma once
#include "config.hpp"
#include "particles.hpp"
#include "utils/spaceshipSDF.hpp"

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
        __host__ __device__ vec4 traceRay(ParticleManager* particleManager = nullptr, Spaceship* spaceship = nullptr) {

            const float tMax = 30.0f;

            float dt = 0.1f;

            for (int i {0}; i < 128; ++i) {
                float particleSpeed = 0.0f;
                CollisionType collision = checkCollision(dt, particleSpeed, particleManager, spaceship);
                
                if (t > tMax) break;
                switch (collision) {
                    case BLACKHOLE:
                        return vec4(0.0f, 0.0f, 0.0f, 1.0f);
                    
                    case DISK: {
                        float speed = glm::clamp(particleSpeed / 1.5f, 0.0f, 1.5f);
                        float brightness = 0.4f + 1.8f * speed;
                        vec3 cool = vec3(1.80f, 0.55f, 0.15f);
                        vec3 hot  = vec3(0.40f, 0.80f, 2.20f);
                        vec3 color = glm::mix(cool, hot, speed);
                        return brightness * vec4(color, 1.0f);
                    }
                    case SPACESHIP:
                        return vec4(1.0f, 0.5f, 0.0f, 1.0f);
                    
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
        enum CollisionType {
            NONE,
            BLACKHOLE,
            DISK,
            SPACESHIP
        };
        __host__ __device__ CollisionType checkCollision(float dt, float& particleSpeed, ParticleManager* particleManager = nullptr, Spaceship* spaceship = nullptr) {
            float r2 = glm::sqrt(position.x * position.x + position.z * position.z);
            float shipDist = spaceship ? spaceship->sdf(position) : FLT_MAX;

            // --- Adapt dt to relative distances ---
            float adaptedDt = dt * (rho / sphereRadius);

            if (spaceship && shipDist < 0.9f) {
                adaptedDt = glm::min(adaptedDt, shipDist);
            }

            if (!spaceship && abs(position.y) < diskHeight * 4.0f && (r2 + 0.01f) < diskRadius) {
                if (particleManager) {
                    adaptedDt = glm::min(adaptedDt, nearestParticleDist);
                }
            }

            dt = glm::clamp(adaptedDt, 0.001f, dt * 2.0f);
            update(dt);

            // --- Collision checks ---
            if (rho <= sphereRadius * 0.8f) {   
                return BLACKHOLE;
            }

            if (spaceship && shipDist < 0.01f) {
                return SPACESHIP;
            }

            if (abs(position.y) < diskHeight && (r2 + 0.01f) < diskRadius) {
                if (particleManager) {
                    nearestParticleDist = 1000.0f;
                    int hitIdx = particleManager->checkCollisions(position, nearestParticleDist);
                    if (nearestParticleDist < 0.01f && hitIdx >= 0) {
                        particleSpeed = glm::length((*particleManager)[hitIdx].velocity);
                        return DISK;
                    }
                }
            }
            return NONE;
        }
        __host__ __device__ virtual void update(float dt) {
            position += direction * dt;
            rho = glm::length(position);
            r = glm::length(vec2(position.x, position.z));
        };

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

