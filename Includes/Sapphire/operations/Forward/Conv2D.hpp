// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#ifndef SAPPHIRE_NN_CONV2D_HPP
#define SAPPHIRE_NN_CONV2D_HPP

#include <Sapphire/tensor/Tensor.hpp>
#include <Sapphire/operations/optimizers/Optimizer.hpp>
#include <Sapphire/util/SharedPtr.hpp>
#include <Sapphire/operations/Unit.hpp>
#include <Sapphire/operations/Initializers/Initialize.hpp>
#include <utility>

namespace Sapphire::NN
{
class Conv2D : public Unit
{
public:
    Conv2D(std::pair<int, int> inputSize,
           std::pair<int, int> stride,
           std::pair<int, int> padSize, std::pair<int, int> dilation,
           Util::SharedPtr<Optimizer::Optimizer> optimizer,
           Tensor kernel, Tensor bias = Tensor());

    ~Conv2D() override = default;

    Conv2D(const Conv2D& conv2D) = default;
    Conv2D(Conv2D&& conv2D) = default;
    Conv2D& operator=(const Conv2D& conv2D) = default;
    Conv2D& operator=(Conv2D&& conv2D) noexcept = default;

    Tensor operator()(Tensor& tensor);

private:
    [[nodiscard]] int m_registerOutputTensor(
        const TensorUtil::TensorDescriptor& xDesc) const;


    void m_checkArguments(
        std::vector<TensorUtil::TensorDescriptor*> arguments) const override;

    int m_xChannels, m_yChannels;
    std::pair<int, int> m_inputSize, m_kernelSize, m_stride, m_padSize,
                        m_dilation;
    bool m_useBias;

    bool m_isSparse;
    int m_yRows, m_yCols;

    Util::SharedPtr<Optimizer::Optimizer> m_optimizer;
};
};

#endif
