#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "EditorApp.h"
#include "Kizuri/Input.h"
#include <windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include <cstdio>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace Kizuri {
EditorApp::EditorApp()
  : rhi(nullptr)
  , cubeReady(false)
  , running(false)
  , showHierarchy(true)
  , showInspector(true)
  , showConsole(true)
  , showViewport(true)
  , showAbout(false)
  , viewX(0.0f)
  , viewY(0.0f)
  , viewW(1280.0f)
  , viewH(720.0f)
  , viewValid(false)
  , lastMouseX(0)
  , lastMouseY(0)
  , downPosValid(false)
  , downX(0)
  , downY(0)
  , rdownX(0)
  , rdownY(0)
  , rdownValid(false)
  , contextPick(EntityId::Invalid())
  , renameActive(false)
  , renameTarget(EntityId::Invalid()) {
  renameBuf[0] = '\0';
}
std::string EditorApp::FindShaderDir() {
  const char* dirs[4] = { "Shaders", "../Shaders", "../../Shaders", "build/bin/Release/Shaders" };
  for (int i = 0; i < 4; ++i) {
    std::string p = std::string(dirs[i]) + "/GeometryVS.hlsl";
    FILE* fp = std::fopen(p.c_str(), "rb");
    if (fp != nullptr) {
      std::fclose(fp);
      return dirs[i];
    }
  }
  return "Shaders";
}
std::string EditorApp::FindAsset(const char* name) {
  const char* dirs[4] = { "Assets", "Samples/Assets", "../Assets", "build/bin/Release/Assets" };
  for (int i = 0; i < 4; ++i) {
    std::string p = std::string(dirs[i]) + "/" + name;
    FILE* fp = std::fopen(p.c_str(), "rb");
    if (fp != nullptr) {
      std::fclose(fp);
      return p;
    }
  }
  return name;
}
bool EditorApp::Initialize() {
  if (!window.Create(L"Kizuri Editor - Fase 3", 1600, 900)) {
    return false;
  }
  window.SetMessageHook([](void* hwnd, unsigned int msg, unsigned long long wParam, long long lParam) -> long long {
    LRESULT r = ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), msg, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam));
    return r != 0 ? 1 : 0;
  });
  RawInputPoll::Initialize(window.NativeHandle());
  RHIDesc desc;
  desc.windowHandle = window.NativeHandle();
  desc.width = window.Width();
  desc.height = window.Height();
  desc.vsync = true;
  rhi = CreateRHI(RHI_API::D3D11);
  if (rhi == nullptr || !rhi->Initialize(desc)) {
    return false;
  }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 0.0f;
  style.FrameRounding = 2.0f;
  style.GrabRounding = 2.0f;
  ImGui_ImplWin32_Init(window.NativeHandle());
  ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(rhi->GetNativeDevice()), static_cast<ID3D11DeviceContext*>(rhi->GetNativeContext()));
  std::string cubePath = FindAsset("cube.gltf");
  if (LoadStaticMeshFromGltf(cubePath.c_str(), cubeMesh)) {
    cubeReady = true;
    log.Add(LogLevel::Info, std::string("Mesh loaded: ") + cubePath);
  } else {
    log.Add(LogLevel::Error, std::string("Mesh load failed: ") + cubePath);
  }
  std::string shaderDir = FindShaderDir();
  if (!renderer.Initialize(rhi, window.Width(), window.Height(), shaderDir.c_str())) {
    log.Add(LogLevel::Error, "Deferred renderer init failed");
    return false;
  }
  if (cubeReady) {
    renderer.SetMesh(cubeMesh.positions.data(), cubeMesh.normals.data(), cubeMesh.uvs.data(), cubeMesh.positions.size() / 3, cubeMesh.indices.data(), cubeMesh.indices.size());
  }
  DeferredMaterial mat;
  mat.albedo[0] = 0.75f;
  mat.albedo[1] = 0.22f;
  mat.albedo[2] = 0.12f;
  mat.roughness = 0.45f;
  mat.metallic = 0.05f;
  renderer.SetMaterial(mat);
  camera.SetPosition(0.0f, 1.5f, -6.0f);
  int mx = 0;
  int my = 0;
  RawInputPoll::GetMousePosition(mx, my);
  lastMouseX = mx;
  lastMouseY = my;
  log.Add(LogLevel::Success, "Kizuri Editor ready");
  running = true;
  return true;
}
void EditorApp::Shutdown() {
  if (rhi != nullptr) {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    renderer.Shutdown();
    DestroyRHI(rhi);
    rhi = nullptr;
  }
  RawInputPoll::Shutdown();
  window.ClearMessageHook();
  window.Destroy();
}
int EditorApp::Run() {
  unsigned long long prev = GetTickCount64();
  while (running && window.PollEvents()) {
    unsigned long long now = GetTickCount64();
    float dt = static_cast<float>(now - prev) / 1000.0f;
    prev = now;
    if (dt > 0.1f) {
      dt = 0.1f;
    }
    edits.ApplyAll(scene);
    RawInputPoll::Poll();
    UpdateCamera(dt);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    Frame();
    RenderScene();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    rhi->Present(true);
  }
  return 0;
}
void EditorApp::UpdateCamera(float dt) {
  if (!viewValid) {
    return;
  }
  ImGuiIO& io = ImGui::GetIO();
  int mx = 0;
  int my = 0;
  RawInputPoll::GetMousePosition(mx, my);
  int mdx = mx - lastMouseX;
  int mdy = my - lastMouseY;
  lastMouseX = mx;
  lastMouseY = my;
  bool look = RawInputPoll::IsMouseDown(VK_RBUTTON) && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
  if (io.WantTextInput) {
    return;
  }
  camera.Update(
    dt,
    RawInputPoll::IsKeyDown(0x57),
    RawInputPoll::IsKeyDown(0x53),
    RawInputPoll::IsKeyDown(0x41),
    RawInputPoll::IsKeyDown(0x44),
    RawInputPoll::IsKeyDown(0x45),
    RawInputPoll::IsKeyDown(0x51),
    look ? static_cast<float>(mdx) : 0.0f,
    look ? static_cast<float>(mdy) : 0.0f);
}
void EditorApp::CreateEntityAt(float x, float y, float z) {
  EntityId id = scene.CreateEntity("Entity");
  Entity* e = scene.Get(id);
  if (e != nullptr) {
    e->transform.position[0] = x;
    e->transform.position[1] = y;
    e->transform.position[2] = z;
  }
  selection.Select(id);
  log.Add(LogLevel::Info, std::string("Created ") + (e != nullptr ? e->name : "Entity"));
}
void EditorApp::FocusEntity(EntityId id) {
  const Entity* e = scene.Get(id);
  if (e == nullptr) {
    return;
  }
  camera.Focus(e->transform.position[0], e->transform.position[1], e->transform.position[2], 5.0f);
  selection.Select(id);
  log.Add(LogLevel::Info, std::string("Focused ") + e->name);
}
void EditorApp::Frame() {
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::DockSpaceOverViewport(0, viewport);
  DrawMenuBar();
  if (showViewport) {
    DrawViewport();
  }
  if (showHierarchy) {
    DrawHierarchy();
  }
  if (showInspector) {
    DrawInspector();
  }
  if (showConsole) {
    DrawConsole();
  }
  if (showAbout) {
    ImGui::Begin("About Kizuri", &showAbout);
    ImGui::Text("Kizuri Engine - Editor Shell Fase 3");
    ImGui::Text("ImGui + ImGuizmo + Deferred PBR");
    ImGui::End();
  }
}
void EditorApp::DrawMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Scene")) {
        scene.Clear();
        selection.Clear();
        edits.Clear();
        log.Add(LogLevel::Warning, "Scene cleared");
      }
      if (ImGui::MenuItem("Exit")) {
        running = false;
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
      ImGui::MenuItem("Viewport", nullptr, &showViewport);
      ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy);
      ImGui::MenuItem("Inspector", nullptr, &showInspector);
      ImGui::MenuItem("Console", nullptr, &showConsole);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About")) {
        showAbout = true;
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}
}
