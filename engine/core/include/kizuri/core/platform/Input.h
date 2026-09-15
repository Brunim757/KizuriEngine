#pragma once

#include <cstdint>

namespace kizuri::core {

enum class Key : uint16_t
{
    None = 0,
    Escape,
    Enter,
    Tab,
    Space,
    Backspace,
    Delete,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
    W,
    A,
    S,
    D,
    Q,
    E,
    R,
    F,
    G,
    Z,
    X,
    C,
    V,
    B,
    N,
    M,
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    Digit0,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    MouseLeft,
    MouseRight,
    MouseMiddle,
    Count
};

class Input {
public:
    Input() = default;

    void BeginFrame() noexcept;
    void SetKeyDown(Key key, bool down) noexcept;
    void SetMousePosition(int32_t x, int32_t y) noexcept;
    void SetMouseDoubleClick(Key key) noexcept;
    void AddScrollDelta(int32_t delta) noexcept;

    bool IsDown(Key key) const noexcept;
    bool WasPressed(Key key) const noexcept;
    bool WasReleased(Key key) const noexcept;
    bool WasDoubleClicked(Key key) const noexcept;

    int32_t MouseX() const noexcept { return mouseX_; }
    int32_t MouseY() const noexcept { return mouseY_; }
    int32_t ScrollDelta() const noexcept { return scrollDelta_; }

private:
    uint8_t currentState_[static_cast<size_t>(Key::Count)]{};
    uint8_t previousState_[static_cast<size_t>(Key::Count)]{};
    uint8_t doubleClickCount_[static_cast<size_t>(Key::Count)]{};
    int32_t mouseX_ = 0;
    int32_t mouseY_ = 0;
    int32_t scrollDelta_ = 0;
};

} // namespace kizuri::core