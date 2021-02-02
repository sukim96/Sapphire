// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#ifndef MOTUTAPU_COMPUTE_CUDA_DENSE_GEMM_CUH
#define MOTUTAPU_COMPUTE_CUDA_DENSE_GEMM_CUH

#include <cuda_fp16.h>
#include <Motutapu/compute/cuda/CudaParams.cuh>

namespace Motutapu::Compute::Cuda::Dense
{
__host__ void GemmTensor(float* out, float* A, float* B, float* C,
                         unsigned int paddedM, unsigned int paddedN,
                         unsigned int paddedK, unsigned int batchSize,
                         bool broadcastA, bool broadcastB, bool broadcastC);

__host__ void GemmCublas(float* out, float* A, float* B, float* C,
                         unsigned int paddedM, unsigned int paddedN,
                         unsigned int paddedK, unsigned int batchSize,
                         bool broadcastA, bool broadcastB, bool broadcastC);

__host__ void GemmCublas(float* out, float* A, float* B, unsigned int paddedM,
                         unsigned int paddedN, unsigned int paddedK,
                         unsigned int batchSize, bool broadcastA,
                         bool broadcastB);

__host__ void GemmNormal(float* out, float* A, float* B, float* C,
                         unsigned int paddedM, unsigned int paddedN,
                         unsigned int paddedK, unsigned int batchSize,
                         bool broadcastA, bool broadcastB, bool broadcastC);

}  // namespace Motutapu::Compute::Cuda::Dense

#endif
