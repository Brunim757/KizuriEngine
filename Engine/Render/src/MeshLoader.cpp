#include "Kizuri/MeshLoader.h"
#include <cgltf.h>
#include <cstring>
namespace Kizuri {
namespace {
bool ReadFloat3(const cgltf_accessor* acc, size_t idx, float& x, float& y, float& z) {
  if (acc == nullptr || acc->type != cgltf_type_vec3 || acc->component_type != cgltf_component_type_r_float) {
    return false;
  }
  if (idx >= acc->count) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  size_t off = acc->buffer_view->offset + acc->offset + idx * acc->stride;
  const float* f = reinterpret_cast<const float*>(base + off);
  x = f[0];
  y = f[1];
  z = f[2];
  return true;
}
bool ReadFloat2(const cgltf_accessor* acc, size_t idx, float& x, float& y) {
  if (acc == nullptr || acc->type != cgltf_type_vec2 || acc->component_type != cgltf_component_type_r_float) {
    return false;
  }
  if (idx >= acc->count) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  size_t off = acc->buffer_view->offset + acc->offset + idx * acc->stride;
  const float* f = reinterpret_cast<const float*>(base + off);
  x = f[0];
  y = f[1];
  return true;
}
bool ReadIndex(const cgltf_accessor* acc, size_t idx, uint32_t& v) {
  if (acc == nullptr || acc->type != cgltf_type_scalar) {
    return false;
  }
  if (idx >= acc->count) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  size_t off = acc->buffer_view->offset + acc->offset + idx * acc->stride;
  if (acc->component_type == cgltf_component_type_r_16u) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(base + off);
    v = *p;
    return true;
  }
  if (acc->component_type == cgltf_component_type_r_32u) {
    const uint32_t* p = reinterpret_cast<const uint32_t*>(base + off);
    v = *p;
    return true;
  }
  if (acc->component_type == cgltf_component_type_r_16) {
    const int16_t* p = reinterpret_cast<const int16_t*>(base + off);
    if (*p < 0) {
      return false;
    }
    v = static_cast<uint32_t>(*p);
    return true;
  }
  return false;
}
}
bool LoadStaticMeshFromGltf(const char* path, StaticMesh& out) {
  out.positions.clear();
  out.normals.clear();
  out.uvs.clear();
  out.indices.clear();
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
  cgltf_options options;
  std::memset(&options, 0, sizeof(options));
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&options, path, &data) != cgltf_result_success) {
    return false;
  }
  bool ok = false;
  if (cgltf_load_buffers(&options, data, path) == cgltf_result_success) {
    if (data->meshes_count >= 1 && data->meshes[0].primitives_count >= 1) {
      const cgltf_primitive& prim = data->meshes[0].primitives[0];
      const cgltf_accessor* posAcc = nullptr;
      const cgltf_accessor* nrmAcc = nullptr;
      const cgltf_accessor* uvAcc = nullptr;
      for (size_t i = 0; i < prim.attributes_count; ++i) {
        if (std::strcmp(prim.attributes[i].name, "POSITION") == 0) {
          posAcc = prim.attributes[i].data;
        } else if (std::strcmp(prim.attributes[i].name, "NORMAL") == 0) {
          nrmAcc = prim.attributes[i].data;
        } else if (std::strcmp(prim.attributes[i].name, "TEXCOORD_0") == 0) {
          uvAcc = prim.attributes[i].data;
        }
      }
      if (posAcc != nullptr && posAcc->count > 0) {
        size_t n = posAcc->count;
        out.positions.reserve(n * 3);
        out.normals.reserve(n * 3);
        out.uvs.reserve(n * 2);
        ok = true;
        for (size_t i = 0; i < n && ok; ++i) {
          float x;
          float y;
          float z;
          if (!ReadFloat3(posAcc, i, x, y, z)) {
            ok = false;
            break;
          }
          out.positions.push_back(x);
          out.positions.push_back(y);
          out.positions.push_back(z);
          float nx = 0.0f;
          float ny = 1.0f;
          float nz = 0.0f;
          if (nrmAcc != nullptr) {
            if (!ReadFloat3(nrmAcc, i, nx, ny, nz)) {
              ok = false;
              break;
            }
          }
          out.normals.push_back(nx);
          out.normals.push_back(ny);
          out.normals.push_back(nz);
          float u = 0.0f;
          float v = 0.0f;
          if (uvAcc != nullptr) {
            if (!ReadFloat2(uvAcc, i, u, v)) {
              ok = false;
              break;
            }
          }
          out.uvs.push_back(u);
          out.uvs.push_back(v);
        }
        if (ok) {
          if (prim.indices != nullptr) {
            size_t m = prim.indices->count;
            out.indices.reserve(m);
            for (size_t i = 0; i < m && ok; ++i) {
              uint32_t idx = 0;
              if (!ReadIndex(prim.indices, i, idx)) {
                ok = false;
                break;
              }
              if (idx >= n) {
                ok = false;
                break;
              }
              out.indices.push_back(idx);
            }
          } else {
            for (uint32_t i = 0; i < static_cast<uint32_t>(n); ++i) {
              out.indices.push_back(i);
            }
          }
        }
      }
    }
  }
  cgltf_free(data);
  if (!ok) {
    out.positions.clear();
    out.normals.clear();
    out.uvs.clear();
    out.indices.clear();
  }
  return ok && !out.positions.empty() && !out.indices.empty();
}
}
