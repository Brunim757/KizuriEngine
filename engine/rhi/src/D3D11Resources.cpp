#include "D3D11Internal.h"

#include <cstring>

#include "kizuri/core/diagnostics/Log.h"

namespace kizuri::rhi {

D3D11Buffer::D3D11Buffer(ID3D11Device* device, const BufferDesc& desc, const void* data)
    : size_(desc.ByteSize)
    , stride_(desc.StrideBytes)
    , constant_(desc.ConstantBuffer)
    , structured_(desc.StructuredBuffer)
    , dynamic_(desc.CpuWritable || desc.ConstantBuffer || desc.StructuredBuffer)
{
    ID3D11DeviceContext* context = nullptr;
    device->GetImmediateContext(&context);
    context_ = context;
    context->Release();

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = size_;
    bufferDesc.Usage = ToUsage(desc);
    bufferDesc.BindFlags = ToBindFlags(desc);
    bufferDesc.CPUAccessFlags = ToCpuAccess(desc);
    if (structured_)
    {
        bufferDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
        bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bufferDesc.StructureByteStride = desc.ElementBytes;
    }

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = data;

    if (FAILED(device->CreateBuffer(&bufferDesc, data != nullptr ? &initData : nullptr, &buffer_)))
    {
        return;
    }

    if (structured_)
    {
        DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
        switch (desc.ElementBytes)
        {
            case 4: srvFormat = DXGI_FORMAT_R32_UINT; break;
            case 8: srvFormat = DXGI_FORMAT_R32G32_UINT; break;
            case 16: srvFormat = DXGI_FORMAT_R32G32B32A32_UINT; break;
            default: break;
        }
        if (srvFormat != DXGI_FORMAT_UNKNOWN)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = srvFormat;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
            srvDesc.BufferEx.FirstElement = 0;
            srvDesc.BufferEx.NumElements = size_ / desc.ElementBytes;
            device->CreateShaderResourceView(buffer_.Get(), &srvDesc, &srv_);
        }
    }
}

void D3D11Buffer::Update(const void* data, uint32_t size)
{
    if (!dynamic_ || data == nullptr)
    {
        return;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context_->Map(buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        size_t copyBytes = size < size_ ? size : size_;
        std::memcpy(mapped.pData, data, copyBytes);
        context_->Unmap(buffer_.Get(), 0);
        mapped_ = false;
    }
}

void* D3D11Buffer::MapForWrite()
{
    if (!dynamic_ || mapped_)
    {
        return nullptr;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return nullptr;
    }
    mapped_ = true;
    return mapped.pData;
}

void D3D11Buffer::Unmap()
{
    if (!mapped_)
    {
        return;
    }
    context_->Unmap(buffer_.Get(), 0);
    mapped_ = false;
}

void D3D11Buffer::Release()
{
    delete this;
}

ID3D11Buffer* D3D11Buffer::Native()
{
    return buffer_.Get();
}

void* D3D11Buffer::GetSrvHandle()
{
    return srv_.Get();
}

ID3D11ShaderResourceView* D3D11Buffer::Srv()
{
    return srv_.Get();
}

D3D11Texture::D3D11Texture(ID3D11Device* device, const TextureDesc& desc, const void* data, uint32_t rowBytes)
    : width_(desc.Width)
    , height_(desc.Height)
{
    D3D11_TEXTURE2D_DESC textureDesc{};
    textureDesc.Width = desc.Width;
    textureDesc.Height = desc.Height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = ToDxgiFormat(desc.Format);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = 0;
    if (desc.ShaderResourceView)
    {
        textureDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }
    if (desc.DepthStencilView)
    {
        textureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
    }

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = data;
    initData.SysMemPitch = rowBytes;

    if (FAILED(device->CreateTexture2D(&textureDesc, data != nullptr ? &initData : nullptr, &texture_)))
    {
        return;
    }

    if (desc.DepthStencilView)
    {
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = ToDxgiFormat(desc.Format);
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;
        device->CreateDepthStencilView(texture_.Get(), &dsvDesc, &dsv_);
        return;
    }

    if (desc.ShaderResourceView)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Format = ToDxgiFormat(desc.Format);
        device->CreateShaderResourceView(texture_.Get(), &srvDesc, &srv_);
    }
}

D3D11Texture::D3D11Texture(ID3D11Device* device, ID3D11Texture2D* external, ID3D11RenderTargetView* rtv)
    : texture_(external)
    , rtv_(rtv)
{
    D3D11_TEXTURE2D_DESC desc;
    external->GetDesc(&desc);
    width_ = desc.Width;
    height_ = desc.Height;
}

uint32_t D3D11Texture::GetWidth()
{
    return width_;
}

uint32_t D3D11Texture::GetHeight()
{
    return height_;
}

void D3D11Texture::Release()
{
    delete this;
}

ID3D11Resource* D3D11Texture::Resource()
{
    return texture_.Get();
}

