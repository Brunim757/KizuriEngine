#pragma once
#include "Kizuri/Scene.h"
#include "Kizuri/Selection.h"
#include "Kizuri/Log.h"
#include "Kizuri/EditQueue.h"
#include "Kizuri/Window.h"
#include "Kizuri/RHI.h"
#include "Kizuri/Camera.h"
#include "Kizuri/MeshLoader.h"
#include "Kizuri/DeferredRenderer.h"
#include <string>
namespace Kizuri {
struct EditorApp {
  Window window;
  IRHI* rhi;
  DeferredRenderer renderer;
  Scene scene;
  SingleSelection selection;
  LogStore log;
  EditQueue edits;
  FreeCamera camera;
  StaticMesh cubeMesh;
  bool cubeReady;
  bool running;
  bool showHierarchy;
  bool showInspector;
  bool showConsole;
  bool showViewport;
  bool showAbout;
  float viewX;
  float viewY;
  float viewW;
  float viewH;
  bool viewValid;
  int lastMouseX;
  int lastMouseY;
  bool downPosValid;
  int downX;
  int downY;
  int rdownX;
  int rdownY;
  bool rdownValid;
  EntityId contextPick;
  char renameBuf[128];
  bool renameActive;
  EntityId renameTarget;
  EditorApp();
  bool Initialize();
  int Run();
  void Shutdown();
  void Frame();
  void DrawMenuBar();
  void DrawViewport();
  void DrawHierarchy();
  void DrawInspector();
  void DrawConsole();
  void RenderScene();
  void UpdateCamera(float dt);
  void HandleViewportClick();
  void CreateEntityAt(float x, float y, float z);
  void FocusEntity(EntityId id);
  std::string FindAsset(const char* name);
  std::string FindShaderDir();
};
}
