#pragma once
#include "Kizuri/Scene.h"
namespace Kizuri {
void ScreenPointRay(float px, float py, float viewW, float viewH, const float view[16], const float proj[16], float origin[3], float dir[3]);
bool RayVsUnitCube(const float origin[3], const float dir[3], const float matrix[16], float& t);
EntityId PickFirst(const Scene& scene, const float origin[3], const float dir[3]);
}
