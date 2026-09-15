#pragma once

#include <cstdint>

#include "kizuri/core/platform/Input.h"

namespace kizuri::core {

struct WindowDesc
{
    const char* Title = "Kizuri Engine";
    uint32_t Width = 1280;
    uint32_t Height = 720;
    bool Resizable = true;
};

class Window {
public:
    bool Create(const WindowDesc& desc);
    void Destroy() noexcept;

    void ProcessMessages();
    void HandleNativeMessage(void* nativeWindow, uint32_t message, uintptr_t wParam, intptr_t lParam);
    bool ShouldClose() const noexcept { return shouldClose_; }
    void RequestClose() noexcept;

    const Input& GetInput() const noexcept { return input_; }
    Input& GetMutableInput() noexcept { return input_; }

    uint32_t Width() const noexcept { return clientWidth_; }
    uint32_t Height() const noexcept { return clientHeight_; }
    void* NativeHandle() const noexcept { return nativeHandle_; }

private:
    enum class NativeCallbackResult;
    static intptr_t HandleMessage(void* windowId, uint32_t message, uintptr_t wParam, intptr_t lParam);

    void* nativeHandle_ = nullptr;
    Input input_;
    uint32_t clientWidth_ = 0;
    uint32_t clientHeight_ = 0;
    bool shouldClose_ = false;
};

} // namespace kizuri::core