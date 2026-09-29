#include "Kizuri/Picking.h"
#include <DirectXMath.h>
#include <cstring>
using namespace DirectX;
namespace Kizuri {
void ScreenPointRay(float px, float py, float viewW, float viewH, const float view[16], const float proj[16], float origin[3], float dir[3]) {
  XMMATRIX vm;
  std::memcpy(&vm, view, sizeof(vm));
  XMMATRIX pm;
  std::memcpy(&pm, proj, sizeof(pm));
  XMMATRIX im = XMMatrixIdentity();
  XMVECTOR nearP = XMVector3Unproject(XMVectorSet(px, py, 0.0f, 1.0f), 0.0f, 0.0f, viewW, viewH, 0.0f, 1.0f, pm, vm, im);
  XMVECTOR farP = XMVector3Unproject(XMVectorSet(px, py, 1.0f, 1.0f), 0.0f, 0.0f, viewW, viewH, 0.0f, 1.0f, pm, vm, im);
  XMVECTOR d = XMVectorSubtract(farP, nearP);
  d = XMVector3Normalize(d);
  XMFLOAT3 o;
  XMFLOAT3 dd;
  XMStoreFloat3(&o, nearP);
  XMStoreFloat3(&dd, d);
  origin[0] = o.x;
  origin[1] = o.y;
  origin[2] = o.z;
  dir[0] = dd.x;
  dir[1] = dd.y;
  dir[2] = dd.z;
}
bool RayVsUnitCube(const float origin[3], const float dir[3], const float matrix[16], float& t) {
  XMMATRIX m;
  std::memcpy(&m, matrix, sizeof(m));
  XMVECTOR det;
  XMMATRIX inv = XMMatrixInverse(&det, m);
  if (XMVectorGetX(det) == 0.0f) {
    return false;
  }
  XMVECTOR lo = XMVector3TransformCoord(XMVectorSet(origin[0], origin[1], origin[2], 1.0f), inv);
  XMVECTOR ld = XMVector3TransformNormal(XMVectorSet(dir[0], dir[1], dir[2], 0.0f), inv);
  ld = XMVector3Normalize(ld);
  XMFLOAT3 o;
  XMFLOAT3 d;
  XMStoreFloat3(&o, lo);
  XMStoreFloat3(&d, ld);
  float tmin = 0.0f;
  float tmax = 1e30f;
  float oo[3] = { o.x, o.y, o.z };
  float dd[3] = { d.x, d.y, d.z };
  for (int i = 0; i < 3; ++i) {
    float inv = 1.0f / (dd[i] == 0.0f ? 1e-9f : dd[i]);
    float t0 = (-0.5f - oo[i]) * inv;
    float t1 = (0.5f - oo[i]) * inv;
    if (t0 > t1) {
      float tmp = t0;
      t0 = t1;
      t1 = tmp;
    }
    if (t0 > tmin) {
      tmin = t0;
    }
    if (t1 < tmax) {
      tmax = t1;
    }
    if (tmin > tmax) {
      return false;
    }
  }
  if (tmax < 0.0f) {
    return false;
  }
  t = tmin < 0.0f ? 0.0f : tmin;
  return true;
}
EntityId PickFirst(const Scene& scene, const float origin[3], const float dir[3]) {
  std::vector<EntityId> all = scene.All();
  EntityId best = EntityId::Invalid();
  float bestT = 1e30f;
  for (size_t i = 0; i < all.size(); ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e == nullptr) {
      continue;
    }
    float m[16];
    ComposeMatrix(e->transform, m);
    float t = 0.0f;
    if (RayVsUnitCube(origin, dir, m, t) && t < bestT) {
      bestT = t;
      best = all[i];
    }
  }
  return best;
}
}
