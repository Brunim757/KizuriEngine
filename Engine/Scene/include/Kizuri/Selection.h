#pragma once
#include "Kizuri/Scene.h"
namespace Kizuri {
class SingleSelection {
public:
  SingleSelection();
  void Select(EntityId id);
  void Clear();
  EntityId Get() const;
  bool HasSelection() const;
  bool IsSelected(EntityId id) const;
  void OnEntityDeleted(EntityId id);
private:
  EntityId selected;
};
}
