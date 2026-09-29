#include "Kizuri/Animation.h"
namespace Kizuri {
const char* Animation_Version() {
  return "0.0.1-fase0";
}
bool Animation_SelfTest() {
  const char* v = Animation_Version();
  return v != nullptr && v[0] != '\0';
}
}
