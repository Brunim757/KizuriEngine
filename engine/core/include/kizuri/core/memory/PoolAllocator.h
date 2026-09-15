#pragma once

#include <cstddef>
#include <cstdint>

#include "kizuri/core/memory/Allocator.h"

namespace kizuri::core {

class PoolAllocator : public Allocator {
public:
    PoolAllocator(size_t objectSize, uint32_t blockCount, size_t slotAlignment = KernelAlignment);
    ~PoolAllocator() override;
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    size_t ObjectSize() const noexcept { return objectSize_; }
    size_t BlockSize() const noexcept { return blockSize_; }
    uint32_t BlockCount() const noexcept { return blockCount_; }
    uint32_t FreeBlockCount() const noexcept { return freeCount_; }

protected:
    void* DoAllocate(size_t size, size_t alignment) override;
    void DoFree(void* pointer) noexcept override;
    void DoReset() noexcept override;

    size_t ReportedSize(void* pointer, size_t requestedSize) noexcept override;
    size_t RetiredSize(void* pointer) noexcept override;

private:
    struct SlotRecord
    {
        uintptr_t Padding;
    };

    unsigned char* buffer_;
    uint32_t* freeSlots_;
    size_t blockSize_;
    size_t objectSize_;
    uint32_t blockCount_;
    uint32_t freeCount_;
};

} // namespace kizuri::core