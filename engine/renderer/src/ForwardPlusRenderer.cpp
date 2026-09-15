#include "kizuri/renderer/ForwardPlusRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>

#include <windows.h>

#include <efsw/efsw.hpp>

#include "kizuri/core/diagnostics/Log.h"
#include "kizuri/core/diagnostics/Profiler.h"

namespace kizuri::renderer {

using namespace kizuri::rhi;

namespace {

constexpr float CheckerTileSize = 2.0f;

struct ShaderWatchListener : public efsw::FileWatchListener
{
    std::mutex mutex;
    std::vector<std::string> changedPaths;

    void handleFileAction(efsw::WatchID watchId, const std::string& dir,
        const std::string& filename, efsw::Action action, std::string oldFilename) override
    {
        (void)watchId;
        (void)dir;
        (void)oldFilename;
        if (action != efsw::Actions::Modified)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        changedPaths.push_back(filename);
    }
};

} // namespace

ForwardPlusRenderer::ForwardPlusRenderer() = default;

ForwardPlusRenderer::~ForwardPlusRenderer() = default;

bool ForwardPlusRenderer::Initialize(
    rhi::IDevice* device,
    void* hwnd,
    uint32_t width,
    uint32_t height,
    const wchar_t* shadersDir)
{
    device_ = device;
    width_ = width;
    height_ = height;
    if (shadersDir != nullptr)
    {
        shadersDir_.assign(shadersDir);
    }
    else
    {
        shadersDir_.clear();
    }

    swapChain_ = device_->CreateSwapChain(hwnd, width, height);
    if (swapChain_ == nullptr)
    {
        core::Log::Error("Failed to create swap chain");
        return false;
    }

    cmd_ = device_->CreateCommandList();

    cluster_.Resize(width_, height_, nearZ_, farZ_);

    BufferDesc cbDesc{};
    cbDesc.ConstantBuffer = true;
    cbDesc.ByteSize = sizeof(FrameConstants);
    cbDesc.CpuWritable = true;
    frameCB_ = device_->CreateBuffer(cbDesc, nullptr);

    cbDesc.ByteSize = sizeof(LightConstants);
    lightCB_ = device_->CreateBuffer(cbDesc, nullptr);

    cbDesc.ByteSize = sizeof(ClusterConstants);
    clusterCB_ = device_->CreateBuffer(cbDesc, nullptr);

    cbDesc.ByteSize = sizeof(ObjectConstants);
    objectCB_ = device_->CreateBuffer(cbDesc, nullptr);

    BufferDesc sbDesc{};
    sbDesc.StructuredBuffer = true;
    sbDesc.CpuWritable = true;
    sbDesc.ByteSize = sizeof(ClusterRange) * MaxClusters;
    sbDesc.ElementBytes = sizeof(ClusterRange);
    gridSB_ = device_->CreateBuffer(sbDesc, nullptr);

    sbDesc.ByteSize = sizeof(uint32_t) * MaxLightIndices;
    sbDesc.ElementBytes = sizeof(uint32_t);
    indexSB_ = device_->CreateBuffer(sbDesc, nullptr);

    cubeData_ = CreateCubeMesh(1.0f);
    planeData_ = CreatePlaneMesh(50.0f, 50.0f);

    BufferDesc vbDesc{};
    vbDesc.VertexBuffer = true;
    vbDesc.ByteSize = static_cast<uint32_t>(cubeData_.Vertices.size() * sizeof(MeshVertex));
    vbDesc.StrideBytes = sizeof(MeshVertex);
    cubeVB_ = device_->CreateBuffer(vbDesc, cubeData_.Vertices.data());

    BufferDesc ibDesc{};
    ibDesc.IndexBuffer = true;
    ibDesc.ByteSize = static_cast<uint32_t>(cubeData_.Indices.size() * sizeof(uint32_t));
    cubeIB_ = device_->CreateBuffer(ibDesc, cubeData_.Indices.data());
    cubeIndexCount_ = static_cast<uint32_t>(cubeData_.Indices.size());

    vbDesc.ByteSize = static_cast<uint32_t>(planeData_.Vertices.size() * sizeof(MeshVertex));
    planeVB_ = device_->CreateBuffer(vbDesc, planeData_.Vertices.data());

    ibDesc.ByteSize = static_cast<uint32_t>(planeData_.Indices.size() * sizeof(uint32_t));
    planeIB_ = device_->CreateBuffer(ibDesc, planeData_.Indices.data());
    planeIndexCount_ = static_cast<uint32_t>(planeData_.Indices.size());

    GenerateCheckerTexture();

    CreateShadersFromDisk();
    CreatePipeline();

    WatchShadersDirectory();

    return true;
}

void ForwardPlusRenderer::Shutdown()
{
    if (watcher_ != nullptr)
    {
        auto* fw = static_cast<efsw::FileWatcher*>(watcher_);
        delete fw;
        watcher_ = nullptr;
    }
    if (listener_ != nullptr)
    {
        auto* listener = static_cast<ShaderWatchListener*>(listener_);
        delete listener;
        listener_ = nullptr;
    }

    auto Release = [](auto*& obj)
    {
        if (obj != nullptr)
        {
            obj->Release();
            obj = nullptr;
        }
    };

    Release(pipeline_);
    Release(meshVs_);
    Release(meshPs_);
    Release(albedo_);
    Release(frameCB_);
    Release(lightCB_);
    Release(clusterCB_);
    Release(objectCB_);
    Release(gridSB_);
    Release(indexSB_);
    Release(cubeVB_);
    Release(cubeIB_);
    Release(planeVB_);
    Release(planeIB_);

    if (cmd_ != nullptr)
    {
        cmd_->Release();
        cmd_ = nullptr;
    }
    if (swapChain_ != nullptr)
    {
        swapChain_->Release();
        swapChain_ = nullptr;
    }
}

void ForwardPlusRenderer::StoreMatrix(XMFLOAT4X4& dst, FXMMATRIX m)
{
    XMMATRIX transposed = XMMatrixTranspose(m);
    XMStoreFloat4x4(&dst, transposed);
}

void ForwardPlusRenderer::CreateShadersFromDisk()
{
    std::string vsSrc, psSrc;
    wchar_t vsPath[MAX_PATH], psPath[MAX_PATH];

    if (ResolveShaderPath(L"MeshVs.hlsl", vsPath, MAX_PATH))
    {
        LoadShaderFile(vsPath, vsSrc);
    }
    if (ResolveShaderPath(L"MeshPs.hlsl", psPath, MAX_PATH))
    {
        LoadShaderFile(psPath, psSrc);
    }

    CreateShadersFromSource(vsSrc.c_str(), psSrc.c_str());
}

void ForwardPlusRenderer::CreateShadersFromSource(const char* vsSource, const char* psSource)
{
    if (meshVs_ != nullptr)
    {
        meshVs_->Release();
        meshVs_ = nullptr;
    }
    if (meshPs_ != nullptr)
    {
        meshPs_->Release();
        meshPs_ = nullptr;
    }

    meshVs_ = device_->CreateShader(rhi::ShaderStage::Vertex, vsSource, "main");
    meshPs_ = device_->CreateShader(rhi::ShaderStage::Pixel, psSource, "main");

    if (meshVs_ != nullptr && meshVs_->IsValid() && meshPs_ != nullptr && meshPs_->IsValid())
    {
        core::Log::Info("KizuriShaders reloaded successfully");
    }
    else
    {
        core::Log::Error("KizuriShader compilation failed");
    }
}

void ForwardPlusRenderer::CreatePipeline()
{
    if (pipeline_ != nullptr)
    {
        pipeline_->Release();
        pipeline_ = nullptr;
    }

    VertexElement elements[] = {
        { "POSITION", 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(MeshVertex, Position)) },
        { "NORMAL", 0, VertexFormat::Float3, static_cast<uint32_t>(offsetof(MeshVertex, Normal)) },
        { "TEXCOORD", 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(MeshVertex, Uv)) },
    };

    PipelineDesc desc{};
    desc.Elements = elements;
    desc.ElementCount = 3;
    desc.VertexStride = sizeof(MeshVertex);
    desc.Topology = 4;
    desc.DepthTest = true;
    desc.DepthWrite = true;
    desc.CullBack = true;

    pipeline_ = device_->CreatePipeline(desc, meshVs_, meshPs_);
}

