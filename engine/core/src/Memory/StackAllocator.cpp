#include "kizuri/core/memory/StackAllocator.h"

#include "kizuri/core/memory/MemoryTracker.h"

namespace kizuri::core {

namespace {

size_t AlignDownSize(size_t value, size_t alignment)
{
    size_t mask = alignment - 1;
    return value & ~mask;
}

} // namespace

StackAllocator::StackAllocator(size_t capacity)
    : Allocator("KizuriStackAllocator")
    , capacity_(capacity)
    , markerCapacity_(capacity / KernelAlignment + 1)
    , markerCount_(0)
    , freeTop_(capacity)
{
    markers_ = static_cast<Marker*>(MemoryTracker::Instance().AllocateRaw(sizeof(Marker) * markerCapacity_));
    payload_ = static_cast<unsigned char*>(MemoryTracker::Instance().AllocateRaw(capacity_));
}

StackAllocator::~StackAllocator()
{
    MemoryTracker::Instance().FreeRaw(payload_);
    MemoryTracker::Instance().FreeRaw(markers_);
    markers_ = nullptr;
    payload_ = nullptr;
}

size_t StackAllocator::UsedBytes() const noexcept
{
    return capacity_ - freeTop_ + markerCount_ * sizeof(Marker);
}

void* StackAllocator::DoAllocate(size_t size, size_t alignment)
{
    if (freeTop_ < size)
    {
        return nullptr;
    }

    size_t payloadOffset = AlignDownSize(freeTop_ - size, alignment);
    size_t nextMarkerEnd = (markerCount_ + 1) * sizeof(Marker);
    if (nextMarkerEnd > markerCapacity_ * sizeof(Marker) || nextMarkerEnd > capacity_ - payloadOffset)
    {
        return nullptr;
    }

    Marker& marker = markers_[markerCount_];
    marker.Begin = payloadOffset;
    marker.Size = size;
    marker.LeadingSlack = freeTop_ - (payloadOffset + size);
    freeTop_ = payloadOffset;
    ++markerCount_;
    return payload_ + payloadOffset;
}

void StackAllocator::DoFree(void* pointer) noexcept
{
    if (markerCount_ == 0)
    {
        return;
    }

    Marker& top = markers_[markerCount_ - 1];
    if (top.Begin != reinterpret_cast<uintptr_t>(pointer))
    {
        return;
    }

    freeTop_ = top.Begin + top.Size + top.LeadingSlack;
    --markerCount_;
}

void StackAllocator::DoReset() noexcept
{
    markerCount_ = 0;
    freeTop_ = capacity_;
}

size_t StackAllocator::RetiredSize(void* pointer) noexcept
{
    if (markerCount_ == 0)
    {
        return 0;
    }

    Marker& top = markers_[markerCount_ - 1];
    if (top.Begin != reinterpret_cast<uintptr_t>(pointer))
    {
        return 0;
    }

    return top.Size;
}

} // namespace kizuri::core