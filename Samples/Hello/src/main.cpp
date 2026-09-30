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
#include "Kizuri/Camera.h"
#include "Kizuri/MeshLoader.h"
#include "Kizuri/PBR.h"
#include "Kizuri/DeferredRenderer.h"
#include "Kizuri/Scene.h"
#include "Kizuri/MultiSelection.h"
#include "Kizuri/Log.h"
#include "Kizuri/EditQueue.h"
#include "Kizuri/Picking.h"
#include "Kizuri/Reflection.h"
#include "Kizuri/SceneSerializer.h"
#include "Kizuri/Undo.h"
#include <DirectXMath.h>
#include <TaskScheduler.h>
#include <cstdio>
#include <cstring>
#include <atomic>
namespace {
int Check(const char* name, bool ok) {
  std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  std::fflush(stdout);
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
bool TestStateCache() {
  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  if (rhi == nullptr) {
    return false;
  }
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 640;
  desc.height = 480;
  desc.vsync = false;
  if (!rhi->Initialize(desc)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  Kizuri::RHIRasterizer rs;
  rs.cull = Kizuri::RHICull::Back;
  rs.fill = Kizuri::RHIFill::Solid;
  rs.frontCCW = false;
  rhi->SetRasterizerState(rs);
  rhi->SetRasterizerState(rs);
  rhi->SetRasterizerState(rs);
  Kizuri::RHIViewport vp;
  vp.x = 0.0f;
  vp.y = 0.0f;
  vp.w = 640.0f;
  vp.h = 480.0f;
  vp.minD = 0.0f;
  vp.maxD = 1.0f;
  rhi->SetViewport(vp);
  rhi->SetViewport(vp);
  uint64_t total = 0;
  uint64_t discarded = 0;
  rhi->GetCacheStats(total, discarded);
  rhi->Shutdown();
  Kizuri::DestroyRHI(rhi);
  return total >= 5 && discarded >= 3;
}
bool TestCamera() {
  Kizuri::FreeCamera cam;
  cam.SetPosition(0.0f, 0.0f, -5.0f);
  cam.SetYawPitch(0.0f, 0.0f);
  DirectX::XMMATRIX v = cam.View();
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMStoreFloat4x4(&vf, v);
  DirectX::XMVECTOR origin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
  DirectX::XMVECTOR vs = DirectX::XMVector3TransformCoord(origin, v);
  DirectX::XMFLOAT3 o;
  DirectX::XMStoreFloat3(&o, vs);
  if (o.z < 4.9f || o.z > 5.1f) {
    return false;
  }
  DirectX::XMMATRIX p = cam.Projection(16.0f / 9.0f);
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&pf, p);
  if (pf.m[0][0] <= 0.0f || pf.m[1][1] <= 0.0f) {
    return false;
  }
  return true;
}
bool TestMeshLoader() {
  const char* paths[3] = { "Samples/Assets/cube.gltf", "Assets/cube.gltf", "build/bin/Release/Assets/cube.gltf" };
  for (int i = 0; i < 3; ++i) {
    Kizuri::StaticMesh m;
    if (Kizuri::LoadStaticMeshFromGltf(paths[i], m)) {
      if (m.positions.size() / 3 != 24) {
        return false;
      }
      if (m.indices.size() != 36) {
        return false;
      }
      if (m.normals.size() != m.positions.size()) {
        return false;
      }
      return true;
    }
  }
  return false;
}
bool TestPBR() {
  float albedo[3] = { 0.8f, 0.2f, 0.15f };
  float N[3] = { 0.0f, 1.0f, 0.0f };
  float V[3] = { 0.3f, 0.9f, 0.2f };
  float L[3] = { 0.4f, 0.9f, 0.1f };
  float light[3] = { 3.0f, 3.0f, 3.0f };
  float out[3] = { 0.0f, 0.0f, 0.0f };
  Kizuri::PBR_Directional(albedo, 0.5f, 0.0f, N, V, L, light, out);
  if (!(out[0] > 0.0f && out[1] > 0.0f && out[2] > 0.0f)) {
    return false;
  }
  float out2[3] = { 0.0f, 0.0f, 0.0f };
  float Ld[3] = { 0.0f, -1.0f, 0.0f };
  Kizuri::PBR_Directional(albedo, 0.5f, 0.0f, N, V, Ld, light, out2);
  if (!(out2[0] == 0.0f && out2[1] == 0.0f && out2[2] == 0.0f)) {
    return false;
  }
  return true;
}
bool TestShaderFiles() {
  const char* dirs[3] = { "Shaders", "Samples/../Shaders", "build/bin/Release/Shaders" };
  const char* files[4] = { "GeometryVS.hlsl", "GeometryPS.hlsl", "LightingVS.hlsl", "LightingPS.hlsl" };
  for (int d = 0; d < 3; ++d) {
    bool allOk = true;
    for (int f = 0; f < 4; ++f) {
      char path[512];
      std::snprintf(path, sizeof(path), "%s/%s", dirs[d], files[f]);
      FILE* fp = std::fopen(path, "rb");
      if (fp == nullptr) {
        allOk = false;
        break;
      }
      std::fseek(fp, 0, SEEK_END);
      long sz = std::ftell(fp);
      std::fclose(fp);
      if (sz < 100) {
        allOk = false;
        break;
      }
    }
    if (allOk) {
      return true;
    }
  }
  return false;
}
bool TestDeferredNull() {  Kizuri::IRHI* rhi = Kizuri::CreateRHI(Kizuri::RHI_API::Null);
  if (rhi == nullptr) {
    return false;
  }
  Kizuri::RHIDesc desc;
  desc.windowHandle = nullptr;
  desc.width = 320;
  desc.height = 200;
  desc.vsync = false;
  if (!rhi->Initialize(desc)) {
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  Kizuri::DeferredRenderer renderer;
  const char* shaderDirs[3] = { "Shaders", "Samples/../Shaders", "." };
  bool rok = false;
  for (int i = 0; i < 3 && !rok; ++i) {
    if (renderer.Initialize(rhi, 320, 200, shaderDirs[i])) {
      rok = true;
    } else {
      renderer.Shutdown();
    }
  }
  if (!rok) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  float tri[9] = { -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.0f };
  float nrm[9] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
  float uv[6] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f };
  uint32_t idx[3] = { 0, 1, 2 };
  if (!renderer.SetMesh(tri, nrm, uv, 3, idx, 3)) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  float view[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,5,1 };
  float proj[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
  float cam[3] = { 0, 0, -5 };
  renderer.Render(view, proj, cam);
  if (!renderer.IsReady()) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  if (!renderer.Resize(160, 100)) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  renderer.Render(view, proj, cam);
  renderer.RenderToTexture(view, proj, cam);
  void* srv = renderer.GetViewportTexture();
  if (srv != nullptr) {
    renderer.Shutdown();
    rhi->Shutdown();
    Kizuri::DestroyRHI(rhi);
    return false;
  }
  float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
  renderer.BeginObjects(view, proj);
  renderer.DrawObject(ident);
  renderer.DrawObject(ident);
  renderer.EndObjectsToTexture(cam);
  renderer.BeginObjects(view, proj);
  renderer.EndObjectsToBackbuffer(cam);
  renderer.Shutdown();
  rhi->Shutdown();
  Kizuri::DestroyRHI(rhi);
  return true;
}
bool TestSceneCrud() {
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("Alpha");
  Kizuri::EntityId b = scene.CreateEntity("Beta");
  if (!a.IsValid() || !b.IsValid() || a == b) {
    return false;
  }
  if (scene.Count() != 2) {
    return false;
  }
  if (!scene.RenameEntity(a, "Gamma")) {
    return false;
  }
  const Kizuri::Entity* e = scene.Get(a);
  if (e == nullptr || e->name != "Gamma") {
    return false;
  }
  if (e->transform.position[0] != 0.0f || e->transform.scale[0] != 1.0f) {
    return false;
  }
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[0] = 3.0f;
  if (!scene.SetTransform(a, t)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 3.0f) {
    return false;
  }
  if (!scene.IsDirty()) {
    return false;
  }
  scene.ClearDirty();
  if (scene.IsDirty()) {
    return false;
  }
  if (!scene.DeleteEntity(a)) {
    return false;
  }
  if (scene.Has(a) || scene.Count() != 1) {
    return false;
  }
  if (scene.Get(a) != nullptr) {
    return false;
  }
  Kizuri::EntityId c = scene.CreateEntity("Delta");
  if (!c.IsValid() || scene.Count() != 2) {
    return false;
  }
  if (scene.DeleteEntity(Kizuri::EntityId::Invalid())) {
    return false;
  }
  return true;
}
bool TestSceneDuplicate() {
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("Base");
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[1] = 2.0f;
  t.scale[0] = 3.0f;
  scene.SetTransform(a, t);
  Kizuri::EntityId copy = scene.DuplicateEntity(a);
  if (!copy.IsValid() || copy == a) {
    return false;
  }
  const Kizuri::Entity* e = scene.Get(copy);
  if (e == nullptr || e->name != "Base Copy") {
    return false;
  }
  if (e->transform.position[1] != 2.0f || e->transform.scale[0] != 3.0f) {
    return false;
  }
  if (scene.Count() != 2) {
    return false;
  }
  if (scene.DuplicateEntity(Kizuri::EntityId::Invalid()).IsValid()) {
    return false;
  }
  return true;
}
bool TestSceneParent() {
  Kizuri::Scene scene;
  Kizuri::EntityId root = scene.CreateEntity("Root");
  Kizuri::EntityId child = scene.CreateEntity("Child");
  Kizuri::EntityId grand = scene.CreateEntity("Grand");
  if (!scene.SetParent(child, root)) {
    return false;
  }
  if (!scene.SetParent(grand, child)) {
    return false;
  }
  if (scene.SetParent(root, grand)) {
    return false;
  }
  if (scene.SetParent(root, root)) {
    return false;
  }
  const Kizuri::Entity* r = scene.Get(root);
  if (r == nullptr || r->children.size() != 1 || !(r->children[0] == child)) {
    return false;
  }
  if (!scene.SetParent(child, Kizuri::EntityId::Invalid())) {
    return false;
  }
  if (scene.Get(root)->children.size() != 0) {
    return false;
  }
  if (!scene.SetParent(child, root)) {
    return false;
  }
  if (!scene.DeleteEntity(root)) {
    return false;
  }
  if (scene.Count() != 0 || scene.Has(child) || scene.Has(grand)) {
    return false;
  }
  return true;
}
bool TestSelectionSet() {
  Kizuri::Scene scene;
  Kizuri::SelectionSet sel;
  if (sel.HasSelection() || sel.Count() != 0) {
    return false;
  }
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::EntityId b = scene.CreateEntity("B");
  Kizuri::EntityId c = scene.CreateEntity("C");
  sel.Select(a);
  if (!sel.HasSelection() || !sel.Contains(a) || sel.Contains(b) || sel.Count() != 1) {
    return false;
  }
  if (!(sel.Get() == a) || !(sel.Primary() == a) || !(sel.At(0) == a)) {
    return false;
  }
  sel.Add(b);
  sel.Add(b);
  if (sel.Count() != 2 || !sel.Contains(b)) {
    return false;
  }
  sel.Toggle(b);
  if (sel.Contains(b) || sel.Count() != 1) {
    return false;
  }
  sel.Toggle(c);
  if (!sel.Contains(c) || sel.Count() != 2) {
    return false;
  }
  sel.Remove(a);
  if (sel.Contains(a) || sel.Count() != 1 || !(sel.Primary() == c)) {
    return false;
  }
  sel.Select(b);
  scene.DeleteEntity(b);
  sel.OnEntityDeleted(b);
  if (sel.HasSelection()) {
    return false;
  }
  sel.Add(a);
  sel.Add(c);
  scene.DeleteEntity(a);
  sel.OnEntityDeleted(a);
  if (sel.Count() != 1 || !sel.Contains(c)) {
    return false;
  }
  sel.Clear();
  if (sel.HasSelection() || !sel.IsEmpty()) {
    return false;
  }
  if (sel.At(99).IsValid()) {
    return false;
  }
  return true;
}
bool TestScreenRect() {
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::FreeCamera cam;
  cam.SetPosition(0.0f, 0.0f, -5.0f);
  cam.SetYawPitch(0.0f, 0.0f);
  DirectX::XMMATRIX v = cam.View();
  DirectX::XMMATRIX p = cam.Projection(800.0f / 600.0f);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, v);
  DirectX::XMStoreFloat4x4(&pf, p);
  float x0;
  float y0;
  float x1;
  float y1;
  if (!Kizuri::EntityScreenRect(scene, a, &vf.m[0][0], &pf.m[0][0], 0.0f, 0.0f, 800.0f, 600.0f, x0, y0, x1, y1)) {
    return false;
  }
  if (x0 > 400.0f || x1 < 400.0f || y0 > 300.0f || y1 < 300.0f) {
    return false;
  }
  if (x1 - x0 <= 1.0f || y1 - y0 <= 1.0f) {
    return false;
  }
  return true;
}
bool TestScreenRectBehind() {
  Kizuri::Scene scene;
  Kizuri::EntityId b = scene.CreateEntity("B");
  Kizuri::Entity* e = scene.Get(b);
  e->transform.position[0] = 0.0f;
  e->transform.position[1] = 0.0f;
  e->transform.position[2] = -50.0f;
  Kizuri::FreeCamera cam;
  cam.SetPosition(0.0f, 0.0f, -5.0f);
  cam.SetYawPitch(0.0f, 0.0f);
  DirectX::XMMATRIX v = cam.View();
  DirectX::XMMATRIX p = cam.Projection(800.0f / 600.0f);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, v);
  DirectX::XMStoreFloat4x4(&pf, p);
  float x0;
  float y0;
  float x1;
  float y1;
  if (Kizuri::EntityScreenRect(scene, b, &vf.m[0][0], &pf.m[0][0], 0.0f, 0.0f, 800.0f, 600.0f, x0, y0, x1, y1)) {
    return false;
  }
  if (Kizuri::EntityScreenRect(scene, Kizuri::EntityId::Invalid(), &vf.m[0][0], &pf.m[0][0], 0.0f, 0.0f, 800.0f, 600.0f, x0, y0, x1, y1)) {
    return false;
  }
  return true;
}
bool TestRectOverlap() {
  if (!Kizuri::RectsOverlap(0.0f, 0.0f, 10.0f, 10.0f, 5.0f, 5.0f, 15.0f, 15.0f)) {
    return false;
  }
  if (Kizuri::RectsOverlap(0.0f, 0.0f, 10.0f, 10.0f, 20.0f, 20.0f, 30.0f, 30.0f)) {
    return false;
  }
  if (!Kizuri::RectsOverlap(10.0f, 10.0f, 0.0f, 0.0f, 5.0f, 5.0f, 15.0f, 15.0f)) {
    return false;
  }
  if (!Kizuri::RectsOverlap(0.0f, 0.0f, 10.0f, 10.0f, 10.0f, 0.0f, 20.0f, 10.0f)) {
    return false;
  }
  return true;
}
bool TestLog() {
  Kizuri::LogStore log;
  log.Add(Kizuri::LogLevel::Info, "hello");
  log.Add(Kizuri::LogLevel::Error, "boom");
  log.Add(Kizuri::LogLevel::Warning, "careful");
  log.Add(Kizuri::LogLevel::Success, "done");
  if (log.Count() != 4) {
    return false;
  }
  if (log.CountLevel(Kizuri::LogLevel::Error) != 1) {
    return false;
  }
  if (log.At(0).text != "hello" || log.At(3).level != Kizuri::LogLevel::Success) {
    return false;
  }
  if (log.At(0).seq >= log.At(3).seq) {
    return false;
  }
  log.Clear();
  return log.Count() == 0;
}
bool TestEditQueue() {
  Kizuri::Scene scene;
  Kizuri::EditQueue queue;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::Transform t1;
  Kizuri::MakeIdentityTransform(t1);
  t1.position[0] = 1.0f;
  Kizuri::Transform t2;
  Kizuri::MakeIdentityTransform(t2);
  t2.position[0] = 2.0f;
  queue.PushTransform(a, t1);
  queue.PushTransform(a, t2);
  queue.PushTransform(Kizuri::EntityId::Invalid(), t1);
  if (queue.Pending() != 2) {
    return false;
  }
  size_t applied = queue.ApplyAll(scene, undo);
  if (applied != 1 || queue.Pending() != 0) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 2.0f) {
    return false;
  }
  if (!undo.CanUndo()) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(a)->transform.position[0] != 0.0f) {
    return false;
  }
  Kizuri::Transform t3;
  Kizuri::MakeIdentityTransform(t3);
  t3.position[2] = 9.0f;
  queue.PushTransform(a, t3);
  queue.Clear();
  if (queue.Pending() != 0) {
    return false;
  }
  return scene.Get(a)->transform.position[2] == 0.0f;
}
bool TestPicking() {
  Kizuri::Scene scene;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::Entity* e = scene.Get(a);
  e->transform.position[0] = 0.0f;
  e->transform.position[1] = 0.0f;
  e->transform.position[2] = 0.0f;
  float view[16];
  float proj[16];
  {
    Kizuri::FreeCamera cam;
    cam.SetPosition(0.0f, 0.0f, -5.0f);
    cam.SetYawPitch(0.0f, 0.0f);
    DirectX::XMMATRIX v = cam.View();
    DirectX::XMMATRIX p = cam.Projection(800.0f / 600.0f);
    DirectX::XMFLOAT4X4 vf;
    DirectX::XMFLOAT4X4 pf;
    DirectX::XMStoreFloat4x4(&vf, v);
    DirectX::XMStoreFloat4x4(&pf, p);
    std::memcpy(view, &vf.m[0][0], sizeof(view));
    std::memcpy(proj, &pf.m[0][0], sizeof(proj));
  }
  float origin[3];
  float dir[3];
  Kizuri::ScreenPointRay(400.0f, 300.0f, 800.0f, 600.0f, view, proj, origin, dir);
  Kizuri::EntityId hit = Kizuri::PickFirst(scene, origin, dir);
  if (!(hit == a)) {
    return false;
  }
  float farOrigin[3] = { 50.0f, 50.0f, 50.0f };
  float farDir[3] = { 0.0f, 1.0f, 0.0f };
  if (Kizuri::PickFirst(scene, farOrigin, farDir).IsValid()) {
    return false;
  }
  float m[16];
  Kizuri::ComposeMatrix(e->transform, m);
  float t = 0.0f;
  float ro[3] = { 0.0f, 0.0f, 5.0f };
  float rd[3] = { 0.0f, 0.0f, -1.0f };
  if (!Kizuri::RayVsUnitCube(ro, rd, m, t)) {
    return false;
  }
  if (t < 4.4f || t > 4.6f) {
    return false;
  }
  float rdUp[3] = { 0.0f, 1.0f, 0.0f };
  if (Kizuri::RayVsUnitCube(ro, rdUp, m, t)) {
    return false;
  }
  return true;
}
bool TestSceneRoundTrip() {
  Kizuri::Scene src;
  Kizuri::EntityId root = src.CreateEntity("Root Node");
  Kizuri::EntityId child = src.CreateEntity("Child \"Quoted\" \\ Test");
  Kizuri::EntityId lone = src.CreateEntity("Lone");
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[0] = 1.5f;
  t.position[1] = -2.25f;
  t.position[2] = 100.125f;
  t.rotation[0] = 30.0f;
  t.rotation[1] = 45.5f;
  t.rotation[2] = -10.0f;
  t.scale[0] = 2.0f;
  t.scale[1] = 0.5f;
  t.scale[2] = 1.0f;
  src.SetTransform(root, t);
  Kizuri::Transform tc;
  Kizuri::MakeIdentityTransform(tc);
  tc.position[2] = 7.0f;
  src.SetTransform(child, tc);
  src.SetParent(child, root);
  const char* path = "test_scene_tmp.kzscene";
  std::remove(path);
  if (!Kizuri::SaveSceneToFile(src, path)) {
    return false;
  }
  Kizuri::Scene dst;
  if (!Kizuri::LoadSceneFromFile(dst, path, nullptr)) {
    std::remove(path);
    return false;
  }
  std::remove(path);
  if (dst.Count() != 3 || dst.IsDirty()) {
    return false;
  }
  const Kizuri::Entity* dr = nullptr;
  const Kizuri::Entity* dc = nullptr;
  const Kizuri::Entity* dl = nullptr;
  std::vector<Kizuri::EntityId> all = dst.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Kizuri::Entity* e = dst.Get(all[i]);
    if (e->name == "Root Node") {
      dr = e;
    } else if (e->name == "Child \"Quoted\" \\ Test") {
      dc = e;
    } else if (e->name == "Lone") {
      dl = e;
    }
  }
  if (dr == nullptr || dc == nullptr || dl == nullptr) {
    return false;
  }
  if (dr->transform.position[0] != 1.5f || dr->transform.position[1] != -2.25f || dr->transform.position[2] != 100.125f) {
    return false;
  }
  if (dr->transform.rotation[1] != 45.5f || dr->transform.scale[0] != 2.0f) {
    return false;
  }
  if (dc->transform.position[2] != 7.0f) {
    return false;
  }
  if (!dc->parent.IsValid()) {
    return false;
  }
  const Kizuri::Entity* dp = dst.Get(dc->parent);
  if (dp == nullptr || dp->name != "Root Node") {
    return false;
  }
  if (dl->parent.IsValid()) {
    return false;
  }
  if (Kizuri::TypeRegistry::Instance().Find("Transform") == nullptr) {
    return false;
  }
  (void)lone;
  return true;
}
bool TestSceneInvalid() {
  Kizuri::Scene scene;
  if (Kizuri::LoadSceneFromFile(scene, "no_such_file_xyz.kzscene", nullptr)) {
    return false;
  }
  const char* bad = "test_scene_bad_tmp.kzscene";
  FILE* fp = std::fopen(bad, "wb");
  if (fp == nullptr) {
    return false;
  }
  std::fputs("NOT A SCENE\nGARBAGE {{{{\n", fp);
  std::fclose(fp);
  bool ok = !Kizuri::LoadSceneFromFile(scene, bad, nullptr);
  std::remove(bad);
  if (!ok) {
    return false;
  }
  const char* bad2 = "test_scene_bad2_tmp.kzscene";
  fp = std::fopen(bad2, "wb");
  if (fp == nullptr) {
    return false;
  }
  std::fputs("KZSCENE 1\nENTITY \"Broken\" notanumber\n", fp);
  std::fclose(fp);
  ok = !Kizuri::LoadSceneFromFile(scene, bad2, nullptr);
  std::remove(bad2);
  return ok;
}
bool TestUndoCreate() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[0] = 5.0f;
  std::unique_ptr<Kizuri::Command> cmd(new Kizuri::CreateEntityCmd("Undo1", t, Kizuri::EntityId::Invalid()));
  if (!undo.Execute(std::move(cmd), scene) || scene.Count() != 1) {
    return false;
  }
  if (!undo.CanUndo() || undo.UndoDepth() != 1) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Count() != 0) {
    return false;
  }
  if (!undo.CanRedo()) {
    return false;
  }
  if (!undo.Redo(scene) || scene.Count() != 1) {
    return false;
  }
  std::vector<Kizuri::EntityId> all = scene.All();
  const Kizuri::Entity* e = scene.Get(all[0]);
  return e != nullptr && e->name == "Undo1" && e->transform.position[0] == 5.0f;
}
bool TestUndoDelete() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("Keep");
  Kizuri::EntityId b = scene.CreateEntity("Gone");
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  t.position[1] = 3.0f;
  scene.SetTransform(b, t);
  scene.SetParent(b, a);
  std::unique_ptr<Kizuri::Command> cmd(new Kizuri::DeleteEntityCmd(b));
  if (!undo.Execute(std::move(cmd), scene) || scene.Count() != 1) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Count() != 2) {
    return false;
  }
  const Kizuri::Entity* rb = nullptr;
  std::vector<Kizuri::EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Kizuri::Entity* e = scene.Get(all[i]);
    if (e->name == "Gone") {
      rb = e;
    }
  }
  if (rb == nullptr || rb->transform.position[1] != 3.0f) {
    return false;
  }
  if (!rb->parent.IsValid()) {
    return false;
  }
  if (!undo.Redo(scene) || scene.Count() != 1) {
    return false;
  }
  return true;
}
bool TestUndoEdit() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("Mover");
  const Kizuri::Entity* e0 = scene.Get(a);
  Kizuri::Transform before = e0->transform;
  Kizuri::Transform after = before;
  after.position[0] = 9.0f;
  after.rotation[1] = 45.0f;
  std::unique_ptr<Kizuri::Command> cmd(new Kizuri::EditTransformCmd(a, before, after));
  if (!undo.Execute(std::move(cmd), scene)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 9.0f) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(a)->transform.position[0] != 0.0f) {
    return false;
  }
  if (!undo.Redo(scene) || scene.Get(a)->transform.rotation[1] != 45.0f) {
    return false;
  }
  return true;
}
bool TestUndoRenameParent() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::EntityId b = scene.CreateEntity("B");
  std::unique_ptr<Kizuri::Command> rcmd(new Kizuri::RenameCmd(a, "A", "A2"));
  if (!undo.Execute(std::move(rcmd), scene) || scene.Get(a)->name != "A2") {
    return false;
  }
  std::unique_ptr<Kizuri::Command> pcmd(new Kizuri::SetParentCmd(b, Kizuri::EntityId::Invalid(), a));
  if (!undo.Execute(std::move(pcmd), scene)) {
    return false;
  }
  if (!scene.Get(b)->parent.IsValid()) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(b)->parent.IsValid()) {
    return false;
  }
  if (!undo.Undo(scene) || scene.Get(a)->name != "A") {
    return false;
  }
  if (!undo.Redo(scene) || scene.Get(a)->name != "A2") {
    return false;
  }
  if (!undo.Redo(scene) || !scene.Get(b)->parent.IsValid()) {
    return false;
  }
  return true;
}
bool TestUndoStack() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  if (undo.CanUndo() || undo.CanRedo()) {
    return false;
  }
  if (undo.Undo(scene) || undo.Redo(scene)) {
    return false;
  }
  Kizuri::Transform t;
  Kizuri::MakeIdentityTransform(t);
  std::unique_ptr<Kizuri::Command> c1(new Kizuri::CreateEntityCmd("One", t, Kizuri::EntityId::Invalid()));
  std::unique_ptr<Kizuri::Command> c2(new Kizuri::CreateEntityCmd("Two", t, Kizuri::EntityId::Invalid()));
  undo.Execute(std::move(c1), scene);
  undo.Execute(std::move(c2), scene);
  if (scene.Count() != 2 || undo.UndoDepth() != 2) {
    return false;
  }
  undo.Undo(scene);
  if (scene.Count() != 1 || undo.RedoDepth() != 1) {
    return false;
  }
  std::unique_ptr<Kizuri::Command> c3(new Kizuri::CreateEntityCmd("Three", t, Kizuri::EntityId::Invalid()));
  undo.Execute(std::move(c3), scene);
  if (undo.CanRedo() || scene.Count() != 2) {
    return false;
  }
  undo.Clear();
  if (undo.CanUndo() || undo.CanRedo()) {
    return false;
  }
  return true;
}
bool TestUndoMultiMove() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::EntityId b = scene.CreateEntity("B");
  Kizuri::EntityId c = scene.CreateEntity("C");
  Kizuri::Transform ba = scene.Get(a)->transform;
  Kizuri::Transform bb = scene.Get(b)->transform;
  Kizuri::Transform bc = scene.Get(c)->transform;
  Kizuri::Transform na = ba;
  Kizuri::Transform nb = bb;
  Kizuri::Transform nc = bc;
  na.position[0] += 10.0f;
  nb.position[1] += 10.0f;
  nc.position[2] += 10.0f;
  na.rotation[0] += 15.0f;
  nb.scale[0] *= 2.0f;
  scene.SetTransform(a, na);
  scene.SetTransform(b, nb);
  scene.SetTransform(c, nc);
  Kizuri::MultiEditTransformCmd* multi = new Kizuri::MultiEditTransformCmd();
  multi->Add(a, ba, na);
  multi->Add(b, bb, nb);
  multi->Add(c, bc, nc);
  if (multi->Empty()) {
    delete multi;
    return false;
  }
  std::unique_ptr<Kizuri::Command> cmd(multi);
  undo.Commit(std::move(cmd));
  if (!undo.CanUndo()) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 10.0f || scene.Get(b)->transform.position[1] != 10.0f || scene.Get(c)->transform.position[2] != 10.0f) {
    return false;
  }
  if (!undo.Undo(scene)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 0.0f || scene.Get(b)->transform.position[1] != 0.0f || scene.Get(c)->transform.position[2] != 0.0f) {
    return false;
  }
  if (scene.Get(a)->transform.rotation[0] != 0.0f || scene.Get(b)->transform.scale[0] != 1.0f) {
    return false;
  }
  if (!undo.Redo(scene)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 10.0f || scene.Get(a)->transform.rotation[0] != 15.0f || scene.Get(b)->transform.scale[0] != 2.0f) {
    return false;
  }
  return true;
}
bool TestUndoMultiPartial() {
  Kizuri::Scene scene;
  Kizuri::UndoStack undo;
  Kizuri::EntityId a = scene.CreateEntity("A");
  Kizuri::EntityId b = scene.CreateEntity("B");
  Kizuri::Transform ba = scene.Get(a)->transform;
  Kizuri::Transform bb = scene.Get(b)->transform;
  Kizuri::Transform na = ba;
  Kizuri::Transform nb = bb;
  na.position[0] = 4.0f;
  nb.position[0] = 8.0f;
  scene.SetTransform(a, na);
  scene.SetTransform(b, nb);
  Kizuri::MultiEditTransformCmd* multi = new Kizuri::MultiEditTransformCmd();
  multi->Add(a, ba, na);
  multi->Add(b, bb, nb);
  std::unique_ptr<Kizuri::Command> cmd(multi);
  undo.Commit(std::move(cmd));
  scene.DeleteEntity(b);
  if (!undo.Undo(scene)) {
    return false;
  }
  if (scene.Get(a)->transform.position[0] != 0.0f) {
    return false;
  }
  if (scene.Has(b)) {
    return false;
  }
  return true;
}
int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
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
  failures += Check("RHI-StateCache", TestStateCache());
  failures += Check("FreeCamera", TestCamera());
  failures += Check("MeshLoader-gltf", TestMeshLoader());
  failures += Check("PBR-Directional", TestPBR());
  failures += Check("ShaderFiles", TestShaderFiles());
  failures += Check("Deferred-Null", TestDeferredNull());
  failures += Check("Scene-CRUD", TestSceneCrud());
  failures += Check("Scene-Duplicate", TestSceneDuplicate());
  failures += Check("Scene-Parent", TestSceneParent());
  failures += Check("Selection-Set", TestSelectionSet());
  failures += Check("ScreenRect", TestScreenRect());
  failures += Check("ScreenRectBehind", TestScreenRectBehind());
  failures += Check("RectOverlap", TestRectOverlap());
  failures += Check("LogStore", TestLog());
  failures += Check("EditQueue", TestEditQueue());
  failures += Check("Picking", TestPicking());
  failures += Check("Scene-RoundTrip", TestSceneRoundTrip());
  failures += Check("Scene-InvalidFile", TestSceneInvalid());
  failures += Check("Undo-Create", TestUndoCreate());
  failures += Check("Undo-Delete", TestUndoDelete());
  failures += Check("Undo-Edit", TestUndoEdit());
  failures += Check("Undo-RenameParent", TestUndoRenameParent());
  failures += Check("Undo-Stack", TestUndoStack());
  failures += Check("Undo-MultiMove", TestUndoMultiMove());
  failures += Check("Undo-MultiPartial", TestUndoMultiPartial());
  if (failures == 0) {
    std::printf("KizuriHello: all bootstrap libs linked and functional\n");
  } else {
    std::printf("KizuriHello: %d failures\n", failures);
  }
  return failures == 0 ? 0 : 1;
}
