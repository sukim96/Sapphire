// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#include <cublas_v2.h>
#include <Motutapu/compute/cuda/CudaParams.cuh>
#include <Motutapu/compute/cuda/Memory.cuh>
#include <Motutapu/compute/cuda/dense/Basic.cuh>
#include <Motutapu/compute/cuda/dense/Gemm.cuh>
#include <Motutapu/compute/cuda/dense/GemmKernel.cuh>
#include <cassert>

namespace Motutapu::Compute::Cuda::Dense
{
//! All size parameters should be at least 1
//! batch sizes must be multiple of each other
__host__ void Gemm(unsigned int totalSize, float* out, float* A, float* B,
                   float* C, unsigned int M, unsigned int N, unsigned int K,
                   cublasHandle_t* handle)
{
    cublasSetMathMode(*handle, CUBLAS_TF32_TENSOR_OP_MATH);

    const float alpha = 1.0f;
    const float beta = 1.0f;

    const auto strideA = M * K;
    const auto strideB = K * N;
    const auto strideOut = M * N;

    float* ptrA = A;
    float* ptrB = B;
    float* ptrC = C;
    float* ptrOut = out;

    CopyGpuToGpu(ptrOut, ptrC, totalSize);

    auto status = cublasGemmStridedBatchedEx(
        *handle, CUBLAS_OP_N, CUBLAS_OP_N, N, M, K, &alpha, ptrB, CUDA_R_32F, N,
        strideB, ptrA, CUDA_R_32F, K, strideA, &beta, ptrOut, CUDA_R_32F, N,
        strideOut, totalSize / strideOut, CUBLAS_COMPUTE_32F_FAST_TF32,
        CUBLAS_GEMM_DEFAULT_TENSOR_OP);

    assert(status == CUBLAS_STATUS_SUCCESS);
}

//! Broadcasts operations matrix-wise
//! while broadcastC is false, broadcastOut must be false
__host__ void GemmMatrixWiseBroadcast(float* out, float* A, float* B, float* C,
                                      unsigned int M, unsigned int N,
                                      unsigned int K, unsigned int batchSize,
                                      bool broadcastA, bool broadcastB,
                                      bool broadcastC)
{
    cublasHandle_t handle;
    cublasStatus_t stat = cublasCreate(&handle);

    cublasSetMathMode(handle, CUBLAS_TF32_TENSOR_OP_MATH);

    const float alpha = 1.0f;
    const float beta = 1.0f;

    const auto strideA = (broadcastA ? 0 : (M * K));
    const auto strideB = (broadcastB ? 0 : (K * N));
    const auto strideOut = M * N;

    if (broadcastC)
    {
        CopyGpuToGpuBroadcast(out, C, M * N * batchSize, M * N);
    }
    else
        CopyGpuToGpu(out, C, M * N * batchSize);

    cublasGemmStridedBatchedEx(handle, CUBLAS_OP_N, CUBLAS_OP_N, N, M, K,
                               &alpha, B, CUDA_R_32F, N, strideB, A, CUDA_R_32F,
                               K, strideA, &beta, out, CUDA_R_32F, N, strideOut,
                               batchSize, CUBLAS_COMPUTE_32F_FAST_TF32,
                               CUBLAS_GEMM_DEFAULT_TENSOR_OP);

    cublasDestroy(handle);
}

__host__ unsigned int Gcd(unsigned int a, unsigned int b)
{
    if (!a)
        return b;
    return Gcd(b % a, a);
}

__host__ unsigned int FindGCD(unsigned int arr[], int n)
{
    unsigned int result = arr[0];
    for (int i = 1; i < n; i++)
    {
        result = Gcd(arr[i], result);

        if (result == 1)
        {
            return 1;
        }
    }
    return result;
}

__host__ void GemmTensor(float* out, float* A, float* B, float* C,
                         unsigned int paddedM, unsigned int paddedN,
                         unsigned int paddedK, unsigned int batchSize,
                         bool broadcastA, bool broadcastB, bool broadcastC)
{
    static constexpr unsigned int tileDim = 16;
    const auto chunkDimM = paddedM / 16;
    const auto chunkDimK = paddedK / 16;
    const auto chunkDimN = paddedN / 16;

    unsigned int arr[] = { chunkDimM, chunkDimK, chunkDimN };

    const auto maxTileSize = FindGCD(arr, sizeof(arr) / sizeof(arr[0]));

    unsigned int chunkSize;
    if (maxTileSize % 2 == 1)
    {
        chunkSize = 1;
    }
    else
    {
        if (maxTileSize % 4 == 0)
            chunkSize = 4;
        else
            chunkSize = 2;
    }

    auto* streams =
        static_cast<cudaStream_t*>(malloc(sizeof(cudaStream_t) * batchSize));

    for (unsigned int batchIdx = 0; batchIdx < batchSize; ++batchIdx)
    {
        cudaStreamCreate(&streams[batchIdx]);
    }
    for (unsigned int chunkIdxK = 0; chunkIdxK * chunkSize * tileDim < paddedN;
         ++chunkIdxK)
    {
        for (unsigned int batchIdx = 0; batchIdx < batchSize; ++batchIdx)
        {
            float* ptrOut = out + paddedM * paddedN * batchIdx;
            const float* ptrA =
                A + paddedM * paddedK * (broadcastA ? 1 : batchIdx);
            const float* ptrB =
                B + paddedK * paddedN * (broadcastB ? 1 : batchIdx);
            const float* ptrC =
                C + paddedM * paddedN * (broadcastC ? 1 : batchIdx);

            const dim3 numBlocks(chunkDimM, chunkDimN);

            WmmaGemm<<<numBlocks, chunkSize * chunkSize * 32, 0,
                       streams[batchIdx]>>>(ptrOut, ptrA, ptrB, ptrC, paddedK,
                                            paddedN, chunkIdxK, chunkSize);
        }

        for (unsigned int batchIdx = 0; batchIdx < batchSize; ++batchIdx)
        {
            cudaStreamSynchronize(streams[batchIdx]);
        }
    }

    for (unsigned int batchIdx = 0; batchIdx < batchSize; ++batchIdx)
    {
        cudaStreamDestroy(streams[batchIdx]);
    }

    free(streams);
}

__host__ void GemmNormal(float* out, float* A, float* B, float* C,
                         unsigned int paddedM, unsigned int paddedN,
                         unsigned int paddedK, unsigned int batchSize,
                         bool broadcastA, bool broadcastB, bool broadcastC)
{
    auto* streams =
        static_cast<cudaStream_t*>(malloc(sizeof(cudaStream_t) * batchSize));
    unsigned int blockSize = paddedM * paddedN / 1024 + 1;

    for (unsigned int batchIdx = 0; batchIdx < batchSize; batchIdx++)
    {
        cudaStreamCreate(&streams[batchIdx]);

        float* ptrOut = out + paddedM * paddedN * batchIdx;
        const float* ptrA = A + paddedM * paddedK * (broadcastA ? 0 : batchIdx);
        const float* ptrB = B + paddedK * paddedN * (broadcastB ? 0 : batchIdx);
        const float* ptrC = C + paddedM * paddedN * (broadcastC ? 0 : batchIdx);
        GemmSimple<<<blockSize, 1024, 0, streams[batchIdx]>>>(
            ptrOut, ptrA, ptrB, ptrC, paddedM, paddedN, paddedK);
    }

    for (unsigned int batchIdx = 0; batchIdx < batchSize; batchIdx++)
    {
        cudaStreamSynchronize(streams[batchIdx]);
        cudaStreamDestroy(streams[batchIdx]);
    }

    free(streams);
}
}  // namespace Motutapu::Compute::Cuda::Dense
