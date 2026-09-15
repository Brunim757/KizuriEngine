#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <vector>

#include "kizuri/renderer/RenderTypes.h"

namespace kizuri::renderer {

class ClusterGrid {
public:
    void Resize(uint32_t screenWidth, uint32_t screenHeight, float nearZ, float farZ);
    void Build(
        FXMMATRIX viewProj,
        FXMMATRIX view,
        const GpuLight* lights,
        uint32_t lightCount);

    uint32_t ClusterCount() const { return clusterCount_; }
    uint32_t ClusterCountX() const { return clusterX_; }
    uint32_t ClusterCountY() const { return clusterY_; }
    uint32_t ClusterCountZ() const { return clusterZ_; }
    uint32_t IndexCount() const { return indexCount_; }
    const ClusterRange* Grid() const { return grid_.data(); }
    const uint32_t* Indices() const { return indices_.data(); }
    ClusterConstants BuildConstants() const;

    uint32_t TileSize() const { return tileSize_; }
    float NearZ() const { return nearZ_; }
    float FarZ() const { return farZ_; }

    uint32_t ClusterIndex(uint32_t x, uint32_t y, uint32_t z) const
    {
        return x + y * clusterX_ + z * clusterX_ * clusterY_;
    }

private:
    uint32_t ZSlice(float linearDepth) const;

    uint32_t clusterX_ = 0;
    uint32_t clusterY_ = 0;
    uint32_t clusterZ_ = ClusterSlicesZ;
    uint32_t clusterCount_ = 0;
    uint32_t tileSize_ = TileSizePixels;
    float nearZ_ = 0.1f;
    float farZ_ = 1000.0f;
    uint32_t indexCount_ = 0;
    std::vector<ClusterRange> grid_;
    std::vector<uint32_t> indices_;
};

} // namespace kizuri::renderer