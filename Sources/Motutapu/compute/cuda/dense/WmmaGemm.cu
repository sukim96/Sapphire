// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.


#include <Motutapu/compute/cuda/dense/DenseMatmul.hpp>
#include <mma.h>

#define WARP_SIZE 32
#define CHUNK_K 4

namespace Motutapu::Cuda::Dense
{
using namespace nvcuda;

constexpr size_t tileDim = 16;
constexpr size_t chunkSize = CHUNK_K;

//! Matrix is divided into chunk with (chunkSize * chunkSize) tiles/
//! Each chunk has tile, which is (16 x 16)
//! Each warp computes each tile as output
//! for 64KB shared memory, use chunkSize of 4
//! We use chunkSize x chunkSize warps in total
//! Each warp is responsible for 16 x 16 tile in output matrix

//! Each chunk is composed of 16*16 elements
//! in case of float(fp32), each matrix tile will take
//! (16 x 16) x (chunkSize chunkSize) x 4;
//! in case of half(fp16), each matrix tile will take
//! (16 x 16) x (chunkSize x chunkSize) x 2 = 8 KB
//! se use chunkSize of 4 for half considering shift
//! we reserve some space on shared memory for shift space to protect from
//! bank conflicts
//! for shared memory more than 64KB, we use chunkSize of 4
//! for float(fp32) data types
//! and we use chunkSize of 8 for half(fp16) data types

//! Each warp will pick each row of A, and column of B
//! and perform fma(fused multiply add) with iterations
//! Iteration number will be same as chunk size

//! We assume A, B, and Out is in row-wise, and 256bit aligned
//! chunkSize must be 4 if shared memory is equal or less than 64kB
//! otherwise it can be set 8

//! chunkIdxK should be called sequentially for available chunk sequences in K direction
//! to make sure results are added
__global__ void WmmaGemmHalf(half* Out, half* A, half* B,
                             size_t paddedColSizeA, size_t paddedColSizeB,
                             size_t paddedColSizeOut, size_t chunkIdxK)
{
    // Minimum shift we can use with 256bit alignment while protecting from bank
    // conflicts;
    constexpr size_t shift = 32 / sizeof(half);
    constexpr size_t shiftedSharedMemoryColSize = chunkSize * tileDim + shift;

    //! chunkSize*tileDim is 32 or 64 depending ong size of
    __shared__ half sharedMemory[chunkSize * tileDim * 2][
        shiftedSharedMemoryColSize];

    const size_t chunkIdxM = blockIdx.x;
    const size_t chunkIdxN = blockIdx.y;

    const size_t warpIdx = threadIdx.x / WARP_SIZE;
    const size_t laneIdx = threadIdx.x % WARP_SIZE;

    const size_t tileRowIdx = warpIdx / 4;
    const size_t tileColIdx = warpIdx % 4;

    const half* chunkPtrA = A + paddedColSizeA * chunkIdxM * tileDim * chunkSize
                            +
                            chunkIdxK * tileDim * chunkSize;
    const half* chunkPtrB = B + paddedColSizeB * chunkIdxK * tileDim * chunkSize
                            +
                            chunkIdxN * tileDim * chunkSize;
    half* chunkPtrOut = Out + paddedColSizeOut * chunkIdxM * tileDim * chunkSize
                        +
                        chunkIdxN * tileDim * chunkSize;

    const half* tilePtrA = chunkPtrA + paddedColSizeA * tileRowIdx * tileDim;
    const half* tilePtrB = chunkPtrB + tileColIdx * tileDim;
    half* tilePtrOut = chunkPtrOut + paddedColSizeOut * tileRowIdx * tileDim +
                       tileColIdx * tileDim;

    const size_t matrixBOffset = chunkSize * tileDim;

    //! For half of the warps, copy matrix A while other half copies B
    const half* copyPtr;

    size_t sharedMemCopyRowIdx;
    if (laneIdx % 2)
    {
        copyPtr = tilePtrA + paddedColSizeA * (laneIdx / 2);
        sharedMemCopyRowIdx = tileRowIdx + laneIdx / 2;
    }
    else
    {
        copyPtr = tilePtrB + paddedColSizeB * (laneIdx / 2);
        sharedMemCopyRowIdx = matrixBOffset + tileRowIdx + laneIdx / 2;
    }

    //! Load the matrix to shared memory
    //! each thread copies consecutive row from their src determined previously
#pragma unroll
    for (int i = 0; i < tileDim; i++)
    {
        const size_t sharedMemCopyColIdx = tileColIdx * tileDim + i;
        sharedMemory[sharedMemCopyRowIdx][sharedMemCopyColIdx + i] =
            *(copyPtr + i);
    }

    //! Load shared memory to fragments and accumulate
    wmma::fragment<wmma::matrix_a, tileDim, tileDim, tileDim, half,
                   wmma::row_major>
        fragA;
    wmma::fragment<wmma::matrix_b, tileDim, tileDim, tileDim, half,
                   wmma::row_major>
        fragB;
    wmma::fragment<wmma::accumulator, tileDim, tileDim, tileDim, half> fragAcc;
    wmma::fragment<wmma::accumulator, tileDim, tileDim, tileDim, half> fragOut;

    wmma::fill_fragment(fragAcc, 0.0f);

    for (int i = 0; i < chunkSize; ++i)
    {
        wmma::load_matrix_sync(fragA, &sharedMemory[tileRowIdx][tileDim * i],
                               shiftedSharedMemoryColSize);
        wmma::load_matrix_sync(
            fragB, &sharedMemory[tileDim * i + matrixBOffset][tileColIdx],
            shiftedSharedMemoryColSize);
        wmma::mma_sync(fragAcc, fragA, fragB, fragAcc);
    }

    wmma::load_matrix_sync(fragOut, tilePtrOut, paddedColSizeOut,
                           wmma::mem_row_major);

    wmma::store_matrix_sync(tilePtrOut, fragAcc, paddedColSizeOut,
                            wmma::mem_row_major);
}

__global__ void WmmaGemmFloat(float* Out, half* A, half* B,
                              size_t paddedColSizeA, size_t paddedColSizeB,
                              size_t paddedColSizeOut, size_t chunkIdxK)
{
    // Minimum shift we can use with 256bit alignment while protecting from bank
    // conflicts;
    constexpr size_t shift = 32 / sizeof(float);
    constexpr size_t shiftedSharedMemoryColSize = chunkSize * tileDim + shift;

    //! chunkSize*tileDim is 32 or 64 depending ong size of
    __shared__ half sharedMemory[chunkSize * tileDim * 2][
        shiftedSharedMemoryColSize];

    const size_t chunkIdxM = blockIdx.x;
    const size_t chunkIdxN = blockIdx.y;

    const size_t warpIdx = threadIdx.x / WARP_SIZE;
    const size_t laneIdx = threadIdx.x % WARP_SIZE;

    const size_t tileRowIdx = warpIdx / 4;
    const size_t tileColIdx = warpIdx % 4;

    const half* chunkPtrA = A + paddedColSizeA * chunkIdxM * tileDim * chunkSize
                            + chunkIdxK * tileDim * chunkSize;
    const half* chunkPtrB = B + paddedColSizeB * chunkIdxK * tileDim * chunkSize
                            + chunkIdxN * tileDim * chunkSize;
    float* chunkPtrOut = Out + paddedColSizeOut * chunkIdxM * tileDim *
                         chunkSize
                         +
                         chunkIdxN * tileDim * chunkSize;

    const half* tilePtrA = chunkPtrA + paddedColSizeA * tileRowIdx * tileDim;
    const half* tilePtrB = chunkPtrB + tileColIdx * tileDim;
    float* tilePtrOut = chunkPtrOut + paddedColSizeOut * tileRowIdx * tileDim +
                        tileColIdx * tileDim;

    const size_t matrixBOffset = chunkSize * tileDim;

    //! For half of the warps, copy matrix A while other half copies B
    const half* copyPtr;

    size_t sharedMemCopyRowIdx;
    if (laneIdx % 2)
    {
        copyPtr = tilePtrA + paddedColSizeA * (laneIdx / 2);
        sharedMemCopyRowIdx = tileRowIdx + laneIdx / 2;
    }
    else
    {
        copyPtr = tilePtrB + paddedColSizeB * (laneIdx / 2);
        sharedMemCopyRowIdx = matrixBOffset + tileRowIdx + laneIdx / 2;
    }

    //! Load the matrix to shared memory
    //! each thread copies consecutive row from their src determined previously
#pragma unroll
    for (int i = 0; i < tileDim; i++)
    {
        const size_t sharedMemCopyColIdx = tileColIdx * tileDim + i;
        sharedMemory[sharedMemCopyRowIdx][sharedMemCopyColIdx + i] =
            *(copyPtr + i);
    }

    //! Load shared memory to fragments and accumulate
    wmma::fragment<wmma::matrix_a, tileDim, tileDim, tileDim, half,
                   wmma::row_major>
        fragA;
    wmma::fragment<wmma::matrix_b, tileDim, tileDim, tileDim, half,
                   wmma::row_major>
        fragB;

    wmma::fragment<wmma::accumulator, tileDim, tileDim, tileDim, float> fragAcc;
    wmma::fragment<wmma::accumulator, tileDim, tileDim, tileDim, float> fragOut;

    wmma::fill_fragment(fragAcc, 0.0f);

    for (int i = 0; i < chunkSize; ++i)
    {
        wmma::load_matrix_sync(fragA, &sharedMemory[tileRowIdx][tileDim * i],
                               shiftedSharedMemoryColSize);
        wmma::load_matrix_sync(
            fragB, &sharedMemory[tileDim * i + matrixBOffset][tileColIdx],
            shiftedSharedMemoryColSize);
        wmma::mma_sync(fragAcc, fragA, fragB, fragAcc);
    }

    wmma::load_matrix_sync(fragOut, tilePtrOut, paddedColSizeOut,
                           wmma::mem_row_major);

    wmma::store_matrix_sync(tilePtrOut, fragAcc, paddedColSizeOut,
                            wmma::mem_row_major);
}


//! M, N, K must be smaller than 32
//! 32 warps required
__global__ void GemmHalf(half* out, half* A, half* B, size_t M,
                         size_t N, size_t K, size_t paddedColSizeA,
                         size_t paddedColSizeB, size_t paddedColSizeOut,
                         size_t chunkIdxK)
{
    __shared__ half matrixA[tileDim * chunkSize][tileDim * chunkSize + 1];
    __shared__ half matrixB[tileDim * chunkSize][tileDim * chunkSize + 1];

    const size_t chunkIdxM = threadIdx.x;
    const size_t chunkIdxN = threadIdx.y;

    size_t rowIdx = threadIdx.x;
    size_t colIdx = threadIdx.y;

    const size_t blockIdxA =
        chunkIdxM * paddedColSizeA * tileDim + chunkIdxK * tileDim;
    const size_t blockIdxB =
        chunkIdxK * paddedColSizeB * tileDim + chunkIdxN * tileDim;
    const size_t blockIdxOut =
        chunkIdxM * paddedColSizeOut * tileDim + chunkIdxN * tileDim;

    half* chunkPtrA =
        A + chunkIdxM * paddedColSizeA * tileDim + chunkIdxK * tileDim;

    half* chunkPtrB =
        B + chunkIdxK * paddedColSizeB * tileDim + chunkIdxN * tileDim;
    half* chunkPtrK =
        out + chunkIdxM * paddedColSizeOut * tileDim + chunkIdxN * tileDim;

    const size_t copyIdxMA = chunkIdxM * chunkSize * tileDim + rowIdx;
    const size_t copyIdxKA = chunkIdxK * chunkSize * tileDim + colIdx;
    const size_t copyIdxKB= chunkIdxK * chunkSize * tileDim + rowIdx;
    const size_t copyIdxNB = chunkIdxN * chunkSize * tileDim + colIdx;

    if (copyIdxMA < M && copyIdxKA < K)
        matrixA[rowIdx][colIdx] = *(A + paddedColSizeA * rowIdx + colIdx);
    if (copyIdxKB < K && copyIdxNB < N)
        matrixB[rowIdx][colIdx] = *(B + paddedColSizeB * rowIdx + colIdx);


}
}
