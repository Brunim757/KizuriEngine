#include "Kizuri/Physics.h"
namespace Kizuri {
const char* Physics_Version() {
  return "0.0.1-fase0";
}
bool Physics_SelfTest() {
  const char* v = Physics_Version();
  return v != nullptr && v[0] != '\0';
}
}
