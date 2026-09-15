#pragma once

#include <cstddef>
#include <cstdint>

#include "kizuri/core/CoreTypes.h"

namespace kizuri::core {

class Allocator {
public:
    explicit Allocator(const char* name);
    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;
    virtual ~Allocator() = default;

    void* Allocate(size_t size, size_t alignment = KernelAlignment);
    void Free(void* pointer) noexcept;
    void Reset() noexcept;

    size_t AllocatedBytes() const noexcept { return allocatedBytes_; }
    size_t PeakBytes() const noexcept { return peakBytes_; }
    uint64_t AllocationCount() const noexcept { return allocationCount_; }
    const char* Name() const noexcept { return name_; }

protected:
    virtual void* DoAllocate(size_t size, size_t alignment) = 0;
    virtual void DoFree(void* pointer) noexcept = 0;
    virtual void DoReset() noexcept {}

    virtual size_t ReportedSize(void* pointer, size_t requestedSize) noexcept
    {
        return requestedSize;
    }

    virtual size_t RetiredSize(void* pointer) noexcept
    {
        return 0;
    }

private:
    void TrackAllocate(size_t bytes) noexcept;
    void TrackFree(size_t bytes) noexcept;

    const char* name_;
    size_t allocatedBytes_ = 0;
    size_t peakBytes_ = 0;
    uint64_t allocationCount_ = 0;
};

} // namespace kizuri::core