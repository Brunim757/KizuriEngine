#pragma once
namespace Kizuri {
class RawInputPoll {
public:
  static bool Initialize(void* hwnd);
  static void Shutdown();
  static void Poll();
  static bool IsKeyDown(int vk);
  static bool IsDodgePressed();
  static bool IsAttackPressed();
  static bool IsMouseDown(int vk);
  static void GetMousePosition(int& x, int& y);
  static void GetMouseDelta(int& dx, int& dy);
  static void OnRawMouseDelta(int dx, int dy);
private:
  RawInputPoll() = delete;
};
}
