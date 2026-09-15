#pragma once

#include <DirectXMath.h>
#include <cstdint>

namespace kizuri::renderer {

using namespace DirectX;

constexpr uint32_t MaxLights = 128;
constexpr uint32_t MaxLightIndices = 8192;
constexpr uint32_t MaxClusters = 65536;
constexpr uint32_t TileSizePixels = 64;
constexpr uint32_t ClusterSlicesZ = 16;

struct GpuLight
{
    XMFLOAT3 Position;
    float Radius;
    XMFLOAT3 Color;
    float Intensity;
};

struct FrameConstants
{
    XMFLOAT4X4 ViewProj;
    XMFLOAT4 CameraPosition;
    XMFLOAT4 ScreenSize;
};

struct LightConstants
{
    int32_t LightCount;
    float Pad[3];
    GpuLight Lights[MaxLights];
};

struct ClusterConstants
{
    float TileSizeX;
    float TileSizeY;
    float ClusterCountX;
    float ClusterCountY;
    float NearZ;
    float FarZ;
    float ClusterCountZ;
    float InvLogDepth;
    float Ambient;
    float AmbientPad[3];
};

struct ObjectConstants
{
    XMFLOAT4X4 World;
    XMFLOAT4X4 WorldInverseTranspose;
    XMFLOAT4 Tint;
};

struct ObjectData
{
    XMMATRIX World;
    XMFLOAT3 Tint;
};

struct ClusterRange
{
    uint32_t First;
    uint32_t Count;
};

} // namespace kizuri::renderer