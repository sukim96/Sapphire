// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#ifndef SAPPHIRE_COMPUTE_DENSE_BASIC_KERNEL_CUH
#define SAPPHIRE_COMPUTE_DENSE_BASIC_KERNEL_CUH
#include <Sapphire/compute/cudaUtil/CudaParams.cuh>

namespace Sapphire::Compute::Dense::Cuda
{
__global__ void AddKernel(float* y, const float* a,
                          const float* b, unsigned int offset,
                          unsigned int launchSize, unsigned int totalSize,
                          unsigned int inputStride, bool broadcastInputA,
                          bool broadcastInputB);

__global__ void AddKernelBroadcast(float* output, const float* inputA,
                                   const float* inputB, unsigned int offset,
                                   unsigned int totalSize,
                                   unsigned int inputStride,
                                   bool broadcastInputA, bool broadcastInputB);

__global__ void SubKernel(float* y, const float* a,
                          const float* b, unsigned int offset,
                          unsigned int launchSize, unsigned int totalSize,
                          unsigned int inputStride, bool broadcastInputA,
                          bool broadcastInputB);

__global__ void DotKernel(float* y, const float* a,
                          const float* b, unsigned int offset,
                          unsigned int launchSize, unsigned int totalSize,
                          unsigned int inputStride, bool broadcastInputA,
                          bool broadcastInputB);

__global__ void TransposeKernel(float* y, const float* x,
                                unsigned int inputNumRows,
                                unsigned int inputNumCols, bool broadcastInput);

__global__ void ScaleKernel(float* y, const float* x,
                            const float scaleFactor, unsigned int totalSize);

__global__ void PowKernel(float* y, const float* x, const float factor,
                          unsigned int totalSize);

__global__ void logKernel(float* y, const float* x,
                          unsigned int totalSize);

__global__ void log10Kernel(float* y, const float* x,
                            unsigned int totalSize);

__global__ void ReLUKernel(float* y, const float* x,
                           unsigned int totalSize);

__global__ void ReLUDerivativeKernel(float* y, const float* x,
                                     unsigned int totalSize);

__global__ void LeakyReLUKernel(float* y, const float* x, float a,
                                unsigned int totalSize);

__global__ void LeakyReLUDerivativeKernel(float* y, const float* x,
                                          float a, unsigned int totalSize);

__global__ void InverseKernel(float* y, const float* x,
                              unsigned int totalSize);

__global__ void MeanKernel(float* y, const float* x,
                           unsigned int totalSize, unsigned int unitSize);

//! Total size must be multiple of unitSize
__global__ void SoftmaxKernel(float* y, const float* x,
                              unsigned int totalSize, unsigned int unitSize);

__global__ void SoftmaxBackKernel(float* dx, const float* dy, const float* x,
                                  unsigned int totalSize,
                                  unsigned int unitSize);
} // namespace Sapphire::Compute::Cuda::Dense

#endif  // Sapphire_BASICKERNEL_CUH
