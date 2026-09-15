#pragma once

#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <dxgidebug.h>
#include <dxgiformat.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "kizuri/rhi/Rhi.h"

namespace kizuri::rhi {

using Microsoft::WRL::ComPtr;

DXGI_FORMAT ToDxgiFormat(VertexFormat format);
DXGI_FORMAT ToDxgiFormat(TextureFormat format);
D3D11_USAGE ToUsage(const BufferDesc& desc);
UINT ToBindFlags(const BufferDesc& desc);
UINT ToCpuAccess(const BufferDesc& desc);

class D3D11SwapChain : public ISwapChain {
public:
    D3D11SwapChain(ID3D11Device* device, ID3D11DeviceContext* context,
        IDXGIFactory2* factory, HWND hwnd, uint32_t w, uint32_t h);
    void Resize(uint32_t w, uint32_t h) override;
    void Present(bool vsync) override;
    ITexture* BackBuffer() override;
    ITexture* DepthBuffer() override;
    void Release() override;

private:
    void RecreateBuffers(uint32_t w, uint32_t h);

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIFactory2> factory_;
    ComPtr<IDXGISwapChain1> swapChain_;
    HWND hwnd_;
    uint32_t width_;
    uint32_t height_;
    D3D11Texture* backBuffer_ = nullptr;
    D3D11Texture* depthBuffer_ = nullptr;
};

class D3D11Device : public IDevice {
public:
    D3D11Device();
    ~D3D11Device() override;

    bool Initialize();

    void* GetNativeDevice() override;
    void GetAdapterName(wchar_t* out, uint32_t maxChars) override;
    ISwapChain* CreateSwapChain(void* hwnd, uint32_t w, uint32_t h) override;
    IBuffer* CreateBuffer(const BufferDesc& desc, const void* data) override;
    ITexture* CreateTexture(const TextureDesc& desc, const void* data, uint32_t rowBytes) override;
    IShader* CreateShader(ShaderStage stage, const char* source, const char* entryPoint) override;
    IPipeline* CreatePipeline(const PipelineDesc& desc, IShader* vs, IShader* ps) override;
    ICommandList* CreateCommandList() override;
    void ReportLiveObjects() override;

    void Release() override;

    ID3D11Device* Device();
    ID3D11DeviceContext* Context();

private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIFactory2> factory_;
    ComPtr<IDXGIAdapter1> adapter_;
    bool initialized_ = false;
};

class D3D11Buffer : public IBuffer {
public:
    D3D11Buffer(ID3D11Device* device, const BufferDesc& desc, const void* data);
    uint32_t Size() const { return size_; }
    uint32_t Stride() const { return stride_; }
    bool IsConstant() const { return constant_; }
    bool IsStructured() const { return structured_; }

    void Update(const void* data, uint32_t size) override;
    void* MapForWrite() override;
    void Unmap() override;
    void* GetSrvHandle() override;
    void Release() override;

    ID3D11Buffer* Native();
    ID3D11ShaderResourceView* Srv();

private:
    ComPtr<ID3D11Buffer> buffer_;
    ComPtr<ID3D11ShaderResourceView> srv_;
    ComPtr<ID3D11DeviceContext> context_;
    uint32_t size_;
    uint32_t stride_;
    bool constant_;
    bool structured_;
    bool dynamic_;
    bool mapped_ = false;
};

class D3D11Texture : public ITexture {
public:
    D3D11Texture(ID3D11Device* device, const TextureDesc& desc, const void* data, uint32_t rowBytes);
    D3D11Texture(ID3D11Device* device, ID3D11Texture2D* external, ID3D11RenderTargetView* rtv);
    uint32_t GetWidth() override;
    uint32_t GetHeight() override;
    void Release() override;

    ID3D11Resource* Resource();
    ID3D11RenderTargetView* Rtv();
    ID3D11DepthStencilView* Dsv();
    ID3D11ShaderResourceView* Srv();

private:
    ComPtr<ID3D11Texture2D> texture_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    ComPtr<ID3D11DepthStencilView> dsv_;
    ComPtr<ID3D11ShaderResourceView> srv_;
    uint32_t width_;
    uint32_t height_;
};

class D3D11Shader : public IShader {
public:
    D3D11Shader(ShaderStage stage, const char* source, const char* entryPoint);
    bool Compile();

    bool IsValid() const override { return valid_; }
    const void* GetBytecode() const override { return bytecode_.data(); }
    size_t GetBytecodeSize() const override { return bytecode_.size(); }
    ShaderStage Stage() const { return stage_; }
    void Release() override;

    const std::vector<uint8_t>& Bytecode() const { return bytecode_; }
    ID3D11VertexShader* Vs();
    ID3D11PixelShader* Ps();
    bool CreateGpu(ID3D11Device* device);

private:
    ShaderStage stage_;
    std::string source_;
    std::string entryPoint_;
    std::vector<uint8_t> bytecode_;
    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> ps_;
    bool valid_ = false;
};

class D3D11Pipeline : public IPipeline {
public:
    D3D11Pipeline(ID3D11Device* device, const PipelineDesc& desc, IShader* vs, IShader* ps);
    void Release() override;

    void Apply(ID3D11DeviceContext* context);

private:
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11RasterizerState> rasterizer_;
    ComPtr<ID3D11DepthStencilState> depthState_;
    ID3D11VertexShader* vs_;
    ID3D11PixelShader* ps_;
    D3D11_PRIMITIVE_TOPOLOGY topology_;
};

class D3D11CommandList : public ICommandList {
public:
    explicit D3D11CommandList(ID3D11DeviceContext* context, ID3D11Device* device);
    void ClearRenderTarget(ITexture* rt, float r, float g, float b, float a) override;
    void ClearDepth(ITexture* dt, float v) override;
    void SetViewports(uint32_t w, uint32_t h) override;
    void SetPipeline(IPipeline* p) override;
    void SetRenderTarget(ITexture* color, ITexture* depth) override;
    void BindCB(IBuffer* cb, uint32_t slot) override;
    void BindSRV(ITexture* tex, uint32_t slot) override;
    void SetVertexBuffer(IBuffer* vb) override;
    void SetIndexBuffer(IBuffer* ib) override;
    void DrawIndexed(uint32_t indexCount, uint32_t indexOffset, int32_t vertexOffset) override;
    void Submit(ISwapChain* swap, bool vsync) override;
    void Release() override;

private:
    ID3D11DeviceContext* context_;
    ID3D11Device* device_;
    ComPtr<ID3D11SamplerState> sampler_;
};

} // namespace kizuri::rhi