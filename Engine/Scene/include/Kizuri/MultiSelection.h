#pragma once
#include "Kizuri/Scene.h"
#include <vector>
namespace Kizuri {
class SelectionSet {
public:
  SelectionSet();
  void Select(EntityId id);
  void Add(EntityId id);
  void Toggle(EntityId id);
  void Remove(EntityId id);
  void Clear();
  size_t Count() const;
  bool IsEmpty() const;
  bool HasSelection() const;
  bool Contains(EntityId id) const;
  EntityId Get() const;
  EntityId Primary() const;
  EntityId At(size_t i) const;
  std::vector<EntityId> All() const;
  void OnEntityDeleted(EntityId id);
private:
  std::vector<EntityId> ids;
};
}
