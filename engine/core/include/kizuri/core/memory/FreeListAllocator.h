#pragma once

#include <cstddef>
#include <cstdint>

#include "kizuri/core/memory/Allocator.h"

namespace kizuri::core {

class FreeListAllocator : public Allocator {
public:
    explicit FreeListAllocator(size_t capacity);
    ~FreeListAllocator() override;
    FreeListAllocator(const FreeListAllocator&) = delete;
    FreeListAllocator& operator=(const FreeListAllocator&) = delete;

    size_t Capacity() const noexcept { return capacity_; }

protected:
    void* DoAllocate(size_t size, size_t alignment) override;
    void DoFree(void* pointer) noexcept override;
    void DoReset() noexcept override;

    size_t ReportedSize(void* pointer, size_t requestedSize) noexcept override;
    size_t RetiredSize(void* pointer) noexcept override;

private:
    struct FreeBlock
    {
        size_t Size;
        FreeBlock* Next;
    };

    struct SlotRecord
    {
        size_t Capacity;
        uintptr_t BlockStart;
    };

    unsigned char* buffer_;
    size_t capacity_;
    FreeBlock* freeHead_;
};

} // namespace kizuri::core