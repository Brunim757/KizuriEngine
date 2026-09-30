#include "Kizuri/Assets/MeshImporter.h"
#include "Kizuri/Assets/Guid.h"
#include "Kizuri/Log.h"
#include <cgltf.h>
#include <cstring>
#include <filesystem>
#include <system_error>
namespace Kizuri {
namespace {
bool ReadVec3(const cgltf_accessor* acc, size_t idx, float* v) {
  if (acc == nullptr || acc->type != cgltf_type_vec3 || acc->component_type != cgltf_component_type_r_32f) {
    return false;
  }
  if (idx >= acc->count || acc->buffer_view == nullptr || acc->buffer_view->buffer == nullptr) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  const float* f = reinterpret_cast<const float*>(base + acc->buffer_view->offset + acc->offset + idx * acc->stride);
  v[0] = f[0];
  v[1] = f[1];
  v[2] = f[2];
  return true;
}
bool ReadVec2(const cgltf_accessor* acc, size_t idx, float* v) {
  if (acc == nullptr || acc->type != cgltf_type_vec2 || acc->component_type != cgltf_component_type_r_32f) {
    return false;
  }
  if (idx >= acc->count || acc->buffer_view == nullptr || acc->buffer_view->buffer == nullptr) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  const float* f = reinterpret_cast<const float*>(base + acc->buffer_view->offset + acc->offset + idx * acc->stride);
  v[0] = f[0];
  v[1] = f[1];
  return true;
}
bool ReadIndex(const cgltf_accessor* acc, size_t idx, uint32_t& v) {
  if (acc == nullptr || acc->type != cgltf_type_scalar || acc->buffer_view == nullptr || acc->buffer_view->buffer == nullptr) {
    return false;
  }
  if (idx >= acc->count) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  size_t off = acc->buffer_view->offset + acc->offset + idx * acc->stride;
  if (acc->component_type == cgltf_component_type_r_16u) {
    v = *reinterpret_cast<const uint16_t*>(base + off);
    return true;
  }
  if (acc->component_type == cgltf_component_type_r_32u) {
    v = *reinterpret_cast<const uint32_t*>(base + off);
    return true;
  }
  return false;
}
}
bool ImportGltfMesh(const std::string& glbPath, const std::string& keepGuid, MeshAssetData& out, LogStore* log) {
  out = MeshAssetData();
  if (glbPath.empty()) {
    return false;
  }
  uint64_t hash = 0;
  if (!Fnv1a64File(glbPath, hash)) {
    return false;
  }
  cgltf_options options;
  std::memset(&options, 0, sizeof(options));
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&options, glbPath.c_str(), &data) != cgltf_result_success) {
    return false;
  }
  bool ok = false;
  if (cgltf_load_buffers(&options, data, glbPath.c_str()) == cgltf_result_success && data->meshes_count > 0) {
    ok = true;
    for (size_t mi = 0; mi < data->meshes_count && ok; ++mi) {
      const cgltf_mesh& mesh = data->meshes[mi];
      for (size_t pi = 0; pi < mesh.primitives_count && ok; ++pi) {
        const cgltf_primitive& prim = mesh.primitives[pi];
        if (prim.type != cgltf_primitive_type_triangles) {
          continue;
        }
        const cgltf_accessor* posAcc = nullptr;
        const cgltf_accessor* nrmAcc = nullptr;
        const cgltf_accessor* uvAcc = nullptr;
        for (size_t ai = 0; ai < prim.attributes_count; ++ai) {
          if (std::strcmp(prim.attributes[ai].name, "POSITION") == 0) {
            posAcc = prim.attributes[ai].data;
          } else if (std::strcmp(prim.attributes[ai].name, "NORMAL") == 0) {
            nrmAcc = prim.attributes[ai].data;
          } else if (std::strcmp(prim.attributes[ai].name, "TEXCOORD_0") == 0) {
            uvAcc = prim.attributes[ai].data;
          }
        }
        if (posAcc == nullptr || posAcc->count == 0) {
          continue;
        }
        uint32_t baseVertex = static_cast<uint32_t>(out.positions.size() / 3);
        for (size_t vi = 0; vi < posAcc->count && ok; ++vi) {
          float p[3] = { 0, 0, 0 };
          float n[3] = { 0, 1, 0 };
          float uv[2] = { 0, 0 };
          if (!ReadVec3(posAcc, vi, p)) {
            ok = false;
            break;
          }
          if (nrmAcc != nullptr && !ReadVec3(nrmAcc, vi, n)) {
            ok = false;
            break;
          }
          if (uvAcc != nullptr && !ReadVec2(uvAcc, vi, uv)) {
            ok = false;
            break;
          }
          out.positions.push_back(p[0]);
          out.positions.push_back(p[1]);
          out.positions.push_back(p[2]);
          out.normals.push_back(n[0]);
          out.normals.push_back(n[1]);
          out.normals.push_back(n[2]);
          out.uvs.push_back(uv[0]);
          out.uvs.push_back(uv[1]);
        }
        if (!ok) {
          break;
        }
        uint32_t matIndex = 0;
        if (prim.material != nullptr) {
          bool found = false;
          std::string matName = prim.material->name != nullptr ? prim.material->name : "Material";
          for (size_t m = 0; m < out.materials.size(); ++m) {
            if (out.materials[m].name == matName) {
              matIndex = static_cast<uint32_t>(m);
              found = true;
              break;
            }
          }
          if (!found) {
            MeshMaterialData md;
            md.name = matName;
            md.albedo[0] = 1.0f;
            md.albedo[1] = 1.0f;
            md.albedo[2] = 1.0f;
            md.metallic = 0.0f;
            md.roughness = 0.5f;
            md.albedoTexGuid = "";
            if (prim.material->has_pbr_metallic_roughness) {
              md.albedo[0] = prim.material->pbr_metallic_roughness.base_color_factor[0];
              md.albedo[1] = prim.material->pbr_metallic_roughness.base_color_factor[1];
              md.albedo[2] = prim.material->pbr_metallic_roughness.base_color_factor[2];
              md.metallic = prim.material->pbr_metallic_roughness.metallic_factor;
              md.roughness = prim.material->pbr_metallic_roughness.roughness_factor;
            }
            matIndex = static_cast<uint32_t>(out.materials.size());
            out.materials.push_back(md);
          }
        } else if (out.materials.empty()) {
          MeshMaterialData md;
          md.name = "Default";
          md.albedo[0] = 1.0f;
          md.albedo[1] = 1.0f;
          md.albedo[2] = 1.0f;
          md.metallic = 0.0f;
          md.roughness = 0.5f;
          out.materials.push_back(md);
        }
        MeshPartData part;
        part.indexOffset = static_cast<uint32_t>(out.indices.size());
        part.material = matIndex;
        if (prim.indices != nullptr) {
          for (size_t ii = 0; ii < prim.indices->count; ++ii) {
            uint32_t idx = 0;
            if (!ReadIndex(prim.indices, ii, idx)) {
              ok = false;
              break;
            }
            out.indices.push_back(baseVertex + idx);
          }
        } else {
          for (uint32_t vi = 0; vi < static_cast<uint32_t>(posAcc->count); ++vi) {
            out.indices.push_back(baseVertex + vi);
          }
        }
        part.indexCount = static_cast<uint32_t>(out.indices.size()) - part.indexOffset;
        if (part.indexCount > 0) {
          out.parts.push_back(part);
        }
      }
    }
    if (ok && data->skins_count > 0 && log != nullptr) {
      log->Add(LogLevel::Warning, "Skinned mesh imported in bind pose; skeleton arrives in Fase 8");
    }
  }
  cgltf_free(data);
  if (!ok || out.positions.empty() || out.indices.empty() || out.parts.empty()) {
    out = MeshAssetData();
    return false;
  }
  if (out.materials.empty()) {
    MeshMaterialData md;
    md.name = "Default";
    md.albedo[0] = 1.0f;
    md.albedo[1] = 1.0f;
    md.albedo[2] = 1.0f;
    md.metallic = 0.0f;
    md.roughness = 0.5f;
    out.materials.push_back(md);
    for (size_t i = 0; i < out.parts.size(); ++i) {
      out.parts[i].material = 0;
    }
  }
  out.guid = keepGuid.empty() ? GenerateGuidString() : keepGuid;
  out.hasSource = true;
  out.sourcePath = glbPath;
  out.sourceHash = hash;
  out.sourceTimestamp = 0;
  std::error_code tec;
  std::filesystem::file_time_type mtime = std::filesystem::last_write_time(glbPath, tec);
  if (!tec) {
    out.sourceTimestamp = static_cast<int64_t>(mtime.time_since_epoch().count());
  }
  ComputeAABB(out.positions, out.aabbMin, out.aabbMax);
  return true;
}
}
