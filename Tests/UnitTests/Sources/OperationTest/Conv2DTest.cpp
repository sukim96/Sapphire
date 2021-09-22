// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#include <OperationTest/Conv2DTest.hpp>
#include <Sapphire/Model.hpp>
#include <Sapphire/operations/Forward/Conv2D.hpp>
#include <Sapphire/operations/optimizers/SGD.hpp>
#include <iostream>
#include <doctest/doctest.h>
#include <Sapphire/operations/Forward/Linear.hpp>

namespace Sapphire::Test
{
void TestConv2D(bool print)
{
    ModelManager::AddModel("myModel");
    ModelManager::SetCurrentModel("myModel");

    const CudaDevice gpu(0, "cuda0");
    const int inputChannels = 3;
    const int outputChannels = 3;
    const int inputRows = 4;
    const int inputCols = 4;

    const auto inputSize = std::make_pair(inputRows, inputCols);
    const auto kernelSize = std::make_pair(3, 3);
    const auto stride = std::make_pair(1, 1);
    const auto dilation = std::make_pair(1, 1);
    const auto padSize = std::make_pair(1, 1);

    Tensor input(Shape({ inputChannels, inputRows, inputCols }), gpu,
                 Type::Dense);
    Tensor kernel(Shape({ outputChannels, inputChannels,
                          std::get<0>(kernelSize), std::get<1>(kernelSize) }),
                  gpu, Type::Dense);
    Tensor bias(Shape({ outputChannels }), gpu, Type::Dense);

    Initialize::Initialize(input, std::make_unique<Initialize::Ones>());
    Initialize::Initialize(kernel, std::make_unique<Initialize::Ones>());
    Initialize::Initialize(bias, std::make_unique<Initialize::Zeros>());
    input.ToCuda();
    kernel.ToCuda();
    bias.ToCuda();

    NN::Conv2D conv2D(inputSize, stride, padSize, dilation,
                      Util::SharedPtr<Optimizer::SGD>::Make(0.1f), kernel,
                      bias);

    auto output = conv2D(input);
    output.ToHost();

    const auto gpuForwardPtr = output.GetForwardDataCopy();
    const auto outputRows = output.GetShape().Rows();
    const auto outputCols = output.GetShape().Cols();

    Initialize::InitializeBackwardData(output,
                                       std::make_unique<Initialize::Ones>());

    output.ToCuda();
    ModelManager::GetCurrentModel().BackProp(output);
    input.ToHost();
    const auto gpuBackwardPtr = input.GetBackwardDataCopy();

    kernel.ToHost();
    bias.ToHost();

    Initialize::Initialize(kernel, std::make_unique<Initialize::Ones>());
    Initialize::Initialize(bias, std::make_unique<Initialize::Zeros>());

    NN::Conv2D conv2DHost(inputSize, stride, padSize, dilation,
                          Util::SharedPtr<Optimizer::SGD>::Make(0.1f),
                          kernel,
                          bias);
    auto hostOutput = conv2DHost(input);
    const auto hostForwardPtr = hostOutput.GetForwardDataCopy();
    const auto outputRowsHost = hostOutput.GetShape().Rows();
    const auto outputColsHost = hostOutput.GetShape().Cols();

    Initialize::InitializeBackwardData(hostOutput,
                                       std::make_unique<Initialize::Ones>());
    Initialize::InitializeBackwardData(input,
                                       std::make_unique<Initialize::Zeros>());
    ModelManager::GetCurrentModel().BackProp(hostOutput);
    const auto hostBackwardPtr = input.GetBackwardDataCopy();

    if (print)
    {
        std::cout << "Conv2D forward result (Host)" << std::endl;
        for (std::size_t i = 0; i < outputRowsHost; ++i)
        {
            for (std::size_t j = 0; j < outputColsHost; ++j)
            {
                std::cout << hostForwardPtr[i * outputCols + j] << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "Conv2D backward result (Host)" << std::endl;
        for (std::size_t i = 0; i < inputRows; ++i)
        {
            for (std::size_t j = 0; j < inputCols; ++j)
            {
                std::cout << hostBackwardPtr[i * outputCols + j] << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "Conv2D forward result (Cuda)" << std::endl;
        for (int i = 0; i < outputRows; ++i)
        {
            for (int j = 0; j < outputCols; ++j)
            {
                std::cout << gpuForwardPtr[i * inputCols + j] << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "Conv2D backward result (Cuda)" << std::endl;
        for (int i = 0; i < inputRows; ++i)
        {
            for (int j = 0; j < inputCols; ++j)
            {
                std::cout << gpuBackwardPtr[i * inputCols + j] << " ";
            }
            std::cout << std::endl;
        }
    }

    CHECK(outputRows == outputRowsHost);
    CHECK(outputCols == outputColsHost);

    for (std::size_t channelIdx = 0; channelIdx < outputChannels; ++channelIdx)
        for (std::size_t rowIdx = 0; rowIdx < outputRows; ++rowIdx)
            for (std::size_t colIdx = 0; colIdx < outputCols; ++ colIdx)
            {
                CHECK(hostForwardPtr[channelIdx * outputRows * outputCols +
                        rowIdx * outputCols + colIdx] ==
                    gpuForwardPtr[channelIdx * outputRows * outputCols +
                        rowIdx * outputCols + colIdx]);
            }

    for (std::size_t channelIdx = 0; channelIdx < inputChannels; ++channelIdx)
        for (std::size_t rowIdx = 0; rowIdx < inputRows; ++rowIdx)
            for (std::size_t colIdx = 0; colIdx < inputCols; ++ colIdx)
            {
                CHECK(hostBackwardPtr[channelIdx * inputRows * inputCols +
                        rowIdx * inputCols + colIdx] ==
                    gpuBackwardPtr[channelIdx * inputRows * inputCols +
                        rowIdx * inputCols + colIdx]);
            }

    ModelManager::GetCurrentModel().Clear();
}
}
