#include "D3D11Internal.h"

namespace kizuri::rhi {

D3D11CommandList::D3D11CommandList(ID3D11DeviceContext* context, ID3D11Device* device)
    : context_(context)
    , device_(device)
{
}

void D3D11CommandList::ClearRenderTarget(ITexture* rt, float r, float g, float b, float a)
{
    D3D11Texture* tex = dynamic_cast<D3D11Texture*>(rt);
    if (tex != nullptr && tex->Rtv() != nullptr)
    {
        float color[4] = { r, g, b, a };
        context_->ClearRenderTargetView(tex->Rtv(), color);
    }
}

void D3D11CommandList::ClearDepth(ITexture* dt, float v)
{
    D3D11Texture* tex = dynamic_cast<D3D11Texture*>(dt);
    if (tex != nullptr && tex->Dsv() != nullptr)
    {
        context_->ClearDepthStencilView(tex->Dsv(), D3D11_CLEAR_DEPTH, v, 0);
    }
}

void D3D11CommandList::SetViewports(uint32_t w, uint32_t h)
{
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(w);
    viewport.Height = static_cast<float>(h);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);
}

void D3D11CommandList::SetPipeline(IPipeline* p)
{
    D3D11Pipeline* pipeline = dynamic_cast<D3D11Pipeline*>(p);
    if (pipeline != nullptr)
    {
        pipeline->Apply(context_);
    }
}

void D3D11CommandList::SetRenderTarget(ITexture* color, ITexture* depth)
{
    D3D11Texture* colorTex = dynamic_cast<D3D11Texture*>(color);
    D3D11Texture* depthTex = dynamic_cast<D3D11Texture*>(depth);
    ID3D11RenderTargetView* rtv = colorTex != nullptr ? colorTex->Rtv() : nullptr;
    ID3D11DepthStencilView* dsv = depthTex != nullptr ? depthTex->Dsv() : nullptr;
    context_->OMSetRenderTargets(rtv != nullptr ? 1 : 0, &rtv, dsv);
}

void D3D11CommandList::BindCB(IBuffer* cb, uint32_t slot)
{
    D3D11Buffer* buffer = dynamic_cast<D3D11Buffer*>(cb);
    if (buffer == nullptr)
    {
        return;
    }
    ID3D11Buffer* native = buffer->Native();
    context_->VSSetConstantBuffers(slot, 1, &native);
    context_->PSSetConstantBuffers(slot, 1, &native);
}

void D3D11CommandList::BindSRVBuffer(IBuffer* cb, uint32_t slot)
{
    D3D11Buffer* buffer = dynamic_cast<D3D11Buffer*>(cb);
    if (buffer == nullptr || buffer->Srv() == nullptr)
    {
        return;
    }
    ID3D11ShaderResourceView* srv = buffer->Srv();
    context_->PSSetShaderResources(slot, 1, &srv);
}

void D3D11CommandList::BindSampler(uint32_t slot)
{
    if (sampler_ == nullptr)
    {
        D3D11_SAMPLER_DESC desc{};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        desc.MinLOD = 0;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        device_->CreateSamplerState(&desc, &sampler_);
    }
    if (sampler_ != nullptr)
    {
        ID3D11SamplerState* s = sampler_.Get();
        context_->PSSetSamplers(slot, 1, &s);
    }
}

void D3D11CommandList::BindSRV(ITexture* tex, uint32_t slot)
{
    D3D11Texture* texture = dynamic_cast<D3D11Texture*>(tex);
    if (texture == nullptr)
    {
        return;
    }
    if (texture->Srv() == nullptr)
    {
        return;
    }
    ID3D11ShaderResourceView* srv = texture->Srv();
    context_->PSSetShaderResources(slot, 1, &srv);
}

void D3D11CommandList::SetVertexBuffer(IBuffer* vb)
{
    D3D11Buffer* buffer = dynamic_cast<D3D11Buffer*>(vb);
    if (buffer == nullptr)
    {
        return;
    }
    UINT stride = buffer->Stride();
    UINT offset = 0;
    ID3D11Buffer* native = buffer->Native();
    context_->IASetVertexBuffers(0, 1, &native, &stride, &offset);
}

void D3D11CommandList::SetIndexBuffer(IBuffer* ib)
{
    D3D11Buffer* buffer = dynamic_cast<D3D11Buffer*>(ib);
    if (buffer == nullptr)
    {
        return;
    }
    context_->IASetIndexBuffer(buffer->Native(), DXGI_FORMAT_R32_UINT, 0);
}

void D3D11CommandList::DrawIndexed(uint32_t indexCount, uint32_t indexOffset, int32_t vertexOffset)
{
    context_->DrawIndexed(indexCount, indexOffset, vertexOffset);
}

void D3D11CommandList::Submit(ISwapChain* swap, bool vsync)
{
    D3D11SwapChain* swapChain = dynamic_cast<D3D11SwapChain*>(swap);
    if (swapChain != nullptr)
    {
        swapChain->Present(vsync);
    }
}

void D3D11CommandList::Release()
{
    delete this;
}

} // namespace kizuri::rhi