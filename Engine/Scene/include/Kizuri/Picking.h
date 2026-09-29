#pragma once
#include "Kizuri/Scene.h"
namespace Kizuri {
void ScreenPointRay(float px, float py, float viewW, float viewH, const float view[16], const float proj[16], float origin[3], float dir[3]);
bool RayVsUnitCube(const float origin[3], const float dir[3], const float matrix[16], float& t);
EntityId PickFirst(const Scene& scene, const float origin[3], const float dir[3]);
bool EntityScreenRect(const Scene& scene, EntityId id, const float view[16], const float proj[16], float viewX, float viewY, float viewW, float viewH, float& x0, float& y0, float& x1, float& y1);
bool RectsOverlap(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1);
}
