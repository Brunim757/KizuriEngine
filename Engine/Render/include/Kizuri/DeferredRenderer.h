#pragma once
#include "Kizuri/RHI.h"
#include <vector>
#include <stdint.h>
namespace Kizuri {
struct DeferredMaterial {
  float albedo[3];
  float roughness;
  float metallic;
};
struct DeferredLight {
  float direction[3];
  float color[3];
  float intensity;
};
struct RenderPointLight {
  float pos[3];
  float color[3];
  float intensity;
  float radius;
};
struct RenderSpotLight {
  float pos[3];
  float dir[3];
  float color[3];
  float intensity;
  float radius;
  float angle;
  float falloff;
};
struct RenderDirectionalLight {
  float dir[3];
  float color[3];
  float intensity;
};
class DeferredRenderer {
public:
  DeferredRenderer();
  ~DeferredRenderer();
  bool Initialize(IRHI* rhi, int w, int h, const char* shaderDir);
  void Shutdown();
  bool SetMesh(const float* positions, const float* normals, const float* uvs, size_t vertexCount, const uint32_t* indices, size_t indexCount);
  void SetMaterial(const DeferredMaterial& mat);
  void SetLight(const DeferredLight& light);
  void ClearLights();
  void AddPointLight(const RenderPointLight& light);
  void AddSpotLight(const RenderSpotLight& light);
  void AddDirectionalLight(const RenderDirectionalLight& light);
  bool Resize(int w, int h);
  void SetViewOffset(float x, float y);
  void Render(const float view[16], const float proj[16], const float camPos[3]);
  void RenderToTexture(const float view[16], const float proj[16], const float camPos[3]);
  void* GetViewportTexture();
  void BeginObjects(const float view[16], const float proj[16]);
  void DrawObject(const float world[16]);
  void DrawObjectEx(const float world[16], RHIBuffer vb, RHIBuffer ib, uint32_t start, uint32_t count, const DeferredMaterial* mat);
  void EndObjectsToTexture(const float camPos[3]);
  void EndObjectsToBackbuffer(const float camPos[3]);
  bool IsReady() const;
private:
  IRHI* rhi;
  int w;
  int h;
  RHIRenderTarget gAlbedo;
  RHIRenderTarget gNormalRough;
  RHIRenderTarget gMetallic;
  RHIRenderTarget gPosition;
  RHIRenderTarget gDepth;
  RHIRenderTarget gViewport;
  RHIBuffer vb;
  RHIBuffer ib;
  RHIConstBuffer geoCB;
  RHIConstBuffer lightCB;
  RHIConstBuffer matCB;
  RHIVertexShader geoVS;
  RHIPixelShader geoPS;
  RHIVertexShader lightVS;
  RHIPixelShader lightPS;
  RHIInputLayout layout;
  RHISampler sampler;
  uint32_t indexCount;
  float viewX;
  float viewY;
  bool begun;
  float lastView[16];
  float lastProj[16];
  bool ready;
  DeferredMaterial material;
  DeferredLight light;
  std::vector<RenderPointLight> pointLights;
  std::vector<RenderSpotLight> spotLights;
  std::vector<RenderDirectionalLight> dirLights;
  bool CreateTargets();
  void DestroyTargets();
  void RenderInternal(const float view[16], const float proj[16], const float camPos[3], bool toTexture);
  void EndInternal(const float camPos[3], bool toTexture);
  DeferredRenderer(const DeferredRenderer&) = delete;
  DeferredRenderer& operator=(const DeferredRenderer&) = delete;
};
}
