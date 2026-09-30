#include "Kizuri/Assets/AssetDatabase.h"
#include "Kizuri/Assets/Guid.h"
#include "Kizuri/Assets/MeshImporter.h"
#include <cctype>
#include <filesystem>
#include <system_error>
namespace Kizuri {
ImportMeshTask::ImportMeshTask()
  : taskId(0)
  , outMutex(nullptr)
  , outQueue(nullptr) {
  m_SetSize = 1;
}
void ImportMeshTask::ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) {
  (void)range;
  (void)threadnum;
  ImportResult result;
  result.ok = false;
  result.taskId = taskId;
  result.sourcePath = sourcePath;
  result.meshPath = meshPath;
  if (ImportGltfMesh(sourcePath, keepGuid, result.data, nullptr)) {
    if (EncodeMeshFile(result.data, meshPath)) {
      result.ok = true;
    }
  }
  if (outMutex != nullptr && outQueue != nullptr) {
    std::lock_guard<std::mutex> lock(*outMutex);
    outQueue->push_back(result);
  }
}
AssetDatabase::AssetDatabase()
  : jobsReady(false)
  , nextTaskId(1) {
}
void AssetDatabase::SetAssetsDir(const std::string& dir) {
  assetsDir = dir;
}
const std::string& AssetDatabase::AssetsDir() const {
  return assetsDir;
}
void AssetDatabase::EnsureJobs() {
  if (!jobsReady) {
    jobsReady = jobs.Initialize();
  }
}
void AssetDatabase::EnqueueImport(const std::string& sourcePath, const std::string& meshPath, const std::string& keepGuid) {
  EnsureJobs();
  if (!jobsReady) {
    return;
  }
  std::unique_ptr<ImportMeshTask> task(new ImportMeshTask());
  task->sourcePath = sourcePath;
  task->meshPath = meshPath;
  task->keepGuid = keepGuid;
  task->taskId = nextTaskId++;
  task->outMutex = &completedMutex;
  task->outQueue = &completed;
  jobs.AddTask(task.get());
  pending.push_back(std::move(task));
}
MeshRecord* AssetDatabase::FindByGuid(const std::string& guid) {
  auto it = records.find(guid);
  return it == records.end() ? nullptr : &it->second;
}
void AssetDatabase::RefreshRecordState(MeshRecord& record) {
  if (!record.hasSource) {
    record.state = MeshAssetState::NoSource;
    return;
  }
  std::error_code ec;
  if (!std::filesystem::exists(record.sourcePath, ec) || ec) {
    record.state = MeshAssetState::SourceMissing;
    return;
  }
  uint64_t hash = 0;
  if (!Fnv1a64File(record.sourcePath, hash)) {
    record.state = MeshAssetState::SourceMissing;
    return;
  }
  record.state = (hash == record.sourceHash) ? MeshAssetState::Ready : MeshAssetState::Outdated;
}
void AssetDatabase::Scan() {
  relocated.clear();
  if (assetsDir.empty()) {
    return;
  }
  std::error_code ec;
  if (!std::filesystem::is_directory(assetsDir, ec) || ec) {
    return;
  }
  std::vector<std::string> glbFiles;
  std::vector<std::string> kzmeshFiles;
  for (std::filesystem::recursive_directory_iterator it(assetsDir, ec), end; it != end && !ec; it.increment(ec)) {
    if (!it->is_regular_file(ec) || ec) {
      continue;
    }
    std::string ext = it->path().extension().string();
    for (size_t i = 0; i < ext.size(); ++i) {
      ext[i] = static_cast<char>(tolower(ext[i]));
    }
    if (ext == ".glb" || ext == ".gltf") {
      glbFiles.push_back(it->path().string());
    } else if (ext == ".kzmesh") {
      kzmeshFiles.push_back(it->path().string());
    }
  }
  for (size_t i = 0; i < kzmeshFiles.size(); ++i) {
    int64_t mtime = 0;
    std::filesystem::file_time_type ft = std::filesystem::last_write_time(kzmeshFiles[i], ec);
    if (!ec) {
      mtime = static_cast<int64_t>(ft.time_since_epoch().count());
    }
    auto pg = pathToGuid.find(kzmeshFiles[i]);
    if (pg != pathToGuid.end()) {
      MeshRecord* known = FindByGuid(pg->second);
      auto st = seenMeshTime.find(kzmeshFiles[i]);
      if (known != nullptr && st != seenMeshTime.end() && st->second == mtime) {
        RefreshRecordState(*known);
        continue;
      }
    }
    MeshAssetData data;
    if (!DecodeMeshFile(kzmeshFiles[i], data) || data.guid.empty()) {
      continue;
    }
    MeshRecord* existing = FindByGuid(data.guid);
    if (existing != nullptr) {
      existing->meshPath = kzmeshFiles[i];
      existing->sourcePath = data.sourcePath;
      existing->sourceHash = data.sourceHash;
      existing->hasSource = data.hasSource;
      existing->data = data;
      existing->loaded = true;
      RefreshRecordState(*existing);
    } else {
      MeshRecord record;
      record.guid = data.guid;
      record.meshPath = kzmeshFiles[i];
      record.sourcePath = data.sourcePath;
      record.sourceHash = data.sourceHash;
      record.hasSource = data.hasSource;
      record.data = data;
      record.loaded = true;
      RefreshRecordState(record);
      records[record.guid] = record;
    }
    pathToGuid[kzmeshFiles[i]] = data.guid;
    seenMeshTime[kzmeshFiles[i]] = mtime;
  }
  for (size_t i = 0; i < glbFiles.size(); ++i) {
    bool known = false;
    for (auto& kv : records) {
      if (kv.second.hasSource && kv.second.sourcePath == glbFiles[i]) {
        known = true;
        RefreshRecordState(kv.second);
        break;
      }
    }
    if (known) {
      continue;
    }
    uint64_t probeHash = 0;
    bool reconnected = false;
    if (Fnv1a64File(glbFiles[i], probeHash)) {
      for (auto& kv : records) {
        if (kv.second.hasSource && kv.second.state == MeshAssetState::SourceMissing && kv.second.sourceHash == probeHash) {
          kv.second.sourcePath = glbFiles[i];
          kv.second.data.sourcePath = glbFiles[i];
          EncodeMeshFile(kv.second.data, kv.second.meshPath);
          kv.second.state = MeshAssetState::Ready;
          std::filesystem::path mp(kv.second.meshPath);
          relocated.push_back(mp.filename().string());
          reconnected = true;
          break;
        }
      }
    }
    if (reconnected) {
      continue;
    }
    bool flying = false;
    for (size_t k = 0; k < inflight.size(); ++k) {
      if (inflight[k] == glbFiles[i]) {
        flying = true;
        break;
      }
    }
    if (flying) {
      continue;
    }
    std::filesystem::path glb(glbFiles[i]);
    std::filesystem::path out = glb;
    out.replace_extension(".kzmesh");
    bool outExists = std::filesystem::exists(out, ec) && !ec;
    if (outExists) {
      MeshAssetData probe;
      if (DecodeMeshFile(out.string(), probe) && !probe.guid.empty()) {
        continue;
      }
    }
    EnqueueImport(glbFiles[i], out.string(), "");
    inflight.push_back(glbFiles[i]);
  }
  for (auto it = pathToGuid.begin(); it != pathToGuid.end();) {
    if (!std::filesystem::exists(it->first, ec) || ec) {
      seenMeshTime.erase(it->first);
      it = pathToGuid.erase(it);
    } else {
      ++it;
    }
  }
}
size_t AssetDatabase::DrainCompleted() {
  std::vector<ImportResult> results;
  {
    std::lock_guard<std::mutex> lock(completedMutex);
    results.swap(completed);
  }
  for (size_t i = 0; i < results.size(); ++i) {
    UpsertResult(results[i]);
    for (size_t p = 0; p < pending.size(); ++p) {
      if (pending[p]->taskId == results[i].taskId) {
        pending.erase(pending.begin() + p);
        break;
      }
    }
    for (size_t f = 0; f < inflight.size(); ++f) {
      if (inflight[f] == results[i].sourcePath) {
        inflight.erase(inflight.begin() + f);
        break;
      }
    }
  }
  return results.size();
}
size_t AssetDatabase::PendingImports() const {
  return pending.size();
}
void AssetDatabase::DrainBlocking() {
  for (size_t i = 0; i < pending.size(); ++i) {
    jobs.WaitForTask(pending[i].get());
  }
  pending.clear();
  DrainCompleted();
}
void AssetDatabase::UpsertResult(const ImportResult& result) {
  if (!result.ok || result.data.guid.empty()) {
    return;
  }
  MeshRecord* existing = FindByGuid(result.data.guid);
  if (existing != nullptr) {
    existing->meshPath = result.meshPath;
    existing->sourcePath = result.data.sourcePath;
    existing->sourceHash = result.data.sourceHash;
    existing->hasSource = result.data.hasSource;
    existing->data = result.data;
    existing->loaded = true;
    RefreshRecordState(*existing);
  } else {
    MeshRecord record;
    record.guid = result.data.guid;
    record.meshPath = result.meshPath;
    record.sourcePath = result.data.sourcePath;
    record.sourceHash = result.data.sourceHash;
    record.hasSource = result.data.hasSource;
    record.data = result.data;
    record.loaded = true;
    RefreshRecordState(record);
    records[record.guid] = record;
  }
}
const MeshRecord* AssetDatabase::GetByGuid(const std::string& guid) const {
  auto it = records.find(guid);
  return it == records.end() ? nullptr : &it->second;
}
const MeshRecord* AssetDatabase::GetByMeshPath(const std::string& path) const {
  for (auto& kv : records) {
    if (kv.second.meshPath == path) {
      return &kv.second;
    }
  }
  return nullptr;
}
std::vector<std::string> AssetDatabase::AllGuids() const {
  std::vector<std::string> out;
  for (auto& kv : records) {
    out.push_back(kv.first);
  }
  return out;
}
bool AssetDatabase::Reimport(const std::string& guid) {
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr || !record->hasSource) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(record->sourcePath, ec) || ec) {
    return false;
  }
  EnqueueImport(record->sourcePath, record->meshPath, guid);
  return true;
}
size_t AssetDatabase::RelocateMissing() {
  if (assetsDir.empty()) {
    return 0;
  }
  std::vector<std::string> glbFiles;
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator it(assetsDir, ec), end; it != end && !ec; it.increment(ec)) {
    if (!it->is_regular_file(ec) || ec) {
      continue;
    }
    std::string ext = it->path().extension().string();
    for (size_t i = 0; i < ext.size(); ++i) {
      ext[i] = static_cast<char>(tolower(ext[i]));
    }
    if (ext == ".glb" || ext == ".gltf") {
      glbFiles.push_back(it->path().string());
    }
  }
  size_t fixed = 0;
  for (auto& kv : records) {
    MeshRecord& record = kv.second;
    if (!record.hasSource || record.state != MeshAssetState::SourceMissing) {
      continue;
    }
    for (size_t i = 0; i < glbFiles.size(); ++i) {
      uint64_t hash = 0;
      if (!Fnv1a64File(glbFiles[i], hash) || hash != record.sourceHash) {
        continue;
      }
      record.sourcePath = glbFiles[i];
      record.data.sourcePath = glbFiles[i];
      EncodeMeshFile(record.data, record.meshPath);
      record.state = MeshAssetState::Ready;
      std::filesystem::path mp(record.meshPath);
      relocated.push_back(mp.filename().string());
      ++fixed;
      break;
    }
  }
  return fixed;
}
std::vector<std::string> AssetDatabase::TakeRelocated() {
  std::vector<std::string> out = relocated;
  relocated.clear();
  return out;
}
bool AssetDatabase::RenameAssetFile(const std::string& guid, const std::string& newFileName) {
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr || newFileName.empty()) {
    return false;
  }
  std::filesystem::path oldPath(record->meshPath);
  std::filesystem::path target = oldPath.parent_path() / newFileName;
  if (target.extension().string() != ".kzmesh") {
    target.replace_extension(".kzmesh");
  }
  std::error_code ec;
  if (std::filesystem::exists(target, ec)) {
    return false;
  }
  std::filesystem::rename(oldPath, target, ec);
  if (ec) {
    return false;
  }
  record->meshPath = target.string();
  return true;
}
bool AssetDatabase::DeleteAssetFile(const std::string& guid, bool force, const std::vector<RefUse>& refs, std::vector<std::string>& blockedBy) {
  blockedBy.clear();
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr) {
    return false;
  }
  for (size_t i = 0; i < refs.size(); ++i) {
    if (refs[i].assetGuid == guid) {
      blockedBy.push_back(refs[i].userLabel);
    }
  }
  if (!blockedBy.empty() && !force) {
    return false;
  }
  std::error_code ec;
  std::filesystem::remove(record->meshPath, ec);
  records.erase(guid);
  return true;
}
std::map<std::string, size_t> AssetDatabase::ComputeRefCounts(const std::vector<RefUse>& refs) const {
  std::map<std::string, size_t> counts;
  for (size_t i = 0; i < refs.size(); ++i) {
    if (!refs[i].assetGuid.empty()) {
      counts[refs[i].assetGuid]++;
    }
  }
  return counts;
}
}
