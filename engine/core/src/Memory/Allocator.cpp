#include "kizuri/core/memory/Allocator.h"

namespace kizuri::core {

Allocator::Allocator(const char* name)
    : name_(name)
{
}

void* Allocator::Allocate(size_t size, size_t alignment)
{
    if (size == 0)
    {
        return nullptr;
    }

    size_t effectiveAlignment = alignment < KernelAlignment ? KernelAlignment : alignment;
    void* pointer = DoAllocate(size, effectiveAlignment);
    if (pointer == nullptr)
    {
        return nullptr;
    }

    size_t reportedSize = ReportedSize(pointer, size);
    TrackAllocate(reportedSize);
    return pointer;
}

void Allocator::Free(void* pointer) noexcept
{
    if (pointer == nullptr)
    {
        return;
    }

    size_t retiredSize = RetiredSize(pointer);
    TrackFree(retiredSize);
    DoFree(pointer);
}

void Allocator::Reset() noexcept
{
    allocationCount_ = 0;
    allocatedBytes_ = 0;
    DoReset();
}

void Allocator::TrackAllocate(size_t bytes) noexcept
{
    allocatedBytes_ += bytes;
    if (allocatedBytes_ > peakBytes_)
    {
        peakBytes_ = allocatedBytes_;
    }
    ++allocationCount_;
}

void Allocator::TrackFree(size_t bytes) noexcept
{
    allocatedBytes_ -= bytes;
}

} // namespace kizuri::core