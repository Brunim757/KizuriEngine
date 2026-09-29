#include "Kizuri/Reflection.h"
#include "Kizuri/Scene.h"
namespace Kizuri {
TypeRegistry::TypeRegistry() {
}
TypeRegistry& TypeRegistry::Instance() {
  static TypeRegistry instance;
  return instance;
}
bool TypeRegistry::Register(const StructDesc& desc) {
  if (desc.name.empty() || Find(desc.name) != nullptr) {
    return false;
  }
  structs.push_back(desc);
  return true;
}
const StructDesc* TypeRegistry::Find(const std::string& name) const {
  for (size_t i = 0; i < structs.size(); ++i) {
    if (structs[i].name == name) {
      return &structs[i];
    }
  }
  return nullptr;
}
void TypeRegistry::Clear() {
  structs.clear();
}
void RegisterCoreTypes() {
  TypeRegistry& reg = TypeRegistry::Instance();
  if (reg.Find("Transform") != nullptr) {
    return;
  }
  StructDesc desc;
  desc.name = "Transform";
  desc.size = sizeof(Transform);
  FieldDesc pos;
  pos.name = "position";
  pos.kind = FieldKind::Float3;
  pos.offset = offsetof(Transform, position);
  desc.fields.push_back(pos);
  FieldDesc rot;
  rot.name = "rotation";
  rot.kind = FieldKind::Float3;
  rot.offset = offsetof(Transform, rotation);
  desc.fields.push_back(rot);
  FieldDesc scl;
  scl.name = "scale";
  scl.kind = FieldKind::Float3;
  scl.offset = offsetof(Transform, scale);
  desc.fields.push_back(scl);
  reg.Register(desc);
}
}
