#pragma once
namespace Kizuri {
void PBR_Directional(
  const float albedo[3],
  float roughness,
  float metallic,
  const float N[3],
  const float V[3],
  const float L[3],
  const float lightColor[3],
  float outColor[3]);
void PBR_Point(
  const float albedo[3],
  float roughness,
  float metallic,
  const float N[3],
  const float V[3],
  const float L[3],
  float dist,
  float range,
  const float lightColor[3],
  float outColor[3]);
void PBR_Spot(
  const float albedo[3],
  float roughness,
  float metallic,
  const float N[3],
  const float V[3],
  const float L[3],
  float dist,
  float range,
  const float spotDir[3],
  float cosOuter,
  float falloff,
  const float lightColor[3],
  float outColor[3]);
}
