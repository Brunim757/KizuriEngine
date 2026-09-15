#pragma once

#include <DirectXMath.h>
#include <cstdint>

#include "kizuri/core/platform/Input.h"

namespace kizuri::renderer {

using namespace DirectX;

class CameraController {
public:
    virtual ~CameraController() = default;
    virtual void Update(const core::Input& input, float deltaSeconds) = 0;

    XMMATRIX ViewMatrix() const;
    XMMATRIX ProjectionMatrix(float fovRadians, float aspectRatio, float nearZ, float farZ) const;
    XMVECTOR Position() const;
    XMFLOAT3 PositionFloat() const { return position_; }

    void Teleport(XMFLOAT3 position) { position_ = position; }

protected:
    XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
};

class FreeCameraController : public CameraController {
public:
    FreeCameraController();

    void Update(const core::Input& input, float deltaSeconds) override;

    void SetMinHeight(float height) { minHeight_ = height; }
    void SetSpeed(float metersPerSecond) { moveSpeed_ = metersPerSecond; }

private:
    void CaptureMouseLook(const core::Input& input, float deltaSeconds);
    void ApplyMovement(const core::Input& input, float deltaSeconds);

    float moveSpeed_ = 8.0f;
    float lookSensitivity_ = 0.0022f;
    float minHeight_ = 0.3f;
    int32_t lastMouseX_ = 0;
    int32_t lastMouseY_ = 0;
    bool firstCapture_ = true;
};

class ThirdPersonCameraController : public CameraController {
public:
    ThirdPersonCameraController();

    void Update(const core::Input& input, float deltaSeconds) override;

    XMFLOAT3 Target() const { return target_; }
    void SetTarget(XMFLOAT3 target) { target_ = target; }
    void SetMinHeight(float height) { minHeight_ = height; }

private:
    void Orbit(const core::Input& input, float deltaSeconds);
    void MoveTarget(const core::Input& input, float deltaSeconds);
    void Clamp();

    XMFLOAT3 target_{ 0.0f, 0.0f, 0.0f };
    float distance_ = 8.0f;
    float minDistance_ = 1.5f;
    float maxDistance_ = 40.0f;
    float orbitSensitivity_ = 0.0022f;
    float moveSpeed_ = 6.0f;
    float minHeight_ = 0.3f;
    int32_t lastMouseX_ = 0;
    int32_t lastMouseY_ = 0;
    bool firstCapture_ = true;
};

} // namespace kizuri::renderer