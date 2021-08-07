// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#ifndef Sapphire_UTIL_TENSORDATA_DECL_HPP
#define Sapphire_UTIL_TENSORDATA_DECL_HPP

#include <Sapphire/compute/sparse/SparseMatrix.hpp>
#include <Sapphire/util/SharedPtr.hpp>
#include <Sapphire/tensor/Shape.hpp>
#include <Sapphire/util/Device.hpp>

namespace Sapphire::TensorUtil
{
class TensorData
{
public:
    TensorData() = default;
    // TensorData(Shape shape, Type type);

    TensorData(Shape shape, Type type, Device device);

    TensorData(Shape shape, Type type, Device device,
               int parentDescKey);

    //! Shallow copies the internal data
    TensorData(const TensorData& tensorData);
    TensorData(TensorData&& tensorData) noexcept;
    TensorData& operator=(const TensorData& tensorData);
    TensorData& operator=(TensorData&& tensorData) noexcept;
    ~TensorData();

    unsigned long DenseTotalLengthHost = 0;
    unsigned long DenseTotalLengthCuda = 0;
    unsigned long SparseTotalLength = 0;
    unsigned long PaddedHostColSize = 0;

    [[nodiscard]] const float* GetDenseHost() const
    {
        return DenseMatHost;
    }

    [[nodiscard]] const float* GetDenseCuda() const
    {
        return DenseMatCuda;
    }

    [[nodiscard]] float* GetMutableDenseHost()
    {
        return DenseMatHost;
    }

    [[nodiscard]] float* GetMutableDenseCuda()
    {
        return DenseMatCuda;
    }

    [[nodiscard]] int GetDescriptorKey() const
    {
        return m_parentDescKey;
    }

    [[nodiscard]] std::size_t GetBatchSize(unsigned int requiredDim) const;

    SparseMatrix* SparseMatHost = nullptr;
    SparseMatrix* SparseMatCuda = nullptr;
    Shape TensorShape;

    [[nodiscard]] unsigned int Rows() const
    {
        return TensorShape.Rows();
    }

    [[nodiscard]] unsigned int Cols() const
    {
        return TensorShape.Cols();
    }

    //! Gets device descriptor (Sparse or Dense)
    //! \return : device descriptor
    [[nodiscard]] const Device& GetDevice() const
    {
        return m_device;
    }

    //! Gets type of the data (Sparse of Dense)
    //! \return : Type of the data
    [[nodiscard]] Type GetType() const
    {
        return m_type;
    }

    [[nodiscard]] Shape GetShape() const
    {
        return TensorShape;
    }

    [[nodiscard]] std::size_t GetHostElementSize() const
    {
        return static_cast<std::size_t>(TensorShape.Size() / TensorShape.Cols())
               * PaddedHostColSize;
    }

    [[nodiscard]] std::size_t GetCudaElementSize() const
    {
        return TensorShape.Size();
    }


    //! Creates and returns same copy as this tensorData
    [[nodiscard]] TensorData CreateCopy() const;

    //! Changes device of the tensor
    //! Transfers data to target device from current device
    //! immediately returns false if change device is requested to same device
    //! \param device : new device to set
    bool SendTo(const Device& device);

    //! Syncs data between host and cuda
    bool SyncCudaDataWithHost();

    //! Helper static functions
    //! These helper functions are used to control the tensorData from the
    //! operation units

    //! Deep copies tensor data from src to dst
    static void DeepCopy(TensorData& dst, const TensorData& src);

private:
    //! Copies data on the Host to Gpu
    //! Only available for CUDA tensors
    static void m_toCuda(const TensorData& tensorData);

    //! Copies data on the Host to Gpu
    //! Only available for CUDA tensors
    static void m_toHost(const TensorData& tensorData);

    //! Allocates data on the HOST with given batchSize
    void m_allocateHost();

    //! Allocates data on the GPU with given batchSize
    void m_allocateCuda();

    //! Free space allocated on HOST memory
    void m_freeHost();

    //! Free space allocated on GPU memory
    void m_freeCuda();

    float* DenseMatHost = nullptr;
    float* DenseMatCuda = nullptr;

    int m_parentDescKey = -1;

    Type m_type = Type::Dense;

    Device m_device;
};
} // namespace Sapphire::TensorUtil

#endif