void ForwardPlusRenderer::GenerateCheckerTexture()
{
    constexpr uint32_t texSize = 256;
    constexpr uint32_t tileSize = 32;
    std::vector<uint8_t> pixels(texSize * texSize * 4);

    for (uint32_t y = 0; y < texSize; ++y)
    {
        for (uint32_t x = 0; x < texSize; ++x)
        {
            bool white = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            uint8_t c = white ? 200 : 60;
            uint32_t idx = (y * texSize + x) * 4;
            pixels[idx + 0] = c;
            pixels[idx + 1] = c;
            pixels[idx + 2] = c;
            pixels[idx + 3] = 255;
        }
    }

    TextureDesc desc{};
    desc.Width = texSize;
    desc.Height = texSize;
    desc.Format = TextureFormat::Rgba8Unorm;
    desc.ShaderResourceView = true;
    albedo_ = device_->CreateTexture(desc, pixels.data(), texSize * 4);
}

void ForwardPlusRenderer::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }
    width_ = width;
    height_ = height;
    if (swapChain_ != nullptr)
    {
        swapChain_->Resize(width_, height_);
    }
    cluster_.Resize(width_, height_, nearZ_, farZ_);
}

void ForwardPlusRenderer::LoadShaderFile(const wchar_t* path, std::string& outSource)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        core::Log::ErrorFormatted("Failed to open shader: %S", path);
        return;
    }
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    outSource.resize(static_cast<size_t>(size));
    file.read(outSource.data(), size);
}

