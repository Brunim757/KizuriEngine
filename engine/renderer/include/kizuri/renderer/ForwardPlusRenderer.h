#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "kizuri/rhi/Rhi.h"
#include "kizuri/renderer/Camera.h"
#include "kizuri/renderer/ClusterGrid.h"
#include "kizuri/renderer/RenderTypes.h"
#include "kizuri/renderer/Meshes.h"

namespace kizuri::renderer {

struct RenderFrameData
{
    const XMMATRIX* View;
    const XMMATRIX* Projection;
    XMFLOAT3 CameraPosition;
    const GpuLight* Lights;
    uint32_t LightCount;
    const ObjectData* Objects;
    uint32_t ObjectCount;
};

class ForwardPlusRenderer {
public:
    ForwardPlusRenderer();
    ~ForwardPlusRenderer();

    bool Initialize(
        rhi::IDevice* device,
        void* hwnd,
        uint32_t width,
        uint32_t height,
        const wchar_t* shadersDir);

    void Shutdown();
    void Resize(uint32_t width, uint32_t height);
    void TickHotReload();
    void Render(const RenderFrameData& frame);

    uint32_t Width() const { return width_; }
    uint32_t Height() const { return height_; }

private:
    void StoreMatrix(XMFLOAT4X4& dst, FXMMATRIX m);
    void CreateShadersFromDisk();
    void CreateShadersFromSource(const char* vsSource, const char* psSource);
    void CreatePipeline();
    void GenerateCheckerTexture();
    void LoadShaderFile(const wchar_t* path, std::string& outSource);
    bool ResolveShaderPath(const wchar_t* shaderName, wchar_t* outFull, size_t outSize);
    void WatchShadersDirectory();

    rhi::IDevice* device_ = nullptr;
    rhi::ISwapChain* swapChain_ = nullptr;
    rhi::ICommandList* cmd_ = nullptr;

    rhi::IBuffer* frameCB_ = nullptr;
    rhi::IBuffer* lightCB_ = nullptr;
    rhi::IBuffer* clusterCB_ = nullptr;
    rhi::IBuffer* objectCB_ = nullptr;
    rhi::IBuffer* gridSB_ = nullptr;
    rhi::IBuffer* indexSB_ = nullptr;

    rhi::IShader* meshVs_ = nullptr;
    rhi::IShader* meshPs_ = nullptr;
    rhi::IPipeline* pipeline_ = nullptr;
    rhi::ITexture* albedo_ = nullptr;

    MeshData cubeData_;
    MeshData planeData_;
    rhi::IBuffer* cubeVB_ = nullptr;
    rhi::IBuffer* cubeIB_ = nullptr;
    uint32_t cubeIndexCount_ = 0;
    rhi::IBuffer* planeVB_ = nullptr;
    rhi::IBuffer* planeIB_ = nullptr;
    uint32_t planeIndexCount_ = 0;

    ClusterGrid cluster_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    float nearZ_ = 0.1f;
    float farZ_ = 500.0f;

    FrameConstants frameConstants_{};
    LightConstants lightConstants_{};
    ClusterConstants clusterConstants_{};

    std::string vsPath_;
    std::string psPath_;
    std::string vsSource_;
    std::string psSource_;
    std::wstring shadersDir_;
    void* watcher_ = nullptr;
    void* listener_ = nullptr;
};

} // namespace kizuri::renderer