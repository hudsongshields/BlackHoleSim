#include "config.h"
#include "lightRay.h"
#include "Schwarzchild.h"

Schwarzschild::Schwarzschild(const vec3& r_init, const vec3& rayDir, float GM_value, float disk_height) 
: LightRay(r_init, rayDir, GM_value, disk_height)
{
    rho = glm::length(position);
    u = 1.0f / rho;
    b = glm::length(glm::cross(position, direction));

    vec3 normal = glm::cross(position, direction);
    e1 = glm::normalize(position);
    e2 = glm::cross(normal, e1);
    if (length(e2) < 0.0001f) {
        std::cout << "Warning: e2 vector length is zero in Schwarzschild constructor." << std::endl;
    }
    e2 = glm::normalize(e2);

    phi = 0.0f;
    sign = glm::dot(position, direction) >= 0.0f ? 1 : -1;

    r = glm::length(vec2(position.x, position.z));
    theta = 3.14159265359/2.0f;
}

// Override update method to implement Schwarzschild solution
void Schwarzschild::update(float lambda) {
    phi += (b * u * u) * lambda;

    r_dot = 1.0f - (1.0f - 2.0f * GM * u) * (b * b * u * u);
    if (r_dot <= 0.0f) r_dot = 0.0f;
    r_dot = std::sqrt(r_dot);

    float epsilon = 1e-3f;
    if (r_dot < epsilon) {
        sign = -sign;
    }
    r_dot = sign * r_dot;
    rho += r_dot * lambda;

    u = 1.0f / rho;

    // reconstruct y when theta=pi/2, phi = phi, and rh0 = rho
    // x = rho * cos(phi) * sin(theta)
    // y = rho * sin(phi) * sin(theta)
    position = rho * (std::cos(phi) * e1 + std::sin(phi) * e2);
    r = glm::length(vec2(position.x, position.z));


    // Update Cartesian position
    t += lambda;
}



