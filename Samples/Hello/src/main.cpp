#include "Kizuri/Core.h"
#include "Kizuri/Render.h"
#include "Kizuri/Physics.h"
#include "Kizuri/Animation.h"
#include "Kizuri/Audio.h"
#include "Kizuri/Scripting.h"
#include <cstdio>
namespace {
int Check(const char* name, bool ok) {
  std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  return ok ? 0 : 1;
}
}
int main() {
  std::printf("KizuriHello %s\n", Kizuri::Core_Version());
  int failures = 0;
  failures += Check("DirectXMath", Kizuri::Core_TestDirectXMath());
  failures += Check("enkiTS", Kizuri::Core_TestTaskSystem());
  failures += Check("zstd", Kizuri::Core_TestZstd());
  failures += Check("FlatBuffers", Kizuri::Core_TestFlatBuffers());
  failures += Check("DearImGui", Kizuri::Render_TestImGui());
  failures += Check("ImGuizmo", Kizuri::Render_TestGizmo());
  failures += Check("cgltf", Kizuri::Render_TestCgltf());
  failures += Check("stb_image", Kizuri::Render_TestStb());
  failures += Check("miniaudio-noDevice", Kizuri::Audio_TestEngineNoDevice());
  failures += Check("Physics", Kizuri::Physics_SelfTest());
  failures += Check("Animation", Kizuri::Animation_SelfTest());
  failures += Check("Scripting", Kizuri::Scripting_SelfTest());
  if (failures == 0) {
    std::printf("KizuriHello: all bootstrap libs linked and functional\n");
  } else {
    std::printf("KizuriHello: %d failures\n", failures);
  }
  return failures == 0 ? 0 : 1;
}
