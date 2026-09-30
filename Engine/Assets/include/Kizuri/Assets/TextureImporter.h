#pragma once
#include "Kizuri/Assets/TexCodec.h"
#include <string>
namespace Kizuri {
bool ImportTextureFile(const std::string& path, const std::string& keepGuid, TextureAssetData& out);
bool ImportTextureMemory(const void* bytes, size_t size, const std::string& keepGuid, TextureAssetData& out, const std::string& nameHint, bool asNormal, const std::string& sourcePath, uint64_t sourceHash);
}