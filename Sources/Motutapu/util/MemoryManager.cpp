// Copyright (c) 2021, Justin Kim

// We are making my contributions/submissions to this project solely in our
// personal capacity and are not conveying any rights to any intellectual
// property of any third parties.

#include <Motutapu/compute/cuda/Memory.cuh>
#include <Motutapu/util/MemoryManager.hpp>
#include <cassert>
#include <utility>

namespace Motutapu::Util
{
std::unordered_multimap<size_t, MemoryChunk>
    MemoryManager::m_hostFreeMemoryPool;
std::unordered_map<intptr_t, MemoryChunk> MemoryManager::m_hostBusyMemoryPool;
std::unordered_multimap<std::pair<int, size_t>, MemoryChunk, pair_hash_free>
    MemoryManager::m_cudaFreeMemoryPool;
std::unordered_map<std::pair<int, intptr_t>, MemoryChunk, pair_hash_busy>
    MemoryManager::m_cudaBusyMemoryPool;

std::mutex MemoryManager::m_hostPoolMtx;
std::mutex MemoryManager::m_cudaPoolMtx;
unsigned int MemoryManager::m_allocationUnitSize = 256;

float* MemoryManager::GetMemoryCuda(size_t size, int deviceId)
{
    std::lock_guard<std::mutex> lock(m_cudaPoolMtx);
    auto success = true;
    float* cudaPtr = nullptr;

    auto allocationSize =
        (size / m_allocationUnitSize) * m_allocationUnitSize +
        ((size % m_allocationUnitSize) ? m_allocationUnitSize : 0);

    const auto itr =
        m_cudaFreeMemoryPool.find(std::make_pair(deviceId, allocationSize));

    if (itr != m_cudaFreeMemoryPool.end())
    {
        MemoryChunk targetChunk = itr->second;
        cudaPtr = targetChunk.Data;
        targetChunk.RefCount += 1;
        m_cudaFreeMemoryPool.erase(itr);
        m_cudaBusyMemoryPool.emplace(
            std::make_pair(deviceId, intptr_t(cudaPtr)), targetChunk);

        return cudaPtr;
    }

    success &= Compute::Cuda::CudaSetDevice(deviceId);
    success &= Compute::Cuda::CudaMalloc((void**)&cudaPtr,
                                         allocationSize * sizeof(float));

    m_cudaBusyMemoryPool.emplace(std::make_pair(deviceId, intptr_t(cudaPtr)),
                                 MemoryChunk(allocationSize, cudaPtr, 1));

    if (!success)
    {
        throw std::runtime_error("GetMemoryCuda - Allocation failure");
    }

    return cudaPtr;
}

float* MemoryManager::GetMemoryHost(size_t size)
{
    std::lock_guard<std::mutex> lock(m_hostPoolMtx);
    float* dataPtr;

    const auto itr = m_hostFreeMemoryPool.find(size);
    if (itr != m_hostFreeMemoryPool.end())
    {
        MemoryChunk targetChunk = itr->second;
        targetChunk.RefCount += 1;
        dataPtr = targetChunk.Data;
        m_hostFreeMemoryPool.erase(itr);
        m_hostBusyMemoryPool.emplace(intptr_t(dataPtr), targetChunk);
        return dataPtr;
    }

    dataPtr = new float[size];
    std::fill(dataPtr, dataPtr + size, 0);
    m_hostBusyMemoryPool.emplace(intptr_t(dataPtr),
                                 MemoryChunk(size, dataPtr, 1));

    return dataPtr;
}

void MemoryManager::AddReferenceCuda(float* ptr, int deviceId)
{
    std::lock_guard<std::mutex> lock(m_cudaPoolMtx);

    const auto itr =
        m_cudaBusyMemoryPool.find(std::make_pair(deviceId, intptr_t(ptr)));
    if (itr == m_cudaBusyMemoryPool.end())
    {
        throw std::runtime_error("AddReferenceCuda - Reference was not found");
    }

    auto& chunk = itr->second;
    chunk.RefCount += 1;
}

void MemoryManager::AddReferenceHost(float* ptr)
{
    std::lock_guard<std::mutex> lock(m_hostPoolMtx);

    const auto itr = m_hostBusyMemoryPool.find(intptr_t(ptr));
    if (itr == m_hostBusyMemoryPool.end())
    {
        throw std::runtime_error("AddReferenceHost - Reference was not found");
    }

    auto& chunk = itr->second;
    chunk.RefCount += 1;
}

void MemoryManager::DeReferenceCuda(float* ptr, int deviceId)
{
    std::lock_guard<std::mutex> lock(m_cudaPoolMtx);

    const auto itr =
        m_cudaBusyMemoryPool.find(std::make_pair(deviceId, intptr_t(ptr)));

    if (itr == m_cudaBusyMemoryPool.end())
    {
        throw std::runtime_error("DeReferenceCuda - Reference was not found");
    }

    auto& chunk = itr->second;
    chunk.RefCount -= 1;

    if (chunk.RefCount == 0)
    {
        m_cudaBusyMemoryPool.erase(itr);
        m_cudaFreeMemoryPool.emplace(std::make_pair(deviceId, chunk.Size),
                                     chunk);
    }
}

void MemoryManager::DeReferenceHost(float* ptr)
{
    std::lock_guard<std::mutex> lock(m_hostPoolMtx);

    const auto itr = m_hostBusyMemoryPool.find(intptr_t(ptr));
    if (itr == m_hostBusyMemoryPool.end())
    {
        throw std::runtime_error("DeReferenceHost - Reference was not found");
    }

    auto& chunk = itr->second;
    chunk.RefCount -= 1;

    if (chunk.RefCount == 0)
    {
        m_hostBusyMemoryPool.erase(itr);
        m_hostFreeMemoryPool.emplace(chunk.Size, chunk);
    }
}

void MemoryManager::ClearUnusedCudaMemoryPool()
{
    std::lock_guard<std::mutex> lock(m_cudaPoolMtx);

    for (auto& [key, memoryChunk] : m_cudaFreeMemoryPool)
    {
        if (!Compute::Cuda::CudaFree(memoryChunk.Data))
            throw std::runtime_error(
                "ClearUnusedCudaMemoryPool - CudaFree failed");
    }

    m_cudaFreeMemoryPool.clear();
}

void MemoryManager::ClearUnusedHostMemoryPool()
{
    std::lock_guard<std::mutex> lock(m_hostPoolMtx);

    for (auto& [key, memoryChunk] : m_hostFreeMemoryPool)
    {
        delete[] memoryChunk.Data;
    }

    m_hostFreeMemoryPool.clear();
}

void MemoryManager::ClearCudaMemoryPool()
{
    std::lock_guard<std::mutex> lock(m_cudaPoolMtx);

    cudaDeviceSynchronize();

    for (auto& [key, memoryChunk] : m_cudaFreeMemoryPool)
    {
        if (!Compute::Cuda::CudaFree(memoryChunk.Data))
            throw std::runtime_error(
                "ClearCudaMemoryPool - CudaFree(Unused) failed");
    }
    m_cudaFreeMemoryPool.clear();

    for (auto& [key, memoryChunk] : m_cudaBusyMemoryPool)
    {
        if (!Compute::Cuda::CudaFree(memoryChunk.Data))
            throw std::runtime_error(
                "ClearCudaMemoryPool - CudaFree(Busy) failed");
    }

    m_cudaBusyMemoryPool.clear();

    assert(m_cudaBusyMemoryPool.empty() && m_cudaFreeMemoryPool.empty() &&
           "CudaPool Not empty!");

    cudaDeviceSynchronize();
}

void MemoryManager::ClearHostMemoryPool()
{
    std::lock_guard<std::mutex> lock(m_hostPoolMtx);

    for (auto& [key, memoryChunk] : m_hostFreeMemoryPool)
    {
        delete[] memoryChunk.Data;
    }

    for (auto& [key, memoryChunk] : m_hostBusyMemoryPool)
    {
        delete[] memoryChunk.Data;
    }

    m_hostFreeMemoryPool.clear();
    m_hostBusyMemoryPool.clear();
}

size_t MemoryManager::GetTotalAllocationByteSizeCuda()
{
    std::lock_guard<std::mutex> lock(m_cudaPoolMtx);

    size_t size = 0;

    for (const auto& [key, chunk] : m_cudaBusyMemoryPool)
    {
        size += chunk.Size * sizeof(float);
    }

    for (const auto& [key, chunk] : m_cudaFreeMemoryPool)
    {
        size += chunk.Size * sizeof(float);
    }

    return size;
}

size_t MemoryManager::GetTotalAllocationByteSizeHost()
{
    std::lock_guard<std::mutex> lock(m_hostPoolMtx);

    size_t size = 0;

    for (const auto& [key, chunk] : m_hostBusyMemoryPool)
    {
        size += chunk.Size * sizeof(float);
    }

    for (const auto& [key, chunk] : m_hostFreeMemoryPool)
    {
        size += chunk.Size * sizeof(float);
    }

    return size;
}

}  // namespace Motutapu::Util