#pragma once

#include "config.hpp"
#include "device_types.h"
#include "myGLFW.hpp"
#include <cuda_runtime.h>

struct CameraData;

void marchColumns(
    int xBegin,
    int xEnd,
    int width,
    int height,
    const CameraData* camData,
    vec3* framebuffer
);

__host__ void render(myGLFW* glfw, const CameraData& camData, std::vector<vec3>* framebuffer);

__global__ void render(
    vec3* fbo,
    int width,
    int height,
    int wordsPerThread,
    const CameraData* camData,
    vec3 sphereCenter,
    float sphereRadius,
    float diskRadius,
    float GM_value
);

__host__ void launchCudaRender(
    cudaGraphicsResource* cudaResource,
    int width,
    int height,
    dim3 gridSize,
    dim3 blockSize,
    int wordsPerThread,
    const CameraData* camData
);