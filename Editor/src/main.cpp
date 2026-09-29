#include "EditorApp.h"
#include <cstdio>
int main() {
  Kizuri::EditorApp app;
  if (!app.Initialize()) {
    std::printf("Editor init failed\n");
    return 1;
  }
  int code = app.Run();
  app.Shutdown();
  return code;
}
