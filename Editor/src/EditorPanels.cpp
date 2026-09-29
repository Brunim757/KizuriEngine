#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "EditorApp.h"
#include "Kizuri/Input.h"
#include "Kizuri/Picking.h"
#include <windows.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include <cstring>
#include <cmath>
namespace Kizuri {
static Transform s_clipboard;
static bool s_hasClipboard = false;
void EditorApp::RenderScene() {
  if (!viewValid || viewW < 8.0f || viewH < 8.0f) {
    return;
  }
  RECT rc;
  GetClientRect(static_cast<HWND>(window.NativeHandle()), &rc);
  int cw = rc.right - rc.left;
  int ch = rc.bottom - rc.top;
  if ((cw != rhi->Width() || ch != rhi->Height()) && cw > 0 && ch > 0) {
    rhi->Resize(cw, ch);
  }
  int rw = static_cast<int>(viewW);
  int rh = static_cast<int>(viewH);
  static int lastRTW = 0;
  static int lastRTH = 0;
  if (rw != lastRTW || rh != lastRTH) {
    renderer.Resize(rw, rh);
    lastRTW = rw;
    lastRTH = rh;
  }
  renderer.SetViewOffset(0.0f, 0.0f);
  DirectX::XMMATRIX view = camera.View();
  float aspect = viewW / viewH;
  DirectX::XMMATRIX proj = camera.Projection(aspect);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, view);
  DirectX::XMStoreFloat4x4(&pf, proj);
  float cpx = 0.0f;
  float cpy = 0.0f;
  float cpz = 0.0f;
  camera.GetPosition(cpx, cpy, cpz);
  float cpos[3] = { cpx, cpy, cpz };
  renderer.RenderToTexture(&vf.m[0][0], &pf.m[0][0], cpos);
  rhi->Clear(0.03f, 0.03f, 0.04f, 1.0f);
}
void EditorApp::HandleViewportClick() {
  DirectX::XMMATRIX view = camera.View();
  float aspect = viewW / viewH;
  DirectX::XMMATRIX proj = camera.Projection(aspect);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, view);
  DirectX::XMStoreFloat4x4(&pf, proj);
  ImVec2 mp = ImGui::GetMousePos();
  float origin[3];
  float dir[3];
  ScreenPointRay(mp.x - viewX, mp.y - viewY, viewW, viewH, &vf.m[0][0], &pf.m[0][0], origin, dir);
  EntityId hit = PickFirst(scene, origin, dir);
  if (hit.IsValid()) {
    selection.Select(hit);
  } else {
    selection.Clear();
  }
}
void EditorApp::DrawViewport() {
  ImGui::Begin("Viewport", &showViewport);
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float iw = avail.x > 8.0f ? avail.x : 8.0f;
  float ih = avail.y > 8.0f ? avail.y : 8.0f;
  ImVec2 ipos = ImGui::GetCursorScreenPos();
  viewX = ipos.x;
  viewY = ipos.y;
  viewW = iw;
  viewH = ih;
  viewValid = true;
  void* tex = renderer.IsReady() ? renderer.GetViewportTexture() : nullptr;
  if (tex != nullptr) {
    ImGui::Image(static_cast<ImTextureID>(tex), ImVec2(iw, ih));
  } else {
    ImGui::Dummy(ImVec2(iw, ih));
  }
  ImGui::SetCursorScreenPos(ipos);
  ImGui::InvisibleButton("ViewCanvas", ImVec2(iw, ih));
  bool canvasHovered = ImGui::IsItemHovered();
  if (ImGui::BeginDragDropTarget()) {
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_MESH");
    if (payload != nullptr) {
      DirectX::XMMATRIX view = camera.View();
      float aspect = viewW / viewH;
      DirectX::XMMATRIX proj = camera.Projection(aspect);
      DirectX::XMFLOAT4X4 vf;
      DirectX::XMFLOAT4X4 pf;
      DirectX::XMStoreFloat4x4(&vf, view);
      DirectX::XMStoreFloat4x4(&pf, proj);
      ImVec2 mp = ImGui::GetMousePos();
      float origin[3];
      float dir[3];
      ScreenPointRay(mp.x - viewX, mp.y - viewY, viewW, viewH, &vf.m[0][0], &pf.m[0][0], origin, dir);
      float t = -1.0f;
      if (fabsf(dir[1]) > 1e-6f) {
        t = -origin[1] / dir[1];
      }
      if (t > 0.0f) {
        CreateEntityAt(origin[0] + dir[0] * t, 0.0f, origin[2] + dir[2] * t);
      } else {
        CreateEntityAt(origin[0] + dir[0] * 5.0f, origin[1] + dir[1] * 5.0f, origin[2] + dir[2] * 5.0f);
      }
    }
    ImGui::EndDragDropTarget();
  }
  if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    downX = static_cast<int>(ImGui::GetMousePos().x);
    downY = static_cast<int>(ImGui::GetMousePos().y);
    downPosValid = true;
  }
  if (downPosValid && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    int ux = static_cast<int>(ImGui::GetMousePos().x);
    int uy = static_cast<int>(ImGui::GetMousePos().y);
    int dx = ux - downX;
    int dy = uy - downY;
    if (dx * dx + dy * dy < 25 && canvasHovered) {
      HandleViewportClick();
    }
    downPosValid = false;
  }
  EntityId rightPick = EntityId::Invalid();
  if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    DirectX::XMMATRIX view = camera.View();
    float aspect = viewW / viewH;
    DirectX::XMMATRIX proj = camera.Projection(aspect);
    DirectX::XMFLOAT4X4 vf;
    DirectX::XMFLOAT4X4 pf;
    DirectX::XMStoreFloat4x4(&vf, view);
    DirectX::XMStoreFloat4x4(&pf, proj);
    ImVec2 mp = ImGui::GetMousePos();
    float origin[3];
    float dir[3];
    ScreenPointRay(mp.x - viewX, mp.y - viewY, viewW, viewH, &vf.m[0][0], &pf.m[0][0], origin, dir);
    rightPick = PickFirst(scene, origin, dir);
    if (rightPick.IsValid()) {
      selection.Select(rightPick);
    }
  }
  if (ImGui::BeginPopupContextWindow("ViewportContext")) {
    if (rightPick.IsValid() || selection.HasSelection()) {
      EntityId target = rightPick.IsValid() ? rightPick : selection.Get();
      const Entity* e = scene.Get(target);
      if (e != nullptr) {
        ImGui::Text("%s", e->name.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem("Focus")) {
          FocusEntity(target);
        }
        if (ImGui::MenuItem("Duplicate")) {
          EntityId copy = scene.DuplicateEntity(target);
          if (copy.IsValid()) {
            selection.Select(copy);
            const Entity* ce = scene.Get(copy);
            log.Add(LogLevel::Info, std::string("Duplicated ") + (ce != nullptr ? ce->name : ""));
          }
        }
        if (ImGui::MenuItem("Delete")) {
          scene.DeleteEntity(target);
          selection.OnEntityDeleted(target);
          log.Add(LogLevel::Warning, "Entity deleted");
        }
      }
    } else {
      if (ImGui::MenuItem("Create Entity Here")) {
        DirectX::XMMATRIX view = camera.View();
        float aspect = viewW / viewH;
        DirectX::XMMATRIX proj = camera.Projection(aspect);
        DirectX::XMFLOAT4X4 vf;
        DirectX::XMFLOAT4X4 pf;
        DirectX::XMStoreFloat4x4(&vf, view);
        DirectX::XMStoreFloat4x4(&pf, proj);
        ImVec2 mp = ImGui::GetMousePos();
        float origin[3];
        float dir[3];
        ScreenPointRay(mp.x - viewX, mp.y - viewY, viewW, viewH, &vf.m[0][0], &pf.m[0][0], origin, dir);
        float t = -1.0f;
        if (fabsf(dir[1]) > 1e-6f) {
          t = -origin[1] / dir[1];
        }
        if (t > 0.0f) {
          CreateEntityAt(origin[0] + dir[0] * t, 0.0f, origin[2] + dir[2] * t);
        } else {
          CreateEntityAt(0.0f, 0.0f, 0.0f);
        }
      }
    }
    ImGui::EndPopup();
  }
  if (selection.HasSelection() && viewValid) {
    Entity* e = scene.Get(selection.Get());
    if (e != nullptr) {
      DirectX::XMMATRIX view = camera.View();
      float aspect = viewW / viewH;
      DirectX::XMMATRIX proj = camera.Projection(aspect);
      DirectX::XMFLOAT4X4 vf;
      DirectX::XMFLOAT4X4 pf;
      DirectX::XMStoreFloat4x4(&vf, view);
      DirectX::XMStoreFloat4x4(&pf, proj);
      float matrix[16];
      ImGuizmo::RecomposeMatrixFromComponents(e->transform.position, e->transform.rotation, e->transform.scale, matrix);
      ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
      ImGuizmo::SetRect(viewX, viewY, viewW, viewH);
      ImGuizmo::Manipulate(&vf.m[0][0], &pf.m[0][0], ImGuizmo::TRANSLATE, ImGuizmo::WORLD, matrix);
      if (ImGuizmo::IsUsing()) {
        Transform t = e->transform;
        ImGuizmo::DecomposeMatrixToComponents(matrix, t.position, t.rotation, t.scale);
        scene.SetTransform(e->id, t);
      }
    } else {
      selection.Clear();
    }
  }
  ImGui::End();
}
void EditorApp::DrawHierarchy() {
  ImGui::Begin("Hierarchy", &showHierarchy);
  ImGui::Text("Meshes:");
  ImGui::Button("Cube Mesh");
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
    ImGui::SetDragDropPayload("KZ_MESH", "cube", 5);
    ImGui::Text("Cube Mesh");
    ImGui::EndDragDropSource();
  }
  ImGui::Separator();
  if (ImGui::Button("Create Entity")) {
    CreateEntityAt(0.0f, 0.0f, 0.0f);
  }
  ImGui::Separator();
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    Entity* e = scene.Get(all[i]);
    if (e == nullptr) {
      continue;
    }
    ImGui::PushID(static_cast<int>(e->id.index * 1315423911u + e->id.generation));
    bool isSel = selection.IsSelected(e->id);
    if (renameActive && renameTarget == e->id) {
      ImGui::SetKeyboardFocusHere(0);
      if (ImGui::InputText("##rename", renameBuf, sizeof(renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        scene.RenameEntity(e->id, renameBuf);
        log.Add(LogLevel::Info, std::string("Renamed to ") + renameBuf);
        renameActive = false;
      }
      if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        renameActive = false;
      }
    } else {
      if (ImGui::Selectable(e->name.c_str(), isSel)) {
        selection.Select(e->id);
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        renameActive = true;
        renameTarget = e->id;
        std::strncpy(renameBuf, e->name.c_str(), sizeof(renameBuf) - 1);
        renameBuf[sizeof(renameBuf) - 1] = '\0';
      }
    }
    if (ImGui::BeginPopupContextItem("EntityContext")) {
      if (ImGui::MenuItem("Create Entity")) {
        CreateEntityAt(0.0f, 0.0f, 0.0f);
      }
      if (ImGui::MenuItem("Duplicate")) {
        EntityId copy = scene.DuplicateEntity(e->id);
        if (copy.IsValid()) {
          selection.Select(copy);
        }
      }
      if (ImGui::MenuItem("Delete")) {
        scene.DeleteEntity(e->id);
        selection.OnEntityDeleted(e->id);
        log.Add(LogLevel::Warning, "Entity deleted");
      }
      if (ImGui::MenuItem("Rename")) {
        renameActive = true;
        renameTarget = e->id;
        std::strncpy(renameBuf, e->name.c_str(), sizeof(renameBuf) - 1);
        renameBuf[sizeof(renameBuf) - 1] = '\0';
      }
      if (ImGui::MenuItem("Focus")) {
        FocusEntity(e->id);
      }
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }
  if (ImGui::BeginPopupContextWindow("HierarchyContext")) {
    if (ImGui::MenuItem("Create Entity")) {
      CreateEntityAt(0.0f, 0.0f, 0.0f);
    }
    ImGui::EndPopup();
  }
  ImGui::End();
}
void EditorApp::DrawInspector() {
  ImGui::Begin("Inspector", &showInspector);
  if (!selection.HasSelection()) {
    ImGui::Text("No selection");
    ImGui::End();
    return;
  }
  Entity* e = scene.Get(selection.Get());
  if (e == nullptr) {
    selection.Clear();
    ImGui::Text("No selection");
    ImGui::End();
    return;
  }
  ImGui::Text("Entity: %s", e->name.c_str());
  ImGui::Separator();
  ImGui::Text("Transform");
  Transform t = e->transform;
  bool changed = false;
  if (ImGui::DragFloat3("Position", t.position, 0.05f)) {
    changed = true;
  }
  if (ImGui::DragFloat3("Rotation", t.rotation, 0.5f)) {
    changed = true;
  }
  if (ImGui::DragFloat3("Scale", t.scale, 0.02f)) {
    if (t.scale[0] < 0.01f) {
      t.scale[0] = 0.01f;
    }
    if (t.scale[1] < 0.01f) {
      t.scale[1] = 0.01f;
    }
    if (t.scale[2] < 0.01f) {
      t.scale[2] = 0.01f;
    }
    changed = true;
  }
  if (changed) {
    edits.PushTransform(e->id, t);
  }
  ImGui::Separator();
  if (ImGui::Button("Reset Transform")) {
    Transform ident;
    MakeIdentityTransform(ident);
    edits.PushTransform(e->id, ident);
  }
  ImGui::SameLine();
  if (ImGui::Button("Copy")) {
    s_clipboard = e->transform;
    s_hasClipboard = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Paste")) {
    if (s_hasClipboard) {
      edits.PushTransform(e->id, s_clipboard);
    } else {
      log.Add(LogLevel::Warning, "Clipboard empty");
    }
  }
  ImGui::End();
}
void EditorApp::DrawConsole() {
  ImGui::Begin("Console", &showConsole);
  if (ImGui::Button("Clear")) {
    log.Clear();
  }
  ImGui::SameLine();
  ImGui::Text("%llu messages", (unsigned long long)log.Count());
  ImGui::Separator();
  ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
  for (size_t i = 0; i < log.Count(); ++i) {
    const LogEntry& entry = log.At(i);
    ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
    const char* tag = "INFO";
    if (entry.level == LogLevel::Success) {
      color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
      tag = "OK";
    } else if (entry.level == LogLevel::Warning) {
      color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
      tag = "WARN";
    } else if (entry.level == LogLevel::Error) {
      color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
      tag = "ERROR";
    }
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::Text("[%s] %s", tag, entry.text.c_str());
    ImGui::PopStyleColor();
  }
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f) {
    ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();
  ImGui::End();
}
}
