#pragma once
#include "config.h"

class LightRay {
    public:
        float GM;
        float diskHeight;
        float t {0.0f};

        enum CollisionType {
            NONE,
            BLACKHOLE,
            DISK
        };

        LightRay(
            const vec3& r_init, 
            const vec3& rayDir, 
            float GM_value, 
            float disk_height
        );

        virtual CollisionType checkCollision(float dt, float sphereRadius, float diskRadius);
        float y_pos() const {
            return position.y;
            }

        float getR() const {
            return r;
        }

    protected:
        vec3 position;
        vec3 direction;

        float rho;
        float theta;
        float r;
        float r_dot;

        
        virtual void update(float dt);

    private:
        float h;
        float y;

        float v_t;
        float theta_dot;
        float y_dot;

};