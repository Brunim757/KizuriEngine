#include "Kizuri/RHI.h"
namespace Kizuri {
class NullRHI : public IRHI {
public:
  NullRHI()
    : w(0)
    , h(0)
    , lastR(0.0f)
    , lastG(0.0f)
    , lastB(0.0f)
    , lastA(1.0f) {
  }
  bool Initialize(const RHIDesc& desc) override {
    w = desc.width;
    h = desc.height;
    return w > 0 && h > 0;
  }
  void Shutdown() override {
    w = 0;
    h = 0;
  }
  bool Resize(int nw, int nh) override {
    if (nw <= 0 || nh <= 0) {
      return false;
    }
    w = nw;
    h = nh;
    return true;
  }
  void Clear(float r, float g, float b, float a) override {
    lastR = r;
    lastG = g;
    lastB = b;
    lastA = a;
  }
  void Present(bool vsync) override {
    (void)vsync;
  }
  const char* BackendName() const override {
    return "Null";
  }
  int Width() const override {
    return w;
  }
  int Height() const override {
    return h;
  }
  int w;
  int h;
  float lastR;
  float lastG;
  float lastB;
  float lastA;
};
IRHI* CreateNullRHI() {
  return new NullRHI();
}
}
