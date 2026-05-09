#pragma once
#include "config.h"


class BaseRaymarch {
    public:
        float GM;
        float diskHeight;

        BaseRaymarch(const vec3& r_init, const vec3& rayDir, float GM_value, float disk_height)
            : GM{GM_value}
            , diskHeight{disk_height}
            , position{r_init}
            , direction{glm::normalize(rayDir)}
        {
            rho = glm::length(r_init);
        }

        float y_pos() const {
            return position.y;
            }

        float getR() const {
            return r;
        }
        vec3 traceRay(BaseRaymarch *ray, const glm::vec3& sphereCenter, float sphereRadius, float diskRadius) {

            const float tMax = 50.0f;

            float hitRadius;
            float dt = 0.1f;

            for (int i {0}; i < 128; ++i) {
                CollisionType collision = ray->checkCollision(dt, sphereRadius, diskRadius);

                if (ray->t > tMax) break;
                switch (collision) {
                    case BLACKHOLE:
                        return vec3(0.0f, 0.0f, 0.0f);

                    case DISK: {
                        float rNormalized = glm::clamp((ray->getR() - sphereRadius) / (diskRadius - sphereRadius), 0.0f, 1.0f);
                        vec3 innerColor = vec3(1.0, 0.35, 0.1);  // reddish inner
                        vec3 outerColor = vec3(1.0, 0.75, 0.25); // more orange outer
                        vec3 baseColor  = mix(innerColor, outerColor, rNormalized);


                        // Narrow bright ring near the inner edge (photon ring-ish)
                        float ring1 = exp(-pow((rNormalized - 0.15) / 0.03, 2.0));
                        float ring2 = exp(-pow((rNormalized - 0.30) / 0.06, 2.0));
                        float ring3 = exp(-pow((rNormalized - 0.51) / 0.10, 2.0));
                        float ring4 = exp(-pow((rNormalized - 0.80) / 0.16, 2.0));
                        float radialGlow = ring1 + 0.8 * ring2 + 0.5 * ring3 + 0.3 * ring4;

                        baseColor *= radialGlow;
                        return baseColor;
                    }
                    case NONE:
                        break;
                }
            }
            
            vec3 backgroundTop = vec3(0.0f, 0.0f, 0.0f);
            vec3 backgroundBot = vec3(0.1f, 0.1f, 0.25f);
            float distance = glm::clamp(ray->y_pos(), 0.0f, 1.0f);
            vec3 background = glm::mix(backgroundBot, backgroundTop, distance);
            return background;
        }

    protected:
        vec3 position;
        vec3 direction;

        float rho;
        float theta;
        float r;
        float r_dot;

        float t {0.0f};

    private:
        enum CollisionType {
            NONE,
            BLACKHOLE,
            DISK
        };
        CollisionType checkCollision(float dt, float sphereRadius, float diskRadius) {
            // adaptive dt
            dt = glm::clamp(dt * (rho / sphereRadius), 0.005f, dt * 2.0f);
            update(dt);

            // Check collision with black hole (sphere)
            if (rho <= sphereRadius) {
                return BLACKHOLE;
            }

            // Check collision with accretion disk (short cylinder)
            if (abs(position.y) < diskHeight && (sqrt(position.x * position.x + position.z * position.z) + 0.01f) < diskRadius) {
                return DISK;
            }

            return NONE;
        }
        virtual void update(float dt) = 0;

};

class LightRay : public BaseRaymarch {
    public:

        LightRay(
            const vec3& r_init, 
            const vec3& rayDir, 
            float GM_value, 
            float disk_height
        );

    private:
        float h;
        float y;

        float v_t;
        float theta_dot;
        float y_dot;

        void update(float dt) override;

};

class Schwarzschild : public BaseRaymarch {
    public:
        float b;
        Schwarzschild(
            const vec3& r_init, 
            const vec3& rayDir, 
            float GM_value, 
            float disk_height
        );


    private:
        float u;
        float phi;

        vec3 e1;
        vec3 e2;

        int sign;

        void update(float dt) override;
};