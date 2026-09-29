#include "Kizuri/MultiSelection.h"
namespace Kizuri {
SelectionSet::SelectionSet() {
}
void SelectionSet::Select(EntityId id) {
  ids.clear();
  if (id.IsValid()) {
    ids.push_back(id);
  }
}
void SelectionSet::Add(EntityId id) {
  if (!id.IsValid() || Contains(id)) {
    return;
  }
  ids.push_back(id);
}
void SelectionSet::Toggle(EntityId id) {
  if (!id.IsValid()) {
    return;
  }
  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] == id) {
      ids.erase(ids.begin() + i);
      return;
    }
  }
  ids.push_back(id);
}
void SelectionSet::Remove(EntityId id) {
  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] == id) {
      ids.erase(ids.begin() + i);
      return;
    }
  }
}
void SelectionSet::Clear() {
  ids.clear();
}
size_t SelectionSet::Count() const {
  return ids.size();
}
bool SelectionSet::IsEmpty() const {
  return ids.empty();
}
bool SelectionSet::HasSelection() const {
  return !ids.empty();
}
bool SelectionSet::Contains(EntityId id) const {
  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] == id) {
      return true;
    }
  }
  return false;
}
EntityId SelectionSet::Get() const {
  return Primary();
}
EntityId SelectionSet::Primary() const {
  if (ids.empty()) {
    return EntityId::Invalid();
  }
  return ids[0];
}
EntityId SelectionSet::At(size_t i) const {
  if (i >= ids.size()) {
    return EntityId::Invalid();
  }
  return ids[i];
}
std::vector<EntityId> SelectionSet::All() const {
  return ids;
}
void SelectionSet::OnEntityDeleted(EntityId id) {
  Remove(id);
}
}
