#include "kizuri/core/memory/FreeListAllocator.h"

#include "kizuri/core/memory/MemoryTracker.h"

namespace kizuri::core {

namespace {

constexpr size_t MinSplitBytes = 48;
constexpr size_t SlotRecordBytes = 16;

size_t AlignUpSize(size_t value, size_t alignment)
{
    size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

} // namespace

FreeListAllocator::FreeListAllocator(size_t capacity)
    : Allocator("KizuriFreeListAllocator")
    , capacity_(capacity)
{
    buffer_ = static_cast<unsigned char*>(MemoryTracker::Instance().AllocateRaw(capacity_));
    freeHead_ = reinterpret_cast<FreeBlock*>(buffer_);
    freeHead_->Size = capacity_;
    freeHead_->Next = nullptr;
}

FreeListAllocator::~FreeListAllocator()
{
    MemoryTracker::Instance().FreeRaw(buffer_);
    buffer_ = nullptr;
    freeHead_ = nullptr;
}

void* FreeListAllocator::DoAllocate(size_t size, size_t alignment)
{
    FreeBlock* previous = nullptr;
    FreeBlock* block = freeHead_;
    while (block != nullptr)
    {
        uintptr_t blockStart = reinterpret_cast<uintptr_t>(block);
        uintptr_t payloadCandidate = AlignUpSize(blockStart + SlotRecordBytes, alignment);
        uintptr_t blockEnd = blockStart + block->Size;
        if (payloadCandidate + size > blockEnd)
        {
            previous = block;
            block = block->Next;
            continue;
        }

        uintptr_t payloadEnd = payloadCandidate + size;
        size_t capacity = block->Size;

        FreeBlock* savedNext = block->Next;
        if (previous == nullptr)
        {
            freeHead_ = savedNext;
        }
        else
        {
            previous->Next = savedNext;
        }

        uintptr_t tailCandidate = AlignUpSize(payloadEnd, KernelAlignment);
        if (blockEnd >= tailCandidate + MinSplitBytes)
        {
            FreeBlock* tail = reinterpret_cast<FreeBlock*>(tailCandidate);
            tail->Size = blockEnd - tailCandidate;
            tail->Next = savedNext;
            if (previous == nullptr)
            {
                freeHead_ = tail;
            }
            else
            {
                previous->Next = tail;
            }
            capacity = tailCandidate - blockStart;
        }

        SlotRecord* record = reinterpret_cast<SlotRecord*>(payloadCandidate - SlotRecordBytes);
        record->Capacity = capacity;
        record->BlockStart = blockStart;
        return reinterpret_cast<void*>(payloadCandidate);
    }

    return nullptr;
}

void FreeListAllocator::DoFree(void* pointer) noexcept
{
    SlotRecord* record = reinterpret_cast<SlotRecord*>(
        reinterpret_cast<uintptr_t>(pointer) - SlotRecordBytes);
    uintptr_t blockStart = record->BlockStart;
    size_t capacity = record->Capacity;

    FreeBlock* block = reinterpret_cast<FreeBlock*>(blockStart);
    block->Size = capacity;

    uintptr_t start = blockStart;
    uintptr_t end = start + capacity;
    FreeBlock** iterator = &freeHead_;
    while (*iterator != nullptr)
    {
        FreeBlock* current = *iterator;
        uintptr_t currentStart = reinterpret_cast<uintptr_t>(current);
        uintptr_t currentEnd = currentStart + current->Size;
        if (end == currentStart)
        {
            block->Size += current->Size;
            *iterator = current->Next;
            continue;
        }
        if (currentEnd == start)
        {
            current->Size += block->Size;
            return;
        }
        if (start < currentStart)
        {
            block->Next = current;
            *iterator = block;
            return;
        }
        iterator = &current->Next;
    }
    block->Next = nullptr;
    *iterator = block;
}

void FreeListAllocator::DoReset() noexcept
{
    freeHead_ = reinterpret_cast<FreeBlock*>(buffer_);
    freeHead_->Size = capacity_;
    freeHead_->Next = nullptr;
}

size_t FreeListAllocator::ReportedSize(void* pointer, size_t requestedSize) noexcept
{
    SlotRecord* record = reinterpret_cast<SlotRecord*>(
        reinterpret_cast<uintptr_t>(pointer) - SlotRecordBytes);
    return record->Capacity;
}

size_t FreeListAllocator::RetiredSize(void* pointer) noexcept
{
    SlotRecord* record = reinterpret_cast<SlotRecord*>(
        reinterpret_cast<uintptr_t>(pointer) - SlotRecordBytes);
    return record->Capacity;
}

} // namespace kizuri::core