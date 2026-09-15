#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cmath>

#include "kizuri/core/diagnostics/Log.h"
#include "kizuri/core/diagnostics/Profiler.h"
#include "kizuri/core/jobs/JobSystem.h"
#include "kizuri/core/memory/MemoryTracker.h"
#include "kizuri/core/platform/Timer.h"
#include "kizuri/core/platform/Window.h"
#include "kizuri/renderer/Camera.h"
#include "kizuri/renderer/ForwardPlusRenderer.h"
#include "kizuri/rhi/Rhi.h"

namespace {

using namespace kizuri;
using namespace kizuri::core;
using namespace kizuri::renderer;

constexpr uint32_t OrbitLightCount = 24;
constexpr uint32_t CubeCount = 6;
constexpr uint32_t FrameCapDurationMs = 16;
constexpr float SceneAspect = 16.0f / 9.0f;
constexpr float SceneFov = 0.9f;

renderer::GpuLight g_lights[OrbitLightCount];
renderer::ObjectData g_objects[CubeCount];
renderer::FreeCameraController g_freeCam;

void BuildSceneObjects(float t)
{
    (void)t;
    for (uint32_t i = 0; i < OrbitLightCount; ++i)
    {
        float angle = static_cast<float>(i) * 6.2831853f / static_cast<float>(OrbitLightCount);
        XMVECTOR pos = XMVectorSet(
            cosf(angle) * 8.0f,
            2.4f + sinf(angle * 2.0f) * 1.2f,
            sinf(angle) * 8.0f,
            1.0f);
        XMStoreFloat3(&g_lights[i].Position, pos);
        g_lights[i].Radius = 4.5f;
        g_lights[i].Color = XMFLOAT3(
            0.5f + 0.5f * sinf(angle * 3.0f),
            0.5f + 0.5f * sinf(angle * 3.0f + 2.0f),
            0.5f + 0.5f * sinf(angle * 3.0f + 4.0f));
        g_lights[i].Intensity = 2.2f;
    }

    const XMFLOAT3 cubePositions[CubeCount] = {
        { -6.0f, 2.0f, -2.0f },
        { -3.0f, 2.0f,  2.0f },
        {  0.0f, 2.0f, -2.0f },
        {  3.0f, 2.0f,  2.0f },
        {  6.0f, 2.0f, -2.0f },
        {  0.0f, 4.5f,  0.0f },
    };
    const XMFLOAT3 cubeTints[CubeCount] = {
        { 0.9f, 0.3f, 0.3f },
        { 0.3f, 0.9f, 0.3f },
        { 0.3f, 0.4f, 0.9f },
        { 0.9f, 0.9f, 0.2f },
        { 0.7f, 0.3f, 0.9f },
        { 0.5f, 0.8f, 0.8f },
    };

    for (uint32_t i = 0; i < CubeCount; ++i)
    {
        XMMATRIX world = XMMatrixScaling(2.0f, 2.0f, 2.0f);
        world *= XMMatrixRotationY(t * (0.3f + 0.15f * static_cast<float>(i)));
        world *= XMMatrixTranslation(cubePositions[i].x, cubePositions[i].y, cubePositions[i].z);
        g_objects[i].World = world;
        g_objects[i].Tint = cubeTints[i];
    }
}

void EmitProfilerSummary()
{
    const uint32_t frameCount = Profiler::FrameCount();
    if (frameCount == 0)
    {
        Log::Info("Profiler captured no frames");
        return;
    }
    ProfileFrame latest = Profiler::FrameAt(0);
    Log::InfoFormatted("Profiler: %u frames, latest frame has %u entries", frameCount, latest.EntryCount);
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    LogConfig logConfig;
    logConfig.LogDirectory = "logs";
    logConfig.LogFileName = "kizuri.log";
    Log::Initialize(logConfig);
    Log::Info("KizuriEngine Fase 1 startup");

    Profiler::Initialize();

    rhi::IDevice* device = rhi::RhiFactory::CreateDevice();
    if (device == nullptr)
    {
        Log::Error("Failed to create RHI device");
        Profiler::Shutdown();
        Log::Shutdown();
        return 1;
    }

    wchar_t adapterName[256];
    device->GetAdapterName(adapterName, 256);
    Log::InfoFormatted("RHI device created (adapter: %S)", adapterName);

    Window window;
    WindowDesc desc;
    desc.Title = "Kizuri Engine - Fase 1 (Forward+ Clustered)";
    desc.Width = 1280;
    desc.Height = 720;
    if (!window.Create(desc))
    {
        Log::Error("Failed to create platform window");
        device->Release();
        Profiler::Shutdown();
        Log::Shutdown();
        return 1;
    }

    ForwardPlusRenderer renderer;
    if (!renderer.Initialize(device, window.NativeHandle(), window.Width(), window.Height(),
            L"engine/renderer/shaders"))
    {
        Log::Error("Failed to initialize renderer");
        window.Destroy();
        device->Release();
        Profiler::Shutdown();
        Log::Shutdown();
        return 1;
    }

    FrameTimer timer;
    timer.Reset();
    g_freeCam.SetSpeed(10.0f);

    while (!window.ShouldClose())
    {
        Profiler::BeginFrame();
        {
            KZ_PROFILE_SCOPE("FrameTotal");

            window.ProcessMessages();
            const Input& input = window.GetInput();

            if (window.Width() != renderer.Width() || window.Height() != renderer.Height())
            {
                renderer.Resize(window.Width(), window.Height());
            }

            if (input.WasPressed(Key::Escape))
            {
                window.RequestClose();
            }

            timer.BeginFrame();
            float dt = static_cast<float>(timer.LastFrameSeconds());

            g_freeCam.Update(input, dt);

            BuildSceneObjects(static_cast<float>(timer.ElapsedSeconds()));

            XMMATRIX view = g_freeCam.ViewMatrix();
            XMMATRIX proj = g_freeCam.ProjectionMatrix(SceneFov, SceneAspect, 0.1f, 500.0f);

            RenderFrameData frame{};
            frame.View = &view;
            frame.Projection = &proj;
            frame.CameraPosition = g_freeCam.PositionFloat();
            frame.Lights = g_lights;
            frame.LightCount = OrbitLightCount;
            frame.Objects = g_objects;
            frame.ObjectCount = CubeCount;

            renderer.TickHotReload();

            KZ_PROFILE_SCOPE("RendererRender");
            renderer.Render(frame);

            if (timer.FrameIndex() % 300 == 0)
            {
                Log::InfoFormatted(
                    "frame %u | dt %.2f ms | lights %u | clusters %u",
                    timer.FrameIndex(),
                    timer.LastFrameSeconds() * 1000.0,
                    OrbitLightCount,
                    0);
            }

            const uint32_t frameMs = static_cast<uint32_t>(timer.LastFrameSeconds() * 1000.0);
            if (frameMs < FrameCapDurationMs)
            {
                Sleep(FrameCapDurationMs - frameMs);
            }
        }
        Profiler::EndFrame();
    }

    window.Destroy();
    renderer.Shutdown();
    EmitProfilerSummary();
    device->ReportLiveObjects();
    device->Release();
    Profiler::Shutdown();

    const bool memoryClean = MemoryTracker::Instance().IsZeroed();
    Log::InfoFormatted(
        "MemoryTracker %s at shutdown (peak %zu bytes)",
        memoryClean ? "clean" : "LEAK DETECTED",
        MemoryTracker::Instance().PeakBytes());
    Log::Shutdown();

    return memoryClean ? 0 : 1;
}