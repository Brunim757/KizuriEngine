#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "EditorApp.h"
#include "Kizuri/Input.h"
#include "Kizuri/SceneSerializer.h"
#include "FileDialog.h"
#include <windows.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include <cstdio>
#include <filesystem>
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
  , frameDt(0.016f)
  , lastMouseX(0)
  , lastMouseY(0)
  , downPosValid(false)
  , downX(0)
  , downY(0)
  , rdownX(0)
  , rdownY(0)
  , rdownValid(false)
  , contextPick(EntityId::Invalid())
  , gizmoHotLast(false)
  , gizmoDragging(false)
  , renameActive(false)
  , renameTarget(EntityId::Invalid())
  , titleDirtyShown(false)
  , pendingAction(0)
  , openDialogQueued(false)
  , saveDialogQueued(false)
  , afterSaveRunPending(false)
  , savePromptQueued(false)
  , restorePromptQueued(false) {
  renameBuf[0] = '\0';
  showNotifHistory = false;
  MakeIdentityTransform(gizmoStart);
  gizmoTarget = EntityId::Invalid();
  gizmoJustEnded = false;
  rubberActive = false;
  rubberX0 = 0.0f;
  rubberY0 = 0.0f;
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
    Announce(LogLevel::Error, std::string("Mesh load failed: ") + cubePath);
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
  gizmoOp = static_cast<int>(ImGuizmo::TRANSLATE);
  InitStoragePaths();
  OfferRestoreFor(ReadLastScene());
  running = true;
  return true;
}
void EditorApp::Shutdown() {
  if (!scene.IsDirty()) {
    DeleteRecoveryFile(CurrentRecoveryPath());
  }
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
    edits.ApplyAll(scene, undo);
    if (autosave.Update(dt, scene, CurrentRecoveryPath())) {
      log.Add(LogLevel::Info, "Autosaved");
    }
    notifications.Update(dt);
    RawInputPoll::Poll();
    frameDt = dt;
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
void EditorApp::UpdateCamera(float dt, bool lookNow) {
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
  if (io.WantTextInput) {
    return;
  }
  camera.Update(
    dt,
    lookNow && RawInputPoll::IsKeyDown(0x57),
    lookNow && RawInputPoll::IsKeyDown(0x53),
    lookNow && RawInputPoll::IsKeyDown(0x41),
    lookNow && RawInputPoll::IsKeyDown(0x44),
    lookNow && RawInputPoll::IsKeyDown(0x45),
    lookNow && RawInputPoll::IsKeyDown(0x51),
    lookNow ? static_cast<float>(mdx) : 0.0f,
    lookNow ? static_cast<float>(mdy) : 0.0f);
}
void EditorApp::CreateEntityAt(float x, float y, float z) {
  Transform t;
  MakeIdentityTransform(t);
  t.position[0] = x;
  t.position[1] = y;
  t.position[2] = z;
  std::unique_ptr<Command> cmd(new CreateEntityCmd("Entity", t, EntityId::Invalid()));
  std::vector<EntityId> beforeIds = scene.All();
  if (!undo.Execute(std::move(cmd), scene)) {
    return;
  }
  SelectNewEntity(beforeIds);
  log.Add(LogLevel::Info, "Created Entity");
}
void EditorApp::DoUndo() {
  if (!undo.CanUndo()) {
    return;
  }
  std::string name = undo.UndoName();
  undo.Undo(scene);
  SyncSelection();
  log.Add(LogLevel::Info, std::string("Undo ") + name);
}
void EditorApp::DoRedo() {
  if (!undo.CanRedo()) {
    return;
  }
  std::string name = undo.RedoName();
  undo.Redo(scene);
  SyncSelection();
  log.Add(LogLevel::Info, std::string("Redo ") + name);
}
void EditorApp::SyncSelection() {
  std::vector<EntityId> all = selection.All();
  for (size_t i = 0; i < all.size(); ++i) {
    if (!scene.Has(all[i])) {
      selection.Remove(all[i]);
    }
  }
}
void EditorApp::SelectNewEntity(const std::vector<EntityId>& beforeIds) {
  std::vector<EntityId> afterIds = scene.All();
  for (size_t i = 0; i < afterIds.size(); ++i) {
    bool found = false;
    for (size_t j = 0; j < beforeIds.size(); ++j) {
      if (afterIds[i] == beforeIds[j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      selection.Select(afterIds[i]);
      return;
    }
  }
}
void EditorApp::DuplicateViaCommand(EntityId id) {
  const Entity* e = scene.Get(id);
  if (e == nullptr) {
    Announce(LogLevel::Warning, "Cannot duplicate: entity not found");
    return;
  }
  std::string name = e->name;
  std::unique_ptr<Command> cmd(new DuplicateCmd(id));
  std::vector<EntityId> beforeIds = scene.All();
  if (!undo.Execute(std::move(cmd), scene)) {
    return;
  }
  SelectNewEntity(beforeIds);
  Announce(LogLevel::Success, std::string("Duplicated ") + name);
}
void EditorApp::DeleteViaCommand(EntityId id) {
  const Entity* e = scene.Get(id);
  if (e == nullptr) {
    Announce(LogLevel::Warning, "Cannot delete: entity not found");
    return;
  }
  std::string name = e->name;
  std::unique_ptr<Command> cmd(new DeleteEntityCmd(id));
  if (!undo.Execute(std::move(cmd), scene)) {
    return;
  }
  selection.OnEntityDeleted(id);
  SyncSelection();
  Announce(LogLevel::Warning, std::string("Deleted ") + name);
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
  ProcessQueuedDialogs();
  if (savePromptQueued) {
    savePromptQueued = false;
    ImGui::OpenPopup("Unsaved Changes");
  }
  if (restorePromptQueued) {
    restorePromptQueued = false;
    ImGui::OpenPopup("Restore Recovery");
  }
  ImGuiIO& keysIo = ImGui::GetIO();
  if (!keysIo.WantTextInput && keysIo.KeyCtrl) {
    if (keysIo.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      DoRedo();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
      DoUndo();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
      DoRedo();
    }
  }
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
  RefreshTitle();
  DrawSavePrompt();
  DrawRestorePrompt();
  DrawToasts();
}
void EditorApp::DrawMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Scene")) {
        RequestAction(1);
      }
      if (ImGui::MenuItem("Open...")) {
        RequestAction(2);
      }
      if (ImGui::MenuItem("Save")) {
        if (currentPath.empty()) {
          saveDialogPrefill.clear();
          saveDialogQueued = true;
          afterSaveRunPending = false;
        } else {
          DoSaveTo(currentPath);
        }
      }
      if (ImGui::MenuItem("Save As...")) {
        saveDialogPrefill = currentPath;
        saveDialogQueued = true;
        afterSaveRunPending = false;
      }
      if (ImGui::BeginMenu("Autosave")) {
        double iv = autosave.Interval();
        if (ImGui::MenuItem("Off", nullptr, iv <= 0.0)) {
          autosave.SetInterval(0.0);
        }
        if (ImGui::MenuItem("Every 1 min", nullptr, iv == 60.0)) {
          autosave.SetInterval(60.0);
        }
        if (ImGui::MenuItem("Every 5 min", nullptr, iv == 300.0)) {
          autosave.SetInterval(300.0);
        }
        if (ImGui::MenuItem("Every 10 min", nullptr, iv == 600.0)) {
          autosave.SetInterval(600.0);
        }
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("Exit")) {
        RequestAction(3);
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      std::string undoLabel = std::string("Undo ") + undo.UndoName();
      std::string redoLabel = std::string("Redo ") + undo.RedoName();
      if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, undo.CanUndo())) {
        DoUndo();
      }
      if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, undo.CanRedo())) {
        DoRedo();
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
void EditorApp::RefreshTitle() {
  bool dirty = scene.IsDirty();
  if (dirty == titleDirtyShown && currentPath == titlePathShown) {
    return;
  }
  titleDirtyShown = dirty;
  titlePathShown = currentPath;
  std::string name = currentPath.empty() ? "Untitled" : currentPath;
  size_t slash = name.find_last_of("/\\");
  if (slash != std::string::npos) {
    name = name.substr(slash + 1);
  }
  if (dirty) {
    name += "*";
  }
  std::string title = "Kizuri Editor - " + name;
  wchar_t wide[1024];
  int n = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wide, 1023);
  if (n <= 0) {
    return;
  }
  wide[1023] = L'\0';
  SetWindowTextW(static_cast<HWND>(window.NativeHandle()), wide);
}
void EditorApp::RequestAction(int action) {
  if (scene.IsDirty()) {
    pendingAction = action;
    savePromptQueued = true;
    return;
  }
  pendingAction = action;
  RunPendingAction();
}
void EditorApp::RunPendingAction() {
  int action = pendingAction;
  pendingAction = 0;
  if (action == 1) {
    DoNewScene();
  } else if (action == 2) {
    openDialogQueued = true;
  } else if (action == 3) {
    running = false;
  }
}
void EditorApp::ProcessQueuedDialogs() {
  if (openDialogQueued) {
    openDialogQueued = false;
    std::string path;
    if (ShowOpenSceneDialog(window.NativeHandle(), path)) {
      DoOpenPath(path);
    } else if (GetLastDialogError() != 0) {
      log.Add(LogLevel::Error, std::string("Open dialog failed: ") + std::to_string(GetLastDialogError()));
    }
  }
  if (saveDialogQueued) {
    saveDialogQueued = false;
    std::string path = saveDialogPrefill;
    if (ShowSaveSceneDialog(window.NativeHandle(), path)) {
      DoSaveTo(path);
      if (afterSaveRunPending) {
        afterSaveRunPending = false;
        RunPendingAction();
      }
    } else {
      afterSaveRunPending = false;
      if (GetLastDialogError() != 0) {
        log.Add(LogLevel::Error, std::string("Save dialog failed: ") + std::to_string(GetLastDialogError()));
      }
    }
  }
}
void EditorApp::InitStoragePaths() {
  wchar_t tmp[1024];
  DWORD n = GetTempPathW(1024, tmp);
  std::string tmpDir = ".";
  if (n > 0 && n < 1024) {
    char narrow[1024];
    int m = WideCharToMultiByte(CP_UTF8, 0, tmp, -1, narrow, 1024, nullptr, nullptr);
    if (m > 1) {
      tmpDir = narrow;
    }
  }
  std::filesystem::path p(tmpDir);
  p /= "Kizuri";
  tmpAutosaveDir = p.string();
  std::error_code ec;
  std::filesystem::create_directories(tmpAutosaveDir, ec);
  wchar_t base[1024];
  DWORD b = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\Kizuri", base, 1024);
  std::string appDir = tmpAutosaveDir;
  if (b > 0 && b < 1024) {
    char narrow[1024];
    int m = WideCharToMultiByte(CP_UTF8, 0, base, -1, narrow, 1024, nullptr, nullptr);
    if (m > 1) {
      appDir = narrow;
    }
  }
  std::filesystem::create_directories(appDir, ec);
  std::filesystem::path sp(appDir);
  sp /= "lastscene.txt";
  sessionFilePath = sp.string();
}
std::string EditorApp::ReadLastScene() {
  if (sessionFilePath.empty()) {
    return "";
  }
  FILE* fp = std::fopen(sessionFilePath.c_str(), "rb");
  if (fp == nullptr) {
    return "";
  }
  char buf[1024];
  size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
  std::fclose(fp);
  buf[n] = '\0';
  std::string out(buf);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
    out.pop_back();
  }
  return out;
}
void EditorApp::WriteLastScene(const std::string& path) {
  if (sessionFilePath.empty()) {
    return;
  }
  FILE* fp = std::fopen(sessionFilePath.c_str(), "wb");
  if (fp == nullptr) {
    return;
  }
  std::fwrite(path.c_str(), 1, path.size(), fp);
  std::fclose(fp);
}
std::string EditorApp::CurrentRecoveryPath() {
  return RecoveryPathFor(currentPath, tmpAutosaveDir);
}
void EditorApp::OfferRestoreFor(const std::string& mainPath) {
  std::string rec = RecoveryPathFor(mainPath, tmpAutosaveDir);
  if (ShouldOfferRecovery(mainPath, rec)) {
    pendingRestoreMain = mainPath;
    restorePromptQueued = true;
  }
}
void EditorApp::DrawRestorePrompt() {
  if (ImGui::BeginPopupModal("Restore Recovery", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    std::string name = pendingRestoreMain.empty() ? "Untitled" : pendingRestoreMain;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) {
      name = name.substr(slash + 1);
    }
    ImGui::Text("Found a newer autosave for %s.", name.c_str());
    ImGui::Text("Restore it?");
    if (ImGui::Button("Restore")) {
      std::string rec = RecoveryPathFor(pendingRestoreMain, tmpAutosaveDir);
      if (LoadSceneFromFile(scene, rec, &log)) {
        currentPath = pendingRestoreMain;
        scene.MarkDirty();
        selection.Clear();
        edits.Clear();
        undo.Clear();
        autosave.ResetTimer();
        WriteLastScene(currentPath);
        Announce(LogLevel::Success, "Recovery restored");
      } else {
        Announce(LogLevel::Error, "Recovery restore failed");
      }
      DeleteRecoveryFile(rec);
      pendingRestoreMain.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
      DeleteRecoveryFile(RecoveryPathFor(pendingRestoreMain, tmpAutosaveDir));
      pendingRestoreMain.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
void EditorApp::DoNewScene() {
  scene.Clear();
  selection.Clear();
  edits.Clear();
  undo.Clear();
  currentPath.clear();
  autosave.ResetTimer();
  WriteLastScene("");
  log.Add(LogLevel::Warning, "Scene cleared");
}
void EditorApp::DoSaveTo(const std::string& path) {
  if (SaveSceneToFile(scene, path)) {
    currentPath = path;
    scene.ClearDirty();
    autosave.ResetTimer();
    DeleteRecoveryFile(RecoveryPathFor(path, tmpAutosaveDir));
    WriteLastScene(path);
    Announce(LogLevel::Success, std::string("Scene saved: ") + path);
  } else {
    Announce(LogLevel::Error, std::string("Scene save failed: ") + path);
  }
}
void EditorApp::DoOpenPath(const std::string& path) {
  if (LoadSceneFromFile(scene, path, &log)) {
    currentPath = path;
    selection.Clear();
    edits.Clear();
    undo.Clear();
    autosave.ResetTimer();
    WriteLastScene(path);
    Announce(LogLevel::Success, std::string("Scene opened: ") + path);
    OfferRestoreFor(path);
  } else {
    Announce(LogLevel::Error, std::string("Scene open failed: ") + path);
  }
}
void EditorApp::Announce(LogLevel level, const std::string& text) {
  log.Add(level, text);
  notifications.Notify(level, text);
}
void EditorApp::DrawToasts() {
  std::vector<Notification> active = notifications.Active();
  if (active.empty()) {
    return;
  }
  ImGuiViewport* vp = ImGui::GetMainViewport();
  ImVec2 pos(vp->WorkPos.x + vp->WorkSize.x - 16.0f, vp->WorkPos.y + vp->WorkSize.y - 16.0f);
  ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize;
  if (ImGui::Begin("##toasts", nullptr, flags)) {
    for (size_t i = 0; i < active.size(); ++i) {
      ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
      const char* tag = "INFO";
      if (active[i].level == LogLevel::Success) {
        color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
        tag = "OK";
      } else if (active[i].level == LogLevel::Warning) {
        color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
        tag = "WARN";
      } else if (active[i].level == LogLevel::Error) {
        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        tag = "ERROR";
      }
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::Text("[%s] %s", tag, active[i].text.c_str());
      ImGui::PopStyleColor();
    }
  }
  ImGui::End();
}
void EditorApp::DrawSavePrompt() {
  if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Save changes before continuing?");
    if (ImGui::Button("Save")) {
      if (currentPath.empty()) {
        saveDialogPrefill.clear();
        saveDialogQueued = true;
        afterSaveRunPending = true;
        ImGui::CloseCurrentPopup();
      } else {
        DoSaveTo(currentPath);
        ImGui::CloseCurrentPopup();
        RunPendingAction();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Don't Save")) {
      ImGui::CloseCurrentPopup();
      RunPendingAction();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      pendingAction = 0;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
}
