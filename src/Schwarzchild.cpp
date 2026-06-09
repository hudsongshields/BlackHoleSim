#include "Raymarch.hpp"
#include <cstdlib>
#include <cuda_runtime.h>

__host__ __device__ Schwarzschild::Schwarzschild(const vec3& r_init, const vec3& rayDir,
    float GM, const vec3& sphereCenter, float sphereRadius, float diskRadius)
: BaseRaymarch(r_init, rayDir, GM, sphereCenter, sphereRadius, diskRadius)
{
    rho = glm::length(position);
    u = 1.0f / rho;
    radial_dir = glm::normalize(position);

    // Build a stable tangent basis in the orbital plane.
    vec3 tangential = direction - glm::dot(direction, radial_dir) * radial_dir;
    float tangentialLen = glm::length(tangential);

    if (tangentialLen < 1e-6f) {
        vec3 up = glm::abs(radial_dir.y) < 0.99f
            ? vec3(0.0f, 1.0f, 0.0f)
            : vec3(1.0f, 0.0f, 0.0f);
        tangential = glm::cross(up, radial_dir);
        tangentialLen = glm::length(tangential);
    }

    tangential_dir = tangential / glm::max(tangentialLen, 1e-6f);
    b = rho * tangentialLen;

    phi = 0.0f;
    sign = glm::dot(direction, radial_dir) >= 0.0f ? 1 : -1;

    r = glm::length(vec2(position.x, position.z));
    theta = 3.14159265359/2.0f;
}

// Override update method to implement Schwarzschild solution
__host__ __device__ void Schwarzschild::update(float lambda) {
    float phi_dot = b * u * u;
    phi += phi_dot * lambda;
    float radialSq = 1.0f - (1.0f - 2.0f * GM * u) * (b * b * u * u);
    r_dot = std::sqrt(glm::max(radialSq, 0.0f));

    float epsilon = 1e-3f;
    if (sign < 0 && r_dot < epsilon) {
        sign = 1;
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