bool ForwardPlusRenderer::ResolveShaderPath(const wchar_t* shaderName, wchar_t* outFull, size_t outSize)
{
    if (!shadersDir_.empty())
    {
        swprintf(outFull, outSize, L"%s\\%s", shadersDir_.c_str(), shaderName);
        DWORD attrs = GetFileAttributesW(outFull);
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            return true;
        }
    }

    static const wchar_t* candidates[] = {
        L"shaders",
        L"engine/renderer/shaders",
        L"../../engine/renderer/shaders",
    };

    for (const wchar_t* dir : candidates)
    {
        swprintf(outFull, outSize, L"%s\\%s", dir, shaderName);
        DWORD attrs = GetFileAttributesW(outFull);
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            return true;
        }
    }
    return false;
}

void ForwardPlusRenderer::WatchShadersDirectory()
{
    auto* listener = new ShaderWatchListener();
    listener_ = listener;

    auto* fw = new efsw::FileWatcher();
    fw->addWatch(".", listener, true);

    for (auto& entry : { L"shaders", L"engine/renderer/shaders", L"../../engine/renderer/shaders" })
    {
        DWORD attrs = GetFileAttributesW(entry);
        if (attrs != INVALID_FILE_ATTRIBUTES)
        {
            fw->addWatch(entry, listener, true);
        }
    }

    fw->watch();
    watcher_ = fw;
}

void ForwardPlusRenderer::TickHotReload()
{
    if (watcher_ == nullptr || listener_ == nullptr)
    {
        return;
    }

    auto* listener = static_cast<ShaderWatchListener*>(listener_);
    std::vector<std::string> changes;
    {
        std::lock_guard<std::mutex> lock(listener->mutex);
        changes.swap(listener->changedPaths);
    }

    if (changes.empty())
    {
        return;
    }

    bool needsReload = false;
    for (const auto& filename : changes)
    {
        if (filename.find("MeshVs") != std::string::npos ||
            filename.find("MeshPs") != std::string::npos)
        {
            needsReload = true;
        }
    }

    if (needsReload)
    {
        core::Log::Info("KizuriShader changes detected, rebuilding...");
        CreateShadersFromDisk();
        CreatePipeline();
    }
}

