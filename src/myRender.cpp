#include <cstdlib>
#include <cuda_runtime.h>
#include "myRender.hpp"
#include "config.hpp"
#include "cuda_runtime_api.h"
#include "myGLFW.hpp"
#include "Raymarch.hpp"
#include "particles.hpp"


#ifndef __CUDACC__
static const dim3 threadIdx {0, 0, 0};
static const dim3 blockIdx  {0, 0, 0};
static const dim3 blockDim  {1, 1, 1};
static const dim3 gridDim   {1, 1, 1};
#define __syncthreads()
#endif

__device__ __constant__ vec3 d_sphereCenter;
__device__ __constant__ float d_sphereRadius;
__device__ __constant__ float d_diskRadius;
__device__ __constant__ float d_GM_value;


// Error checking macros
// ---------------------
#define CUDA_CHECK(expr_to_check) do {                  \
    cudaError_t result  = expr_to_check;                \
    if(result != cudaSuccess)                           \
    {                                                   \
        fprintf(stderr,                                 \
                "CUDA Runtime Error: %s:%i:%d = %s\n",  \
                __FILE__,                               \
                __LINE__,                               \
                result,\
                cudaGetErrorString(result));            \
    }                                                   \
} while(0)

#define CUDA_ASYNC_CHECK() do {                         \
    cudaError_t result = cudaGetLastError();            \
    if(result != cudaSuccess)                           \
    {                                                   \
        fprintf(stderr,                                 \
                "CUDA Async Error: %s:%i:%d = %s\n",    \
                __FILE__,                               \
                __LINE__,                               \
                result,\
                cudaGetErrorString(result));            \
    }                                                   \
} while(0)


__host__ __device__ vec3 computeRayDirWorld(float u, float v,
    const CameraData *camData, float width, float height)
{
    float aspect = width / height;
    vec2  ndc    = { (2.f * u) - 1.f, 1.f - (2.f * v) };
    vec3  dir    = glm::normalize(vec3(ndc.x * aspect, ndc.y, -1.f));
    return glm::normalize(dir.x * camData->camRight + dir.y * camData->camUp + (-dir.z) * camData->camForward);
}


// GPU-Based Rendering
//--------------------
__global__ void render(vec4* fbo, int width, int height, int wordsPerThread, const CameraData* camData, ParticleManager* particleManager) {

    __shared__ unsigned char sharedCamBytes[sizeof(CameraData)];
    CameraData* sharedCamData = (CameraData*)(sharedCamBytes);
    if (threadIdx.x == 0 && threadIdx.y == 0) {
        *sharedCamData = *camData;
    }
    __syncthreads();
    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;
    int stride_x = blockDim.x * gridDim.x;
    int stride_y = blockDim.y * gridDim.y;

    for (int yy = y; yy < height; yy += stride_y) {
        for (int xx = x; xx < width; xx += stride_x) {
            float u = (xx + 0.5f) / float(width);
            float v = (yy + 0.5f) / float(height);

            vec3 rayDir = computeRayDirWorld(u, v, sharedCamData, width, height);
            
            Schwarzschild ray(
                sharedCamData->camPos,
                rayDir,
                d_GM_value,
                d_sphereCenter,
                d_sphereRadius,
                d_diskRadius
            );
            
            /*
            BaseRaymarch ray(
                sharedCamData->camPos,
                rayDir,
                d_GM_value,
                d_sphereCenter,
                d_sphereRadius,
                d_diskRadius
            );
            */

            vec4 pixel = static_cast<BaseRaymarch&>(ray).traceRay(particleManager);

            fbo[yy * width + xx] = pixel;
        }
    }
}
__host__ void init_gpu_constants() {
    cudaMemcpyToSymbol((const void*)&d_sphereCenter, &sphereCenter, sizeof(vec3));
    cudaMemcpyToSymbol((const void*)&d_sphereRadius, &sphereRadius, sizeof(float));
    cudaMemcpyToSymbol((const void*)&d_diskRadius, &diskRadius, sizeof(float));
    cudaMemcpyToSymbol((const void*)&d_GM_value, &GM, sizeof(float));
    
    CUDA_ASYNC_CHECK();
}

__host__ void launchCudaRender(
    cudaGraphicsResource* cudaResource,
    int width,
    int height,
    dim3 gridSize,
    dim3 blockSize,
    int wordsPerThread,
    const CameraData* camData,
    ParticleManager* particleManager
) {
    vec4* fbo;
    size_t numBytes;

    CUDA_ASYNC_CHECK();
    CUDA_CHECK(cudaGraphicsMapResources(1, &cudaResource, 0));
    CUDA_CHECK(cudaGraphicsResourceGetMappedPointer((void**)&fbo, &numBytes, cudaResource));
    #ifdef __CUDACC__
        render<<<gridSize, blockSize>>>(fbo, width, height, wordsPerThread, camData, particleManager);
    #endif
    CUDA_CHECK(cudaGraphicsUnmapResources(1, &cudaResource, 0));
}

#ifdef __CUDACC__
__global__ void updateParticlesDevice(ParticleManager* particleManager, float dt) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    int stride = blockDim.x * gridDim.x;
    while (idx < particleManager->numParticles) {
        particleManager->updateParticlesDevice(dt, idx);
        idx += stride;
    }
}
#endif
#ifdef __CUDACC__
__global__ void updateParticleGridDevice(ParticleManager* particleManager) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    int stride = blockDim.x * gridDim.x;
    while (idx < particleManager->numParticles) {
        particleManager->updateGridDevice(idx);
        idx += stride;
    }
}
#endif


__host__ void launchParticleUpdate(ParticleManager* particleManager, float dt, dim3 gridSize, dim3 blockSize) {
    #ifdef __CUDACC__
        updateParticlesDevice<<<gridSize, blockSize>>>(particleManager, dt);
        CUDA_ASYNC_CHECK();
    #endif
}

__host__ void launchParticleGridUpdate(ParticleManager* particleManager, dim3 gridSize, dim3 blockSize) {
    #ifdef __CUDACC__
        updateParticleGridDevice<<<gridSize, blockSize>>>(particleManager);
        CUDA_ASYNC_CHECK();
    #endif
}

// CPU-Based Rendering
//--------------------
void marchColumns(
    int xBegin, int xEnd,
    int width,  int height,
    const CameraData *camData,
    vec4 *framebuffer)
{
    for (int i = xBegin; i < xEnd; ++i) {
        for (int j = 0; j < height; ++j) {
            float u = (i + 0.5f) / float(width);
            float v = (j + 0.5f) / float(height);
            vec3 rayDir = computeRayDirWorld(u, v, camData, width, height);
            Schwarzschild ray(camData->camPos, rayDir, GM, sphereCenter, sphereRadius, diskRadius);
            framebuffer[j * width + i] = static_cast<BaseRaymarch&>(ray).traceRay();
        }
    }
}

__host__ void render(myGLFW *glfw, const CameraData& camData, std::vector<vec4> *framebuffer) {

    unsigned numThreads = std::max(1u, std::thread::hardware_concurrency());
    int columnsPerThread = width / numThreads;
    int remainderColumns = width % numThreads;
    std::vector<std::thread> threads;
    for (unsigned t = 0; t < numThreads; ++t) {
        int xBegin = t * columnsPerThread + std::min((int)t, remainderColumns);
        int xEnd   = xBegin + columnsPerThread + ((int)t < remainderColumns ? 1 : 0);
        threads.emplace_back(marchColumns, xBegin, xEnd, width, height, &camData, framebuffer->data());
    }
    for (auto& t : threads) t.join();

    glfw->uploadFramebuffer(framebuffer);

}