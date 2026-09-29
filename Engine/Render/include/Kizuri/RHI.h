#pragma once
namespace Kizuri {
enum class RHI_API {
  Null,
  D3D11
};
struct RHIDesc {
  void* windowHandle;
  int width;
  int height;
  bool vsync;
};
class IRHI {
public:
  virtual ~IRHI() = default;
  virtual bool Initialize(const RHIDesc& desc) = 0;
  virtual void Shutdown() = 0;
  virtual bool Resize(int w, int h) = 0;
  virtual void Clear(float r, float g, float b, float a) = 0;
  virtual void Present(bool vsync) = 0;
  virtual const char* BackendName() const = 0;
  virtual int Width() const = 0;
  virtual int Height() const = 0;
};
IRHI* CreateRHI(RHI_API api);
void DestroyRHI(IRHI* rhi);
}
