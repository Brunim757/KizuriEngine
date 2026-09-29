#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/Input.h"
#include <windows.h>
#include <atomic>
namespace Kizuri {
namespace {
bool g_inputReady = false;
std::atomic<int> g_dx(0);
std::atomic<int> g_dy(0);
}
bool RawInputPoll::Initialize(void* hwnd) {
  HWND hWnd = static_cast<HWND>(hwnd);
  RAWINPUTDEVICE rid[2];
  rid[0].usUsagePage = 0x01;
  rid[0].usUsage = 0x02;
  rid[0].dwFlags = RIDEV_INPUTSINK;
  rid[0].hwndTarget = hWnd;
  rid[1].usUsagePage = 0x01;
  rid[1].usUsage = 0x06;
  rid[1].dwFlags = RIDEV_INPUTSINK;
  rid[1].hwndTarget = hWnd;
  BOOL ok = RegisterRawInputDevices(rid, 2, sizeof(rid[0]));
  g_inputReady = (ok == TRUE);
  return g_inputReady;
}
void RawInputPoll::Shutdown() {
  g_inputReady = false;
}
void RawInputPoll::Poll() {
  (void)g_inputReady;
}
bool RawInputPoll::IsKeyDown(int vk) {
  SHORT s = GetAsyncKeyState(vk);
  return (s & 0x8000) != 0;
}
bool RawInputPoll::IsDodgePressed() {
  return IsKeyDown(VK_SPACE);
}
bool RawInputPoll::IsAttackPressed() {
  return IsKeyDown(VK_LBUTTON) || IsKeyDown(0x46);
}
bool RawInputPoll::IsMouseDown(int vk) {
  return IsKeyDown(vk);
}
void RawInputPoll::GetMousePosition(int& x, int& y) {
  POINT p;
  p.x = 0;
  p.y = 0;
  GetCursorPos(&p);
  x = p.x;
  y = p.y;
}
void RawInputPoll::GetMouseDelta(int& dx, int& dy) {
  dx = g_dx.exchange(0);
  dy = g_dy.exchange(0);
}
void RawInputPoll::OnRawMouseDelta(int dx, int dy) {
  g_dx.fetch_add(dx);
  g_dy.fetch_add(dy);
}
}
