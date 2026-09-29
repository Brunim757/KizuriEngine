#pragma once
#include <DirectXMath.h>
namespace Kizuri {
class FreeCamera {
public:
  FreeCamera();
  void SetPosition(float x, float y, float z);
  void SetYawPitch(float yaw, float pitch);
  void GetPosition(float& x, float& y, float& z) const;
  void Update(float dt, bool fwd, bool back, bool left, bool right, bool up, bool down, float mouseDX, float mouseDY);
  DirectX::XMMATRIX View() const;
  DirectX::XMMATRIX Projection(float aspect) const;
  float moveSpeed;
  float lookSpeed;
  float fovY;
  float nearZ;
  float farZ;
private:
  float px;
  float py;
  float pz;
  float yaw;
  float pitch;
};
}