void ForwardPlusRenderer::Render(const RenderFrameData& frame)
{
    KZ_PROFILE_SCOPE("ForwardPlusRenderer::Render");

    if (cmd_ == nullptr || swapChain_ == nullptr || pipeline_ == nullptr)
    {
        return;
    }

    if (frame.View == nullptr || frame.Projection == nullptr)
    {
        return;
    }

    XMMATRIX viewProj = XMMatrixMultiply(*frame.View, *frame.Projection);

    cmd_->ClearRenderTarget(swapChain_->BackBuffer(), 0.15f, 0.15f, 0.2f, 1.0f);
    cmd_->ClearDepth(swapChain_->DepthBuffer(), 1.0f);
    cmd_->SetViewports(width_, height_);
    cmd_->SetPipeline(pipeline_);
    cmd_->SetRenderTarget(swapChain_->BackBuffer(), swapChain_->DepthBuffer());
    cmd_->BindSampler(0);

    StoreMatrix(frameConstants_.ViewProj, viewProj);
    frameConstants_.CameraPosition = XMFLOAT4(frame.CameraPosition.x, frame.CameraPosition.y, frame.CameraPosition.z, 0.0f);
    frameConstants_.ScreenSize = XMFLOAT4(static_cast<float>(width_), static_cast<float>(height_),
        1.0f / static_cast<float>(width_), 1.0f / static_cast<float>(height_));
    frameCB_->Update(&frameConstants_, sizeof(FrameConstants));
    cmd_->BindCB(frameCB_, 0);

    uint32_t lightCount = frame.LightCount;
    if (lightCount > MaxLights)
    {
        lightCount = MaxLights;
    }
    lightConstants_.LightCount = static_cast<int32_t>(lightCount);
    if (lightCount > 0)
    {
        std::memcpy(lightConstants_.Lights, frame.Lights, lightCount * sizeof(GpuLight));
    }
    lightCB_->Update(&lightConstants_, sizeof(LightConstants));
    cmd_->BindCB(lightCB_, 1);

    cluster_.Build(viewProj, *frame.View, frame.Lights, frame.LightCount);
    clusterConstants_ = cluster_.BuildConstants();
    clusterConstants_.Ambient = 0.12f;
    clusterCB_->Update(&clusterConstants_, sizeof(ClusterConstants));
    cmd_->BindCB(clusterCB_, 2);

    if (cluster_.IndexCount() > 0 && cluster_.ClusterCount() > 0)
    {
        gridSB_->Update(cluster_.Grid(), static_cast<uint32_t>(cluster_.ClusterCount() * sizeof(ClusterRange)));
        indexSB_->Update(cluster_.Indices(), static_cast<uint32_t>(cluster_.IndexCount() * sizeof(uint32_t)));
        cmd_->BindSRVBuffer(gridSB_, 2);
        cmd_->BindSRVBuffer(indexSB_, 3);
    }

    cmd_->BindSRV(albedo_, 0);

    {
        KZ_PROFILE_SCOPE("DrawPlane");
        ObjectConstants planeObj{};
        StoreMatrix(planeObj.World, XMMatrixIdentity());
        StoreMatrix(planeObj.WorldInverseTranspose, XMMatrixIdentity());
        planeObj.Tint = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
        objectCB_->Update(&planeObj, sizeof(ObjectConstants));
        cmd_->BindCB(objectCB_, 3);
        cmd_->SetVertexBuffer(planeVB_);
        cmd_->SetIndexBuffer(planeIB_);
        cmd_->DrawIndexed(planeIndexCount_, 0, 0);
    }

    {
        KZ_PROFILE_SCOPE("DrawCubes");
        for (uint32_t i = 0; i < frame.ObjectCount; ++i)
        {
            ObjectConstants obj{};
            StoreMatrix(obj.World, frame.Objects[i].World);
            XMMATRIX inv = XMMatrixInverse(nullptr, frame.Objects[i].World);
            StoreMatrix(obj.WorldInverseTranspose, inv);
            obj.Tint = XMFLOAT4(frame.Objects[i].Tint.x, frame.Objects[i].Tint.y, frame.Objects[i].Tint.z, 1.0f);
            objectCB_->Update(&obj, sizeof(ObjectConstants));
            cmd_->BindCB(objectCB_, 3);
            cmd_->SetVertexBuffer(cubeVB_);
            cmd_->SetIndexBuffer(cubeIB_);
            cmd_->DrawIndexed(cubeIndexCount_, 0, 0);
        }
    }

    cmd_->Submit(swapChain_, vsync_);
}

} // namespace kizuri::renderer