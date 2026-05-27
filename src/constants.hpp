#include <glm/glm.hpp>
constexpr float GM = 1.0f;
const int width = 1920, height = 1080;

const float fov              = glm::radians(90.f);
const glm::vec3 sphereCenter = glm::vec3(0.f);
constexpr float sphereRadius = 1.f * GM;
constexpr float diskRadius   = 5.f;
constexpr float diskHeight   = 0.1f;