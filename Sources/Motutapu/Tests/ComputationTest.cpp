// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#include <Motutapu/Tests/ComputationTest.hpp>
#include <Motutapu/compute/Compute.hpp>
#include <Motutapu/compute/Initialize.hpp>
#include <Motutapu/tensor/Shape.hpp>
#include <Motutapu/tensor/TensorData.hpp>
#include <Motutapu/util/Device.hpp>
#include <Motutapu/util/MemoryManager.hpp>
#include <atomic>
#include <cmath>
#include <iostream>
#include <random>
#include "doctest.h"

namespace Motutapu::Test
{
void TestGemm1()
{
    for (int j = 0; j < 3; j++)
    {
        std::random_device
            rd;  // Will be used to obtain a seed for the random number engine
        std::mt19937 gen(
            rd());  // Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> distrib(1, 100);

        const unsigned int M = distrib(gen);
        const unsigned int N = distrib(gen);
        const unsigned int K = distrib(gen);
        const auto batchSize = distrib(gen) % 30;

        const Shape shapeA({ M, K });
        const Shape shapeB({ K, N });
        const Shape shapeC({ M, N });
        const Shape shapeOut({ M, N });

        std::cout << "M : " << M << " N: " << N << " K: " << K
                  << " batchSize : " << batchSize << std::endl;

        const Device cuda(0, "device0");
        const Device host("host");

        TensorUtil::TensorData A(shapeA, Type::Dense, host, batchSize);

        TensorUtil::TensorData B(shapeB, Type::Dense, host, batchSize);

        TensorUtil::TensorData C(shapeC, Type::Dense, host, batchSize);

        TensorUtil::TensorData Out(shapeOut, Type::Dense, host, batchSize);

        Compute::Initialize::Normal(A, 10, 5);
        Compute::Initialize::Normal(B, 10, 5);
        Compute::Initialize::Normal(C, 10, 5);
        Compute::Initialize::Zeros(C);

        Compute::Gemm(Out, A, B, C);

        float cpuGemmResult[Out.DenseTotalLengthHost];

#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < Out.DenseTotalLengthHost; ++i)
        {
            cpuGemmResult[i] = Out.DenseMatHost[i];
        }

        Compute::Initialize::Zeros(Out);

        A.SendTo(cuda);
        B.SendTo(cuda);
        C.SendTo(cuda);
        Out.SendTo(cuda);

        Compute::Gemm(Out, A, B, C);

        Out.SendTo(host);

        std::atomic<float> largestError = 0.0f;

#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < Out.DenseTotalLengthHost; ++i)
        {
            auto error = std::abs(cpuGemmResult[i] - Out.DenseMatHost[i]);
            if (largestError < error)
                largestError = error;

            //            std::cout << "cpu : " << cpuGemmResult[i]
            //                      << " cuda : " << Out.DenseMatHost[i] <<
            //                      std::endl;

            CHECK(error <= 2.0f);
        }

        std::cout << "Largest error : " << largestError << std::endl;
    }

    Util::MemoryManager::ClearCudaMemoryPool();
    Util::MemoryManager::ClearHostMemoryPool();
}

void TestGemm2()
{
    for (int j = 0; j < 1; j++)
    {
        std::random_device
            rd;  // Will be used to obtain a seed for the random number engine
        std::mt19937 gen(
            rd());  // Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> distrib(8, 16);

        const unsigned int M = distrib(gen);
        const unsigned int N = distrib(gen);
        const unsigned int K = distrib(gen);
        const auto batchSize = distrib(gen) % 3 + 1;

        std::cout << "M : " << M << " N: " << N << " K: " << K
                  << " batchSize : " << batchSize << std::endl;

        const Shape shapeA({ M, K });
        const Shape shapeB({ K, N });
        const Shape shapeC({ M, N });
        const Shape shapeOut({ M, N });

        const Device cuda(0, "device0");
        const Device host("host");

        TensorUtil::TensorData A(shapeA, Type::Dense, cuda, batchSize);
        TensorUtil::TensorData B(shapeB, Type::Dense, cuda, batchSize);
        TensorUtil::TensorData C(shapeC, Type::Dense, cuda, batchSize);
        TensorUtil::TensorData out(shapeOut, Type::Dense, cuda, batchSize);

        Compute::Initialize::Normal(A, 10, 5);
        Compute::Initialize::Normal(B, 10, 5);
        Compute::Initialize::Normal(C, 10, 5);
        Compute::Initialize::Zeros(out);

        Compute::Gemm(out, A, B, C);

        A.SendTo(host);
        B.SendTo(host);
        C.SendTo(host);
        out.SendTo(host);

        float cudaGemmResult[out.DenseTotalLengthHost];

#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < out.DenseTotalLengthHost; ++i)
        {
            cudaGemmResult[i] = out.DenseMatHost[i];
        }

        Compute::Initialize::Zeros(out);
        Compute::Gemm(out, A, B, C);

        std::atomic<float> largestError = 0.0f;

#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < out.DenseTotalLengthHost; ++i)
        {
            auto error = std::abs(cudaGemmResult[i] - out.DenseMatHost[i]);
            if (largestError < error)
                largestError = error;

//            std::cout << "cuda : " << cudaGemmResult[i]
//                      << " cpu : " << out.DenseMatHost[i] << std::endl;

            CHECK(error <= std::abs(out.DenseMatHost[i] / 100.0f));
        }

        std::cout << "Largest error : " << largestError << std::endl;
    }
    Util::MemoryManager::ClearCudaMemoryPool();
    Util::MemoryManager::ClearHostMemoryPool();
}

void TestGemmBroadcast()
{
    for (int j = 0; j < 1; j++)
    {
        std::random_device
            rd;  // Will be used to obtain a seed for the random number engine
        std::mt19937 gen(
            rd());  // Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> distrib(1, 16);

        const unsigned int M = distrib(gen);
        const unsigned int N = distrib(gen);
        const unsigned int K = distrib(gen);
        const auto batchSize = distrib(gen) % 3 + 1;

        std::cout << "M : " << M << " N: " << N << " K: " << K
                  << " batchSize : " << batchSize << std::endl;

        const Shape shapeA({ M, K });
        const Shape shapeB({ K, N });
        const Shape shapeC({ M, N });
        const Shape shapeOut({ M, N });

        const Device cuda(0, "device0");
        const Device host("host");

        TensorUtil::TensorData A(shapeA, Type::Dense, cuda, 1);

        TensorUtil::TensorData B(shapeB, Type::Dense, cuda, batchSize);

        TensorUtil::TensorData C(shapeC, Type::Dense, cuda, 1);

        TensorUtil::TensorData out(shapeOut, Type::Dense, cuda, batchSize);

        Compute::Initialize::Normal(A, 100, 1);
        Compute::Initialize::Normal(B, 100, 4);
        Compute::Initialize::Normal(C, 100, 1);
        Compute::Initialize::Zeros(out);

        Compute::Gemm(out, A, B, C);

        A.SendTo(host);
        B.SendTo(host);
        C.SendTo(host);
        out.SendTo(host);

        float cudaGemmResult[out.DenseTotalLengthHost];

#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < out.DenseTotalLengthHost; ++i)
        {
            cudaGemmResult[i] = out.DenseMatHost[i];
        }

        Compute::Initialize::Zeros(out);
        Compute::Gemm(out, A, B, C);

        std::atomic<float> largestError = 0.0f;

#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < out.DenseTotalLengthHost; ++i)
        {
            auto error = std::abs(cudaGemmResult[i] - out.DenseMatHost[i]);
            if (largestError < error)
                largestError = error;

            //            std::cout << "cuda : " << cudaGemmResult[i]
            //                      << " cpu : " << out.DenseMatHost[i] <<
            //                      std::endl;

            CHECK(error <= std::abs(out.DenseMatHost[i] / 100.0f));
        }

        std::cout << "Largest error : " << largestError << std::endl;
    }
    Util::MemoryManager::ClearCudaMemoryPool();
    Util::MemoryManager::ClearHostMemoryPool();
}

}  // namespace Motutapu::Test
