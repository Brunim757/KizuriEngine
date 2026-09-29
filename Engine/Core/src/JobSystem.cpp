#include "Kizuri/JobSystem.h"
#include <TaskScheduler.h>
namespace Kizuri {
JobSystem::JobSystem()
  : scheduler(nullptr)
  , initialized(false) {
}
JobSystem::~JobSystem() {
  Shutdown();
}
bool JobSystem::Initialize() {
  if (initialized) {
    return true;
  }
  scheduler = new enki::TaskScheduler();
  scheduler->Initialize();
  if (scheduler->GetNumTaskThreads() == 0) {
    delete scheduler;
    scheduler = nullptr;
    return false;
  }
  initialized = true;
  return true;
}
bool JobSystem::Initialize(uint32_t totalThreads) {
  if (initialized) {
    return true;
  }
  if (totalThreads == 0) {
    return false;
  }
  scheduler = new enki::TaskScheduler();
  scheduler->Initialize(totalThreads);
  initialized = true;
  return true;
}
void JobSystem::Shutdown() {
  if (!initialized) {
    if (scheduler != nullptr) {
      delete scheduler;
      scheduler = nullptr;
    }
    return;
  }
  scheduler->WaitforAllAndShutdown();
  delete scheduler;
  scheduler = nullptr;
  initialized = false;
}
uint32_t JobSystem::GetThreadCount() const {
  if (!initialized || scheduler == nullptr) {
    return 0;
  }
  return scheduler->GetNumTaskThreads();
}
void JobSystem::AddTask(enki::ITaskSet* task) {
  if (initialized && scheduler != nullptr && task != nullptr) {
    scheduler->AddTaskSetToPipe(task);
  }
}
void JobSystem::WaitForTask(enki::ITaskSet* task) {
  if (initialized && scheduler != nullptr && task != nullptr) {
    scheduler->WaitforTask(task);
  }
}
bool JobSystem::IsInitialized() const {
  return initialized;
}
}
