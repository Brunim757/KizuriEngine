#include "Kizuri/Assets/Guid.h"
#include <cstdio>
#include <random>
#include <chrono>
namespace Kizuri {
std::string GenerateGuidString() {
  std::random_device rd;
  uint64_t hi = (static_cast<uint64_t>(rd()) << 32) | rd();
  uint64_t lo = (static_cast<uint64_t>(rd()) << 32) | rd();
  if (hi == 0 && lo == 0) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    hi = static_cast<uint64_t>(now);
    lo = static_cast<uint64_t>(now * 1099511628211ULL);
  }
  char buf[40];
  std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx",
    static_cast<unsigned int>((hi >> 32) & 0xFFFFFFFFULL),
    static_cast<unsigned int>((hi >> 16) & 0xFFFFULL),
    static_cast<unsigned int>(hi & 0xFFFFULL),
    static_cast<unsigned int>((lo >> 48) & 0xFFFFULL),
    static_cast<unsigned long long>(lo & 0xFFFFFFFFFFFFULL));
  return buf;
}
uint64_t Fnv1a64(const void* bytes, size_t size) {
  const unsigned char* p = static_cast<const unsigned char*>(bytes);
  uint64_t h = 14695981039346656037ULL;
  for (size_t i = 0; i < size; ++i) {
    h ^= p[i];
    h *= 1099511628211ULL;
  }
  return h;
}
bool Fnv1a64File(const std::string& path, uint64_t& outHash) {
  FILE* fp = std::fopen(path.c_str(), "rb");
  if (fp == nullptr) {
    return false;
  }
  uint64_t h = 14695981039346656037ULL;
  unsigned char buf[65536];
  size_t n = 0;
  while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) {
    for (size_t i = 0; i < n; ++i) {
      h ^= buf[i];
      h *= 1099511628211ULL;
    }
  }
  std::fclose(fp);
  outHash = h;
  return true;
}
}
