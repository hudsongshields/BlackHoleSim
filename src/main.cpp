#include "config.hpp"
#include "cuda_runtime.h"
#include "cuda_runtime_api.h"
#include "driver_types.h"
#include "myGLFW.hpp"
#include "myRender.hpp"
#include "particles.hpp"

#include <cuda_gl_interop.h>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <new>
#include <string>
#include <vector>

extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#define CUDA_CHECK(expr_to_check) do {                                \
    cudaError_t result = expr_to_check;                               \
    if (result != cudaSuccess) {                                      \
        std::fprintf(                                                 \
            stderr,                                                   \
            "CUDA Runtime Error: %s:%i:%d = %s\n",                   \
            __FILE__,                                                 \
            __LINE__,                                                 \
            result,                                                   \
            cudaGetErrorString(result)                                \
        );                                                            \
        std::fflush(stderr);                                          \
    }                                                                 \
} while (0)

static void pointCameraAt(CameraData& camData, const glm::vec3& target) {
    const glm::vec3 forward = glm::normalize(target - camData.camPos);

    camData.yaw = glm::degrees(std::atan2(forward.z, forward.x));
    camData.pitch = glm::degrees(
        std::asin(glm::clamp(forward.y, -1.0f, 1.0f))
    );

    camData.updateCameraVectors();
}

static void updateCinematicCamera(CameraData& camData, float elapsedSeconds) {
    // Slow orbit with subtle variation in speed, radius, and height
    const float orbitAngle =
        0.15f * elapsedSeconds
        + 0.18f * std::sin(0.08f * elapsedSeconds);

    const float radius =
        10.0f
        + 1.0f * std::sin(0.19f * elapsedSeconds);

    const float elevation =
        -1.0f
        + 1.4f * std::sin(0.11f * elapsedSeconds);

    camData.camPos = glm::vec3(
        radius * std::sin(orbitAngle),
        elevation,
        radius * std::cos(orbitAngle)
    );

    // Slightly vary the focal point so the motion feels less mechanical.
    const glm::vec3 lookTarget(
        0.35f * std::sin(0.10f * elapsedSeconds),
        0.20f * std::sin(0.14f * elapsedSeconds),
        0.0f
    );

    pointCameraAt(camData, lookTarget);
}

