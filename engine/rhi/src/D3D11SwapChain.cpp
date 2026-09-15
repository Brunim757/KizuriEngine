#include "D3D11Internal.h"

namespace kizuri::rhi {

D3D11SwapChain::D3D11SwapChain(ID3D11Device* device, ID3D11DeviceContext* context,
    IDXGIFactory2* factory, HWND hwnd, uint32_t w, uint32_t h)
    : device_(device)
    , context_(context)
    , factory_(factory)
    , hwnd_(hwnd)
    , width_(w)
    , height_(h)
{
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = w;
    desc.Height = h;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 0;

    HRESULT hr = factory_->CreateSwapChainForHwnd(
        device, hwnd, &desc, nullptr, nullptr, &swapChain_);
    if (SUCCEEDED(hr) && swapChain_ != nullptr && width_ > 0 && height_ > 0)
    {
        RecreateBuffers(width_, height_);
    }
}

void D3D11SwapChain::RecreateBuffers(uint32_t w, uint32_t h)
{
    if (backBuffer_ != nullptr)
    {
        delete backBuffer_;
        backBuffer_ = nullptr;
    }
    if (depthBuffer_ != nullptr)
    {
        delete depthBuffer_;
        depthBuffer_ = nullptr;
    }

    ComPtr<ID3D11Texture2D> colorBuffer;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&colorBuffer));

    if (SUCCEEDED(hr))
    {
        ComPtr<ID3D11RenderTargetView> rtv;
        hr = device_->CreateRenderTargetView(colorBuffer.Get(), nullptr, &rtv);
        if (SUCCEEDED(hr))
        {
            backBuffer_ = new D3D11Texture(device_.Get(), colorBuffer.Get(), rtv.Get());
        }
    }

    TextureDesc depth;
    depth.Width = w;
    depth.Height = h;
    depth.Format = TextureFormat::Depth32Float;
    depth.DepthStencilView = true;
    depthBuffer_ = new D3D11Texture(device_.Get(), depth, nullptr, 0);
}

void D3D11SwapChain::Resize(uint32_t w, uint32_t h)
{
    if (w == 0 || h == 0)
    {
        return;
    }
    if (backBuffer_ != nullptr)
    {
        delete backBuffer_;
        backBuffer_ = nullptr;
    }
    if (depthBuffer_ != nullptr)
    {
        delete depthBuffer_;
        depthBuffer_ = nullptr;
    }
    swapChain_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    width_ = w;
    height_ = h;
    RecreateBuffers(w, h);
}

void D3D11SwapChain::Present(bool vsync)
{
    swapChain_->Present(vsync ? 1 : 0, 0);
}

ITexture* D3D11SwapChain::BackBuffer()
{
    return backBuffer_;
}

ITexture* D3D11SwapChain::DepthBuffer()
{
    return depthBuffer_;
}

void D3D11SwapChain::Release()
{
    delete this;
}

} // namespace kizuri::rhi