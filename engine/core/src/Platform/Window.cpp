#include "kizuri/core/platform/Window.h"

#ifndef _WIN32
#error "KizuriCore Window targets the Windows platform."
#endif

#include <windows.h>

#include <cstring>

namespace kizuri::core {

namespace {

constexpr const wchar_t* WindowClassName = L"KizuriEngineWindow";

Key TranslateVirtualKey(uintptr_t virtualKey)
{
    switch (virtualKey)
    {
        case VK_ESCAPE: return Key::Escape;
        case VK_RETURN: return Key::Enter;
        case VK_TAB: return Key::Tab;
        case VK_SPACE: return Key::Space;
        case VK_BACK: return Key::Backspace;
        case VK_DELETE: return Key::Delete;
        case VK_LSHIFT: return Key::LeftShift;
        case VK_RSHIFT: return Key::RightShift;
        case VK_LCONTROL: return Key::LeftControl;
        case VK_RCONTROL: return Key::RightControl;
        case VK_LMENU: return Key::LeftAlt;
        case VK_RMENU: return Key::RightAlt;
        case VK_UP: return Key::ArrowUp;
        case VK_DOWN: return Key::ArrowDown;
        case VK_LEFT: return Key::ArrowLeft;
        case VK_RIGHT: return Key::ArrowRight;
        case VK_F1: return Key::F1;
        case VK_F2: return Key::F2;
        case VK_F3: return Key::F3;
        case VK_F4: return Key::F4;
        case VK_F5: return Key::F5;
        case VK_F6: return Key::F6;
        case VK_F7: return Key::F7;
        case VK_F8: return Key::F8;
        case VK_F9: return Key::F9;
        case VK_F10: return Key::F10;
        case VK_F11: return Key::F11;
        case VK_F12: return Key::F12;
        case 'W': return Key::W;
        case 'A': return Key::A;
        case 'S': return Key::S;
        case 'D': return Key::D;
        case 'Q': return Key::Q;
        case 'E': return Key::E;
        case 'R': return Key::R;
        case 'F': return Key::F;
        case 'G': return Key::G;
        case 'Z': return Key::Z;
        case 'X': return Key::X;
        case 'C': return Key::C;
        case 'V': return Key::V;
        case 'B': return Key::B;
        case 'N': return Key::N;
        case 'M': return Key::M;
        case '0': return Key::Digit0;
        case '1': return Key::Digit1;
        case '2': return Key::Digit2;
        case '3': return Key::Digit3;
        case '4': return Key::Digit4;
        case '5': return Key::Digit5;
        case '6': return Key::Digit6;
        case '7': return Key::Digit7;
        case '8': return Key::Digit8;
        case '9': return Key::Digit9;
        default: return Key::None;
    }
}

LRESULT CALLBACK KizuriWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        const CREATESTRUCT* createStruct = reinterpret_cast<const CREATESTRUCT*>(lParam);
        Window* window = static_cast<Window*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    Window* window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (window == nullptr)
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    window->HandleNativeMessage(hwnd, message, wParam, lParam);
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

bool Window::Create(const WindowDesc& desc)
{
    DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (desc.Resizable)
    {
        windowStyle |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    }

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = &KizuriWndProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = WindowClassName;
    windowClass.hCursor = static_cast<HCURSOR>(LoadCursorW(nullptr, IDC_ARROWW));
    windowClass.hIcon = static_cast<HICON>(LoadIconW(nullptr, IDI_APPLICATIONW));

    const ATOM classAtom = RegisterClassW(&windowClass);
    if (classAtom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    wchar_t wideTitle[256];
    MultiByteToWideChar(CP_UTF8, 0, desc.Title, -1, wideTitle, 256);

    HWND hwnd = CreateWindowExW(
        0,
        WindowClassName,
        wideTitle,
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        static_cast<int>(desc.Width),
        static_cast<int>(desc.Height),
        nullptr,
        nullptr,
        windowClass.hInstance,
        this);
    if (hwnd == nullptr)
    {
        return false;
    }

    nativeHandle_ = hwnd;
    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);
    clientWidth_ = static_cast<uint32_t>(clientRect.right - clientRect.left);
    clientHeight_ = static_cast<uint32_t>(clientRect.bottom - clientRect.top);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return true;
}

void Window::Destroy() noexcept
{
    if (nativeHandle_ != nullptr)
    {
        DestroyWindow(static_cast<HWND>(nativeHandle_));
        nativeHandle_ = nullptr;
    }
}

void Window::ProcessMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    input_.BeginFrame();
}

void Window::HandleNativeMessage(void* nativeWindow, uint32_t message, uintptr_t wParam, intptr_t lParam)
{
    switch (message)
    {
        case WM_KEYDOWN:
        {
            Key key = TranslateVirtualKey(wParam);
            if (key != Key::None)
            {
                input_.SetKeyDown(key, true);
            }
            break;
        }
        case WM_KEYUP:
        {
            Key key = TranslateVirtualKey(wParam);
            if (key != Key::None)
            {
                input_.SetKeyDown(key, false);
            }
            break;
        }
        case WM_LBUTTONDOWN:
            input_.SetKeyDown(Key::MouseLeft, true);
            break;
        case WM_LBUTTONUP:
            input_.SetKeyDown(Key::MouseLeft, false);
            break;
        case WM_LBUTTONDBLCLK:
            input_.SetMouseDoubleClick(Key::MouseLeft);
            break;
        case WM_RBUTTONDOWN:
            input_.SetKeyDown(Key::MouseRight, true);
            break;
        case WM_RBUTTONUP:
            input_.SetKeyDown(Key::MouseRight, false);
            break;
        case WM_RBUTTONDBLCLK:
            input_.SetMouseDoubleClick(Key::MouseRight);
            break;
        case WM_MOUSEMOVE:
            input_.SetMousePosition(
                static_cast<int32_t>(static_cast<int16_t>(LOWORD(lParam))),
                static_cast<int32_t>(static_cast<int16_t>(HIWORD(lParam))));
            break;
        case WM_MOUSEWHEEL:
        {
            int32_t delta = static_cast<int32_t>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            input_.AddScrollDelta(delta);
            break;
        }
        case WM_SIZE:
        {
            clientWidth_ = static_cast<uint32_t>(LOWORD(lParam));
            clientHeight_ = static_cast<uint32_t>(HIWORD(lParam));
            break;
        }
        case WM_CLOSE:
            RequestClose();
            break;
        default:
            break;
    }
}

void Window::RequestClose() noexcept
{
    shouldClose_ = true;
}

} // namespace kizuri::core