ID3D11RenderTargetView* D3D11Texture::Rtv()
{
    return rtv_.Get();
}

ID3D11DepthStencilView* D3D11Texture::Dsv()
{
    return dsv_.Get();
}

ID3D11ShaderResourceView* D3D11Texture::Srv()
{
    return srv_.Get();
}

D3D11Shader::D3D11Shader(ShaderStage stage, const char* source, const char* entryPoint)
    : stage_(stage)
    , source_(source != nullptr ? source : "")
    , entryPoint_(entryPoint != nullptr ? entryPoint : "main")
{
}

bool D3D11Shader::Compile()
{
    const char* profile = stage_ == ShaderStage::Vertex ? "vs_5_0" : "ps_5_0";
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompile(
        source_.c_str(),
        source_.size(),
        nullptr,
        nullptr,
        nullptr,
        entryPoint_.c_str(),
        profile,
        flags,
        0,
        &blob,
        &errors);

    if (FAILED(hr))
    {
        if (errors != nullptr && errors->GetBufferSize() > 0)
        {
            core::Log::ErrorFormatted("KizuriShader %s failed: %s",
                profile, static_cast<const char*>(errors->GetBufferPointer()));
        }
        valid_ = false;
        bytecode_.clear();
        return false;
    }

    bytecode_.assign(
        static_cast<const uint8_t*>(blob->GetBufferPointer()),
        static_cast<const uint8_t*>(blob->GetBufferPointer()) + blob->GetBufferSize());
    valid_ = true;
    return true;
}

bool D3D11Shader::CreateGpu(ID3D11Device* device)
{
    if (!valid_)
    {
        return false;
    }
    HRESULT hr = S_OK;
    if (stage_ == ShaderStage::Vertex)
    {
        hr = device->CreateVertexShader(bytecode_.data(), bytecode_.size(), nullptr, &vs_);
    }
    else
    {
        hr = device->CreatePixelShader(bytecode_.data(), bytecode_.size(), nullptr, &ps_);
    }
    valid_ = SUCCEEDED(hr);
    return valid_;
}

void D3D11Shader::Release()
{
    delete this;
}

ID3D11VertexShader* D3D11Shader::Vs()
{
    return vs_.Get();
}

ID3D11PixelShader* D3D11Shader::Ps()
{
    return ps_.Get();
}

D3D11Pipeline::D3D11Pipeline(ID3D11Device* device, const PipelineDesc& desc, IShader* vs, IShader* ps)
{
    D3D11Shader* vertex = dynamic_cast<D3D11Shader*>(vs);
    D3D11Shader* pixel = dynamic_cast<D3D11Shader*>(ps);
    vs_ = vertex != nullptr && vertex->IsValid() ? vertex->Vs() : nullptr;
    ps_ = pixel != nullptr && pixel->IsValid() ? pixel->Ps() : nullptr;

    if (vertex != nullptr && vertex->IsValid() && desc.Elements != nullptr && desc.ElementCount > 0)
    {
        D3D11_INPUT_ELEMENT_DESC elements[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
        uint32_t elementCount = desc.ElementCount;
        if (elementCount > D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT)
        {
            elementCount = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
        }
        for (uint32_t i = 0; i < elementCount; ++i)
        {
            elements[i].SemanticName = desc.Elements[i].SemanticName;
            elements[i].SemanticIndex = desc.Elements[i].SemanticIndex;
            elements[i].Format = ToDxgiFormat(desc.Elements[i].Format);
            elements[i].InputSlot = 0;
            elements[i].AlignedByteOffset = desc.Elements[i].Offset;
            elements[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            elements[i].InstanceDataStepRate = 0;
        }
        device->CreateInputLayout(elements, elementCount,
            vertex->Bytecode().data(), vertex->Bytecode().size(), &inputLayout_);
    }

    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = desc.CullBack ? D3D11_CULL_BACK : D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    device->CreateRasterizerState(&rasterizerDesc, &rasterizer_);

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = desc.DepthTest;
    depthDesc.DepthWriteMask = desc.DepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;
    device->CreateDepthStencilState(&depthDesc, &depthState_);

    switch (desc.Topology)
    {
        case 0: topology_ = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
        case 1: topology_ = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
        case 2: topology_ = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
        case 3: topology_ = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
        default: topology_ = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    }
}

void D3D11Pipeline::Apply(ID3D11DeviceContext* context)
{
    if (vs_ == nullptr)
    {
        return;
    }
    context->IASetInputLayout(inputLayout_.Get());
    context->IASetPrimitiveTopology(topology_);
    context->VSSetShader(vs_, nullptr, 0);
    context->PSSetShader(ps_, nullptr, 0);
    context->RSSetState(rasterizer_.Get());
    context->OMSetDepthStencilState(depthState_.Get(), 0);
}

void D3D11Pipeline::Release()
{
    delete this;
}

} // namespace kizuri::rhi