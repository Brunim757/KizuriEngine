#include "kizuri/core/memory/LinearAllocator.h"

#include "kizuri/core/memory/MemoryTracker.h"

namespace kizuri::core {

namespace {

size_t AlignUp(size_t value, size_t alignment)
{
    size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

} // namespace

LinearAllocator::LinearAllocator(size_t capacity)
    : Allocator("KizuriLinearAllocator")
    , capacity_(capacity)
    , cursor_(0)
{
    buffer_ = static_cast<unsigned char*>(MemoryTracker::Instance().AllocateRaw(capacity_));
}

LinearAllocator::~LinearAllocator()
{
    MemoryTracker::Instance().FreeRaw(buffer_);
    buffer_ = nullptr;
}

void* LinearAllocator::DoAllocate(size_t size, size_t alignment)
{
    size_t alignedCursor = AlignUp(cursor_, alignment);
    if (alignedCursor + size > capacity_)
    {
        return nullptr;
    }
    cursor_ = alignedCursor + size;
    return buffer_ + alignedCursor;
}

void LinearAllocator::DoFree(void* pointer) noexcept
{
}

void LinearAllocator::DoReset() noexcept
{
    cursor_ = 0;
}

} // namespace kizuri::core