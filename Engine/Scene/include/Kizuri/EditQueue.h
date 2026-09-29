#pragma once
#include "Kizuri/Scene.h"
#include <vector>
namespace Kizuri {
struct TransformEdit {
  EntityId target;
  Transform value;
};
class EditQueue {
public:
  void PushTransform(EntityId target, const Transform& value);
  size_t Pending() const;
  size_t ApplyAll(Scene& scene);
  void Clear();
private:
  std::vector<TransformEdit> edits;
};
}
