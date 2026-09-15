#include "kizuri/core/platform/Input.h"

#include <cstring>

namespace kizuri::core {

void Input::BeginFrame() noexcept
{
    std::memcpy(previousState_, currentState_, sizeof(currentState_));
    for (uint16_t i = 0; i < static_cast<uint16_t>(Key::Count); ++i)
    {
        doubleClickCount_[i] = 0;
    }
    scrollDelta_ = 0;
}

void Input::SetKeyDown(Key key, bool down) noexcept
{
    size_t index = static_cast<size_t>(key);
    if (index >= static_cast<size_t>(Key::Count))
    {
        return;
    }
    currentState_[index] = down ? 1 : 0;
}

void Input::SetMousePosition(int32_t x, int32_t y) noexcept
{
    mouseX_ = x;
    mouseY_ = y;
}

void Input::SetMouseDoubleClick(Key key) noexcept
{
    size_t index = static_cast<size_t>(key);
    if (index >= static_cast<size_t>(Key::Count))
    {
        return;
    }
    doubleClickCount_[index] = 1;
}

void Input::AddScrollDelta(int32_t delta) noexcept
{
    scrollDelta_ = delta;
}

bool Input::IsDown(Key key) const noexcept
{
    size_t index = static_cast<size_t>(key);
    if (index >= static_cast<size_t>(Key::Count))
    {
        return false;
    }
    return currentState_[index] != 0;
}

bool Input::WasPressed(Key key) const noexcept
{
    size_t index = static_cast<size_t>(key);
    if (index >= static_cast<size_t>(Key::Count))
    {
        return false;
    }
    return currentState_[index] != 0 && previousState_[index] == 0;
}

bool Input::WasReleased(Key key) const noexcept
{
    size_t index = static_cast<size_t>(key);
    if (index >= static_cast<size_t>(Key::Count))
    {
        return false;
    }
    return currentState_[index] == 0 && previousState_[index] != 0;
}

bool Input::WasDoubleClicked(Key key) const noexcept
{
    size_t index = static_cast<size_t>(key);
    if (index >= static_cast<size_t>(Key::Count))
    {
        return false;
    }
    return doubleClickCount_[index] != 0;
}

} // namespace kizuri::core