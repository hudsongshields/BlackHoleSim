#pragma once

#include "config.hpp"
#include "device_types.h"
#include "myGLFW.hpp"
#include <cuda_runtime.h>
#include "particles.hpp"

struct CameraData;

void marchColumns(
    int xBegin,
    int xEnd,
    int width,
    int height,
    const CameraData* camData,
    vec4* framebuffer
);

__host__ void render(myGLFW* glfw, const CameraData& camData, std::vector<vec4>* framebuffer);

__global__ void render(
    vec4* fbo,
    int width,
    int height,
    int wordsPerThread,
    const CameraData* camData,
    ParticleManager* particleManager
);

__host__ void launchCudaRender(
    cudaGraphicsResource* cudaResource,
    int width,
    int height,
    dim3 gridSize,
    dim3 blockSize,
    int wordsPerThread,
    const CameraData* camData,
    ParticleManager* particleManager
);
__global__ void updateParticlesDevice(ParticleManager* particleManager, float dt);
__host__ void launchParticleUpdate(ParticleManager* particleManager, float dt, dim3 gridSize, dim3 blockSize);
__host__ void launchParticleGridUpdate(ParticleManager* particleManager, dim3 gridSize, dim3 blockSize);

__host__ void init_gpu_constants();
__host__ void printCudaKernelDiagnostics(dim3 blockSize);