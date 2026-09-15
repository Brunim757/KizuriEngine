#pragma once

#include <cstddef>
#include <cstdint>

#include "kizuri/core/memory/Allocator.h"

namespace kizuri::core {

class StackAllocator : public Allocator {
public:
    explicit StackAllocator(size_t capacity);
    ~StackAllocator() override;
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    size_t Capacity() const noexcept { return capacity_; }
    size_t UsedBytes() const noexcept;

protected:
    void* DoAllocate(size_t size, size_t alignment) override;
    void DoFree(void* pointer) noexcept override;
    void DoReset() noexcept override;

    size_t RetiredSize(void* pointer) noexcept override;

private:
    struct Marker
    {
        uintptr_t Begin;
        size_t Size;
        size_t LeadingSlack;
    };

    Marker* markers_;
    unsigned char* payload_;
    size_t capacity_;
    size_t markerCapacity_;
    size_t markerCount_;
    size_t freeTop_;
};

} // namespace kizuri::core