#pragma once
#include "Kizuri/Scene.h"
#include <vector>
namespace Kizuri {
class UndoStack;
struct TransformEdit {
  EntityId target;
  Transform value;
};
struct LightEdit {
  EntityId target;
  LightData value;
};
class EditQueue {
public:
  void PushTransform(EntityId target, const Transform& value);
  void PushLight(EntityId target, const LightData& value);
  size_t Pending() const;
  size_t ApplyAll(Scene& scene, UndoStack& undo);
  void Clear();
private:
  std::vector<TransformEdit> edits;
  std::vector<LightEdit> lightEdits;
};
}
