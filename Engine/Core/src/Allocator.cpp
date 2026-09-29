#include "Kizuri/Allocator.h"
#include <malloc.h>
namespace Kizuri {
size_t AlignUp(size_t value, size_t alignment) {
  if (alignment == 0) {
    return value;
  }
  size_t mask = alignment - 1;
  return (value + mask) & ~mask;
}
PoolAllocator::PoolAllocator(size_t bSize, size_t bCount, size_t align)
  : requestedSize(bSize)
  , blockSize(0)
  , blockCount(bCount)
  , alignment(align == 0 ? 8 : align)
  , memory(nullptr)
  , freeList(nullptr)
  , freeCount(0)
  , initialized(false) {
  size_t minSize = sizeof(void*);
  size_t base = requestedSize < minSize ? minSize : requestedSize;
  blockSize = AlignUp(base, alignment);
}
PoolAllocator::~PoolAllocator() {
  Shutdown();
}
bool PoolAllocator::Initialize() {
  if (initialized) {
    return true;
  }
  if (blockCount == 0 || blockSize == 0) {
    return false;
  }
  size_t total = blockSize * blockCount;
  memory = static_cast<unsigned char*>(_aligned_malloc(total, alignment));
  if (memory == nullptr) {
    return false;
  }
  freeList = nullptr;
  for (size_t i = 0; i < blockCount; ++i) {
    void* block = memory + (i * blockSize);
    *static_cast<void**>(block) = freeList;
    freeList = block;
  }
  freeCount = blockCount;
  initialized = true;
  return true;
}
void PoolAllocator::Shutdown() {
  if (!initialized) {
    return;
  }
  _aligned_free(memory);
  memory = nullptr;
  freeList = nullptr;
  freeCount = 0;
  initialized = false;
}
void* PoolAllocator::Allocate() {
  std::lock_guard<std::mutex> lock(mtx);
  if (freeList == nullptr) {
    return nullptr;
  }
  void* block = freeList;
  freeList = *static_cast<void**>(block);
  --freeCount;
  return block;
}
void PoolAllocator::Free(void* p) {
  if (p == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(mtx);
  *static_cast<void**>(p) = freeList;
  freeList = p;
  ++freeCount;
}
size_t PoolAllocator::BlockSize() const {
  return blockSize;
}
size_t PoolAllocator::Capacity() const {
  return blockCount;
}
size_t PoolAllocator::FreeCount() const {
  return freeCount;
}
ArenaAllocator::ArenaAllocator(size_t cap)
  : capacity(cap)
  , memory(nullptr)
  , offset(0)
  , initialized(false) {
}
ArenaAllocator::~ArenaAllocator() {
  Shutdown();
}
bool ArenaAllocator::Initialize() {
  if (initialized) {
    return true;
  }
  if (capacity == 0) {
    return false;
  }
  memory = static_cast<unsigned char*>(_aligned_malloc(capacity, 16));
  if (memory == nullptr) {
    return false;
  }
  offset = 0;
  initialized = true;
  return true;
}
void ArenaAllocator::Shutdown() {
  if (!initialized) {
    return;
  }
  _aligned_free(memory);
  memory = nullptr;
  offset = 0;
  initialized = false;
}
void* ArenaAllocator::Allocate(size_t size, size_t alignment) {
  if (!initialized || size == 0) {
    return nullptr;
  }
  size_t align = alignment == 0 ? 8 : alignment;
  size_t aligned = AlignUp(offset, align);
  if (aligned + size > capacity) {
    return nullptr;
  }
  void* p = memory + aligned;
  offset = aligned + size;
  return p;
}
void ArenaAllocator::Reset() {
  offset = 0;
}
size_t ArenaAllocator::Used() const {
  return offset;
}
size_t ArenaAllocator::Capacity() const {
  return capacity;
}
}
