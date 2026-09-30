#pragma once
#include <string>
#include <stdint.h>
#include <vector>
namespace Kizuri {
std::string GenerateGuidString();
uint64_t Fnv1a64(const void* bytes, size_t size);
bool Fnv1a64File(const std::string& path, uint64_t& outHash);
}