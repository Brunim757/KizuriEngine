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
}
