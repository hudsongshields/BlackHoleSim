#include "config.hpp"
#include "cuda_runtime.h"
#include "cuda_runtime_api.h"
#include "driver_types.h"
#include "myGLFW.hpp"
#include "myRender.hpp"
#include <iostream>
#include <cmath>
#include "myRender.hpp"
#include "particles.hpp"

#include <cuda_gl_interop.h>


extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
   #define CUDA_CHECK(expr_to_check) do {            \
    cudaError_t result  = expr_to_check;          \
    if(result != cudaSuccess)                     \
    {                                             \
        fprintf(stderr,                           \
                "CUDA Runtime Error: %s:%i:%d = %s\n", \
                __FILE__,                         \
                __LINE__,                         \
                result,                           \
                cudaGetErrorString(result));      \
        fflush(stderr);                           \
    }                                             \
} while(0)


int main(int argc, char** argv) {

    std::string device;
    if (argc == 1) {
        device = "cpu";
    }
    else if (argc == 2) {
        device = argv[1];
        if (device == "cpu") {
            std::cout << "Using CPU for rendering\n";
        } else if (device == "cuda") {
            std::cout << "Using CUDA for rendering\n";
        } else {
            std::cerr << "Unknown device: " << device << "\n";
            return -1;
        }
    }
    else {
        std::cerr << "Usage: " << argv[0] << " <device>\n";
        return -1;
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

    // Camera setup
    glm::vec3 camPos(0.f, -1.f, 15.f);
    CameraData camData(camPos);
    glm::vec3 initForward = glm::normalize(glm::vec3(0.f) - camPos);
    camData.yaw   = glm::degrees(std::atan2(initForward.z, initForward.x));
    camData.pitch = glm::degrees(std::asin(glm::clamp(initForward.y, -1.f, 1.f)));
    camData.updateCameraVectors();
    glfw.setCameraData(camData);

    // Background cubemap setup
    std::vector<std::string> faces = {
        std::string(ASSET_DIR) + "/skybox/cubemap_4/right.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/left.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/top.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/bottom.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/front.png",
        std::string(ASSET_DIR) + "/skybox/cubemap_4/back.png",
    };
    unsigned int cubemapTex = glfw.loadCubemap(faces);


    int   fpsFrames  = 0;
    float lastFPSTime = (float)glfwGetTime();
    float lastTime    = (float)glfwGetTime();

    if (device == "cuda") {
        std::cout << "Starting CUDA rendering\n";
        init_gpu_constants();
    }
    else if (device == "cpu") {
        std::cout << "Starting CPU rendering\n";
    }
    // FOR CPU
    std::vector<vec4> framebuffer(width * height);
    int numParticles = 400000;
    ParticleGenerator generator(numParticles);
    Particle* particleArray = generator.getArrayPtr();
    ParticleManager particleManager(numParticles, particleArray);


    // FOR GPU
    dim3 blockSize(32, 16);
    dim3 gridSize((width + blockSize.x - 1) / blockSize.x, (height + blockSize.y - 1) / blockSize.y);
    int N = width * height;
    int wordsPerThread = (N + (gridSize.x * blockSize.x) - 1) / (gridSize.x * blockSize.x);

    dim3 particleBlockSize(256);
    dim3 particleGridSize((numParticles + 255) / 256);

    GLuint pbo = 0;
    cudaGraphicsResource* cudaResource = nullptr;
    CameraData* camPtr = nullptr;
    ParticleManager* particleManagerPtr = nullptr;
    Particle* particleArrayPtr = nullptr;
    if (device == "cuda") {
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * sizeof(vec4), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        // configure device for interop
        unsigned int cudaDeviceCount = 0;
        int cudaDevices[1] = {0};
        CUDA_CHECK(cudaGLGetDevices(&cudaDeviceCount, cudaDevices, 1, cudaGLDeviceListAll));
        std::cout << "GL Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "GL Vendor: " << glGetString(GL_VENDOR) << std::endl;

        CUDA_CHECK(cudaSetDevice(cudaDevices[0]));
        CUDA_CHECK(cudaGraphicsGLRegisterBuffer(&cudaResource, pbo, cudaGraphicsRegisterFlagsWriteDiscard));
        
        CUDA_CHECK(cudaMalloc((void**)&camPtr, sizeof(CameraData)));
        CUDA_CHECK(cudaMemcpy(camPtr, &camData, sizeof(CameraData), cudaMemcpyHostToDevice));

        CUDA_CHECK(cudaMalloc((void**)&particleArrayPtr, numParticles * sizeof(Particle)));
        CUDA_CHECK(cudaMemcpy(particleArrayPtr, particleArray, numParticles * sizeof(Particle), cudaMemcpyHostToDevice));

        CUDA_CHECK(cudaMallocManaged((void**)&particleManagerPtr, sizeof(ParticleManager)));
        new (particleManagerPtr) ParticleManager(numParticles, particleArrayPtr);

    }
    
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
        // if (device == "cpu") render(&glfw, camData, &framebuffer);
        if (device == "cuda") {
            launchParticleUpdate(particleManagerPtr, 0.01f, particleGridSize, particleBlockSize);
            CUDA_CHECK(cudaMemcpy(camPtr, &camData, sizeof(CameraData), cudaMemcpyHostToDevice));
            CUDA_CHECK(cudaDeviceSynchronize());

            particleManagerPtr->resetGrid();
            CUDA_CHECK(cudaDeviceSynchronize());

            launchParticleGridUpdate(particleManagerPtr, particleGridSize, particleBlockSize);
            CUDA_CHECK(cudaDeviceSynchronize());

            launchCudaRender(cudaResource, width, height, gridSize, blockSize, wordsPerThread, camPtr, particleManagerPtr);
            CUDA_CHECK(cudaDeviceSynchronize());
            glfw.uploadFromPBO(pbo);
        }
        glfw.draw();
        glfw.swapBuffers();
    }

    if (cudaResource != nullptr) {
        cudaGraphicsUnregisterResource(cudaResource);
    }
    if (pbo != 0) {
        glDeleteBuffers(1, &pbo);
    }
    if (camPtr != nullptr) {
        cudaFree(camPtr);
    }
    if (particleManagerPtr != nullptr) {
        particleManagerPtr->~ParticleManager();
        cudaFree(particleManagerPtr);
    }
    if (particleArrayPtr != nullptr) {
        cudaFree(particleArrayPtr);
    }

    return 0;
}