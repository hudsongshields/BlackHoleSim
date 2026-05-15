#include "GLFW/glfw3.h"
#include "config.hpp"
#include "Raymarch.hpp"
#include "cuda_runtime_api.h"
#include "driver_types.h"
#include "myGLFW.hpp"
#include "myRender.hpp"
#include <iostream>
#include <cmath>

#include <cuda_runtime.h>
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
                result,\
                cudaGetErrorString(result));      \
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

    // Camera setup
    glm::vec3 camPos(0.f, 7.f, 7.f);
    CameraData camData(camPos);
    glm::vec3 initForward = glm::normalize(glm::vec3(0.f) - camPos);
    camData.yaw   = glm::degrees(std::atan2(initForward.z, initForward.x));
    camData.pitch = glm::degrees(std::asin(glm::clamp(initForward.y, -1.f, 1.f)));
    camData.updateCameraVectors();
    glfw.setCameraData(camData);


    int   fpsFrames  = 0;
    float lastFPSTime = (float)glfwGetTime();
    float lastTime    = (float)glfwGetTime();

    if (device == "cuda") {
        std::cout << "Starting CUDA rendering\n";
        
    }
    else if (device == "cpu") {
        std::cout << "Starting CPU rendering\n";
    }
    // FOR CPU
    std::vector<vec3> framebuffer(width * height);

    // FOR GPU
    dim3 blockSize(32, 16);
    dim3 gridSize((width + blockSize.x - 1) / blockSize.x, (height + blockSize.y - 1) / blockSize.y);
    int N = width * height;
    int wordsPerThread = (N + (gridSize.x * blockSize.x) - 1) / (gridSize.x * blockSize.x);
    

    GLuint pbo = 0;
    cudaGraphicsResource* cudaResource = nullptr;
    CameraData* camPtr = nullptr;
    if (device == "cuda") {
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * sizeof(vec3), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        unsigned int cudaDeviceCount = 0;
        int cudaDevices[1] = {0};
        CUDA_CHECK(cudaGLGetDevices(&cudaDeviceCount, cudaDevices, 1, cudaGLDeviceListAll));
        std::cout << "GL Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "GL Vendor: " << glGetString(GL_VENDOR) << std::endl;
        CUDA_CHECK(cudaSetDevice(cudaDevices[0]));
        CUDA_CHECK(cudaGraphicsGLRegisterBuffer(&cudaResource, pbo, cudaGraphicsRegisterFlagsWriteDiscard));
        
        CUDA_CHECK(cudaMalloc((void**)&camPtr, sizeof(CameraData)));
        CUDA_CHECK(cudaMemcpy(camPtr, &camData, sizeof(CameraData), cudaMemcpyHostToDevice));
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
        if (device == "cpu") render(&glfw, camData, &framebuffer);
        if (device == "cuda") {
            cudaMemcpy(camPtr, &camData, sizeof(CameraData), cudaMemcpyHostToDevice);
            launchCudaRender(cudaResource, width, height, gridSize, blockSize, wordsPerThread, camPtr);
            cudaDeviceSynchronize();
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

    return 0;
}