#include "config.h"
#include "Raymarch.h"
#include "myGLFW.h"
#include <iostream>
#include <cmath>

constexpr float GM = 1.0f;

vec3 computeRayDirWorld(float u, float v,
    const vec3& camPos, const vec3& camForward, const vec3& camRight, const vec3& camUp,
    float fov, float width, float height)
{
    (void)camPos; (void)fov;
    float aspect = width / height;
    vec2  ndc    = { (2.f * u) - 1.f, 1.f - (2.f * v) };
    vec3  dir    = glm::normalize(vec3(ndc.x * aspect, ndc.y, -1.f));
    return glm::normalize(dir.x * camRight + dir.y * camUp + (-dir.z) * camForward);
}

void marchColumns(
    int xBegin, int xEnd,
    int width,  int height,
    const vec3& camPos, const vec3& camForward, const vec3& camRight, const vec3& camUp,
    float fov,
    const vec3& sphereCenter, float sphereRadius, float diskRadius,
    std::vector<vec3>& framebuffer)
{
    for (int i = xBegin; i < xEnd; ++i) {
        for (int j = 0; j < height; ++j) {
            float u = (i + 0.5f) / float(width);
            float v = (j + 0.5f) / float(height);
            vec3 rayDir = computeRayDirWorld(u, v, camPos, camForward, camRight, camUp, fov, width, height);
            Schwarzschild ray(camPos, rayDir, GM, 0.01f * diskRadius);
            framebuffer[j * width + i] = ray.traceRay(&ray, sphereCenter, sphereRadius, diskRadius);
        }
    }
}

int main() {
    const int width = 480, height = 270;

    myGLFW glfw(width, height);
    glfw.loadShaders(
        std::string(SHADER_DIR) + "/vertex.glsl",
        std::string(SHADER_DIR) + "/fragment.glsl"
    );
    glfw.setupFullscreenTriangle();
    glfw.setupSceneTexture();

    // Camera setup
    glm::vec3 camPos(0.f, 7.f, 7.f);
    CameraData camData(camPos);
    glm::vec3 initForward = glm::normalize(glm::vec3(0.f) - camPos);
    camData.yaw   = glm::degrees(std::atan2(initForward.z, initForward.x));
    camData.pitch = glm::degrees(std::asin(glm::clamp(initForward.y, -1.f, 1.f)));
    camData.updateCameraVectors();
    glfw.setCameraData(camData);

    const float fov              = glm::radians(45.f);
    const glm::vec3 sphereCenter = glm::vec3(0.f);
    constexpr float sphereRadius = 2.f * GM;
    constexpr float diskRadius   = sphereRadius * 2.5f;

    std::vector<vec3> framebuffer(width * height);
    int   fpsFrames  = 0;
    float lastFPSTime = (float)glfwGetTime();
    float lastTime    = (float)glfwGetTime();

    while (!glfw.shouldClose()) {
        glfw.pollEvents();
        glfw.clear();

        float now         = (float)glfwGetTime();
        camData.deltaTime = now - lastTime;
        lastTime          = now;

        if (now - lastFPSTime >= 5.0f) {
            std::cout << "FPS: " << fpsFrames / (now - lastFPSTime) << "\n";
            fpsFrames = 0; lastFPSTime = now;
        }
        ++fpsFrames;

        unsigned numThreads = std::max(1u, std::thread::hardware_concurrency());
        int columnsPerThread = width / numThreads;
        int remainderColumns = width % numThreads;
        std::vector<std::thread> threads;
        for (unsigned t = 0; t < numThreads; ++t) {
            int xBegin = t * columnsPerThread + std::min((int)t, remainderColumns);
            int xEnd   = xBegin + columnsPerThread + ((int)t < remainderColumns ? 1 : 0);
            threads.emplace_back(marchColumns,
                xBegin, xEnd, width, height,
                camData.camPos, camData.camForward, camData.camRight, camData.camUp,
                fov, sphereCenter, sphereRadius, diskRadius,
                std::ref(framebuffer));
        }
        for (auto& t : threads) t.join();

        glfw.uploadFramebuffer(framebuffer);
        glfw.draw();
        glfw.swapBuffers();
    }
    return 0;
}