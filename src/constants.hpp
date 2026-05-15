#include <glm/glm.hpp>
constexpr float GM = 1.0f;
const int width = 1200, height = 800;

const float fov              = glm::radians(45.f);
const glm::vec3 sphereCenter = glm::vec3(0.f);
constexpr float sphereRadius = 2.f * GM;
constexpr float diskRadius   = sphereRadius * 2.5f;