int main(int argc, char** argv) {
    std::string device = "cpu";
    bool deviceWasSpecified = false;
    bool useFPSCounter = true;
    bool cinematicMode = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "cpu" || arg == "cuda") {
            if (deviceWasSpecified) {
                std::cerr << "Only one rendering device may be specified.\n";
                std::cerr
                    << "Usage: " << argv[0]
                    << " [cpu|cuda] [--cinematic] [--no-fps]\n";
                return -1;
            }

            device = arg;
            deviceWasSpecified = true;
        }
        else if (arg == "--cinematic") {
            cinematicMode = true;
        }
        else if (arg == "--no-fps" || arg == "no-fps") {
            useFPSCounter = false;
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            std::cerr
                << "Usage: " << argv[0]
                << " [cpu|cuda] [--cinematic] [--no-fps]\n";
            return -1;
        }
    }

    std::cout
        << "Using "
        << (device == "cuda" ? "CUDA" : "CPU")
        << " for rendering\n";

    if (cinematicMode) {
        std::cout << "Cinematic camera enabled\n";
    }

    myGLFW glfw(width, height);

    glfw.loadShaders(
        std::string(SHADER_DIR) + "/vertex.glsl",
        std::string(SHADER_DIR) + "/fragment.glsl"
    );

    glfw.setupFullscreenTriangle();
    glfw.setupSceneTexture();

    glfw.setupBloomPipeline(
        std::string(SHADER_DIR) + "/vertex.glsl",
        std::string(SHADER_DIR) + "/bright_extract.glsl",
        std::string(SHADER_DIR) + "/gaussian_blur.glsl",
        std::string(SHADER_DIR) + "/bloom_composite.glsl"
    );

    glfw.setBloomSettings(1.0f, 1.2f, 1.0f, 10);

    // ------------
    // Camera setup
    // ------------

    glm::vec3 camPos(0.0f, -1.0f, 10.0f);
    CameraData camData(camPos);

    const glm::vec3 initForward =
        glm::normalize(glm::vec3(0.0f) - camPos);

    camData.yaw =
        glm::degrees(std::atan2(initForward.z, initForward.x));

    camData.pitch =
        glm::degrees(
            std::asin(glm::clamp(initForward.y, -1.0f, 1.0f))
        );

    camData.updateCameraVectors();
    glfw.setCameraData(camData);

    // ------------------
    // Background cubemap
    // ------------------

    const std::vector<std::string> faces = {
        std::string(ASSET_DIR) + "/skybox/cubemap_4/right.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/left.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/top.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/bottom.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/front.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/back.png",
    };

    const unsigned int cubemapTex = glfw.loadCubemap(faces);
    (void)cubemapTex;

    // -----------------------
    // Renderer initialization
    // -----------------------

    if (device == "cuda") {
        std::cout << "Starting CUDA rendering\n";
        init_gpu_constants();
    }
    else {
        std::cout << "Starting CPU rendering\n";
    }

    if (useFPSCounter) {
        std::cout << "FPS counter enabled\n";

        if (!glfw.setupFPSCounter("fonts/arial.ttf", 24)) {
            std::cout
                << "FPS counter font load failed at fonts/arial.ttf\n";

            std::cout
                << "Trying fallback: C:/Windows/Fonts/arial.ttf\n";

            if (!glfw.setupFPSCounter("C:/Windows/Fonts/arial.ttf", 24)) {
                std::cout
                    << "FPS counter disabled because font initialization failed\n";

                useFPSCounter = false;
            }
        }
    }
    else {
        std::cout << "FPS counter disabled\n";
    }

    // Particle settings
    // -----------------

    constexpr int numParticles = 500000;

    constexpr float particleDt = 0.002f;

    constexpr int cinematicPrewarmSteps = 3500;

    ParticleGenerator generator(numParticles);
    Particle* particleArray = generator.getArrayPtr();

    // CPU-side particle manager
    ParticleManager particleManager(numParticles, particleArray);

    // CUDA launch configuration
    // -------------------------

    const dim3 blockSize(32, 16);

    const dim3 gridSize(
        (width + blockSize.x - 1) / blockSize.x,
        (height + blockSize.y - 1) / blockSize.y
    );

    const int numPixels = width * height;

    const int wordsPerThread = (numPixels + (gridSize.x * blockSize.x) - 1) / (gridSize.x * blockSize.x);

    const dim3 particleBlockSize(256);

    const dim3 particleGridSize((numParticles + particleBlockSize.x - 1) / particleBlockSize.x);

    // CUDA resources
    // --------------

    GLuint pbo = 0;
    cudaGraphicsResource* cudaResource = nullptr;

    CameraData* camPtr = nullptr;
    ParticleManager* particleManagerPtr = nullptr;
    Particle* particleArrayPtr = nullptr;

    if (device == "cuda") {
        glGenBuffers(1, &pbo);

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);

        glBufferData(
            GL_PIXEL_UNPACK_BUFFER,
            width * height * sizeof(vec4),
            nullptr,
            GL_DYNAMIC_DRAW
        );

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        unsigned int cudaDeviceCount = 0;
        int cudaDevices[1] = {0};

        CUDA_CHECK(
            cudaGLGetDevices(
                &cudaDeviceCount,
                cudaDevices,
                1,
                cudaGLDeviceListAll
            )
        );

        if (cudaDeviceCount == 0) {
            std::cerr
                << "No CUDA device associated with the OpenGL context was found\n";

            return -1;
        }

        std::cout << "GL Renderer: " << glGetString(GL_RENDERER) << "\n";
        std::cout << "GL Vendor: " << glGetString(GL_VENDOR) << "\n";

        CUDA_CHECK(cudaSetDevice(cudaDevices[0]));

        CUDA_CHECK(
            cudaGraphicsGLRegisterBuffer(
                &cudaResource,
                pbo,
                cudaGraphicsRegisterFlagsWriteDiscard
            )
        );

        CUDA_CHECK(cudaMalloc((void**)&camPtr, sizeof(CameraData)));

        CUDA_CHECK(
            cudaMemcpy(
                camPtr,
                &camData,
                sizeof(CameraData),
                cudaMemcpyHostToDevice
            )
        );

        CUDA_CHECK(
            cudaMalloc(
                (void**)&particleArrayPtr,
                numParticles * sizeof(Particle)
            )
        );

        CUDA_CHECK(
            cudaMemcpy(
                particleArrayPtr,
                particleArray,
                numParticles * sizeof(Particle),
                cudaMemcpyHostToDevice
            )
        );

        CUDA_CHECK(
            cudaMallocManaged(
                (void**)&particleManagerPtr,
                sizeof(ParticleManager)
            )
        );

        new (particleManagerPtr)
            ParticleManager(numParticles, particleArrayPtr);
    }

    // Hidden cinematic prewarm
    // ------------------------
    // This evolves the particle system before the first visible frame

    if (device == "cuda" && cinematicMode) {
        std::cout
            << "Prewarming particles for cinematic opening with "
            << cinematicPrewarmSteps
            << " hidden updates...\n";

        for (int step = 0; step < cinematicPrewarmSteps; ++step) {
            launchParticleUpdate(
                particleManagerPtr,
                particleDt,
                particleGridSize,
                particleBlockSize
            );

            CUDA_CHECK(cudaDeviceSynchronize());

            particleManagerPtr->resetGrid();

            CUDA_CHECK(cudaDeviceSynchronize());

            launchParticleGridUpdate(
                particleManagerPtr,
                particleGridSize,
                particleBlockSize
            );

            CUDA_CHECK(cudaDeviceSynchronize());

            if (step % 100 == 0) {
                glfw.pollEvents();
            }
        }

        std::cout << "Particle prewarm complete\n";
    }

    // Begin frame timing only after the hidden prewarm
    const float cinematicStartTime =
        static_cast<float>(glfwGetTime());

    float lastFPSTime =
        static_cast<float>(glfwGetTime());

    float lastTime =
        static_cast<float>(glfwGetTime());

    int fpsFrames = 0;
    float smoothedFPS = 0.0f;

    // CPU-side framebuffer
    std::vector<vec4> framebuffer(width * height);

    // Main loop
    // ---------

    while (!glfw.shouldClose()) {
        glfw.pollEvents();
        glfw.clear();

        const float now =
            static_cast<float>(glfwGetTime());

        camData.deltaTime = now - lastTime;
        lastTime = now;

        if (cinematicMode) {
            updateCinematicCamera(
                camData,
                now - cinematicStartTime
            );
            glfw.setCameraData(camData);
        }

        if (camData.deltaTime > 0.000001f) {
            const float instantFPS =
                1.0f / camData.deltaTime;

            smoothedFPS =
                (smoothedFPS == 0.0f)
                ? instantFPS
                : glm::mix(smoothedFPS, instantFPS, 0.1f);
        }

        if (now - lastFPSTime >= 5.0f) {
            std::cout
                << "FPS: "
                << fpsFrames / (now - lastFPSTime)
                << "\n";

            fpsFrames = 0;
            lastFPSTime = now;
        }

        ++fpsFrames;

        if (device == "cpu") {
            render(
                &glfw,
                camData,
                &framebuffer
            );
        }
        else if (device == "cuda") {
            launchParticleUpdate(
                particleManagerPtr,
                particleDt,
                particleGridSize,
                particleBlockSize
            );

            CUDA_CHECK(
                cudaMemcpy(
                    camPtr,
                    &camData,
                    sizeof(CameraData),
                    cudaMemcpyHostToDevice
                )
            );

            CUDA_CHECK(cudaDeviceSynchronize());

            particleManagerPtr->resetGrid();

            CUDA_CHECK(cudaDeviceSynchronize());

            launchParticleGridUpdate(
                particleManagerPtr,
                particleGridSize,
                particleBlockSize
            );

            CUDA_CHECK(cudaDeviceSynchronize());

            launchCudaRender(
                cudaResource,
                width,
                height,
                gridSize,
                blockSize,
                wordsPerThread,
                camPtr,
                particleManagerPtr
            );

            CUDA_CHECK(cudaDeviceSynchronize());

            glfw.uploadFromPBO(pbo);
        }

        glfw.draw();

        if (useFPSCounter) {
            glfw.drawFPSCounter(smoothedFPS);
        }

        glfw.swapBuffers();
    }

    // Cleanup
    // -------

    if (cudaResource != nullptr) {
        CUDA_CHECK(cudaGraphicsUnregisterResource(cudaResource));
    }

    if (pbo != 0) {
        glDeleteBuffers(1, &pbo);
    }

    if (camPtr != nullptr) {
        CUDA_CHECK(cudaFree(camPtr));
    }

    if (particleManagerPtr != nullptr) {
        particleManagerPtr->~ParticleManager();
        CUDA_CHECK(cudaFree(particleManagerPtr));
    }

    if (particleArrayPtr != nullptr) {
        CUDA_CHECK(cudaFree(particleArrayPtr));
    }

    return 0;
}