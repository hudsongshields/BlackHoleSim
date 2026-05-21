#include "Raymarch.hpp"
#include <cstdlib>
#include <cuda_runtime.h>

__host__ __device__ Schwarzschild::Schwarzschild(const vec3& r_init, const vec3& rayDir,
    float GM, const vec3& sphereCenter, float sphereRadius, float diskRadius)
: BaseRaymarch(r_init, rayDir, GM, sphereCenter, sphereRadius, diskRadius)
{
    rho = glm::length(position);
    u = 1.0f / rho;
    b = glm::length(glm::cross(position, direction));

    vec3 normal = glm::cross(position, direction);
    radial_dir = glm::normalize(position);
    tangential_dir = glm::cross(normal, radial_dir);
    if (length(tangential_dir) < 0.0001f) {
        // 
    }
    tangential_dir = glm::normalize(tangential_dir);

    phi = 0.0f;
    sign = glm::dot(position, direction) >= 0.0f ? 1 : -1;

    r = glm::length(vec2(position.x, position.z));
    theta = 3.14159265359/2.0f;
}

// Override update method to implement Schwarzschild solution
__host__ __device__ void Schwarzschild::update(float lambda) {
    float phi_dot = b * u * u;
    phi += phi_dot * lambda;
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
    position = rho * (std::cos(phi) * radial_dir + std::sin(phi) * tangential_dir);
    r = glm::length(vec2(position.x, position.z));


    // Update Cartesian position
    t += lambda;

    vec3 radialDir = std::cos(phi) * radial_dir + std::sin(phi) * tangential_dir;
    vec3 angularDir = -std::sin(phi) * radial_dir + std::cos(phi) * tangential_dir;
    vec3 tangent = r_dot * radialDir + (rho * phi_dot) * angularDir;
    direction = glm::normalize(tangent);
}


