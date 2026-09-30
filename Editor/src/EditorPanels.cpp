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
  renderer.BeginObjects(&vf.m[0][0], &pf.m[0][0]);
  std::vector<EntityId> ids = scene.All();
  for (size_t i = 0; i < ids.size(); ++i) {
    const Entity* e = scene.Get(ids[i]);
    if (e == nullptr) {
      continue;
    }
    float world[16];
    ComposeMatrix(e->transform, world);
    renderer.DrawObject(world);
  }
  renderer.EndObjectsToTexture(cpos);
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
  ImGuiIO& cio = ImGui::GetIO();
  if (hit.IsValid()) {
    if (cio.KeyShift) {
      selection.Add(hit);
    } else if (cio.KeyCtrl) {
      selection.Toggle(hit);
    } else {
      selection.Select(hit);
    }
  } else if (!cio.KeyShift && !cio.KeyCtrl) {
    selection.Clear();
  }
}
void EditorApp::HandleRubberSelect(float x0, float y0, float x1, float y1) {
  DirectX::XMMATRIX view = camera.View();
  float aspect = viewW / viewH;
  DirectX::XMMATRIX proj = camera.Projection(aspect);
  DirectX::XMFLOAT4X4 vf;
  DirectX::XMFLOAT4X4 pf;
  DirectX::XMStoreFloat4x4(&vf, view);
  DirectX::XMStoreFloat4x4(&pf, proj);
  std::vector<EntityId> hits;
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    float ex0;
    float ey0;
    float ex1;
    float ey1;
    if (EntityScreenRect(scene, all[i], &vf.m[0][0], &pf.m[0][0], viewX, viewY, viewW, viewH, ex0, ey0, ex1, ey1)) {
      if (RectsOverlap(x0, y0, x1, y1, ex0, ey0, ex1, ey1)) {
        hits.push_back(all[i]);
      }
    }
  }
  ImGuiIO& rio = ImGui::GetIO();
  if (rio.KeyShift) {
    for (size_t i = 0; i < hits.size(); ++i) {
      selection.Add(hits[i]);
    }
  } else if (rio.KeyCtrl) {
    for (size_t i = 0; i < hits.size(); ++i) {
      selection.Toggle(hits[i]);
    }
  } else {
    selection.Clear();
    for (size_t i = 0; i < hits.size(); ++i) {
      selection.Add(hits[i]);
    }
  }
  log.Add(LogLevel::Info, std::string("Rubber select: ") + std::to_string(hits.size()));
}
void EditorApp::DrawViewport() {
  ImGui::Begin("Viewport", &showViewport);
  ImGuizmo::OPERATION curOp = static_cast<ImGuizmo::OPERATION>(gizmoOp);
  if (ImGui::RadioButton("Move", curOp == ImGuizmo::TRANSLATE)) {
    gizmoOp = static_cast<int>(ImGuizmo::TRANSLATE);
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Rotate", curOp == ImGuizmo::ROTATE)) {
    gizmoOp = static_cast<int>(ImGuizmo::ROTATE);
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Scale", curOp == ImGuizmo::SCALE)) {
    gizmoOp = static_cast<int>(ImGuizmo::SCALE);
  }
  if (ImGui::IsWindowHovered() && !ImGui::GetIO().WantTextInput && !RawInputPoll::IsMouseDown(VK_RBUTTON)) {
    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
      gizmoOp = static_cast<int>(ImGuizmo::TRANSLATE);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
      gizmoOp = static_cast<int>(ImGuizmo::ROTATE);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
      gizmoOp = static_cast<int>(ImGuizmo::SCALE);
    }
  }
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float iw = avail.x > 8.0f ? avail.x : 8.0f;
  float ih = avail.y > 8.0f ? avail.y : 8.0f;
  ImVec2 ipos = ImGui::GetCursorScreenPos();
  viewX = ipos.x;
  viewY = ipos.y;
  viewW = iw;
  viewH = ih;
  viewValid = true;
  ImDrawList* vdl = ImGui::GetWindowDrawList();
  vdl->ChannelsSplit(2);
  vdl->ChannelsSetCurrent(1);
  if (selection.HasSelection()) {
    SyncSelection();
  }
  if (selection.HasSelection()) {
    DirectX::XMMATRIX view = camera.View();
    float aspect = viewW / viewH;
    DirectX::XMMATRIX proj = camera.Projection(aspect);
    DirectX::XMFLOAT4X4 vf;
    DirectX::XMFLOAT4X4 pf;
    DirectX::XMStoreFloat4x4(&vf, view);
    DirectX::XMStoreFloat4x4(&pf, proj);
    if (selection.Count() == 1) {
      Entity* e = scene.Get(selection.Get());
      if (e == nullptr) {
        selection.Clear();
        gizmoDragging = false;
      } else {
        float matrix[16];
        ImGuizmo::RecomposeMatrixFromComponents(e->transform.position, e->transform.rotation, e->transform.scale, matrix);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(viewX, viewY, viewW, viewH);
        if (ImGuizmo::Manipulate(&vf.m[0][0], &pf.m[0][0], static_cast<ImGuizmo::OPERATION>(gizmoOp), ImGuizmo::WORLD, matrix)) {
          if (!gizmoDragging) {
            gizmoDragging = true;
            gizmoStart = e->transform;
            gizmoTarget = e->id;
          }
          Transform t = e->transform;
          ImGuizmo::DecomposeMatrixToComponents(matrix, t.position, t.rotation, t.scale);
          scene.SetTransform(e->id, t);
        } else if (gizmoDragging) {
          gizmoDragging = false;
          gizmoJustEnded = true;
          const Entity* cur = scene.Get(gizmoTarget);
          if (cur != nullptr && std::memcmp(&gizmoStart, &cur->transform, sizeof(Transform)) != 0) {
            std::unique_ptr<Command> cmd(new EditTransformCmd(gizmoTarget, gizmoStart, cur->transform));
            undo.Commit(std::move(cmd));
          }
          gizmoTarget = EntityId::Invalid();
        }
      }
    } else {
      float avg[3] = { 0.0f, 0.0f, 0.0f };
      std::vector<EntityId> members = selection.All();
      size_t alive = 0;
      for (size_t i = 0; i < members.size(); ++i) {
        const Entity* e = scene.Get(members[i]);
        if (e == nullptr) {
          continue;
        }
        avg[0] += e->transform.position[0];
        avg[1] += e->transform.position[1];
        avg[2] += e->transform.position[2];
        ++alive;
      }
      if (alive == 0) {
        selection.Clear();
        gizmoDragging = false;
      } else {
        avg[0] /= static_cast<float>(alive);
        avg[1] /= static_cast<float>(alive);
        avg[2] /= static_cast<float>(alive);
        float one[3] = { 1.0f, 1.0f, 1.0f };
        float zero[3] = { 0.0f, 0.0f, 0.0f };
        float matrix[16];
        ImGuizmo::RecomposeMatrixFromComponents(avg, zero, one, matrix);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(viewX, viewY, viewW, viewH);
        if (ImGuizmo::Manipulate(&vf.m[0][0], &pf.m[0][0], static_cast<ImGuizmo::OPERATION>(gizmoOp), ImGuizmo::WORLD, matrix)) {
          if (!gizmoDragging) {
            gizmoDragging = true;
            gizmoOrigins.clear();
            for (size_t i = 0; i < members.size(); ++i) {
              const Entity* e = scene.Get(members[i]);
              if (e != nullptr) {
                gizmoOrigins[members[i]] = e->transform;
              }
            }
          }
          float npos[3];
          float nrot[3];
          float nscl[3];
          ImGuizmo::DecomposeMatrixToComponents(matrix, npos, nrot, nscl);
          float dpos[3] = { npos[0] - avg[0], npos[1] - avg[1], npos[2] - avg[2] };
          float dfac[3] = { nscl[0] < 0.01f ? 0.01f : nscl[0], nscl[1] < 0.01f ? 0.01f : nscl[1], nscl[2] < 0.01f ? 0.01f : nscl[2] };
          for (auto& kv : gizmoOrigins) {
            Entity* e = scene.Get(kv.first);
            if (e == nullptr) {
              continue;
            }
            Transform t = kv.second;
            t.position[0] += dpos[0];
            t.position[1] += dpos[1];
            t.position[2] += dpos[2];
            t.rotation[0] += nrot[0];
            t.rotation[1] += nrot[1];
            t.rotation[2] += nrot[2];
            t.scale[0] *= dfac[0];
            t.scale[1] *= dfac[1];
            t.scale[2] *= dfac[2];
            if (t.scale[0] < 0.01f) {
              t.scale[0] = 0.01f;
            }
            if (t.scale[1] < 0.01f) {
              t.scale[1] = 0.01f;
            }
            if (t.scale[2] < 0.01f) {
              t.scale[2] = 0.01f;
            }
            scene.SetTransform(e->id, t);
          }
        } else if (gizmoDragging) {
          gizmoDragging = false;
          gizmoJustEnded = true;
          MultiEditTransformCmd* multi = new MultiEditTransformCmd();
          for (auto& kv : gizmoOrigins) {
            const Entity* cur = scene.Get(kv.first);
            if (cur != nullptr && std::memcmp(&kv.second, &cur->transform, sizeof(Transform)) != 0) {
              multi->Add(kv.first, kv.second, cur->transform);
            }
          }
          if (!multi->Empty()) {
            std::unique_ptr<Command> cmd(multi);
            undo.Commit(std::move(cmd));
          } else {
            delete multi;
          }
          gizmoOrigins.clear();
        }
      }
    }
  } else {
    gizmoDragging = false;
    gizmoOrigins.clear();
  }
  vdl->ChannelsSetCurrent(0);
  gizmoHotLast = ImGuizmo::IsOver();
  void* tex = renderer.IsReady() ? renderer.GetViewportTexture() : nullptr;
  if (tex != nullptr) {
    ImGui::Image(reinterpret_cast<ImTextureID>(tex), ImVec2(iw, ih));
  } else {
    ImGui::Dummy(ImVec2(iw, ih));
  }
  bool showButton = !gizmoHotLast;
  bool canvasHovered = false;
  if (showButton) {
    ImGui::SetCursorScreenPos(ipos);
    ImGui::InvisibleButton("ViewCanvas", ImVec2(iw, ih));
    canvasHovered = ImGui::IsItemHovered();
  }
  bool lookNow = canvasHovered && RawInputPoll::IsMouseDown(VK_RBUTTON);
  UpdateCamera(frameDt, lookNow);
  if (showButton && ImGui::BeginDragDropTarget()) {
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
    if (gizmoJustEnded) {
      gizmoJustEnded = false;
    } else {
      int ux = static_cast<int>(ImGui::GetMousePos().x);
      int uy = static_cast<int>(ImGui::GetMousePos().y);
      int dx = ux - downX;
      int dy = uy - downY;
      if (dx * dx + dy * dy < 25 && canvasHovered) {
        HandleViewportClick();
      } else if (canvasHovered) {
        HandleRubberSelect(static_cast<float>(downX), static_cast<float>(downY), static_cast<float>(ux), static_cast<float>(uy));
      }
    }
    downPosValid = false;
    rubberActive = false;
  }
  if (downPosValid && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    ImVec2 mp = ImGui::GetMousePos();
    float dx = mp.x - static_cast<float>(downX);
    float dy = mp.y - static_cast<float>(downY);
    if (dx * dx + dy * dy >= 25.0f) {
      rubberActive = true;
      rubberX0 = static_cast<float>(downX);
      rubberY0 = static_cast<float>(downY);
      ImGui::GetWindowDrawList()->AddRect(ImVec2(rubberX0, rubberY0), mp, IM_COL32(255, 255, 255, 255));
      ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(rubberX0, rubberY0), mp, IM_COL32(120, 180, 255, 40));
    } else {
      rubberActive = false;
    }
  }
  if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    rdownX = static_cast<int>(ImGui::GetMousePos().x);
    rdownY = static_cast<int>(ImGui::GetMousePos().y);
    rdownValid = true;
  }
  if (rdownValid && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    int ux = static_cast<int>(ImGui::GetMousePos().x);
    int uy = static_cast<int>(ImGui::GetMousePos().y);
    int rdx = ux - rdownX;
    int rdy = uy - rdownY;
    rdownValid = false;
    if (rdx * rdx + rdy * rdy < 25 && canvasHovered) {
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
      contextPick = PickFirst(scene, origin, dir);
      if (contextPick.IsValid()) {
        selection.Select(contextPick);
      }
      ImGui::OpenPopup("ViewportContext");
    }
  }
  if (ImGui::BeginPopup("ViewportContext")) {
    if (contextPick.IsValid() || selection.HasSelection()) {
      EntityId target = contextPick.IsValid() ? contextPick : selection.Get();
      const Entity* e = scene.Get(target);
      if (e != nullptr) {
        ImGui::Text("%s", e->name.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem("Focus")) {
          FocusEntity(target);
        }
        if (ImGui::MenuItem("Duplicate")) {
          DuplicateViaCommand(target);
        }
        if (ImGui::MenuItem("Delete")) {
          DeleteViaCommand(target);
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
  vdl->ChannelsMerge();
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
  bool itemMenu = false;
  for (size_t i = 0; i < all.size(); ++i) {
    Entity* e = scene.Get(all[i]);
    if (e == nullptr) {
      continue;
    }
    ImGui::PushID(static_cast<int>(e->id.index * 1315423911u + e->id.generation));
    bool isSel = selection.Contains(e->id);
    if (renameActive && renameTarget == e->id) {
      ImGui::SetKeyboardFocusHere(0);
      if (ImGui::InputText("##rename", renameBuf, sizeof(renameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string oldName = e->name;
        std::string newName = renameBuf;
        if (oldName != newName) {
          std::unique_ptr<Command> cmd(new RenameCmd(e->id, oldName, newName));
          if (undo.Execute(std::move(cmd), scene)) {
            log.Add(LogLevel::Info, std::string("Renamed to ") + newName);
          }
        }
        renameActive = false;
      }
      if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        renameActive = false;
      }
    } else {
      if (ImGui::Selectable(e->name.c_str(), isSel)) {
        ImGuiIO& hio = ImGui::GetIO();
        if (hio.KeyShift) {
          selection.Add(e->id);
        } else if (hio.KeyCtrl) {
          selection.Toggle(e->id);
        } else {
          selection.Select(e->id);
        }
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        renameActive = true;
        renameTarget = e->id;
        std::strncpy(renameBuf, e->name.c_str(), sizeof(renameBuf) - 1);
        renameBuf[sizeof(renameBuf) - 1] = '\0';
      }
    }
    if (ImGui::BeginPopupContextItem("EntityContext")) {
      itemMenu = true;
      if (ImGui::MenuItem("Create Entity")) {
        CreateEntityAt(0.0f, 0.0f, 0.0f);
      }
      if (ImGui::MenuItem("Duplicate")) {
        DuplicateViaCommand(e->id);
      }
      if (ImGui::MenuItem("Delete")) {
        DeleteViaCommand(e->id);
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
  if (!itemMenu && ImGui::BeginPopupContextWindow("HierarchyContext")) {
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
  if (selection.Count() > 1) {
    ImGui::Text("%llu selected", (unsigned long long)selection.Count());
  }
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
