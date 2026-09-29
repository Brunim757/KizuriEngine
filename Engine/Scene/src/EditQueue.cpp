#include "Kizuri/EditQueue.h"
namespace Kizuri {
void EditQueue::PushTransform(EntityId target, const Transform& value) {
  if (!target.IsValid()) {
    return;
  }
  TransformEdit e;
  e.target = target;
  e.value = value;
  edits.push_back(e);
}
size_t EditQueue::Pending() const {
  return edits.size();
}
size_t EditQueue::ApplyAll(Scene& scene) {
  size_t applied = 0;
  for (size_t i = 0; i < edits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < edits.size(); ++j) {
      if (edits[j].target == edits[i].target) {
        last = false;
        break;
      }
    }
    if (last && scene.SetTransform(edits[i].target, edits[i].value)) {
      ++applied;
    }
  }
  edits.clear();
  return applied;
}
void EditQueue::Clear() {
  edits.clear();
}
}
