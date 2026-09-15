#pragma once

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

namespace kizuri::renderer {

using namespace DirectX;

struct MeshVertex
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT2 Uv;
};

struct MeshData
{
    std::vector<MeshVertex> Vertices;
    std::vector<uint32_t> Indices;
};

MeshData CreateCubeMesh(float size);
MeshData CreatePlaneMesh(float extentX, float extentZ);

} // namespace kizuri::renderer