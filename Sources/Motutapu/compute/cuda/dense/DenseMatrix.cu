// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.


#include <Motutapu/compute/cuda/dense/DenseMatrix.hpp>
#include <mma.h>

namespace Motutapu::Cuda::Dense
{

__host__ unsigned int Gcd(unsigned int a, unsigned int b)
{
    if (!a)
        return b;
    return Gcd(b % a, a);
}

__host__ unsigned int FindGCD(unsigned int arr[], int n)
{
    int result = arr[0];
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

__host__ void GemmTensorCore(half* out, half* A, half* B, unsigned int paddedN,
                             unsigned int paddedK, unsigned int paddedM,
                             unsigned int batchSize)
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

    cudaStream_t streams[batchSize];
    for (unsigned int batchIdx = 0; batchIdx < batchSize; ++batchIdx)
    {
        cudaStreamCreate(&streams[batchIdx]);
    }
    for (unsigned int chunkIdxK = 0; chunkIdxK * chunkSize * tileDim < paddedN;
         ++chunkIdxK)
    {
        for (unsigned int batchIdx = 0; batchIdx < batchSize; ++batchIdx)
        {
            half* ptrOut = out + paddedM * paddedN * batchIdx;
            const half* ptrA = A + paddedM * paddedK * batchIdx;
            const half* ptrB = B + paddedK * paddedN * batchIdx;

            const dim3 numBlocks(chunkDimM, chunkDimN);

            if (chunkSize == 4)
                WmmaGemmHalf<4><<<numBlocks, chunkSize * chunkSize * 32, 0,
                                  streams[batchIdx]>>>(
                    ptrOut, ptrA, ptrB, paddedK, paddedN, chunkIdxK);
            if (chunkSize == 2)
                WmmaGemmHalf<2><<<numBlocks, chunkSize * chunkSize * 32, 0,
                                  streams[batchIdx]>>>(
                    ptrOut, ptrA, ptrB, paddedK, paddedN, chunkIdxK);
            if (chunkSize == 1)
                WmmaGemmHalf<1><<<numBlocks, chunkSize * chunkSize * 32, 0,
                                  streams[batchIdx]>>>(
                    ptrOut, ptrA, ptrB, paddedK, paddedN, chunkIdxK);
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
}

__host__ void GemmNormalFloat(float* out, float* A, float* B,
                              unsigned int paddedN, unsigned int paddedK,
                              unsigned int paddedM, unsigned int batchSize)
{
    static constexpr unsigned int tileDim = 8;
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

    cudaStream_t streams[batchSize];
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
            float* ptrA = A + paddedM * paddedK * batchIdx;
            float* ptrB = B + paddedK * paddedN * batchIdx;

            const dim3 numBlocks(chunkDimM, chunkDimN);

            if (chunkSize == 4)
                Gemm<float, tileDim, 4><<<numBlocks, chunkSize * chunkSize * 32,
                                          0, streams[batchIdx]>>>(
                    ptrOut, ptrA, ptrB, paddedK, paddedN, chunkIdxK);
            if (chunkSize == 2)
                Gemm<float, tileDim, 2><<<numBlocks, chunkSize * chunkSize * 32,
                                          0, streams[batchIdx]>>>(
                    ptrOut, ptrA, ptrB, paddedK, paddedN, chunkIdxK);
            if (chunkSize == 1)
                Gemm<float, tileDim, 1><<<numBlocks, chunkSize * chunkSize * 32,
                                          0, streams[batchIdx]>>>(
                    ptrOut, ptrA, ptrB, paddedK, paddedN, chunkIdxK);
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
}

__host__ void GemmNormalHalf(half* out, half* A, half* B, unsigned int paddedN,
                             unsigned int paddedK, unsigned int paddedM,
                             unsigned int batchSize)
{
    static constexpr unsigned int tileDim = 8;
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

    cudaStream_t streams[batchSize];
    for (unsigned int batchIdx = 0; batchIdx < batchSize; ++batchIdx)
    {
        cudaStreamCreate(&streams[batchIdx]);
    }
    for (unsigned int chunkIdxK = 0; chunkIdxK * chunkSize * tileDim < paddedN;
         ++chunkIdxK)
    {
        for (unsigned int batchIdx = 0; batchIdx < batchSize; ++batchIdx)
        {
            half* ptrOut = out + paddedM * paddedN * batchIdx;
            half* ptrA = A + paddedM * paddedK * batchIdx;
            half* ptrB = B + paddedK * paddedN * batchIdx;

            const dim3 numBlocks(chunkDimM, chunkDimN);

            if (chunkSize == 4)
                Gemm<half, tileDim, 4><<<numBlocks, chunkSize * chunkSize * 32,
                                         0, streams[batchIdx]>>>(
                    ptrOut, ptrA, ptrB, paddedK, paddedN, chunkIdxK);
            if (chunkSize == 2)
                Gemm<half, tileDim, 2><<<numBlocks, chunkSize * chunkSize * 32,
                                         0, streams[batchIdx]>>>(
                    ptrOut, ptrA, ptrB, paddedK, paddedN, chunkIdxK);
            if (chunkSize == 1)
                Gemm<half, tileDim, 1><<<numBlocks, chunkSize * chunkSize * 32,
                                         0, streams[batchIdx]>>>(
                    ptrOut, ptrA, ptrB, paddedK, paddedN, chunkIdxK);
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
}
}
