#include "D3D11Internal.h"

#ifdef _DEBUG
extern "C" HRESULT WINAPI DXGIGetDebugInterface1(UINT flags, REFIID riid, void** ppv);
#endif

namespace kizuri::rhi {

DXGI_FORMAT ToDxgiFormat(VertexFormat format)
{
    switch (format)
    {
        case VertexFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
        case VertexFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    return DXGI_FORMAT_R32G32B32_FLOAT;
}

DXGI_FORMAT ToDxgiFormat(TextureFormat format)
{
    switch (format)
    {
        case TextureFormat::Rgba8Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::Depth32Float: return DXGI_FORMAT_D32_FLOAT;
    }
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

D3D11_USAGE ToUsage(const BufferDesc& desc)
{
    if (desc.CpuWritable || desc.ConstantBuffer)
    {
        return D3D11_USAGE_DYNAMIC;
    }
    return D3D11_USAGE_DEFAULT;
}

UINT ToBindFlags(const BufferDesc& desc)
{
    UINT flags = 0;
    if (desc.VertexBuffer)
    {
        flags |= D3D11_BIND_VERTEX_BUFFER;
    }
    if (desc.IndexBuffer)
    {
        flags |= D3D11_BIND_INDEX_BUFFER;
    }
    if (desc.ConstantBuffer)
    {
        flags |= D3D11_BIND_CONSTANT_BUFFER;
    }
    return flags;
}

UINT ToCpuAccess(const BufferDesc& desc)
{
    if (desc.CpuWritable || desc.ConstantBuffer)
    {
        return D3D11_CPU_ACCESS_WRITE;
    }
    return 0;
}

D3D11Device::D3D11Device() = default;

D3D11Device::~D3D11Device() = default;

bool D3D11Device::Initialize()
{
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory_));
    if (FAILED(hr))
    {
        return false;
    }

    IDXGIAdapter1* adapter = nullptr;
    if (SUCCEEDED(factory_->EnumAdapters1(0, &adapter)))
    {
        adapter_ = adapter;
    }

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

    hr = D3D11CreateDevice(
        adapter_.Get(),
        adapter_ ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels,
        1,
        D3D11_SDK_VERSION,
        &device_,
        nullptr,
        &context_);

    if (FAILED(hr))
    {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags,
            levels,
            1,
            D3D11_SDK_VERSION,
            &device_,
            nullptr,
            &context_);
    }

    if (FAILED(hr))
    {
        return false;
    }

    initialized_ = true;
    return true;
}

void* D3D11Device::GetNativeDevice()
{
    return device_.Get();
}

void D3D11Device::GetAdapterName(wchar_t* out, uint32_t maxChars)
{
    if (out == nullptr || maxChars == 0)
    {
        return;
    }
    out[0] = L'\0';
    if (adapter_ != nullptr)
    {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(adapter_->GetDesc(&desc)))
        {
            wcsncpy(out, desc.Description, maxChars - 1);
            out[maxChars - 1] = L'\0';
        }
    }
}

ISwapChain* D3D11Device::CreateSwapChain(void* hwnd, uint32_t w, uint32_t h)
{
    return new D3D11SwapChain(device_.Get(), context_.Get(), factory_.Get(),
        static_cast<HWND>(hwnd), w, h);
}

IBuffer* D3D11Device::CreateBuffer(const BufferDesc& desc, const void* data)
{
    return new D3D11Buffer(device_.Get(), desc, data);
}

ITexture* D3D11Device::CreateTexture(const TextureDesc& desc, const void* data, uint32_t rowBytes)
{
    return new D3D11Texture(device_.Get(), desc, data, rowBytes);
}

IShader* D3D11Device::CreateShader(ShaderStage stage, const char* source, const char* entryPoint)
{
    D3D11Shader* shader = new D3D11Shader(stage, source, entryPoint);
    shader->Compile();
    if (shader->IsValid())
    {
        shader->CreateGpu(device_.Get());
    }
    return shader;
}

IPipeline* D3D11Device::CreatePipeline(const PipelineDesc& desc, IShader* vs, IShader* ps)
{
    return new D3D11Pipeline(device_.Get(), desc, vs, ps);
}

ICommandList* D3D11Device::CreateCommandList()
{
    return new D3D11CommandList(context_.Get(), device_.Get());
}

void D3D11Device::ReportLiveObjects()
{
#ifdef _DEBUG
    ComPtr<IDXGIDebug> debugLayer;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugLayer))))
    {
        debugLayer->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
    }
#endif
}

void D3D11Device::Release()
{
    delete this;
}

ID3D11Device* D3D11Device::Device()
{
    return device_.Get();
}

ID3D11DeviceContext* D3D11Device::Context()
{
    return context_.Get();
}

} // namespace kizuri::rhi