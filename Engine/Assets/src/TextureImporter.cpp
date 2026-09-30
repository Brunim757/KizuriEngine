#include "Kizuri/Assets/TextureImporter.h"
#include "Kizuri/Assets/Guid.h"
#include <stb_image.h>
#include <DirectXTex.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
namespace Kizuri {
namespace {
bool NameHintsNormal(const std::string& name) {
  std::string lower = name;
  for (size_t i = 0; i < lower.size(); ++i) {
    lower[i] = static_cast<char>(tolower(lower[i]));
  }
  return lower.find("normal") != std::string::npos || lower.find("_nrm") != std::string::npos || lower.find("_nor") != std::string::npos || lower.find("normalmap") != std::string::npos;
}
}
bool ImportTextureMemory(const void* bytes, size_t size, const std::string& keepGuid, TextureAssetData& out, const std::string& nameHint, bool asNormal, const std::string& sourcePath, uint64_t sourceHash) {
  out = TextureAssetData();
  if (bytes == nullptr || size == 0) {
    return false;
  }
  int w = 0;
  int h = 0;
  int comp = 0;
  unsigned char* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(bytes), static_cast<int>(size), &w, &h, &comp, 4);
  if (pixels == nullptr || w <= 0 || h <= 0) {
    return false;
  }
  bool normal = asNormal || NameHintsNormal(nameHint);
  bool hasAlpha = false;
  for (int i = 0; i < w * h; ++i) {
    if (pixels[i * 4 + 3] < 255) {
      hasAlpha = true;
      break;
    }
  }
  bool srgb = !normal;
  DirectX::ScratchImage src;
  if (FAILED(src.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, w, h, 1, 1))) {
    stbi_image_free(pixels);
    return false;
  }
  std::memcpy(src.GetPixels(), pixels, static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  stbi_image_free(pixels);
  DirectX::ScratchImage mipChain;
  uint32_t filter = TEX_FILTER_DEFAULT;
  if (srgb) {
    filter |= TEX_FILTER_SRGB;
  }
  if (FAILED(DirectX::GenerateMipMaps(src.GetImages(), src.GetImageCount(), src.GetMetadata(), filter, 0, mipChain))) {
    return false;
  }
  DXGI_FORMAT destFormat = DXGI_FORMAT_BC1_UNORM;
  TexFormat outFormat = TexFormat::Bc1;
  if (normal) {
    destFormat = DXGI_FORMAT_BC5_UNORM;
    outFormat = TexFormat::Bc5;
  } else if (hasAlpha) {
    destFormat = srgb ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM;
    outFormat = TexFormat::Bc3;
  } else {
    destFormat = srgb ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM;
    outFormat = TexFormat::Bc1;
  }
  DirectX::ScratchImage compressed;
  if (FAILED(DirectX::Compress(mipChain.GetImages(), mipChain.GetImageCount(), mipChain.GetMetadata(), destFormat, TEX_COMPRESS_DEFAULT, TEX_THRESHOLD_DEFAULT, compressed))) {
    return false;
  }
  out.guid = keepGuid.empty() ? GenerateGuidString() : keepGuid;
  out.width = static_cast<uint32_t>(w);
  out.height = static_cast<uint32_t>(h);
  out.format = outFormat;
  out.srgb = srgb;
  for (size_t i = 0; i < compressed.GetImageCount(); ++i) {
    const DirectX::Image* img = compressed.GetImages() + i;
    TextureMipData mip;
    mip.width = static_cast<uint32_t>(img->width);
    mip.height = static_cast<uint32_t>(img->height);
    mip.rowPitch = static_cast<uint32_t>(img->rowPitch);
    mip.data.assign(img->pixels, img->pixels + img->slicePitch);
    out.mips.push_back(mip);
  }
  if (out.mips.empty()) {
    out = TextureAssetData();
    return false;
  }
  out.hasSource = !sourcePath.empty();
  out.sourcePath = sourcePath;
  out.sourceHash = sourceHash;
  out.sourceTimestamp = 0;
  if (out.hasSource) {
    std::error_code ec;
    std::filesystem::file_time_type mtime = std::filesystem::last_write_time(sourcePath, ec);
    if (!ec) {
      out.sourceTimestamp = static_cast<int64_t>(mtime.time_since_epoch().count());
    }
  }
  return true;
}
bool ImportTextureFile(const std::string& path, const std::string& keepGuid, TextureAssetData& out) {
  out = TextureAssetData();
  if (path.empty()) {
    return false;
  }
  FILE* fp = std::fopen(path.c_str(), "rb");
  if (fp == nullptr) {
    return false;
  }
  std::fseek(fp, 0, SEEK_END);
  long sz = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);
  if (sz <= 0) {
    std::fclose(fp);
    return false;
  }
  std::vector<unsigned char> bytes(static_cast<size_t>(sz));
  size_t n = std::fread(bytes.data(), 1, bytes.size(), fp);
  std::fclose(fp);
  if (n != bytes.size()) {
    return false;
  }
  uint64_t hash = Fnv1a64(bytes.data(), bytes.size());
  std::filesystem::path p(path);
  return ImportTextureMemory(bytes.data(), bytes.size(), keepGuid, out, p.filename().string(), false, path, hash);
}
}
