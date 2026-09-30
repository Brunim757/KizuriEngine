#pragma once
#include "Kizuri/Assets/MeshCodec.h"
#include "Kizuri/Assets/TexCodec.h"
#include "Kizuri/JobSystem.h"
#include "Kizuri/RHI.h"
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
enum class TextureAssetState {
  Ready,
  Outdated,
  NoSource,
  SourceMissing
};
struct TextureRecord {
  std::string guid;
  std::string texPath;
  std::string sourcePath;
  uint64_t sourceHash;
  bool hasSource;
  TextureAssetState state;
  TextureAssetData data;
  bool loaded;
  RHITexture gpu;
  int residentLevels;
  int selectedLevels;
};
struct ImportTexResult {
  bool ok;
  size_t taskId;
  std::string sourcePath;
  std::string texPath;
  TextureAssetData data;
};
struct ImportTexTask : public enki::ITaskSet {
  ImportTexTask();
  void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override;
  std::string sourcePath;
  std::string texPath;
  std::string keepGuid;
  size_t taskId;
  std::mutex* outMutex;
  std::vector<ImportTexResult>* outQueue;
};
struct MeshUse {
  std::string meshGuid;
  float pos[3];
};
struct TexUpload {
  std::string guid;
  int mip;
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
  const TextureRecord* GetTexByGuid(const std::string& guid) const;
  std::vector<std::string> AllTexGuids() const;
  bool ReimportTexture(const std::string& guid);
  bool RenameTextureFile(const std::string& guid, const std::string& newFileName);
  bool DeleteTextureFile(const std::string& guid, bool force, std::vector<std::string>& blockedBy);
  std::map<std::string, size_t> ComputeTexRefCounts() const;
  static int SelectMipLevel(float dist, int mipCount);
  void UpdateStreaming(const float cameraPos[3], const std::vector<MeshUse>& uses);
  size_t DrainUploads(IRHI* rhi, size_t maxBytes);
private:
  std::string assetsDir;
  std::map<std::string, MeshRecord> records;
  JobSystem jobs;
  bool jobsReady;
  size_t nextTaskId;
  std::vector<std::unique_ptr<ImportMeshTask>> pending;
  std::vector<std::string> inflight;
  std::vector<std::unique_ptr<ImportTexTask>> pendingTex;
  std::vector<std::string> inflightTex;
  std::mutex completedTexMutex;
  std::vector<ImportTexResult> completedTex;
  std::map<std::string, TextureRecord> texRecords;
  std::map<std::string, int64_t> seenTexTime;
  std::map<std::string, std::string> texPathToGuid;
  std::vector<TexUpload> uploadQueue;
  std::mutex completedMutex;
  std::vector<ImportResult> completed;
  std::vector<std::string> relocated;
  std::map<std::string, int64_t> seenMeshTime;
  std::map<std::string, std::string> pathToGuid;
  void EnsureJobs();
  void EnqueueImport(const std::string& sourcePath, const std::string& meshPath, const std::string& keepGuid);
  void EnqueueTexImport(const std::string& sourcePath, const std::string& texPath, const std::string& keepGuid);
  void UpsertResult(const ImportResult& result);
  void UpsertTexResult(const ImportTexResult& result);
  MeshRecord* FindByGuid(const std::string& guid);
  void RefreshRecordState(MeshRecord& record);
  void RefreshTexRecordState(TextureRecord& record);
};
}
