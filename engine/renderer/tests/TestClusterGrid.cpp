#include <catch2/catch_test_macros.hpp>
#include "kizuri/renderer/ClusterGrid.h"

using namespace kizuri::renderer;

TEST_CASE("ClusterGrid resize creates correct dimensions", "[cluster]")
{
    ClusterGrid grid;
    grid.Resize(1280, 720, 0.1f, 100.0f);

    REQUIRE(grid.ClusterCountX() > 0);
    REQUIRE(grid.ClusterCountY() > 0);
    REQUIRE(grid.ClusterCountZ() == ClusterSlicesZ);
    REQUIRE(grid.ClusterCount() == grid.ClusterCountX() * grid.ClusterCountY() * grid.ClusterCountZ());
}

TEST_CASE("ClusterGrid build with no lights produces zero index count", "[cluster]")
{
    ClusterGrid grid;
    grid.Resize(1280, 720, 0.1f, 100.0f);

    XMMATRIX view = XMMatrixLookToRH(XMVectorSet(0, 2, -6, 0), XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 1, 0, 0));
    XMMATRIX proj = XMMatrixPerspectiveFovRH(1.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    grid.Build(viewProj, view, nullptr, 0);
    REQUIRE(grid.IndexCount() == 0);
}

TEST_CASE("ClusterGrid assigns light to correct clusters", "[cluster]")
{
    ClusterGrid grid;
    grid.Resize(1280, 720, 0.1f, 500.0f);

    XMMATRIX view = XMMatrixLookToRH(XMVectorSet(0, 2, -10, 0), XMVectorSet(0, 0, 1, 0), XMVectorSet(0, 1, 0, 0));
    XMMATRIX proj = XMMatrixPerspectiveFovRH(1.0f, 16.0f / 9.0f, 0.1f, 500.0f);
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    GpuLight light{};
    light.Position = XMFLOAT3(0.0f, 1.0f, 5.0f);
    light.Radius = 10.0f;
    light.Color = XMFLOAT3(1.0f, 1.0f, 1.0f);
    light.Intensity = 1.0f;

    grid.Build(viewProj, view, &light, 1);
    REQUIRE(grid.IndexCount() > 0);

    bool foundLight = false;
    for (uint32_t i = 0; i < grid.IndexCount(); ++i)
    {
        if (grid.Indices()[i] == 0)
        {
            foundLight = true;
            break;
        }
    }
    REQUIRE(foundLight);
}

TEST_CASE("ClusterGrid BuildConstants produces valid values", "[cluster]")
{
    ClusterGrid grid;
    grid.Resize(1280, 720, 0.1f, 100.0f);

    ClusterConstants constants = grid.BuildConstants();
    REQUIRE(constants.NearZ == 0.1f);
    REQUIRE(constants.FarZ == 100.0f);
    REQUIRE(constants.InvLogDepth > 0.0f);
    REQUIRE(constants.ClusterCountZ > 0.0f);
}