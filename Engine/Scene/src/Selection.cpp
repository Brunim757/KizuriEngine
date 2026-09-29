#include "Kizuri/Selection.h"
namespace Kizuri {
SingleSelection::SingleSelection()
  : selected(EntityId::Invalid()) {
}
void SingleSelection::Select(EntityId id) {
  selected = id;
}
void SingleSelection::Clear() {
  selected = EntityId::Invalid();
}
EntityId SingleSelection::Get() const {
  return selected;
}
bool SingleSelection::HasSelection() const {
  return selected.IsValid();
}
bool SingleSelection::IsSelected(EntityId id) const {
  return selected.IsValid() && selected == id;
}
void SingleSelection::OnEntityDeleted(EntityId id) {
  if (selected.IsValid() && selected == id) {
    selected = EntityId::Invalid();
  }
}
}
