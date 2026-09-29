#pragma once
#include <string>
#include <vector>
#include <stddef.h>
namespace Kizuri {
enum class FieldKind {
  Float,
  Float3,
  Text
};
struct FieldDesc {
  std::string name;
  FieldKind kind;
  size_t offset;
};
struct StructDesc {
  std::string name;
  size_t size;
  std::vector<FieldDesc> fields;
};
class TypeRegistry {
public:
  static TypeRegistry& Instance();
  bool Register(const StructDesc& desc);
  const StructDesc* Find(const std::string& name) const;
  void Clear();
private:
  TypeRegistry();
  std::vector<StructDesc> structs;
};
void RegisterCoreTypes();
}
