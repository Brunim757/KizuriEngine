#include "Kizuri/DeferredRenderer.h"
#include <cstring>
#include <string>
namespace Kizuri {
namespace {
struct GeoConstants {
  float world[16];
  float view[16];
  float proj[16];
};
struct LightConstants {
  float camPos[4];
  float lightDir[4];
  float lightColor[4];
};
struct MatConstants {
  float albedo[4];
  float params[4];
};
}
DeferredRenderer::DeferredRenderer()
  : rhi(nullptr)
  , w(0)
  , h(0)
  , gAlbedo(0)
  , gNormalRough(0)
  , gMetallic(0)
  , gPosition(0)
  , gDepth(0)
  , gViewport(0)
  , vb(0)
  , ib(0)
  , geoCB(0)
  , lightCB(0)
  , matCB(0)
  , geoVS(0)
  , geoPS(0)
  , lightVS(0)
  , lightPS(0)
  , layout(0)
  , sampler(0)
  , indexCount(0)
  , viewX(0.0f)
  , viewY(0.0f)
  , ready(false) {
  material.albedo[0] = 0.8f;
  material.albedo[1] = 0.2f;
  material.albedo[2] = 0.15f;
  material.roughness = 0.5f;
  material.metallic = 0.0f;
  light.direction[0] = 0.4f;
  light.direction[1] = -1.0f;
  light.direction[2] = 0.3f;
  light.color[0] = 1.0f;
  light.color[1] = 1.0f;
  light.color[2] = 1.0f;
  light.intensity = 2.5f;
}
DeferredRenderer::~DeferredRenderer() {
  Shutdown();
}
bool DeferredRenderer::Initialize(IRHI* r, int vw, int vh, const char* shaderDir) {
  if (r == nullptr || vw <= 0 || vh <= 0 || shaderDir == nullptr) {
    return false;
  }
  rhi = r;
  w = vw;
  h = vh;
  std::string dir(shaderDir);
  std::string geoVSPath = dir + "/GeometryVS.hlsl";
  std::string geoPSPath = dir + "/GeometryPS.hlsl";
  std::string lightVSPath = dir + "/LightingVS.hlsl";
  std::string lightPSPath = dir + "/LightingPS.hlsl";
  geoVS = rhi->CreateVertexShaderFromFile(geoVSPath.c_str(), "main");
  geoPS = rhi->CreatePixelShaderFromFile(geoPSPath.c_str(), "main");
  lightVS = rhi->CreateVertexShaderFromFile(lightVSPath.c_str(), "main");
  lightPS = rhi->CreatePixelShaderFromFile(lightPSPath.c_str(), "main");
  if (geoVS == 0 || geoPS == 0 || lightVS == 0 || lightPS == 0) {
    return false;
  }
  layout = rhi->CreateInputLayoutPNU(geoVS);
  if (layout == 0) {
    return false;
  }
  GeoConstants gc;
  std::memset(&gc, 0, sizeof(gc));
  LightConstants lc;
  std::memset(&lc, 0, sizeof(lc));
  MatConstants mc;
  std::memset(&mc, 0, sizeof(mc));
  geoCB = rhi->CreateConstantBuffer(sizeof(GeoConstants), &gc);
  lightCB = rhi->CreateConstantBuffer(sizeof(LightConstants), &lc);
  matCB = rhi->CreateConstantBuffer(sizeof(MatConstants), &mc);
  if (geoCB == 0 || lightCB == 0 || matCB == 0) {
    return false;
  }
  sampler = rhi->CreateSamplerLinear();
  if (sampler == 0) {
    return false;
  }
  if (!CreateTargets()) {
    return false;
  }
  ready = true;
  return true;
}
void DeferredRenderer::Shutdown() {
  if (rhi == nullptr) {
    return;
  }
  DestroyTargets();
  if (vb != 0) {
    rhi->DestroyBuffer(vb);
    vb = 0;
  }
  if (ib != 0) {
    rhi->DestroyBuffer(ib);
    ib = 0;
  }
  if (geoCB != 0) {
    rhi->DestroyConstantBuffer(geoCB);
    geoCB = 0;
  }
  if (lightCB != 0) {
    rhi->DestroyConstantBuffer(lightCB);
    lightCB = 0;
  }
  if (matCB != 0) {
    rhi->DestroyConstantBuffer(matCB);
    matCB = 0;
  }
  rhi = nullptr;
  ready = false;
}
bool DeferredRenderer::SetMesh(const float* positions, const float* normals, const float* uvs, size_t vertexCount, const uint32_t* indices, size_t idxCount) {
  if (rhi == nullptr || positions == nullptr || normals == nullptr || indices == nullptr) {
    return false;
  }
  if (vertexCount == 0 || idxCount == 0) {
    return false;
  }
  std::vector<float> interleaved;
  interleaved.reserve(vertexCount * 8);
  for (size_t i = 0; i < vertexCount; ++i) {
    interleaved.push_back(positions[i * 3 + 0]);
    interleaved.push_back(positions[i * 3 + 1]);
    interleaved.push_back(positions[i * 3 + 2]);
    interleaved.push_back(normals[i * 3 + 0]);
    interleaved.push_back(normals[i * 3 + 1]);
    interleaved.push_back(normals[i * 3 + 2]);
    float u = 0.0f;
    float v = 0.0f;
    if (uvs != nullptr) {
      u = uvs[i * 2 + 0];
      v = uvs[i * 2 + 1];
    }
    interleaved.push_back(u);
    interleaved.push_back(v);
  }
  uint64_t vbSize = static_cast<uint64_t>(interleaved.size() * sizeof(float));
  uint64_t ibSize = static_cast<uint64_t>(idxCount * sizeof(uint32_t));
  RHIBuffer nvb = rhi->CreateBuffer(vbSize, 32, false, interleaved.data());
  RHIBuffer nib = rhi->CreateBuffer(ibSize, 4, true, indices);
  if (nvb == 0 || nib == 0) {
    return false;
  }
  if (vb != 0) {
    rhi->DestroyBuffer(vb);
  }
  if (ib != 0) {
    rhi->DestroyBuffer(ib);
  }
  vb = nvb;
  ib = nib;
  indexCount = static_cast<uint32_t>(idxCount);
  return true;
}
void DeferredRenderer::SetMaterial(const DeferredMaterial& mat) {
  material = mat;
}
void DeferredRenderer::SetLight(const DeferredLight& l) {
  light = l;
}
bool DeferredRenderer::Resize(int nw, int nh) {
  if (nw <= 0 || nh <= 0) {
    return false;
  }
  w = nw;
  h = nh;
  DestroyTargets();
  return CreateTargets();
}
void DeferredRenderer::SetViewOffset(float x, float y) {
  viewX = x;
  viewY = y;
}
void DeferredRenderer::Render(const float view[16], const float proj[16], const float camPos[3]) {
  RenderInternal(view, proj, camPos, false);
}
void DeferredRenderer::RenderToTexture(const float view[16], const float proj[16], const float camPos[3]) {
  RenderInternal(view, proj, camPos, true);
}
void* DeferredRenderer::GetViewportTexture() {
  if (rhi == nullptr || gViewport == 0) {
    return nullptr;
  }
  return rhi->GetRenderTargetSRV(gViewport);
}
void DeferredRenderer::RenderInternal(const float view[16], const float proj[16], const float camPos[3], bool toTexture) {
  if (!ready || rhi == nullptr || vb == 0 || ib == 0) {
    return;
  }
  RHIRenderTarget mrts[4] = { gAlbedo, gNormalRough, gMetallic, gPosition };
  rhi->SetRenderTargets(4, mrts, gDepth);
  rhi->ClearRenderTarget(gAlbedo, 0.02f, 0.02f, 0.03f, 1.0f);
  rhi->ClearRenderTarget(gNormalRough, 0.5f, 0.5f, 1.0f, 1.0f);
  rhi->ClearRenderTarget(gMetallic, 0.0f, 0.0f, 0.0f, 1.0f);
  rhi->ClearRenderTarget(gPosition, 0.0f, 0.0f, 0.0f, 1.0f);
  rhi->ClearDepth(gDepth);
  RHIViewport vp;
  vp.x = viewX;
  vp.y = viewY;
  vp.w = static_cast<float>(w);
  vp.h = static_cast<float>(h);
  vp.minD = 0.0f;
  vp.maxD = 1.0f;
  rhi->SetViewport(vp);
  rhi->SetTopology(RHITopology::TriangleList);
  RHIRasterizer rs;
  rs.cull = RHICull::Back;
  rs.fill = RHIFill::Solid;
  rs.frontCCW = false;
  rhi->SetRasterizerState(rs);
  RHIDepthStencil ds;
  ds.depthEnable = true;
  ds.depthWrite = true;
  rhi->SetDepthStencilState(ds);
  RHIBlend blend;
  blend.enable = false;
  rhi->SetBlendState(blend);
  rhi->SetInputLayout(layout);
  rhi->SetVertexShader(geoVS);
  rhi->SetPixelShader(geoPS);
  rhi->SetVertexBuffer(vb, 0);
  rhi->SetIndexBuffer(ib);
  GeoConstants gc;
  gc.world[0] = 1.0f; gc.world[1] = 0.0f; gc.world[2] = 0.0f; gc.world[3] = 0.0f;
  gc.world[4] = 0.0f; gc.world[5] = 1.0f; gc.world[6] = 0.0f; gc.world[7] = 0.0f;
  gc.world[8] = 0.0f; gc.world[9] = 0.0f; gc.world[10] = 1.0f; gc.world[11] = 0.0f;
  gc.world[12] = 0.0f; gc.world[13] = 0.0f; gc.world[14] = 0.0f; gc.world[15] = 1.0f;
  std::memcpy(gc.view, view, sizeof(gc.view));
  std::memcpy(gc.proj, proj, sizeof(gc.proj));
  rhi->UpdateConstantBuffer(geoCB, &gc, sizeof(gc));
  rhi->SetVertexConstantBuffer(0, geoCB);
  MatConstants mc;
  mc.albedo[0] = material.albedo[0];
  mc.albedo[1] = material.albedo[1];
  mc.albedo[2] = material.albedo[2];
  mc.albedo[3] = 1.0f;
  mc.params[0] = material.roughness;
  mc.params[1] = material.metallic;
  mc.params[2] = 0.0f;
  mc.params[3] = 0.0f;
  rhi->UpdateConstantBuffer(matCB, &mc, sizeof(mc));
  rhi->SetPixelConstantBuffer(0, matCB);
  rhi->DrawIndexed(indexCount, 0, 0);
  if (toTexture) {
    rhi->SetRenderTargets(1, &gViewport, 0);
  } else {
    rhi->BindBackbuffer();
  }
  RHIViewport lvp;
  lvp.x = viewX;
  lvp.y = viewY;
  lvp.w = static_cast<float>(w);
  lvp.h = static_cast<float>(h);
  lvp.minD = 0.0f;
  lvp.maxD = 1.0f;
  rhi->SetViewport(lvp);
  RHIDepthStencil dsOff;
  dsOff.depthEnable = false;
  dsOff.depthWrite = false;
  rhi->SetDepthStencilState(dsOff);
  rhi->SetVertexShader(lightVS);
  rhi->SetPixelShader(lightPS);
  rhi->SetPixelTexture(0, gAlbedo);
  rhi->SetPixelTexture(1, gNormalRough);
  rhi->SetPixelTexture(2, gMetallic);
  rhi->SetPixelTexture(3, gPosition);
  rhi->SetPixelSampler(0, sampler);
  LightConstants lc;
  lc.camPos[0] = camPos[0];
  lc.camPos[1] = camPos[1];
  lc.camPos[2] = camPos[2];
  lc.camPos[3] = 1.0f;
  lc.lightDir[0] = light.direction[0];
  lc.lightDir[1] = light.direction[1];
  lc.lightDir[2] = light.direction[2];
  lc.lightDir[3] = 0.0f;
  lc.lightColor[0] = light.color[0] * light.intensity;
  lc.lightColor[1] = light.color[1] * light.intensity;
  lc.lightColor[2] = light.color[2] * light.intensity;
  lc.lightColor[3] = 1.0f;
  rhi->UpdateConstantBuffer(lightCB, &lc, sizeof(lc));
  rhi->SetPixelConstantBuffer(0, lightCB);
  rhi->DrawFullscreenTriangle();
  rhi->SetPixelTexture(0, 0);
  rhi->SetPixelTexture(1, 0);
  rhi->SetPixelTexture(2, 0);
  rhi->SetPixelTexture(3, 0);
  if (toTexture) {
    rhi->BindBackbuffer();
  }
}
bool DeferredRenderer::IsReady() const {
  return ready;
}
bool DeferredRenderer::CreateTargets() {
  if (rhi == nullptr) {
    return false;
  }
  gAlbedo = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  gNormalRough = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA16F);
  gMetallic = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  gPosition = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA16F);
  gDepth = rhi->CreateRenderTarget(w, h, RHIFormat::D24S8);
  gViewport = rhi->CreateRenderTarget(w, h, RHIFormat::RGBA8_UNORM);
  return gAlbedo != 0 && gNormalRough != 0 && gMetallic != 0 && gPosition != 0 && gDepth != 0 && gViewport != 0;
}
void DeferredRenderer::DestroyTargets() {
  if (rhi == nullptr) {
    return;
  }
  if (gAlbedo != 0) {
    rhi->DestroyRenderTarget(gAlbedo);
    gAlbedo = 0;
  }
  if (gNormalRough != 0) {
    rhi->DestroyRenderTarget(gNormalRough);
    gNormalRough = 0;
  }
  if (gMetallic != 0) {
    rhi->DestroyRenderTarget(gMetallic);
    gMetallic = 0;
  }
  if (gPosition != 0) {
    rhi->DestroyRenderTarget(gPosition);
    gPosition = 0;
  }
  if (gDepth != 0) {
    rhi->DestroyRenderTarget(gDepth);
    gDepth = 0;
  }
  if (gViewport != 0) {
    rhi->DestroyRenderTarget(gViewport);
    gViewport = 0;
  }
}
}
