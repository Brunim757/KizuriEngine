#include "kizuri/renderer/ClusterGrid.h"

#include <algorithm>
#include <cmath>

namespace kizuri::renderer {

namespace {

float LogDepth(float linearDepth, float nearZ, float farZ)
{
    float clamped = linearDepth < nearZ ? nearZ : linearDepth;
    return logf(clamped / nearZ) / logf(farZ / nearZ);
}

void ComputeXyRange(
    FXMMATRIX viewProj,
    const XMFLOAT3& lightCenter,
    float radius,
    uint32_t clusterX,
    uint32_t clusterY,
    int32_t& minX,
    int32_t& maxX,
    int32_t& minY,
    int32_t& maxY)
{
    minX = static_cast<int32_t>(clusterX);
    maxX = 0;
    minY = static_cast<int32_t>(clusterY);
    maxY = 0;

    for (uint32_t i = 0; i < 8; ++i)
    {
        XMFLOAT3 corner = lightCenter;
        corner.x += ((i & 1) ? radius : -radius);
        corner.y += ((i & 2) ? radius : -radius);
        corner.z += ((i & 4) ? radius : -radius);

        XMVECTOR projected = XMVector3TransformCoord(XMLoadFloat3(&corner), viewProj);
        float px = XMVectorGetX(projected);
        float py = XMVectorGetY(projected);
        if (px < -1.0f)
        {
            px = -1.0f;
        }
        if (px > 1.0f)
        {
            px = 1.0f;
        }
        if (py < -1.0f)
        {
            py = -1.0f;
        }
        if (py > 1.0f)
        {
            py = 1.0f;
        }

        int32_t cx = static_cast<int32_t>((px * 0.5f + 0.5f) * static_cast<float>(clusterX));
        int32_t cy = static_cast<int32_t>((py * 0.5f + 0.5f) * static_cast<float>(clusterY));
        if (cx < 0)
        {
            cx = 0;
        }
        if (cx >= static_cast<int32_t>(clusterX))
        {
            cx = static_cast<int32_t>(clusterX) - 1;
        }
        if (cy < 0)
        {
            cy = 0;
        }
        if (cy >= static_cast<int32_t>(clusterY))
        {
            cy = static_cast<int32_t>(clusterY) - 1;
        }

        minX = std::min(minX, cx);
        maxX = std::max(maxX, cx);
        minY = std::min(minY, cy);
        maxY = std::max(maxY, cy);
    }
}

void ComputeZRange(
    FXMMATRIX view,
    const XMFLOAT3& lightCenter,
    float radius,
    uint32_t clusterZ,
    float nearZ,
    float farZ,
    uint32_t& minZ,
    uint32_t& maxZ)
{
    minZ = clusterZ;
    maxZ = 0;

    for (uint32_t i = 0; i < 8; ++i)
    {
        XMFLOAT3 corner = lightCenter;
        corner.x += ((i & 1) ? radius : -radius);
        corner.y += ((i & 2) ? radius : -radius);
        corner.z += ((i & 4) ? radius : -radius);

        XMVECTOR viewPos = XMVector3TransformCoord(XMLoadFloat3(&corner), view);
        float viewZ = XMVectorGetZ(viewPos);
        if (viewZ <= nearZ)
        {
            viewZ = nearZ;
        }

        uint32_t slice = static_cast<uint32_t>(LogDepth(viewZ, nearZ, farZ) * static_cast<float>(clusterZ));
        if (slice >= clusterZ)
        {
            slice = clusterZ - 1;
        }
        minZ = std::min(minZ, slice);
        maxZ = std::max(maxZ, slice);
    }
}

} // namespace

void ClusterGrid::Resize(uint32_t screenWidth, uint32_t screenHeight, float nearZ, float farZ)
{
    nearZ_ = nearZ;
    farZ_ = farZ;
    clusterX_ = (screenWidth + tileSize_ - 1) / tileSize_;
    clusterY_ = (screenHeight + tileSize_ - 1) / tileSize_;
    clusterCount_ = clusterX_ * clusterY_ * clusterZ_;
    grid_.assign(clusterCount_, ClusterRange{ 0, 0 });
    indices_.assign(MaxLightIndices, 0);
    indexCount_ = 0;
}

uint32_t ClusterGrid::ZSlice(float linearDepth) const
{
    float slice = LogDepth(linearDepth, nearZ_, farZ_) * static_cast<float>(clusterZ_);
    uint32_t result = static_cast<uint32_t>(slice);
    if (result >= clusterZ_)
    {
        result = clusterZ_ - 1;
    }
    return result;
}

void ClusterGrid::Build(
    FXMMATRIX viewProj,
    FXMMATRIX view,
    const GpuLight* lights,
    uint32_t lightCount)
{
    for (auto& range : grid_)
    {
        range.First = 0;
        range.Count = 0;
    }

    if (lightCount > MaxLights)
    {
        lightCount = MaxLights;
    }

    std::vector<uint32_t> clusterPairs;
    clusterPairs.reserve(static_cast<size_t>(lightCount) * 64);

    std::vector<uint32_t> xMin(lightCount, clusterX_);
    std::vector<uint32_t> xMax(lightCount, 0);
    std::vector<uint32_t> yMin(lightCount, clusterY_);
    std::vector<uint32_t> yMax(lightCount, 0);
    std::vector<uint32_t> zMin(lightCount, clusterZ_);
    std::vector<uint32_t> zMax(lightCount, 0);

    for (uint32_t l = 0; l < lightCount; ++l)
    {
        int32_t minX = static_cast<int32_t>(clusterX_);
        int32_t maxX = 0;
        int32_t minY = static_cast<int32_t>(clusterY_);
        int32_t maxY = 0;
        ComputeXyRange(viewProj, lights[l].Position, lights[l].Radius, clusterX_, clusterY_,
            minX, maxX, minY, maxY);

        uint32_t minZ = clusterZ_;
        uint32_t maxZ = 0;
        ComputeZRange(view, lights[l].Position, lights[l].Radius, clusterZ_, nearZ_, farZ_,
            minZ, maxZ);

        xMin[l] = static_cast<uint32_t>(minX < 0 ? 0 : minX);
        xMax[l] = static_cast<uint32_t>(maxX < 0 ? 0 : maxX);
        yMin[l] = static_cast<uint32_t>(minY < 0 ? 0 : minY);
        yMax[l] = static_cast<uint32_t>(maxY < 0 ? 0 : maxY);
        zMin[l] = minZ;
        zMax[l] = maxZ;
    }

    std::vector<uint32_t> counts(clusterCount_, 0);
    for (uint32_t l = 0; l < lightCount; ++l)
    {
        for (uint32_t z = zMin[l]; z <= zMax[l]; ++z)
        {
            for (uint32_t y = yMin[l]; y <= yMax[l]; ++y)
            {
                for (uint32_t x = xMin[l]; x <= xMax[l]; ++x)
                {
                    uint32_t cluster = ClusterIndex(x, y, z);
                    if (cluster < clusterCount_)
                    {
                        ++counts[cluster];
                    }
                }
            }
        }
    }

    uint32_t offset = 0;
    for (uint32_t i = 0; i < clusterCount_; ++i)
    {
        grid_[i].First = offset;
        grid_[i].Count = counts[i];
        offset += counts[i];
    }

    std::vector<uint32_t> writeCursor(clusterCount_);
    for (uint32_t i = 0; i < clusterCount_; ++i)
    {
        writeCursor[i] = grid_[i].First;
    }

    uint32_t written = 0;
    for (uint32_t l = 0; l < lightCount; ++l)
    {
        for (uint32_t z = zMin[l]; z <= zMax[l]; ++z)
        {
            for (uint32_t y = yMin[l]; y <= yMax[l]; ++y)
            {
                for (uint32_t x = xMin[l]; x <= xMax[l]; ++x)
                {
                    uint32_t cluster = ClusterIndex(x, y, z);
                    if (cluster >= clusterCount_)
                    {
                        continue;
                    }
                    uint32_t target = writeCursor[cluster]++;
                    if (target < indices_.size())
                    {
                        indices_[target] = l;
                        ++written;
                    }
                }
            }
        }
    }

    indexCount_ = static_cast<uint32_t>(
        std::min<size_t>(offset, indices_.size()));
}

ClusterConstants ClusterGrid::BuildConstants() const
{
    ClusterConstants out{};
    out.TileSizeX = static_cast<float>(tileSize_);
    out.TileSizeY = static_cast<float>(tileSize_);
    out.ClusterCountX = static_cast<float>(clusterX_);
    out.ClusterCountY = static_cast<float>(clusterY_);
    out.NearZ = nearZ_;
    out.FarZ = farZ_;
    out.ClusterCountZ = static_cast<float>(clusterZ_);
    out.InvLogDepth = 1.0f / logf(farZ_ / nearZ_);
    return out;
}

} // namespace kizuri::renderer