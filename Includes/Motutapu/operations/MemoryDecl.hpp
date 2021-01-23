// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#ifndef MOTUTAPU_MEMORY_DECL_HPP
#define MOTUTAPU_MEMORY_DECL_HPP

#include <Motutapu/operations/Unit.hpp>

namespace Motutapu
{

template <typename T>
class DeepCopy : public Unit<T>
{
    DeepCopy();
};
}

#endif