#include "Kizuri/Core.h"
#include "Kizuri/Render.h"
#include "Kizuri/Physics.h"
#include "Kizuri/Animation.h"
#include "Kizuri/Audio.h"
#include "Kizuri/Scripting.h"
#include "Kizuri/Allocator.h"
#include "Kizuri/JobSystem.h"
#include "Kizuri/Origin.h"
#include "Kizuri/RHI.h"
#include <TaskScheduler.h>
#include <cstdio>
#include <cstring>
#include <atomic>
namespace {
int Check(const char* name, bool ok) {
  std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  return ok ? 0 : 1;
}
struct AddTask : public enki::ITaskSet {
  std::atomic<int>* acc;
  int value;
  AddTask(std::atomic<int>* a, int v) : acc(a), value(v) { m_SetSize = 1; }
  void ExecuteRange(enki::TaskSetPartition, uint32_t) override {
    acc->fetch_add(value);
  }
};
bool TestPool() {
  Kizuri::PoolAllocator pool(64, 128, 16);
  if (!pool.Initialize()) {
    return false;
  }
  void* ptrs[128];
  for (int i = 0; i < 128; ++i) {
    ptrs[i] = pool.Allocate();
    if (ptrs[i] == nullptr) {
      return false;
    }
    std::memset(ptrs[i], 0xAB, 64);
  }
  if (pool.Allocate() != nullptr) {
    return false;
  }
  for (int i = 0; i < 128; ++i) {
    pool.Free(ptrs[i]);
  }
  if (pool.FreeCount() != pool.Capacity()) {
    return false;
  }
  void* a = pool.Allocate();
  void* b = pool.Allocate();
  bool ok = (a != nullptr && b != nullptr && a != b);
  pool.Free(a);
  pool.Free(b);
  pool.Shutdown();
  return ok;
}
bool TestArena() {
  Kizuri::ArenaAllocator arena(4096);
  if (!arena.Initialize()) {
    return false;
  }
  void* a = arena.Allocate(256, 16);
  void* b = arena.Allocate(512, 16);
  if (a == nullptr || b == nullptr) {
    return false;
  }
  if ((reinterpret_cast<uintptr_t>(a) % 16) != 0) {
    return false;
  }
  if ((reinterpret_cast<uintptr_t>(b) % 16) != 0) {
    return false;
  }
  if (arena.Used() == 0) {
    return false;
  }
  arena.Reset();
  if (arena.Used() != 0) {
    return false;
  }
  void* c = arena.Allocate(1024, 8);
  bool ok = (c != nullptr);
  arena.Shutdown();
  return ok;
}
bool TestJobSystem() {
  Kizuri::JobSystem js;
  if (!js.Initialize()) {
    return false;
  }
  if (js.GetThreadCount() == 0) {
    return false;
  }
  std::atomic<int> acc(0);
  AddTask t1(&acc, 10);
  AddTask t2(&acc, 20);
  AddTask t3(&acc, 30);
  AddTask t4(&acc, 40);
  js.AddTask(&t1);
  js.AddTask(&t2);
  js.AddTask(&t3);
  js.AddTask(&t4);
  js.WaitForTask(&t1);
  js.WaitForTask(&t2);
  js.WaitForTask(&t3);
  js.WaitForTask(&t4);
  bool ok = (acc.load() == 100);
  js.Shutdown();
  return ok;
}
bool TestOrigin() {
  Kizuri::OriginRebaser o;
  double ox;
  double oy;
  double oz;
  o.GetOrigin(ox, oy, oz);
  if (ox != 0.0 || oy != 0.0 || oz != 0.0) {
    return false;
  }
  float lx;
  float ly;
  float lz;
  o.WorldToLocal(100000.0, 0.0, 0.0, lx, ly, lz);
  if (lx < 99999.0f || lx > 100001.0f) {
    return false;
  }
  bool rebased = o.RebaseIfNeeded(100000.0, 0.0, 0.0, 5000.0);
  if (!rebased) {
    return false;
  }
  o.GetOrigin(ox, oy, oz);
  if (ox != 100000.0) {
    return false;
  }
  o.WorldToLocal(100010.0, 5.0, -3.0, lx, ly, lz);
  if (lx < 9.9f || lx > 10.1f) {
    return false;
  }
  double wx;
  double wy;
  double wz;
  o.LocalToWorld(lx, ly, lz, wx, wy, wz);
  double dx = wx - 100010.0;
  double dy = wy - 5.0;
  double dz = wz + 3.0;
  if (dx < -0.01 || dx > 0.01) {
    return false;
  }
  if (dy < -0.01 || dy > 0.01) {
    return false;
  }
  if (dz < -0.01 || dz > 0.01) {
    return false;
  }
  bool notNeeded = o.RebaseIfNeeded(100010.0, 5.0, -3.0, 5000.0);
  if (notNeeded) {
    return false;
  }
  return true;
}
bool TestRHINull() {
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  if (rhi == nullptr) {
    return false;
  }
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 1280;
  desc.height = 720;
  desc.vsync = true;
  if (!rhi->Initialize(desc)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (std::strcmp(rhi->BackendName(), "Null") != 0) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  rhi->Clear(0.1f, 0.2f, 0.3f, 1.0f);
  if (!rhi->Resize(800, 600)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (rhi->Width() != 800 || rhi->Height() != 600) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  rhi->Present(true);
  rhi->Shutdown();
  Kizuri::DestroyRHI(rhi);
  return true;
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
  failures += Check("PoolAllocator", TestPool());
  failures += Check("ArenaAllocator", TestArena());
  failures += Check("JobSystem", TestJobSystem());
  failures += Check("OriginRebasing", TestOrigin());
  failures += Check("RHI-Null", TestRHINull());
  if (failures == 0) {
    std::printf("KizuriHello: all bootstrap libs linked and functional\n");
  } else {
    std::printf("KizuriHello: %d failures\n", failures);
  }
  return failures == 0 ? 0 : 1;
}
