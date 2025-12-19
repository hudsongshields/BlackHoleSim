#pragma once
#include "config.h"
#include "LightRay.h"

class Schwarzschild : public LightRay {
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
