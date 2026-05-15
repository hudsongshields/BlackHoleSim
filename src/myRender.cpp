#include <cstdlib>
#include <cuda_runtime.h>
#include "myRender.hpp"
#include "config.hpp"
#include "cuda_runtime_api.h"
#include "myGLFW.hpp"
#include "Raymarch.hpp"

#ifndef __CUDACC__
static const dim3 threadIdx {0, 0, 0};
static const dim3 blockIdx  {0, 0, 0};
static const dim3 blockDim  {1, 1, 1};
static const dim3 gridDim   {1, 1, 1};
#endif

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
__global__ void render(vec3* fbo, int width, int height, int wordsPerThread, const CameraData* camData, 
    vec3 sphereCenter, float sphereRadius, float diskRadius, float GM_value) {

    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;
    int stride_x = blockDim.x * gridDim.x;
    int stride_y = blockDim.y * gridDim.y;

    for (int i {0}; i < wordsPerThread; ++i) {
        int x_index = x + i * stride_x;
        int y_index = y + i * stride_y;
        if (x_index < width && y_index < height) {
            float u = (x_index + 0.5f) / float(width);
            float v = (y_index + 0.5f) / float(height);
            vec3 rayDir = computeRayDirWorld(u, v, camData, width, height);
            Schwarzschild ray(camData->camPos, rayDir, GM_value, sphereCenter, sphereRadius, diskRadius);

            vec3 pixel = static_cast<BaseRaymarch&>(ray).traceRay();
            // force white
            // vec3 pixel = vec3(0.5f, 0.5f, 0.5f);

            fbo[y_index * width + x_index] = pixel;
        }
    }
}

__host__ void launchCudaRender(
    cudaGraphicsResource* cudaResource,
    int width,
    int height,
    dim3 gridSize,
    dim3 blockSize,
    int wordsPerThread,
    const CameraData* camData
) {
    vec3* fbo;
    size_t numBytes;
    float GM_value = GM;
    // get last cuda error
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA Runtime Error before kernel launch: %s\n", cudaGetErrorString(err));
        std::exit(-1);
    }
    CUDA_CHECK(cudaGraphicsMapResources(1, &cudaResource, 0));
    CUDA_CHECK(cudaGraphicsResourceGetMappedPointer((void**)&fbo, &numBytes, cudaResource));
#ifdef __CUDACC__
    render<<<gridSize, blockSize>>>(fbo, width, height, wordsPerThread, camData, ::sphereCenter, ::sphereRadius, ::diskRadius, GM_value);
#endif
    CUDA_CHECK(cudaGraphicsUnmapResources(1, &cudaResource, 0));
}



// CPU-Based Rendering
//--------------------
void marchColumns(
    int xBegin, int xEnd,
    int width,  int height,
    const CameraData *camData,
    vec3 *framebuffer)
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

__host__ void render(myGLFW *glfw, const CameraData& camData, std::vector<vec3> *framebuffer) {

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