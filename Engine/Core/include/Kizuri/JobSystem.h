#pragma once
#include <stdint.h>
namespace enki {
class TaskScheduler;
struct ITaskSet;
}
namespace Kizuri {
class JobSystem {
public:
  JobSystem();
  ~JobSystem();
  bool Initialize();
  bool Initialize(uint32_t totalThreads);
  void Shutdown();
  uint32_t GetThreadCount() const;
  void AddTask(enki::ITaskSet* task);
  void WaitForTask(enki::ITaskSet* task);
  bool IsInitialized() const;
private:
  enki::TaskScheduler* scheduler;
  bool initialized;
  JobSystem(const JobSystem&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;
};
}
