#include "Kizuri/Core.h"
#include "Kizuri/Render.h"
#include "Kizuri/Physics.h"
#include "Kizuri/Animation.h"
#include "Kizuri/Audio.h"
#include "Kizuri/Scripting.h"
#include <cstdio>
int main() {
  std::printf("Kizuri Editor %s\n", Kizuri::Core_Version());
  std::printf("Render %s Physics %s Animation %s Audio %s Scripting %s\n",
    Kizuri::Render_Version(),
    Kizuri::Physics_Version(),
    Kizuri::Animation_Version(),
    Kizuri::Audio_Version(),
    Kizuri::Scripting_Version());
  return 0;
}
