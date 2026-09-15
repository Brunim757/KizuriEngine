#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "kizuri/core/memory/FreeListAllocator.h"
#include "kizuri/core/memory/LinearAllocator.h"
#include "kizuri/core/memory/MemoryTracker.h"
#include "kizuri/core/memory/PoolAllocator.h"
#include "kizuri/core/memory/StackAllocator.h"

namespace {

bool IsAligned(void* pointer, size_t alignment)
{
    return reinterpret_cast<uintptr_t>(pointer) % alignment == 0;
}

} // namespace

TEST_CASE("LinearAllocator allocates with alignment", "[allocator][linear]")
{
    kizuri::core::LinearAllocator allocator(1024);
    REQUIRE(allocator.Name() != nullptr);

    void* first = allocator.Allocate(32, 16);
    REQUIRE(first != nullptr);
    REQUIRE(IsAligned(first, 16));

    void* second = allocator.Allocate(64, 64);
    REQUIRE(second != nullptr);
    REQUIRE(IsAligned(second, 64));
}

TEST_CASE("LinearAllocator exhausts deterministically", "[allocator][linear]")
{
    kizuri::core::LinearAllocator allocator(256);
    void* a = allocator.Allocate(128);
    void* b = allocator.Allocate(128);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    void* overflow = allocator.Allocate(1);
    REQUIRE(overflow == nullptr);
}

TEST_CASE("LinearAllocator reset reuses region", "[allocator][linear]")
{
    kizuri::core::LinearAllocator allocator(512);
    void* a = allocator.Allocate(256);
    REQUIRE(a != nullptr);
    allocator.Reset();
    void* b = allocator.Allocate(256);
    REQUIRE(b != nullptr);
}

TEST_CASE("StackAllocator free follows LIFO", "[allocator][stack]")
{
    kizuri::core::StackAllocator allocator(1024);
    void* a = allocator.Allocate(128);
    void* b = allocator.Allocate(128, 32);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(IsAligned(b, 32));

    allocator.Free(b);
    void* c = allocator.Allocate(128, 32);
    REQUIRE(c != nullptr);
    REQUIRE(IsAligned(c, 32));

    allocator.Free(c);
    allocator.Free(a);
    allocator.Reset();
    void* d = allocator.Allocate(1024);
    REQUIRE(d != nullptr);
}

TEST_CASE("StackAllocator exhausts capacity", "[allocator][stack]")
{
    kizuri::core::StackAllocator allocator(512);
    void* a = allocator.Allocate(500);
    REQUIRE(a != nullptr);
    void* overflow = allocator.Allocate(32);
    REQUIRE(overflow == nullptr);
}

TEST_CASE("PoolAllocator services fixed slots", "[allocator][pool]")
{
    kizuri::core::PoolAllocator allocator(64, 4);
    void* slots[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        slots[i] = allocator.Allocate(48);
        REQUIRE(slots[i] != nullptr);
    }
    void* overflow = allocator.Allocate(48);
    REQUIRE(overflow == nullptr);

    allocator.Free(slots[1]);
    allocator.Free(slots[3]);
    void* reusedA = allocator.Allocate(48);
    void* reusedB = allocator.Allocate(48);
    REQUIRE(reusedA != nullptr);
    REQUIRE(reusedB != nullptr);

    allocator.Reset();
    REQUIRE(allocator.FreeBlockCount() == 4);
    void* again = allocator.Allocate(48);
    REQUIRE(again != nullptr);
}

TEST_CASE("PoolAllocator rejects oversized objects", "[allocator][pool]")
{
    kizuri::core::PoolAllocator allocator(32, 2);
    void* oversized = allocator.Allocate(64);
    REQUIRE(oversized == nullptr);
}

TEST_CASE("FreeListAllocator round trips mixed sizes", "[allocator][freelist]")
{
    kizuri::core::FreeListAllocator allocator(4096);
    void* ptrs[8];
    size_t sizes[8] = { 16, 248, 64, 512, 32, 128, 96, 256 };
    for (uint32_t i = 0; i < 8; ++i)
    {
        ptrs[i] = allocator.Allocate(sizes[i]);
        REQUIRE(ptrs[i] != nullptr);
        REQUIRE(IsAligned(ptrs[i], 16));
    }
    for (uint32_t i = 0; i < 8; i += 2)
    {
        allocator.Free(ptrs[i]);
    }
    void* reclaimed = allocator.Allocate(64);
    REQUIRE(reclaimed != nullptr);
    allocator.Reset();
    void* full = allocator.Allocate(4000);
    REQUIRE(full != nullptr);
}

TEST_CASE("FreeListAllocator aligns payloads", "[allocator][freelist]")
{
    kizuri::core::FreeListAllocator allocator(1024);
    void* a = allocator.Allocate(16, 64);
    REQUIRE(a != nullptr);
    REQUIRE(IsAligned(a, 64));
    void* b = allocator.Allocate(16, 64);
    REQUIRE(b != nullptr);
    REQUIRE(IsAligned(b, 64));
}

TEST_CASE("FreeListAllocator exhausts capacity", "[allocator][freelist]")
{
    kizuri::core::FreeListAllocator allocator(128);
    void* a = allocator.Allocate(100);
    REQUIRE(a != nullptr);
    void* overflow = allocator.Allocate(100);
    REQUIRE(overflow == nullptr);
}

TEST_CASE("Allocators report usage the tracker can verify", "[memory]")
{
    const size_t trackedBefore = kizuri::core::MemoryTracker::Instance().TotalBytes();
    {
        kizuri::core::LinearAllocator linear(1024);
        kizuri::core::StackAllocator stack(1024);
        kizuri::core::PoolAllocator pool(64, 8);
        kizuri::core::FreeListAllocator freeList(1024);
        void* a = linear.Allocate(128);
        void* b = stack.Allocate(64);
        void* c = pool.Allocate(48);
        void* d = freeList.Allocate(128);
        REQUIRE(a != nullptr);
        REQUIRE(b != nullptr);
        REQUIRE(c != nullptr);
        REQUIRE(d != nullptr);
        REQUIRE(linear.AllocatedBytes() >= 128);
        REQUIRE(linear.PeakBytes() >= 128);
        REQUIRE(pool.AllocationCount() >= 1);
    }
    REQUIRE(kizuri::core::MemoryTracker::Instance().TotalBytes() == trackedBefore);
}