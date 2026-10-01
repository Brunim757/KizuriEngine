#pragma once
#include <cstdio>
#include <vector>
#include <zstd.h>
namespace Kizuri {
inline bool StreamZstdToFile(const uint8_t* src, size_t srcSize, FILE* fp) {
  ZSTD_CCtx* cctx = ZSTD_createCCtx();
  if (cctx == nullptr) {
    return false;
  }
  if (ZSTD_isError(ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, 3))) {
    ZSTD_freeCCtx(cctx);
    return false;
  }
  std::vector<unsigned char> dst(1048576);
  ZSTD_inBuffer input;
  input.src = src;
  input.size = srcSize;
  input.pos = 0;
  size_t remaining = 1;
  while (input.pos < input.size) {
    ZSTD_outBuffer output;
    output.dst = dst.data();
    output.size = dst.size();
    output.pos = 0;
    remaining = ZSTD_compressStream2(cctx, &output, &input, ZSTD_e_continue);
    if (ZSTD_isError(remaining)) {
      ZSTD_freeCCtx(cctx);
      return false;
    }
    if (output.pos > 0 && std::fwrite(dst.data(), 1, output.pos, fp) != output.pos) {
      ZSTD_freeCCtx(cctx);
      return false;
    }
  }
  do {
    ZSTD_outBuffer output;
    output.dst = dst.data();
    output.size = dst.size();
    output.pos = 0;
    remaining = ZSTD_compressStream2(cctx, &output, &input, ZSTD_e_end);
    if (ZSTD_isError(remaining)) {
      ZSTD_freeCCtx(cctx);
      return false;
    }
    if (output.pos > 0 && std::fwrite(dst.data(), 1, output.pos, fp) != output.pos) {
      ZSTD_freeCCtx(cctx);
      return false;
    }
  } while (remaining != 0);
  ZSTD_freeCCtx(cctx);
  return true;
}
}