#pragma once

#include <cstdint>

namespace kizuri::rhi {

enum class ShaderStage : uint8_t { Vertex, Pixel };

enum class VertexFormat : uint8_t { Float2, Float3, Float4 };

enum class TextureFormat : uint8_t { Rgba8Unorm, Depth32Float };

struct VertexElement
{
    const char* SemanticName = nullptr;
    uint8_t SemanticIndex = 0;
    VertexFormat Format = VertexFormat::Float3;
    uint32_t Offset = 0;
};

struct BufferDesc
{
    uint32_t ByteSize = 0;
    uint32_t StrideBytes = 0;
    bool VertexBuffer = false;
    bool IndexBuffer = false;
    bool ConstantBuffer = false;
    bool StructuredBuffer = false;
    uint32_t ElementBytes = 0;
    bool CpuWritable = false;
};

struct TextureDesc
{
    uint32_t Width = 0;
    uint32_t Height = 0;
    TextureFormat Format = TextureFormat::Rgba8Unorm;
    bool ShaderResourceView = false;
    bool DepthStencilView = false;
};

struct PipelineDesc
{
    const VertexElement* Elements = nullptr;
    uint32_t ElementCount = 0;
    uint32_t VertexStride = 0;
    uint32_t Topology = 4;
    bool DepthTest = true;
    bool DepthWrite = true;
    bool DepthWriteMaskAll = true;
    bool CullBack = true;
};

class IObject {
public:
    virtual ~IObject() = default;
    virtual void Release() = 0;
};

class IDevice : public IObject {
public:
    virtual void* GetNativeDevice() = 0;
    virtual void GetAdapterName(wchar_t* out, uint32_t maxChars) = 0;
    virtual class ISwapChain* CreateSwapChain(void* hwnd, uint32_t w, uint32_t h) = 0;
    virtual class IBuffer* CreateBuffer(const BufferDesc& desc, const void* data) = 0;
    virtual class ITexture* CreateTexture(const TextureDesc& desc, const void* data, uint32_t rowBytes) = 0;
    virtual class IShader* CreateShader(ShaderStage stage, const char* source, const char* entryPoint) = 0;
    virtual class IPipeline* CreatePipeline(const PipelineDesc& desc, class IShader* vs, class IShader* ps) = 0;
    virtual class ICommandList* CreateCommandList() = 0;
    virtual void ReportLiveObjects() = 0;
};

class ISwapChain : public IObject {
public:
    virtual void Resize(uint32_t w, uint32_t h) = 0;
    virtual void Present(bool vsync) = 0;
    virtual ITexture* BackBuffer() = 0;
    virtual ITexture* DepthBuffer() = 0;
};

class IBuffer : public IObject {
public:
    virtual void Update(const void* data, uint32_t size) = 0;
    virtual void* MapForWrite() = 0;
    virtual void Unmap() = 0;
    virtual void* GetSrvHandle() = 0;
};

class ITexture : public IObject {
public:
    virtual uint32_t GetWidth() = 0;
    virtual uint32_t GetHeight() = 0;
};

class IShader : public IObject {
public:
    virtual bool IsValid() const = 0;
    virtual const void* GetBytecode() const = 0;
    virtual size_t GetBytecodeSize() const = 0;
};

class IPipeline : public IObject {};

class ICommandList : public IObject {
public:
    virtual void ClearRenderTarget(ITexture* rt, float r, float g, float b, float a) = 0;
    virtual void ClearDepth(ITexture* dt, float v) = 0;
    virtual void SetViewports(uint32_t w, uint32_t h) = 0;
    virtual void SetPipeline(IPipeline* p) = 0;
    virtual void SetRenderTarget(ITexture* color, ITexture* depth) = 0;
    virtual void BindCB(IBuffer* cb, uint32_t slot) = 0;
    virtual void BindSRVBuffer(IBuffer* buffer, uint32_t slot) = 0;
    virtual void BindSampler(uint32_t slot) = 0;
    virtual void BindSRV(ITexture* tex, uint32_t slot) = 0;
    virtual void SetVertexBuffer(IBuffer* vb) = 0;
    virtual void SetIndexBuffer(IBuffer* ib) = 0;
    virtual void DrawIndexed(uint32_t indexCount, uint32_t indexOffset, int32_t vertexOffset) = 0;
    virtual void Submit(ISwapChain* swap, bool vsync) = 0;
};

class RhiFactory {
public:
    static IDevice* CreateDevice();
};

} // namespace kizuri::rhi