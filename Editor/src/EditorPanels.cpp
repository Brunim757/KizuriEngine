#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "EditorApp.h"
#include "FileDialog.h"
#include "Kizuri/Input.h"
#include "Kizuri/Picking.h"
#include <windows.h>
#include <shellapi.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <DirectXMath.h>
#include <cstring>
#include <cmath>
#include <filesystem>
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
  DeferredMaterial grayMat;
  grayMat.albedo[0] = 0.4f;
  grayMat.albedo[1] = 0.4f;
  grayMat.albedo[2] = 0.45f;
  grayMat.roughness = 0.9f;
  grayMat.metallic = 0.0f;
  DeferredMaterial pinkMat;
  pinkMat.albedo[0] = 1.0f;
  pinkMat.albedo[1] = 0.0f;
  pinkMat.albedo[2] = 1.0f;
  pinkMat.roughness = 1.0f;
  pinkMat.metallic = 0.0f;
  std::vector<EntityId> ids = scene.All();
  for (size_t i = 0; i < ids.size(); ++i) {
    const Entity* e = scene.Get(ids[i]);
    if (e == nullptr) {
      continue;
    }
    float world[16];
    ComposeMatrix(e->transform, world);
    if (e->meshGuid.empty()) {
      continue;
    }
    const MeshRecord* rec = assets.GetByGuid(e->meshGuid);
    if (rec != nullptr && rec->loaded && assets.EnsureMeshGpu(e->meshGuid, 4194304)) {
      rec = assets.GetByGuid(e->meshGuid);
      if (rec != nullptr && rec->gpuReady) {
        if (rec->data.parts.empty()) {
          renderer.DrawObjectEx(world, rec->gpuVB, rec->gpuIB, 0, rec->gpuCount, nullptr);
        } else {
          for (size_t p = 0; p < rec->data.parts.size(); ++p) {
            const MeshPartData& part = rec->data.parts[p];
            const DeferredMaterial* useMat = nullptr;
            DeferredMaterial partMat;
            if (part.material < rec->data.materials.size()) {
              const MeshMaterialData& mm = rec->data.materials[part.material];
              partMat.albedo[0] = mm.albedo[0];
              partMat.albedo[1] = mm.albedo[1];
              partMat.albedo[2] = mm.albedo[2];
              partMat.roughness = mm.roughness;
              partMat.metallic = mm.metallic;
              useMat = &partMat;
            }
            renderer.DrawObjectEx(world, rec->gpuVB, rec->gpuIB, part.indexOffset, part.indexCount, useMat);
          }
        }
        continue;
      }
    }
    if (defaultVB != 0) {
      const DeferredMaterial* fallback = &grayMat;
      if (rec == nullptr) {
        fallback = &pinkMat;
      }
      renderer.DrawObjectEx(world, defaultVB, defaultIB, 0, defaultCount, fallback);
    }
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
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_ASSET");
    if (payload != nullptr && payload->Data != nullptr && payload->DataSize > 0) {
      std::string guid(static_cast<const char*>(payload->Data));
      const MeshRecord* rec = assets.GetByGuid(guid);
      if (rec == nullptr) {
        Announce(LogLevel::Warning, "Dropped asset not found");
      } else {
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
          CreateEntityAt(origin[0] + dir[0] * t, 0.0f, origin[2] + dir[2] * t, guid);
        } else {
          CreateEntityAt(origin[0] + dir[0] * 5.0f, origin[1] + dir[1] * 5.0f, origin[2] + dir[2] * 5.0f, guid);
        }
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
      Announce(LogLevel::Warning, "Clipboard empty");
    }
  }
  DrawMeshSection(e);
  ImGui::End();
}
void EditorApp::DrawConsole() {
  ImGui::Begin("Console", &showConsole);
  if (ImGui::Button("Clear")) {
    log.Clear();
  }
  ImGui::SameLine();
  ImGui::Text("%llu messages", (unsigned long long)log.Count());
  ImGui::SameLine();
  if (ImGui::Button(showNotifHistory ? "Hide Notifications" : "Notifications")) {
    showNotifHistory = !showNotifHistory;
  }
  ImGui::Separator();
  if (showNotifHistory) {
    std::vector<Notification> hist = notifications.History();
    ImGui::BeginChild("NotifHistory", ImVec2(0, 120), true);
    for (size_t i = 0; i < hist.size(); ++i) {
      ImVec4 color(0.8f, 0.8f, 0.8f, 1.0f);
      const char* tag = "INFO";
      if (hist[i].level == LogLevel::Success) {
        color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
        tag = "OK";
      } else if (hist[i].level == LogLevel::Warning) {
        color = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
        tag = "WARN";
      } else if (hist[i].level == LogLevel::Error) {
        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        tag = "ERROR";
      }
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::Text("[%s] %s", tag, hist[i].text.c_str());
      ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::Separator();
  }
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
void EditorApp::DrawMeshSection(Entity* e) {
  if (e == nullptr) {
    return;
  }
  ImGui::Separator();
  ImGui::Text("Mesh Renderer");
  if (e->meshGuid.empty()) {
    ImGui::Text("Mesh: (none)");
  } else {
    const MeshRecord* rec = assets.GetByGuid(e->meshGuid);
    if (rec == nullptr) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
      ImGui::Text("Missing asset");
      ImGui::PopStyleColor();
      ImGui::Text("Drop an asset here to reassign");
    } else if (!rec->loaded || !rec->gpuReady) {
      ImGui::Text("Mesh: loading...");
    } else {
      std::filesystem::path mp(rec->meshPath);
      ImGui::Text("Mesh: %s", mp.filename().string().c_str());
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_ASSET");
    if (payload != nullptr && payload->Data != nullptr && payload->DataSize > 0) {
      std::string guid(static_cast<const char*>(payload->Data));
      if (assets.GetByGuid(guid) == nullptr) {
        Announce(LogLevel::Warning, "Dropped asset not found");
      } else if (guid != e->meshGuid) {
        std::unique_ptr<Command> cmd(new SetMeshGuidCmd(e->id, e->meshGuid, guid));
        if (undo.Execute(std::move(cmd), scene)) {
          log.Add(LogLevel::Info, "Mesh assigned");
        }
      }
    }
    ImGui::EndDragDropTarget();
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Mesh")) {
    if (!e->meshGuid.empty()) {
      std::unique_ptr<Command> cmd(new SetMeshGuidCmd(e->id, e->meshGuid, ""));
      undo.Execute(std::move(cmd), scene);
    }
  }
}
void EditorApp::DrawAssetBrowser() {
  ImGui::Begin("Asset Browser", &showAssetBrowser);
  if (ImGui::Button("Refresh")) {
    assets.Scan();
  }
  ImGui::SameLine();
  ImGui::Text("%s", assets.AssetsDir().c_str());
  if (assets.PendingImports() > 0) {
    ImGui::SameLine();
    ImGui::Text("Importing %llu...", (unsigned long long)assets.PendingImports());
  }
  ImGui::Separator();
  ImGui::Text("Meshes:");
  std::vector<std::string> guids = assets.AllGuids();
  for (size_t i = 0; i < guids.size(); ++i) {
    const MeshRecord* rec = assets.GetByGuid(guids[i]);
    if (rec == nullptr) {
      continue;
    }
    std::filesystem::path mp(rec->meshPath);
    std::string label = "M " + mp.filename().string();
    ImGui::PushID(static_cast<int>(i));
    ImGui::Selectable(label.c_str(), selectedAssetGuid == rec->guid && !selectedAssetIsTex);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      selectedAssetGuid = rec->guid;
      selectedAssetIsTex = false;
    }
    if (rec->state == MeshAssetState::Outdated) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[OUTDATED]");
    } else if (rec->state == MeshAssetState::NoSource) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[NO SOURCE]");
    } else if (rec->state == MeshAssetState::SourceMissing) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[SRC MISSING]");
    } else if (!rec->loaded) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[LOADING]");
    }
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
      ImGui::SetDragDropPayload("KZ_ASSET", rec->guid.c_str(), rec->guid.size() + 1);
      ImGui::Text("%s", mp.filename().string().c_str());
      ImGui::EndDragDropSource();
    }
    if (ImGui::BeginPopupContextItem("MeshCtx")) {
      bool canReimport = rec->hasSource;
      if (ImGui::MenuItem("Reimport", nullptr, false, canReimport)) {
        if (assets.Reimport(rec->guid)) {
          Announce(LogLevel::Info, std::string("Reimporting ") + mp.filename().string());
        } else {
          Announce(LogLevel::Warning, "No source to reimport from");
        }
      }
      if (ImGui::MenuItem("Rename")) {
        assetRenameActive = true;
        assetRenameIsTex = false;
        assetRenameGuid = rec->guid;
        std::string fn = mp.filename().string();
        std::strncpy(assetRenameBuf, fn.c_str(), sizeof(assetRenameBuf) - 1);
        assetRenameBuf[sizeof(assetRenameBuf) - 1] = '\0';
      }
      if (ImGui::MenuItem("Delete")) {
        std::vector<RefUse> refs = SceneMeshRefs();
        std::vector<std::string> blocked;
        if (assets.DeleteAssetFile(rec->guid, false, refs, blocked)) {
          Announce(LogLevel::Warning, std::string("Deleted ") + mp.filename().string());
        } else {
          std::string msg = "Cannot delete '" + mp.filename().string() + "': used by " + std::to_string(blocked.size()) + " (";
          for (size_t b = 0; b < blocked.size(); ++b) {
            if (b > 0) {
              msg += ", ";
            }
            msg += blocked[b];
          }
          msg += ")";
          Announce(LogLevel::Error, msg);
        }
      }
      if (ImGui::MenuItem("Force Delete")) {
        forceDeleteGuid = rec->guid;
        forceDeleteIsTex = false;
        ImGui::OpenPopup("Force Delete?");
      }
      if (ImGui::MenuItem("Show in folder")) {
        std::string dir = mp.parent_path().string();
        ShellExecuteW(nullptr, L"open", std::filesystem::path(dir).wstring().c_str(), nullptr, nullptr, SW_SHOW);
      }
      if (rec->state == MeshAssetState::SourceMissing) {
        if (ImGui::MenuItem("Locate source...")) {
          std::string picked;
          if (ShowOpenGltfDialog(window.NativeHandle(), picked)) {
            if (assets.SetSourcePath(rec->guid, picked)) {
              Announce(LogLevel::Success, "Source relinked");
            } else {
              Announce(LogLevel::Error, "Could not relink source");
            }
          }
        }
      }
      ImGui::EndPopup();
    }
    if (assetRenameActive && !assetRenameIsTex && assetRenameGuid == rec->guid) {
      ImGui::SetKeyboardFocusHere(0);
      if (ImGui::InputText("##assetrename", assetRenameBuf, sizeof(assetRenameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (!assets.RenameAssetFile(rec->guid, assetRenameBuf)) {
          Announce(LogLevel::Error, "Rename failed");
        }
        assetRenameActive = false;
      }
    }
    ImGui::PopID();
  }
  ImGui::Separator();
  ImGui::Text("Textures:");
  std::vector<std::string> tguids = assets.AllTexGuids();
  for (size_t i = 0; i < tguids.size(); ++i) {
    const TextureRecord* rec = assets.GetTexByGuid(tguids[i]);
    if (rec == nullptr) {
      continue;
    }
    std::filesystem::path tp(rec->texPath);
    std::string label = "T " + tp.filename().string() + " (" + std::to_string(rec->data.width) + "x" + std::to_string(rec->data.height) + " " + std::to_string(rec->data.mips.size()) + "mip)";
    ImGui::PushID(static_cast<int>(100000 + i));
    ImGui::Selectable(label.c_str(), selectedAssetGuid == rec->guid && selectedAssetIsTex);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      selectedAssetGuid = rec->guid;
      selectedAssetIsTex = true;
    }
    if (rec->state == TextureAssetState::Outdated) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "[OUTDATED]");
    } else if (rec->state == TextureAssetState::NoSource) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[NO SOURCE]");
    } else if (rec->state == TextureAssetState::SourceMissing) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[SRC MISSING]");
    } else if (!rec->loaded) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "[LOADING]");
    }
    ImGui::SameLine();
    ImGui::Text("gpu:%d/%llu", rec->residentLevels, (unsigned long long)rec->data.mips.size());
    if (ImGui::BeginPopupContextItem("TexCtx")) {
      bool canReimport = rec->hasSource;
      if (ImGui::MenuItem("Reimport", nullptr, false, canReimport)) {
        if (assets.ReimportTexture(rec->guid)) {
          Announce(LogLevel::Info, std::string("Reimporting ") + tp.filename().string());
        } else {
          Announce(LogLevel::Warning, "No source to reimport from");
        }
      }
      if (ImGui::MenuItem("Rename")) {
        assetRenameActive = true;
        assetRenameIsTex = true;
        assetRenameGuid = rec->guid;
        std::string fn = tp.filename().string();
        std::strncpy(assetRenameBuf, fn.c_str(), sizeof(assetRenameBuf) - 1);
        assetRenameBuf[sizeof(assetRenameBuf) - 1] = '\0';
      }
      if (ImGui::MenuItem("Delete")) {
        std::vector<std::string> blocked;
        if (assets.DeleteTextureFile(rec->guid, false, blocked)) {
          Announce(LogLevel::Warning, std::string("Deleted ") + tp.filename().string());
        } else {
          std::string msg = "Cannot delete '" + tp.filename().string() + "': used by " + std::to_string(blocked.size()) + " (";
          for (size_t b = 0; b < blocked.size(); ++b) {
            if (b > 0) {
              msg += ", ";
            }
            msg += blocked[b];
          }
          msg += ")";
          Announce(LogLevel::Error, msg);
        }
      }
      if (ImGui::MenuItem("Force Delete")) {
        forceDeleteGuid = rec->guid;
        forceDeleteIsTex = true;
        ImGui::OpenPopup("Force Delete?");
      }
      if (ImGui::MenuItem("Show in folder")) {
        std::string dir = tp.parent_path().string();
        ShellExecuteW(nullptr, L"open", std::filesystem::path(dir).wstring().c_str(), nullptr, nullptr, SW_SHOW);
      }
      ImGui::EndPopup();
    }
    if (assetRenameActive && assetRenameIsTex && assetRenameGuid == rec->guid) {
      ImGui::SetKeyboardFocusHere(0);
      if (ImGui::InputText("##texrename", assetRenameBuf, sizeof(assetRenameBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (!assets.RenameTextureFile(rec->guid, assetRenameBuf)) {
          Announce(LogLevel::Error, "Rename failed");
        }
        assetRenameActive = false;
      }
    }
    ImGui::PopID();
  }
  if (ImGui::BeginPopupModal("Force Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Force delete? Dependents will show missing asset.");
    if (ImGui::Button("Force")) {
      std::vector<RefUse> refs = SceneMeshRefs();
      std::vector<std::string> blocked;
      if (forceDeleteIsTex) {
        assets.DeleteTextureFile(forceDeleteGuid, true, blocked);
      } else {
        assets.DeleteAssetFile(forceDeleteGuid, true, refs, blocked);
      }
      Announce(LogLevel::Warning, "Asset force deleted");
      forceDeleteGuid.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      forceDeleteGuid.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::End();
}
}
