#pragma once
namespace Kizuri {
class Window {
public:
  Window();
  ~Window();
  bool Create(const wchar_t* title, int width, int height);
  void Destroy();
  bool IsOpen() const;
  bool PollEvents();
  void* NativeHandle() const;
  int Width() const;
  int Height() const;
private:
  void* hwnd;
  unsigned long long instance;
  int width;
  int height;
  bool open;
  bool created;
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
};
}
