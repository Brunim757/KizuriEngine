#include "Kizuri/Scripting.h"
namespace Kizuri {
const char* Scripting_Version() {
  return "0.0.1-fase0";
}
bool Scripting_SelfTest() {
  const char* v = Scripting_Version();
  return v != nullptr && v[0] != '\0';
}
}
