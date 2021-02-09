// Copyright (c) 2021, Jaewoo Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.
#include <Motutapu/Tests/BasicComputationTest.hpp>
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
void TestAdd()
{
    for (int j = 0; j < 1; j++)
    {
        std::random_device
            rd;  // Will be used to obtain a seed for the random number engine
        std::mt19937 gen(
            rd());  // Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> distrib(1, 100);

        const unsigned int M = distrib(gen);
        const unsigned int N = distrib(gen);
        const auto batchSize = distrib(gen) % 10 + 1;

        const Shape shapeA({ M, N });
        const Shape shapeB({ M, N });
        const Shape shapeOut({ M, N });

        std::cout << "M : " << M << " N: " << N << " batchSize : " << batchSize
                  << std::endl;

        const Device cuda(0, "device0");
        const Device host("host");

        TensorUtil::TensorData A(shapeA, Type::Dense, host, 1);

        TensorUtil::TensorData B(shapeB, Type::Dense, host, batchSize);

        TensorUtil::TensorData Out(shapeOut, Type::Dense, host, 1);

        Compute::Initialize::Normal(A, 10, 5);
        Compute::Initialize::Normal(B, 10, 5);
        //        Compute::Initialize::Ones(A);
        //        Compute::Initialize::Ones(B);

        Compute::Add(Out, A, B);

        float cpuGemmResult[Out.DenseTotalLengthHost];

#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < Out.DenseTotalLengthHost; ++i)
        {
            cpuGemmResult[i] = Out.DenseMatHost[i];
        }

        Compute::Initialize::Zeros(Out);

        A.SendTo(cuda);
        B.SendTo(cuda);
        Out.SendTo(cuda);

        Compute::Add(Out, A, B);

        Out.SendTo(host);

        std::atomic<float> largestError = 0.0f;

        //#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < Out.DenseTotalLengthHost; ++i)
        {
            auto error = std::abs(cpuGemmResult[i] - Out.DenseMatHost[i]);
            if (largestError < error)
                largestError = error;

            //            std::cout << "cpu : " << cpuGemmResult[i]
            //                      << " cuda : " << Out.DenseMatHost[i] <<
            //                      std::endl;

            CHECK(error <= 1.0f);
        }

        std::cout << "Largest error : " << largestError << std::endl;
    }

    Util::MemoryManager::ClearCudaMemoryPool();
    Util::MemoryManager::ClearHostMemoryPool();
}

void TestAdd2()
{
    for (int j = 0; j < 1; j++)
    {
        std::random_device
            rd;  // Will be used to obtain a seed for the random number engine
        std::mt19937 gen(
            rd());  // Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> distrib(8, 16);

        const unsigned int M = 16;  // distrib(gen);
        const unsigned int N = 11;  // distrib(gen);
        const auto batchSize = 3;   // distrib(gen) % 3 + 1;

        std::cout << "M : " << M << " N: " << N << " batchSize : " << batchSize
                  << std::endl;

        const Shape shapeA({ M, N });
        const Shape shapeB({ M, N });
        const Shape shapeOut({ M, N });

        const Device cuda(0, "device0");
        const Device host("host");

        TensorUtil::TensorData A(shapeA, Type::Dense, cuda, batchSize);
        TensorUtil::TensorData B(shapeB, Type::Dense, cuda, batchSize);
        TensorUtil::TensorData out(shapeOut, Type::Dense, cuda, batchSize);

        Compute::Initialize::Normal(A, 10, 5);
        Compute::Initialize::Normal(B, 10, 5);
        Compute::Initialize::Zeros(out);

        Compute::Add(out, A, B);

        A.SendTo(host);
        B.SendTo(host);
        out.SendTo(host);

        float cudaGemmResult[out.DenseTotalLengthHost];

#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < out.DenseTotalLengthHost; ++i)
        {
            cudaGemmResult[i] = out.DenseMatHost[i];
        }

        Compute::Initialize::Zeros(out);
        Compute::Add(out, A, B);

        std::atomic<float> largestError = 0.0f;

        //#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < out.DenseTotalLengthHost; ++i)
        {
            auto error = std::abs(cudaGemmResult[i] - out.DenseMatHost[i]);
            if (largestError < error)
                largestError = error;
            //
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

void TestAddBroadcast1()
{
    for (int j = 0; j < 1; j++)
    {
        std::random_device
            rd;  // Will be used to obtain a seed for the random number engine
        std::mt19937 gen(
            rd());  // Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> distrib(1, 32);

        const unsigned int M = distrib(gen);
        const unsigned int N = distrib(gen);
        const auto batchSize = distrib(gen) % 5 + 1;

        std::cout << "M : " << M << " N: " << N << " batchSize : " << batchSize
                  << std::endl;

        const Shape shapeA({ M, M, N });
        const Shape shapeB({ M, M, N });
        const Shape shapeOut({ M, M, N });

        const Device cuda(0, "device0");
        const Device host("host");

        TensorUtil::TensorData A(shapeA, Type::Dense, cuda, 1);

        TensorUtil::TensorData B(shapeB, Type::Dense, cuda, batchSize);

        TensorUtil::TensorData out(shapeOut, Type::Dense, cuda, batchSize);

        Compute::Initialize::Normal(A, 100, 1);
        Compute::Initialize::Normal(B, 100, 4);
        Compute::Initialize::Zeros(out);

        Compute::Add(out, A, B);

        A.SendTo(host);
        B.SendTo(host);
        out.SendTo(host);

        float cudaGemmResult[out.DenseTotalLengthHost];

        //#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < out.DenseTotalLengthHost; ++i)
        {
            cudaGemmResult[i] = out.DenseMatHost[i];
        }

        Compute::Initialize::Zeros(out);
        Compute::Add(out, A, B);

        std::atomic<float> largestError = 0.0f;

        //#pragma omp parallel for default(shared) schedule(static)
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

void TestAddBroadcast2()
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
        const auto batchSize = distrib(gen) % 5 + 1;

        std::cout << "M : " << M << " N: " << N << " batchSize : " << batchSize
                  << std::endl;

        const Shape shapeA({ N, 1, M, N });
        const Shape shapeB({ M, N });
        const Shape shapeOut({ N, M, M, N });

        const Device cuda(0, "device0");
        const Device host("host");

        TensorUtil::TensorData A(shapeA, Type::Dense, cuda, 1);

        TensorUtil::TensorData B(shapeB, Type::Dense, cuda, batchSize);

        TensorUtil::TensorData out(shapeOut, Type::Dense, cuda, batchSize);

        Compute::Initialize::Normal(A, 100, 1);
        Compute::Initialize::Normal(B, 100, 4);
        Compute::Initialize::Zeros(out);

        Compute::Add(out, A, B);

        A.SendTo(host);
        B.SendTo(host);
        out.SendTo(host);

        float cudaGemmResult[out.DenseTotalLengthHost];

#pragma omp parallel for default(shared) schedule(static)
        for (size_t i = 0; i < out.DenseTotalLengthHost; ++i)
        {
            cudaGemmResult[i] = out.DenseMatHost[i];
        }

        Compute::Initialize::Zeros(out);
        Compute::Add(out, A, B);

        std::atomic<float> largestError = 0.0f;

        //#pragma omp parallel for default(shared) schedule(static)
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