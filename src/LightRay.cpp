#include "config.h"
#include "LightRay.h"

LightRay::LightRay(const vec3& r_init, const vec3& rayDir, float GM_value, float disk_height) 
: GM {GM_value}
, direction {glm::normalize(rayDir)}
, diskHeight {disk_height}
, position {r_init}
{
    r     = glm::length(vec2(r_init.x, r_init.z));
    theta = std::atan2(r_init.z, r_init.x);
    y     = r_init.y;

    r_dot     = glm::dot(direction, vec3(std::cos(theta), 0.0f, std::sin(theta)));
    theta_dot = glm::dot(direction, vec3(-std::sin(theta) / r, 0.0f, std::cos(theta) / r));
    y_dot     = direction.y;

    v_t = glm::dot(direction, vec3(-std::sin(theta), 0.0f, std::cos(theta)));

    h = r * v_t;
}

void LightRay::update(float dt) {
    // Compute gravitational acceleration
    rho = std::sqrt(r * r + y * y);
    float g_magnitude = GM / (rho * rho);
    vec3 g_direction = -glm::normalize(vec3(r * std::cos(theta), y, r * std::sin(theta)));
    vec3 g = g_direction * g_magnitude;

    // Convert g to cylindrical coordinates
    float g_r     = glm::dot(g, vec3(std::cos(theta), 0.0f, std::sin(theta)));
    float g_y     = g.y;


    // Update velocities
    theta_dot  = h / glm::clamp((r * r), 0.0001f, 1000000.0f);
    r_dot     += (g_r + r * theta_dot * theta_dot) * dt;
    y_dot     += g_y * dt;


    // Update positions
    r     += r_dot * dt;
    theta += theta_dot * dt;
    y     += y_dot * dt;

    position = vec3(r * std::cos(theta), y, r * std::sin(theta));

    // Update Cartesian position
    t += dt;
}

LightRay::CollisionType LightRay::checkCollision(float dt, float sphereRadius, float diskRadius) {
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