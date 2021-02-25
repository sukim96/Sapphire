// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#ifndef MOTUTAPU_INITIALIZE_HPP
#define MOTUTAPU_INITIALIZE_HPP

#include <Motutapu/tensor/TensorData.hpp>

namespace Motutapu::Compute::Initialize
{
void Normal(const TensorUtil::TensorData& data, float mean, float sd);

void Ones(const TensorUtil::TensorData& data);

void Zeros(const TensorUtil::TensorData& data);

void HeNormal(const TensorUtil::TensorData& data, int fanIn);

void Xavier(const TensorUtil::TensorData& data, int fanIn, int fanOut);

}  // namespace Motutapu::Compute::Initialize

#endif  // MOTUTAPU_INITIALIZE_HPP