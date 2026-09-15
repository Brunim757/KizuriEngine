#include "kizuri/renderer/Meshes.h"

namespace kizuri::renderer {

MeshData CreateCubeMesh(float size)
{
    MeshData out;
    float h = size * 0.5f;

    struct FaceDef
    {
        XMFLOAT3 n;
        float dirs[4][3];
    };

    FaceDef faces[6] = {
        { XMFLOAT3(1,0,0),  {{h,-h,h},{h,h,h},{h,h,-h},{h,-h,-h}} },
        { XMFLOAT3(-1,0,0), {{-h,h,h},{-h,-h,h},{-h,-h,-h},{-h,h,-h}} },
        { XMFLOAT3(0,1,0),  {{-h,h,h},{h,h,h},{h,h,-h},{-h,h,-h}} },
        { XMFLOAT3(0,-1,0), {{h,-h,h},{-h,-h,h},{-h,-h,-h},{h,-h,-h}} },
        { XMFLOAT3(0,0,1),  {{-h,h,h},{-h,-h,h},{h,-h,h},{h,h,h}} },
        { XMFLOAT3(0,0,-1), {{h,h,-h},{h,-h,-h},{-h,-h,-h},{-h,h,-h}} },
    };

    float uv[4][2] = {{0,0},{0,1},{1,1},{1,0}};

    for (auto& face : faces)
    {
        uint32_t base = static_cast<uint32_t>(out.Vertices.size());
        for (int v = 0; v < 4; ++v)
        {
            MeshVertex vert{};
            vert.Position = XMFLOAT3(face.dirs[v][0], face.dirs[v][1], face.dirs[v][2]);
            vert.Normal = face.n;
            vert.Uv = XMFLOAT2(uv[v][0], uv[v][1]);
            out.Vertices.push_back(vert);
        }
        out.Indices.push_back(base);
        out.Indices.push_back(base + 1);
        out.Indices.push_back(base + 2);
        out.Indices.push_back(base);
        out.Indices.push_back(base + 2);
        out.Indices.push_back(base + 3);
    }

    return out;
}

MeshData CreatePlaneMesh(float extentX, float extentZ)
{
    MeshData out;

    MeshVertex corners[4] = {
        { XMFLOAT3(-extentX, 0.0f, -extentZ), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(-extentX, 0.0f,  extentZ), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, static_cast<float>(extentZ)) },
        { XMFLOAT3( extentX, 0.0f,  extentZ), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(static_cast<float>(extentX), static_cast<float>(extentZ)) },
        { XMFLOAT3( extentX, 0.0f, -extentZ), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(static_cast<float>(extentX), 0.0f) },
    };

    for (auto& v : corners)
    {
        out.Vertices.push_back(v);
    }
    out.Indices.push_back(0);
    out.Indices.push_back(1);
    out.Indices.push_back(2);
    out.Indices.push_back(0);
    out.Indices.push_back(2);
    out.Indices.push_back(3);

    return out;
}

} // namespace kizuri::renderer