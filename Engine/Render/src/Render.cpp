#include "Kizuri/Render.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <cgltf.h>
#include <stb_image.h>
#include <cstring>
namespace Kizuri {
const char* Render_Version() {
  return "0.0.1-fase0";
}
bool Render_TestImGui() {
  const char* v = ImGui::GetVersion();
  if (v == nullptr || v[0] == '\0') {
    return false;
  }
  ImGui::CreateContext();
  ImGui::DestroyContext();
  return true;
}
bool Render_TestGizmo() {
  ImGui::CreateContext();
  ImGuizmo::Enable(true);
  ImGuizmo::SetOrthographic(false);
  bool probe = !ImGuizmo::IsOver();
  ImGui::DestroyContext();
  return probe;
}
bool Render_TestCgltf() {
  const char* json = "{\"asset\":{\"version\":\"2.0\"}}";
  cgltf_options options;
  std::memset(&options, 0, sizeof(options));
  cgltf_data* data = nullptr;
  cgltf_result r = cgltf_parse(&options, json, std::strlen(json), &data);
  if (r != cgltf_result_success) {
    return false;
  }
  cgltf_free(data);
  return true;
}
bool Render_TestStb() {
  static const unsigned char png[] = {
    0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
    0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
    0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
    0x08,0x06,0x00,0x00,0x00,0x1F,0x15,0xC4,
    0x89,0x00,0x00,0x00,0x0A,0x49,0x44,0x41,
    0x54,0x78,0x9C,0x63,0x00,0x01,0x00,0x00,
    0x05,0x00,0x01,0x0D,0x0A,0x2D,0xB4,0x00,
    0x00,0x00,0x00,0x49,0x45,0x4E,0x44,0xAE,
    0x42,0x60,0x82
  };
  int w = 0;
  int h = 0;
  int comp = 0;
  unsigned char* px = stbi_load_from_memory(png, sizeof(png), &w, &h, &comp, 4);
  if (px == nullptr) {
    return false;
  }
  bool ok = (w == 1 && h == 1);
  stbi_image_free(px);
  return ok;
}
}
