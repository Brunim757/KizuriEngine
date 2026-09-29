#pragma once
#include <stddef.h>
#include <stdint.h>
#include <mutex>
namespace Kizuri {
class PoolAllocator {
public:
  PoolAllocator(size_t blockSize, size_t blockCount, size_t alignment);
  ~PoolAllocator();
  bool Initialize();
  void Shutdown();
  void* Allocate();
  void Free(void* p);
  size_t BlockSize() const;
  size_t Capacity() const;
  size_t FreeCount() const;
private:
  size_t requestedSize;
  size_t blockSize;
  size_t blockCount;
  size_t alignment;
  unsigned char* memory;
  void* freeList;
  size_t freeCount;
  bool initialized;
  mutable std::mutex mtx;
  PoolAllocator(const PoolAllocator&) = delete;
  PoolAllocator& operator=(const PoolAllocator&) = delete;
};
class ArenaAllocator {
public:
  explicit ArenaAllocator(size_t capacity);
  ~ArenaAllocator();
  bool Initialize();
  void Shutdown();
  void* Allocate(size_t size, size_t alignment);
  void Reset();
  size_t Used() const;
  size_t Capacity() const;
private:
  size_t capacity;
  unsigned char* memory;
  size_t offset;
  bool initialized;
  ArenaAllocator(const ArenaAllocator&) = delete;
  ArenaAllocator& operator=(const ArenaAllocator&) = delete;
};
size_t AlignUp(size_t value, size_t alignment);
}
