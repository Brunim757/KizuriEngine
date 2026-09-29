#include "Kizuri/PBR.h"
#include <cmath>
namespace Kizuri {
namespace {
float Dot3(const float a[3], const float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
void Norm3(float v[3]) {
  float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l > 1e-6f) {
    v[0] /= l;
    v[1] /= l;
    v[2] /= l;
  }
}
float NDF_GGX(float cosLh, float roughness) {
  float alpha = roughness * roughness;
  float alphaSq = alpha * alpha;
  float denom = (cosLh * cosLh) * (alphaSq - 1.0f) + 1.0f;
  return alphaSq / (3.14159265f * denom * denom + 1e-6f);
}
float GA_SchlickG1(float cosTheta, float k) {
  return cosTheta / (cosTheta * (1.0f - k) + k + 1e-6f);
}
float GA_SchlickGGX(float cosLi, float cosLo, float roughness) {
  float r = roughness + 1.0f;
  float k = (r * r) / 8.0f;
  return GA_SchlickG1(cosLi, k) * GA_SchlickG1(cosLo, k);
}
}
void PBR_Directional(
  const float albedo[3],
  float roughness,
  float metallic,
  const float N[3],
  const float V[3],
  const float L[3],
  const float lightColor[3],
  float outColor[3]) {
  float nn[3] = { N[0], N[1], N[2] };
  float vv[3] = { V[0], V[1], V[2] };
  float ll[3] = { L[0], L[1], L[2] };
  Norm3(nn);
  Norm3(vv);
  Norm3(ll);
  float hh[3] = { ll[0] + vv[0], ll[1] + vv[1], ll[2] + vv[2] };
  Norm3(hh);
  if (roughness < 0.04f) {
    roughness = 0.04f;
  }
  if (roughness > 1.0f) {
    roughness = 1.0f;
  }
  if (metallic < 0.0f) {
    metallic = 0.0f;
  }
  if (metallic > 1.0f) {
    metallic = 1.0f;
  }
  float cosLi = nn[0] * ll[0] + nn[1] * ll[1] + nn[2] * ll[2];
  float cosLo = nn[0] * vv[0] + nn[1] * vv[1] + nn[2] * vv[2];
  if (cosLi < 0.0f) {
    cosLi = 0.0f;
  }
  if (cosLo < 0.0f) {
    cosLo = 0.0f;
  }
  float cosLh = nn[0] * hh[0] + nn[1] * hh[1] + nn[2] * hh[2];
  if (cosLh < 0.0f) {
    cosLh = 0.0f;
  }
  float vh = vv[0] * hh[0] + vv[1] * hh[1] + vv[2] * hh[2];
  if (vh < 0.0f) {
    vh = 0.0f;
  }
  float F0die = 0.04f;
  float F0[3] = {
    F0die + (albedo[0] - F0die) * metallic,
    F0die + (albedo[1] - F0die) * metallic,
    F0die + (albedo[2] - F0die) * metallic
  };
  float f = std::pow(1.0f - vh, 5.0f);
  float F[3] = {
    F0[0] + (1.0f - F0[0]) * f,
    F0[1] + (1.0f - F0[1]) * f,
    F0[2] + (1.0f - F0[2]) * f
  };
  float D = NDF_GGX(cosLh, roughness);
  float G = GA_SchlickGGX(cosLi, cosLo, roughness);
  float denom = 4.0f * cosLi * cosLo + 1e-5f;
  float spec[3] = {
    (F[0] * D * G) / denom,
    (F[1] * D * G) / denom,
    (F[2] * D * G) / denom
  };
  float kd[3] = {
    (1.0f - F[0]) * (1.0f - metallic),
    (1.0f - F[1]) * (1.0f - metallic),
    (1.0f - F[2]) * (1.0f - metallic)
  };
  float diff[3] = {
    kd[0] * albedo[0],
    kd[1] * albedo[1],
    kd[2] * albedo[2]
  };
  outColor[0] = (diff[0] + spec[0]) * lightColor[0] * cosLi;
  outColor[1] = (diff[1] + spec[1]) * lightColor[1] * cosLi;
  outColor[2] = (diff[2] + spec[2]) * lightColor[2] * cosLi;
  (void)Dot3;
}
}
