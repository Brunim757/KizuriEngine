#include "Kizuri/EditQueue.h"
#include "Kizuri/Undo.h"
#include <cstring>
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
void EditQueue::PushLight(EntityId target, const LightData& value) {
  if (!target.IsValid()) {
    return;
  }
  LightEdit e;
  e.target = target;
  e.value = value;
  lightEdits.push_back(e);
}
size_t EditQueue::Pending() const {
  return edits.size() + lightEdits.size();
}
size_t EditQueue::ApplyAll(Scene& scene, UndoStack& undo) {
  size_t applied = 0;
  for (size_t i = 0; i < edits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < edits.size(); ++j) {
      if (edits[j].target == edits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(edits[i].target);
    if (e == nullptr) {
      continue;
    }
    if (std::memcmp(&e->transform, &edits[i].value, sizeof(Transform)) == 0) {
      continue;
    }
    std::unique_ptr<Command> cmd(new EditTransformCmd(edits[i].target, e->transform, edits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  for (size_t i = 0; i < lightEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < lightEdits.size(); ++j) {
      if (lightEdits[j].target == lightEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(lightEdits[i].target);
    if (e == nullptr || !e->hasLight) {
      continue;
    }
    if (std::memcmp(&e->light, &lightEdits[i].value, sizeof(LightData)) == 0) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetLightCmd(lightEdits[i].target, e->light, lightEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  edits.clear();
  lightEdits.clear();
  return applied;
}
void EditQueue::Clear() {
  edits.clear();
  lightEdits.clear();
}
}
