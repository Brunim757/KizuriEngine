#pragma once

#include <cstddef>

#include "kizuri/core/memory/Allocator.h"

namespace kizuri::core {

class LinearAllocator : public Allocator {
public:
    explicit LinearAllocator(size_t capacity);
    ~LinearAllocator() override;
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

    size_t Capacity() const noexcept { return capacity_; }
    size_t Cursor() const noexcept { return cursor_; }

protected:
    void* DoAllocate(size_t size, size_t alignment) override;
    void DoFree(void* pointer) noexcept override;
    void DoReset() noexcept override;

private:
    unsigned char* buffer_;
    size_t capacity_;
    size_t cursor_;
};

} // namespace kizuri::core