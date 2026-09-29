#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/RHI.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
namespace Kizuri {
using Microsoft::WRL::ComPtr;
class D3D11RHI : public IRHI {
public:
  D3D11RHI()
    : hwnd(nullptr)
    , w(0)
    , h(0) {
  }
  bool Initialize(const RHIDesc& desc) override {
    hwnd = static_cast<HWND>(desc.windowHandle);
    w = desc.width;
    h = desc.height;
    if (hwnd == nullptr || w <= 0 || h <= 0) {
      return false;
    }
    DXGI_SWAP_CHAIN_DESC scd;
    scd.BufferDesc.Width = static_cast<UINT>(w);
    scd.BufferDesc.Height = static_cast<UINT>(h);
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 1;
    scd.OutputWindow = hwnd;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.Flags = 0;
    D3D_FEATURE_LEVEL requested[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr,
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,
      0,
      requested,
      4,
      D3D11_SDK_VERSION,
      &scd,
      swapchain.GetAddressOf(),
      device.GetAddressOf(),
      &obtained,
      context.GetAddressOf());
    if (FAILED(hr)) {
      hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_WARP,
        nullptr,
        0,
        requested,
        4,
        D3D11_SDK_VERSION,
        &scd,
        swapchain.GetAddressOf(),
        device.GetAddressOf(),
        &obtained,
        context.GetAddressOf());
      if (FAILED(hr)) {
        return false;
      }
    }
    if (!CreateTarget()) {
      return false;
    }
    return true;
  }
  void Shutdown() override {
    target.Reset();
    context.Reset();
    device.Reset();
    swapchain.Reset();
    hwnd = nullptr;
    w = 0;
    h = 0;
  }
  bool Resize(int nw, int nh) override {
    if (nw <= 0 || nh <= 0) {
      return false;
    }
    if (swapchain == nullptr) {
      return false;
    }
    context->OMSetRenderTargets(0, nullptr, nullptr);
    target.Reset();
    HRESULT hr = swapchain->ResizeBuffers(1, static_cast<UINT>(nw), static_cast<UINT>(nh), DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) {
      return false;
    }
    w = nw;
    h = nh;
    return CreateTarget();
  }
  void Clear(float r, float g, float b, float a) override {
    if (context == nullptr || target == nullptr) {
      return;
    }
    float c[4] = { r, g, b, a };
    context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
    context->ClearRenderTargetView(target.Get(), c);
  }
  void Present(bool vsync) override {
    if (swapchain == nullptr) {
      return;
    }
    swapchain->Present(vsync ? 1 : 0, 0);
  }
  const char* BackendName() const override {
    return "D3D11";
  }
  int Width() const override {
    return w;
  }
  int Height() const override {
    return h;
  }
private:
  bool CreateTarget() {
    ComPtr<ID3D11Texture2D> back;
    HRESULT hr = swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back.GetAddressOf()));
    if (FAILED(hr)) {
      return false;
    }
    hr = device->CreateRenderTargetView(back.Get(), nullptr, target.GetAddressOf());
    if (FAILED(hr)) {
      return false;
    }
    context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<FLOAT>(w);
    vp.Height = static_cast<FLOAT>(h);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
    return true;
  }
  HWND hwnd;
  int w;
  int h;
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<IDXGISwapChain> swapchain;
  ComPtr<ID3D11RenderTargetView> target;
};
IRHI* CreateD3D11RHI() {
  return new D3D11RHI();
}
}
