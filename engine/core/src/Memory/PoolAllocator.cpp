#include "kizuri/core/memory/PoolAllocator.h"

#include "kizuri/core/memory/MemoryTracker.h"

namespace kizuri::core {

namespace {

size_t AlignUpSize(size_t value, size_t alignment)
{
    size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

} // namespace

PoolAllocator::PoolAllocator(size_t objectSize, uint32_t blockCount, size_t /*slotAlignment*/)
    : Allocator("KizuriPoolAllocator")
    , objectSize_(objectSize < KernelAlignment ? KernelAlignment : objectSize)
    , blockCount_(blockCount)
    , freeCount_(blockCount)
{
    blockSize_ = AlignUpSize(objectSize_ + sizeof(SlotRecord) + 64, KernelAlignment);
    buffer_ = static_cast<unsigned char*>(MemoryTracker::Instance().AllocateRaw(blockSize_ * blockCount_));
    freeSlots_ = static_cast<uint32_t*>(MemoryTracker::Instance().AllocateRaw(sizeof(uint32_t) * blockCount_));
    for (uint32_t i = 0; i < blockCount_; ++i)
    {
        freeSlots_[blockCount_ - 1 - i] = i;
    }
}

PoolAllocator::~PoolAllocator()
{
    MemoryTracker::Instance().FreeRaw(freeSlots_);
    MemoryTracker::Instance().FreeRaw(buffer_);
    buffer_ = nullptr;
    freeSlots_ = nullptr;
}

void* PoolAllocator::DoAllocate(size_t size, size_t alignment)
{
    if (freeCount_ == 0)
    {
        return nullptr;
    }
    if (size > objectSize_)
    {
        return nullptr;
    }

    uint32_t slotIndex = freeSlots_[freeCount_ - 1];
    unsigned char* blockStart = buffer_ + slotIndex * blockSize_;
    uintptr_t payloadAddress = AlignUpSize(
        reinterpret_cast<uintptr_t>(blockStart) + sizeof(SlotRecord), alignment);
    if (payloadAddress + size > reinterpret_cast<uintptr_t>(blockStart) + blockSize_)
    {
        return nullptr;
    }

    SlotRecord* record = reinterpret_cast<SlotRecord*>(payloadAddress - sizeof(SlotRecord));
    record->Padding = payloadAddress - reinterpret_cast<uintptr_t>(blockStart);

    --freeCount_;
    return reinterpret_cast<void*>(payloadAddress);
}

void PoolAllocator::DoFree(void* pointer) noexcept
{
    SlotRecord* record = reinterpret_cast<SlotRecord*>(
        reinterpret_cast<uintptr_t>(pointer) - sizeof(SlotRecord));
    uintptr_t blockStart = reinterpret_cast<uintptr_t>(pointer) - record->Padding;
    uint32_t slotIndex = static_cast<uint32_t>((blockStart - reinterpret_cast<uintptr_t>(buffer_)) / blockSize_);
    freeSlots_[freeCount_] = slotIndex;
    ++freeCount_;
}

void PoolAllocator::DoReset() noexcept
{
    freeCount_ = 0;
    for (uint32_t i = 0; i < blockCount_; ++i)
    {
        freeSlots_[blockCount_ - 1 - i] = i;
    }
}

size_t PoolAllocator::ReportedSize(void* /*pointer*/, size_t /*requestedSize*/) noexcept
{
    return blockSize_;
}

size_t PoolAllocator::RetiredSize(void* /*pointer*/) noexcept
{
    return blockSize_;
}

} // namespace kizuri::core