#pragma once
#include "Kizuri/Assets/MeshCodec.h"
#include "Kizuri/JobSystem.h"
#include <TaskScheduler.h>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
namespace Kizuri {
enum class MeshAssetState {
  Ready,
  Outdated,
  NoSource,
  SourceMissing
};
struct MeshRecord {
  std::string guid;
  std::string meshPath;
  std::string sourcePath;
  uint64_t sourceHash;
  bool hasSource;
  MeshAssetState state;
  MeshAssetData data;
  bool loaded;
};
struct RefUse {
  std::string userLabel;
  std::string assetGuid;
};
struct ImportResult {
  bool ok;
  size_t taskId;
  std::string sourcePath;
  std::string meshPath;
  MeshAssetData data;
  std::string warning;
};
struct ImportMeshTask : public enki::ITaskSet {
  ImportMeshTask();
  void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override;
  std::string sourcePath;
  std::string meshPath;
  std::string keepGuid;
  size_t taskId;
  std::mutex* outMutex;
  std::vector<ImportResult>* outQueue;
};
class AssetDatabase {
public:
  AssetDatabase();
  void SetAssetsDir(const std::string& dir);
  const std::string& AssetsDir() const;
  void Scan();
  size_t DrainCompleted();
  size_t PendingImports() const;
  void DrainBlocking();
  const MeshRecord* GetByGuid(const std::string& guid) const;
  const MeshRecord* GetByMeshPath(const std::string& path) const;
  std::vector<std::string> AllGuids() const;
  bool Reimport(const std::string& guid);
  size_t RelocateMissing();
  std::vector<std::string> TakeRelocated();
  bool RenameAssetFile(const std::string& guid, const std::string& newFileName);
  bool DeleteAssetFile(const std::string& guid, bool force, const std::vector<RefUse>& refs, std::vector<std::string>& blockedBy);
  std::map<std::string, size_t> ComputeRefCounts(const std::vector<RefUse>& refs) const;
private:
  std::string assetsDir;
  std::map<std::string, MeshRecord> records;
  JobSystem jobs;
  bool jobsReady;
  size_t nextTaskId;
  std::vector<std::unique_ptr<ImportMeshTask>> pending;
  std::vector<std::string> inflight;
  std::mutex completedMutex;
  std::vector<ImportResult> completed;
  std::vector<std::string> relocated;
  std::map<std::string, int64_t> seenMeshTime;
  std::map<std::string, std::string> pathToGuid;
  void EnsureJobs();
  void EnqueueImport(const std::string& sourcePath, const std::string& meshPath, const std::string& keepGuid);
  void UpsertResult(const ImportResult& result);
  MeshRecord* FindByGuid(const std::string& guid);
  void RefreshRecordState(MeshRecord& record);
};
}
