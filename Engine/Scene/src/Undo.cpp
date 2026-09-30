#include "Kizuri/Undo.h"
#include <cstring>
namespace Kizuri {
bool EntitySnapshot::Capture(const Scene& scene, EntityId root) {
  nodes.clear();
  originalParent = EntityId::Invalid();
  const Entity* r = scene.Get(root);
  if (r == nullptr) {
    return false;
  }
  originalParent = r->parent;
  std::vector<EntityId> stack;
  std::vector<long> parentStack;
  stack.push_back(root);
  parentStack.push_back(-1);
  while (!stack.empty()) {
    EntityId cur = stack.back();
    stack.pop_back();
    long par = parentStack.back();
    parentStack.pop_back();
    const Entity* e = scene.Get(cur);
    if (e == nullptr) {
      return false;
    }
    SnapshotNode node;
    node.name = e->name;
    node.transform = e->transform;
    node.parent = par;
    long idx = static_cast<long>(nodes.size());
    nodes.push_back(node);
    for (size_t i = 0; i < e->children.size(); ++i) {
      stack.push_back(e->children[i]);
      parentStack.push_back(idx);
    }
  }
  return !nodes.empty();
}
EntityId EntitySnapshot::Restore(Scene& scene, EntityId newParent) const {
  if (nodes.empty()) {
    return EntityId::Invalid();
  }
  std::vector<EntityId> created;
  for (size_t i = 0; i < nodes.size(); ++i) {
    EntityId id = scene.CreateEntity(nodes[i].name);
    if (!id.IsValid()) {
      return EntityId::Invalid();
    }
    scene.SetTransform(id, nodes[i].transform);
    created.push_back(id);
  }
  for (size_t i = 1; i < nodes.size(); ++i) {
    if (nodes[i].parent >= 0) {
      scene.SetParent(created[i], created[static_cast<size_t>(nodes[i].parent)]);
    }
  }
  if (newParent.IsValid()) {
    scene.SetParent(created[0], newParent);
  }
  return created[0];
}
CreateEntityCmd::CreateEntityCmd(const std::string& n, const Transform& t, EntityId p)
  : name(n)
  , transform(t)
  , parent(p)
  , live(EntityId::Invalid()) {
}
bool CreateEntityCmd::Apply(Scene& scene) {
  EntityId useParent = parent.IsValid() && scene.Has(parent) ? parent : EntityId::Invalid();
  live = scene.CreateEntity(name.empty() ? "Entity" : name);
  if (!live.IsValid()) {
    return false;
  }
  scene.SetTransform(live, transform);
  if (useParent.IsValid()) {
    scene.SetParent(live, useParent);
  }
  return true;
}
bool CreateEntityCmd::Revert(Scene& scene) {
  if (!live.IsValid() || !scene.Has(live)) {
    return false;
  }
  return scene.DeleteEntity(live);
}
const char* CreateEntityCmd::Name() const {
  return "Create Entity";
}
DeleteEntityCmd::DeleteEntityCmd(EntityId t)
  : target(t)
  , live(t)
  , captured(false) {
}
bool DeleteEntityCmd::Apply(Scene& scene) {
  if (!captured) {
    if (!snapshot.Capture(scene, target)) {
      return false;
    }
    captured = true;
    live = target;
  }
  if (!live.IsValid() || !scene.Has(live)) {
    return false;
  }
  return scene.DeleteEntity(live);
}
bool DeleteEntityCmd::Revert(Scene& scene) {
  EntityId useParent = snapshot.originalParent.IsValid() && scene.Has(snapshot.originalParent) ? snapshot.originalParent : EntityId::Invalid();
  live = snapshot.Restore(scene, useParent);
  return live.IsValid();
}
const char* DeleteEntityCmd::Name() const {
  return "Delete Entity";
}
DuplicateCmd::DuplicateCmd(EntityId s)
  : source(s)
  , live(EntityId::Invalid())
  , captured(false) {
}
bool DuplicateCmd::Apply(Scene& scene) {
  if (!captured) {
    const Entity* e = scene.Get(source);
    if (e == nullptr) {
      return false;
    }
    if (!snapshot.Capture(scene, source)) {
      return false;
    }
    for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
      snapshot.nodes[i].name += " Copy";
    }
    captured = true;
  }
  const Entity* e = scene.Get(source);
  EntityId useParent = (e != nullptr && e->parent.IsValid() && scene.Has(e->parent)) ? e->parent : EntityId::Invalid();
  if (!useParent.IsValid() && snapshot.originalParent.IsValid() && scene.Has(snapshot.originalParent)) {
    useParent = snapshot.originalParent;
  }
  live = snapshot.Restore(scene, useParent);
  return live.IsValid();
}
bool DuplicateCmd::Revert(Scene& scene) {
  if (!live.IsValid() || !scene.Has(live)) {
    return false;
  }
  return scene.DeleteEntity(live);
}
const char* DuplicateCmd::Name() const {
  return "Duplicate Entity";
}
EditTransformCmd::EditTransformCmd(EntityId t, const Transform& b, const Transform& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool EditTransformCmd::Apply(Scene& scene) {
  if (std::memcmp(&before, &after, sizeof(Transform)) == 0) {
    return scene.Has(target);
  }
  return scene.SetTransform(target, after);
}
bool EditTransformCmd::Revert(Scene& scene) {
  return scene.SetTransform(target, before);
}
const char* EditTransformCmd::Name() const {
  return "Edit Transform";
}
RenameCmd::RenameCmd(EntityId t, const std::string& b, const std::string& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool RenameCmd::Apply(Scene& scene) {
  return scene.RenameEntity(target, after);
}
bool RenameCmd::Revert(Scene& scene) {
  return scene.RenameEntity(target, before);
}
const char* RenameCmd::Name() const {
  return "Rename Entity";
}
SetParentCmd::SetParentCmd(EntityId c, EntityId b, EntityId a)
  : child(c)
  , before(b)
  , after(a) {
}
bool SetParentCmd::Apply(Scene& scene) {
  return scene.SetParent(child, after);
}
bool SetParentCmd::Revert(Scene& scene) {
  return scene.SetParent(child, before);
}
const char* SetParentCmd::Name() const {
  return "Set Parent";
}
MultiEditTransformCmd::MultiEditTransformCmd() {
}
void MultiEditTransformCmd::Add(EntityId target, const Transform& before, const Transform& after) {
  if (!target.IsValid()) {
    return;
  }
  MultiTransformEdit e;
  e.target = target;
  e.before = before;
  e.after = after;
  edits.push_back(e);
}
bool MultiEditTransformCmd::Empty() const {
  return edits.empty();
}
bool MultiEditTransformCmd::Apply(Scene& scene) {
  size_t done = 0;
  for (size_t i = 0; i < edits.size(); ++i) {
    if (scene.SetTransform(edits[i].target, edits[i].after)) {
      ++done;
    }
  }
  return done > 0;
}
bool MultiEditTransformCmd::Revert(Scene& scene) {
  size_t done = 0;
  for (size_t i = 0; i < edits.size(); ++i) {
    if (scene.SetTransform(edits[i].target, edits[i].before)) {
      ++done;
    }
  }
  return done > 0;
}
const char* MultiEditTransformCmd::Name() const {
  return "Edit Transforms";
}
UndoStack::UndoStack() {
}
bool UndoStack::Execute(std::unique_ptr<Command> cmd, Scene& scene) {
  if (cmd == nullptr || !cmd->Apply(scene)) {
    return false;
  }
  undo.push_back(std::move(cmd));
  redo.clear();
  return true;
}
bool UndoStack::Commit(std::unique_ptr<Command> cmd) {
  if (cmd == nullptr) {
    return false;
  }
  undo.push_back(std::move(cmd));
  redo.clear();
  return true;
}
bool UndoStack::Undo(Scene& scene) {
  if (undo.empty()) {
    return false;
  }
  std::unique_ptr<Command> cmd = std::move(undo.back());
  undo.pop_back();
  cmd->Revert(scene);
  redo.push_back(std::move(cmd));
  return true;
}
bool UndoStack::Redo(Scene& scene) {
  if (redo.empty()) {
    return false;
  }
  std::unique_ptr<Command> cmd = std::move(redo.back());
  redo.pop_back();
  cmd->Apply(scene);
  undo.push_back(std::move(cmd));
  return true;
}
bool UndoStack::CanUndo() const {
  return !undo.empty();
}
bool UndoStack::CanRedo() const {
  return !redo.empty();
}
const char* UndoStack::UndoName() const {
  return undo.empty() ? "" : undo.back()->Name();
}
const char* UndoStack::RedoName() const {
  return redo.empty() ? "" : redo.back()->Name();
}
void UndoStack::Clear() {
  undo.clear();
  redo.clear();
}
size_t UndoStack::UndoDepth() const {
  return undo.size();
}
size_t UndoStack::RedoDepth() const {
  return redo.size();
}
}
