#include "kizuri/renderer/Camera.h"

namespace kizuri::renderer {

namespace {

constexpr float Pi = 3.14159265358979323846f;

XMVECTOR ForwardDirection(float yaw, float pitch)
{
    float cp = cosf(pitch);
    return XMVectorSet(sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp, 0.0f);
}

XMVECTOR RightDirection(float yaw)
{
    return XMVectorSet(cosf(yaw), 0.0f, -sinf(yaw), 0.0f);
}

void ClampAngle(float& angle, float low, float high)
{
    if (angle < low)
    {
        angle = low;
    }
    if (angle > high)
    {
        angle = high;
    }
}

} // namespace

XMMATRIX CameraController::ViewMatrix() const
{
    XMVECTOR eye = XMLoadFloat3(&position_);
    XMVECTOR forward = ForwardDirection(yaw_, pitch_);
    return XMMatrixLookToRH(eye, forward, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
}

XMMATRIX CameraController::ProjectionMatrix(float fovRadians, float aspectRatio, float nearZ, float farZ) const
{
    return XMMatrixPerspectiveFovRH(fovRadians, aspectRatio, nearZ, farZ);
}

XMVECTOR CameraController::Position() const
{
    return XMLoadFloat3(&position_);
}

FreeCameraController::FreeCameraController()
{
    pitch_ = -0.2f;
    position_ = XMFLOAT3(0.0f, 2.0f, -6.0f);
}

void FreeCameraController::CaptureMouseLook(const core::Input& input, float deltaSeconds)
{
    (void)deltaSeconds;
    if (!input.IsDown(core::Key::MouseRight))
    {
        firstCapture_ = true;
        lastMouseX_ = input.MouseX();
        lastMouseY_ = input.MouseY();
        return;
    }

    if (firstCapture_)
    {
        firstCapture_ = false;
        lastMouseX_ = input.MouseX();
        lastMouseY_ = input.MouseY();
        return;
    }

    float dx = static_cast<float>(input.MouseX() - lastMouseX_);
    float dy = static_cast<float>(input.MouseY() - lastMouseY_);
    yaw_ += dx * lookSensitivity_;
    pitch_ += dy * lookSensitivity_;
    ClampAngle(pitch_, -1.55f, 1.55f);
    lastMouseX_ = input.MouseX();
    lastMouseY_ = input.MouseY();
}

void FreeCameraController::ApplyMovement(const core::Input& input, float deltaSeconds)
{
    XMVECTOR forward = ForwardDirection(yaw_, 0.0f);
    XMVECTOR right = RightDirection(yaw_);
    XMVECTOR move = XMVectorZero();

    if (input.IsDown(core::Key::W))
    {
        move += forward;
    }
    if (input.IsDown(core::Key::S))
    {
        move -= forward;
    }
    if (input.IsDown(core::Key::D))
    {
        move += right;
    }
    if (input.IsDown(core::Key::A))
    {
        move -= right;
    }

    XMFLOAT3 motion;
    XMStoreFloat3(&motion, move);
    position_.x += motion.x * moveSpeed_ * deltaSeconds;
    position_.y += motion.y * moveSpeed_ * deltaSeconds;
    position_.z += motion.z * moveSpeed_ * deltaSeconds;

    if (position_.y < minHeight_)
    {
        position_.y = minHeight_;
    }
}

void FreeCameraController::Update(const core::Input& input, float deltaSeconds)
{
    CaptureMouseLook(input, deltaSeconds);
    ApplyMovement(input, deltaSeconds);
}

ThirdPersonCameraController::ThirdPersonCameraController()
{
    yaw_ = 0.6f;
    pitch_ = -0.35f;
}

void ThirdPersonCameraController::Orbit(const core::Input& input, float deltaSeconds)
{
    (void)deltaSeconds;
    if (!input.IsDown(core::Key::MouseLeft))
    {
        firstCapture_ = true;
        lastMouseX_ = input.MouseX();
        lastMouseY_ = input.MouseY();
        return;
    }

    if (firstCapture_)
    {
        firstCapture_ = false;
        lastMouseX_ = input.MouseX();
        lastMouseY_ = input.MouseY();
        return;
    }

    float dx = static_cast<float>(input.MouseX() - lastMouseX_);
    float dy = static_cast<float>(input.MouseY() - lastMouseY_);
    yaw_ += dx * orbitSensitivity_;
    pitch_ += dy * orbitSensitivity_;
    ClampAngle(pitch_, -1.4f, -0.08f);
    lastMouseX_ = input.MouseX();
    lastMouseY_ = input.MouseY();
}

void ThirdPersonCameraController::MoveTarget(const core::Input& input, float deltaSeconds)
{
    XMVECTOR forward = ForwardDirection(yaw_, 0.0f);
    XMVECTOR right = RightDirection(yaw_);
    XMVECTOR move = XMVectorZero();

    if (input.IsDown(core::Key::W))
    {
        move += forward;
    }
    if (input.IsDown(core::Key::S))
    {
        move -= forward;
    }
    if (input.IsDown(core::Key::D))
    {
        move += right;
    }
    if (input.IsDown(core::Key::A))
    {
        move -= right;
    }

    XMFLOAT3 motion;
    XMStoreFloat3(&motion, move);
    target_.x += motion.x * moveSpeed_ * deltaSeconds;
    target_.y += motion.y * moveSpeed_ * deltaSeconds;
    target_.z += motion.z * moveSpeed_ * deltaSeconds;

    int32_t scroll = input.ScrollDelta();
    if (scroll != 0)
    {
        float factor = scroll > 0 ? 0.9f : 1.1f;
        distance_ *= factor;
    }
}

void ThirdPersonCameraController::Clamp()
{
    if (distance_ < minDistance_)
    {
        distance_ = minDistance_;
    }
    if (distance_ > maxDistance_)
    {
        distance_ = maxDistance_;
    }

    XMVECTOR back = XMVectorNegate(ForwardDirection(yaw_, pitch_));
    XMVectorScale(back, distance_);
    XMFLOAT3 offsetVec;
    XMStoreFloat3(&offsetVec, back);
    position_.x = target_.x + offsetVec.x;
    position_.y = target_.y + offsetVec.y;
    position_.z = target_.z + offsetVec.z;

    if (position_.y < minHeight_)
    {
        position_.y = minHeight_;
    }
}

void ThirdPersonCameraController::Update(const core::Input& input, float deltaSeconds)
{
    Orbit(input, deltaSeconds);
    MoveTarget(input, deltaSeconds);
    Clamp();
}

} // namespace kizuri::renderer