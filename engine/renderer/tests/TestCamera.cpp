#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "kizuri/renderer/Camera.h"

using namespace kizuri;
using namespace kizuri::renderer;

TEST_CASE("FreeCameraController stays above minHeight", "[camera]")
{
    FreeCameraController cam;
    cam.SetMinHeight(0.5f);
    cam.Teleport(XMFLOAT3(0.0f, -2.0f, 0.0f));
    core::Input input;
    input.BeginFrame();
    cam.Update(input, 0.016f);
    REQUIRE(cam.PositionFloat().y >= 0.49f);
}

TEST_CASE("FreeCameraController view matrix is valid", "[camera]")
{
    FreeCameraController cam;
    cam.Teleport(XMFLOAT3(0.0f, 2.0f, -6.0f));
    XMMATRIX view = cam.ViewMatrix();
    XMVECTOR det = XMMatrixDeterminant(view);
    REQUIRE(std::fabsf(XMVectorGetX(det)) > 1e-3f);
}

TEST_CASE("ThirdPersonCameraController orbits target", "[camera]")
{
    ThirdPersonCameraController cam;
    cam.SetTarget(XMFLOAT3(0.0f, 0.0f, 0.0f));
    cam.Teleport(XMFLOAT3(0.0f, 5.0f, -5.0f));
    XMFLOAT3 pos = cam.PositionFloat();
    float dist = std::sqrtf(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
    REQUIRE(dist > 0.5f);
}