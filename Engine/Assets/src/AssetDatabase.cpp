#include "Kizuri/Assets/AssetDatabase.h"
#include "Kizuri/Assets/Guid.h"
#include "Kizuri/Assets/MeshImporter.h"
#include "Kizuri/Assets/TexCodec.h"
#include "Kizuri/Assets/TextureImporter.h"
#include <cctype>
#include <cmath>
#include <filesystem>
#include <system_error>
namespace Kizuri {
namespace {
bool IsImageExt(const std::string& ext) {
  return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
}
RHITextureFormat ToRHIFormat(TexFormat fmt, bool srgb) {
  if (fmt == TexFormat::Bc1) {
    return srgb ? RHITextureFormat::BC1_UNORM_SRGB : RHITextureFormat::BC1_UNORM;
  }
  if (fmt == TexFormat::Bc3) {
    return srgb ? RHITextureFormat::BC3_UNORM_SRGB : RHITextureFormat::BC3_UNORM;
  }
  if (fmt == TexFormat::Bc5) {
    return RHITextureFormat::BC5_UNORM;
  }
  return srgb ? RHITextureFormat::RGBA8_UNORM_SRGB : RHITextureFormat::RGBA8_UNORM;
}
}
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
  std::filesystem::file_time_type ft = std::filesystem::last_write_time(record.sourcePath, ec);
  if (ec) {
    record.state = MeshAssetState::SourceMissing;
    return;
  }
  int64_t ticks = static_cast<int64_t>(ft.time_since_epoch().count());
  if (ticks == record.lastSeenSourceTime) {
    return;
  }
  uint64_t hash = 0;
  if (!Fnv1a64File(record.sourcePath, hash)) {
    record.state = MeshAssetState::SourceMissing;
    return;
  }
  record.lastSeenSourceTime = ticks;
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
  std::vector<std::string> imgFiles;
  std::vector<std::string> kztexFiles;
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
    } else if (IsImageExt(ext)) {
      imgFiles.push_back(it->path().string());
    } else if (ext == ".kztex") {
      kztexFiles.push_back(it->path().string());
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
      DropMeshGpu(*existing);
      existing->meshPath = kzmeshFiles[i];
      existing->sourcePath = data.sourcePath;
      existing->sourceHash = data.sourceHash;
      existing->hasSource = data.hasSource;
      existing->data = data;
      existing->loaded = true;
      existing->lastSeenSourceTime = 0;
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
      record.lastSeenSourceTime = 0;
      record.gpuVB = 0;
      record.gpuIB = 0;
      record.gpuCount = 0;
      record.gpuReady = false;
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
  for (size_t i = 0; i < kztexFiles.size(); ++i) {
    int64_t mtime = 0;
    std::filesystem::file_time_type ft = std::filesystem::last_write_time(kztexFiles[i], ec);
    if (!ec) {
      mtime = static_cast<int64_t>(ft.time_since_epoch().count());
    }
    auto pg = texPathToGuid.find(kztexFiles[i]);
    if (pg != texPathToGuid.end()) {
      auto tr = texRecords.find(pg->second);
      auto st = seenTexTime.find(kztexFiles[i]);
      if (tr != texRecords.end() && st != seenTexTime.end() && st->second == mtime) {
        RefreshTexRecordState(tr->second);
        continue;
      }
    }
    TextureAssetData data;
    if (!DecodeTextureFile(kztexFiles[i], data) || data.guid.empty()) {
      continue;
    }
    auto existing = texRecords.find(data.guid);
    if (existing != texRecords.end()) {
      DropTexGpu(existing->second);
      existing->second.texPath = kztexFiles[i];
      existing->second.sourcePath = data.sourcePath;
      existing->second.sourceHash = data.sourceHash;
      existing->second.hasSource = data.hasSource;
      existing->second.data = data;
      existing->second.loaded = true;
      existing->second.lastSeenSourceTime = 0;
      RefreshTexRecordState(existing->second);
    } else {
      TextureRecord record;
      record.guid = data.guid;
      record.texPath = kztexFiles[i];
      record.sourcePath = data.sourcePath;
      record.sourceHash = data.sourceHash;
      record.hasSource = data.hasSource;
      record.data = data;
      record.loaded = true;
      record.lastSeenSourceTime = 0;
      record.gpu = 0;
      record.residentLevels = 0;
      record.selectedLevels = 0;
      RefreshTexRecordState(record);
      texRecords[record.guid] = record;
    }
    texPathToGuid[kztexFiles[i]] = data.guid;
    seenTexTime[kztexFiles[i]] = mtime;
  }
  for (size_t i = 0; i < imgFiles.size(); ++i) {
    bool known = false;
    for (auto& kv : texRecords) {
      if (kv.second.hasSource && kv.second.sourcePath == imgFiles[i]) {
        known = true;
        RefreshTexRecordState(kv.second);
        break;
      }
    }
    if (known) {
      continue;
    }
    uint64_t probeHash = 0;
    bool reconnected = false;
    if (Fnv1a64File(imgFiles[i], probeHash)) {
      for (auto& kv : texRecords) {
        if (kv.second.hasSource && kv.second.state == TextureAssetState::SourceMissing && kv.second.sourceHash == probeHash) {
          kv.second.sourcePath = imgFiles[i];
          kv.second.data.sourcePath = imgFiles[i];
          EncodeTextureFile(kv.second.data, kv.second.texPath);
          kv.second.state = TextureAssetState::Ready;
          std::filesystem::path mp(kv.second.texPath);
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
    for (size_t k = 0; k < inflightTex.size(); ++k) {
      if (inflightTex[k] == imgFiles[i]) {
        flying = true;
        break;
      }
    }
    if (flying) {
      continue;
    }
    std::filesystem::path img(imgFiles[i]);
    std::filesystem::path out = img;
    out.replace_extension(".kztex");
    bool outExists = std::filesystem::exists(out, ec) && !ec;
    if (outExists) {
      TextureAssetData probe;
      if (DecodeTextureFile(out.string(), probe) && !probe.guid.empty()) {
        continue;
      }
    }
    EnqueueTexImport(imgFiles[i], out.string(), "");
    inflightTex.push_back(imgFiles[i]);
  }
  for (auto it = texPathToGuid.begin(); it != texPathToGuid.end();) {
    if (!std::filesystem::exists(it->first, ec) || ec) {
      seenTexTime.erase(it->first);
      it = texPathToGuid.erase(it);
    } else {
      ++it;
    }
  }
}
ImportTexTask::ImportTexTask()
  : taskId(0)
  , outMutex(nullptr)
  , outQueue(nullptr) {
  m_SetSize = 1;
}
void ImportTexTask::ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) {
  (void)range;
  (void)threadnum;
  ImportTexResult result;
  result.ok = false;
  result.taskId = taskId;
  result.sourcePath = sourcePath;
  result.texPath = texPath;
  if (ImportTextureFile(sourcePath, keepGuid, result.data)) {
    if (EncodeTextureFile(result.data, texPath)) {
      result.ok = true;
    }
  }
  if (outMutex != nullptr && outQueue != nullptr) {
    std::lock_guard<std::mutex> lock(*outMutex);
    outQueue->push_back(result);
  }
}
void AssetDatabase::EnqueueTexImport(const std::string& sourcePath, const std::string& texPath, const std::string& keepGuid) {
  EnsureJobs();
  if (!jobsReady) {
    return;
  }
  std::unique_ptr<ImportTexTask> task(new ImportTexTask());
  task->sourcePath = sourcePath;
  task->texPath = texPath;
  task->keepGuid = keepGuid;
  task->taskId = nextTaskId++;
  task->outMutex = &completedTexMutex;
  task->outQueue = &completedTex;
  jobs.AddTask(task.get());
  pendingTex.push_back(std::move(task));
}
void AssetDatabase::RefreshTexRecordState(TextureRecord& record) {
  if (!record.hasSource) {
    record.state = TextureAssetState::NoSource;
    return;
  }
  std::error_code ec;
  std::filesystem::file_time_type ft = std::filesystem::last_write_time(record.sourcePath, ec);
  if (ec) {
    record.state = TextureAssetState::SourceMissing;
    return;
  }
  int64_t ticks = static_cast<int64_t>(ft.time_since_epoch().count());
  if (ticks == record.lastSeenSourceTime) {
    return;
  }
  uint64_t hash = 0;
  if (!Fnv1a64File(record.sourcePath, hash)) {
    record.state = TextureAssetState::SourceMissing;
    return;
  }
  record.lastSeenSourceTime = ticks;
  record.state = (hash == record.sourceHash) ? TextureAssetState::Ready : TextureAssetState::Outdated;
}
void AssetDatabase::UpsertTexResult(const ImportTexResult& result) {
  if (!result.ok || result.data.guid.empty()) {
    return;
  }
  auto existing = texRecords.find(result.data.guid);
  if (existing != texRecords.end()) {
    DropTexGpu(existing->second);
    existing->second.texPath = result.texPath;
    existing->second.sourcePath = result.data.sourcePath;
    existing->second.sourceHash = result.data.sourceHash;
    existing->second.hasSource = result.data.hasSource;
    existing->second.data = result.data;
    existing->second.loaded = true;
    existing->second.lastSeenSourceTime = 0;
    RefreshTexRecordState(existing->second);
  } else {
    TextureRecord record;
    record.guid = result.data.guid;
    record.texPath = result.texPath;
    record.sourcePath = result.data.sourcePath;
    record.sourceHash = result.data.sourceHash;
    record.hasSource = result.data.hasSource;
    record.data = result.data;
    record.loaded = true;
    record.lastSeenSourceTime = 0;
    record.gpu = 0;
    record.residentLevels = 0;
    record.selectedLevels = 0;
    RefreshTexRecordState(record);
    texRecords[record.guid] = record;
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
  std::vector<ImportTexResult> texResults;
  {
    std::lock_guard<std::mutex> lock(completedTexMutex);
    texResults.swap(completedTex);
  }
  for (size_t i = 0; i < texResults.size(); ++i) {
    UpsertTexResult(texResults[i]);
    for (size_t p = 0; p < pendingTex.size(); ++p) {
      if (pendingTex[p]->taskId == texResults[i].taskId) {
        pendingTex.erase(pendingTex.begin() + p);
        break;
      }
    }
    for (size_t f = 0; f < inflightTex.size(); ++f) {
      if (inflightTex[f] == texResults[i].sourcePath) {
        inflightTex.erase(inflightTex.begin() + f);
        break;
      }
    }
  }
  return results.size() + texResults.size();
}
size_t AssetDatabase::PendingImports() const {
  return pending.size() + pendingTex.size();
}
void AssetDatabase::DrainBlocking() {
  for (size_t i = 0; i < pending.size(); ++i) {
    jobs.WaitForTask(pending[i].get());
  }
  pending.clear();
  for (size_t i = 0; i < pendingTex.size(); ++i) {
    jobs.WaitForTask(pendingTex[i].get());
  }
  pendingTex.clear();
  DrainCompleted();
}
void AssetDatabase::UpsertResult(const ImportResult& result) {
  if (!result.ok || result.data.guid.empty()) {
    return;
  }
  MeshRecord* existing = FindByGuid(result.data.guid);
  if (existing != nullptr) {
    DropMeshGpu(*existing);
    existing->meshPath = result.meshPath;
    existing->sourcePath = result.data.sourcePath;
    existing->sourceHash = result.data.sourceHash;
    existing->hasSource = result.data.hasSource;
    existing->data = result.data;
    existing->loaded = true;
    existing->lastSeenSourceTime = 0;
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
    record.lastSeenSourceTime = 0;
    record.gpuVB = 0;
    record.gpuIB = 0;
    record.gpuCount = 0;
    record.gpuReady = false;
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
const TextureRecord* AssetDatabase::GetTexByGuid(const std::string& guid) const {
  auto it = texRecords.find(guid);
  return it == texRecords.end() ? nullptr : &it->second;
}
std::vector<std::string> AssetDatabase::AllTexGuids() const {
  std::vector<std::string> out;
  for (auto& kv : texRecords) {
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
bool AssetDatabase::ReimportTexture(const std::string& guid) {
  auto it = texRecords.find(guid);
  if (it == texRecords.end() || !it->second.hasSource) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(it->second.sourcePath, ec) || ec) {
    return false;
  }
  EnqueueTexImport(it->second.sourcePath, it->second.texPath, guid);
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
  std::vector<std::string> imgFiles;
  for (std::filesystem::recursive_directory_iterator it(assetsDir, ec), end; it != end && !ec; it.increment(ec)) {
    if (!it->is_regular_file(ec) || ec) {
      continue;
    }
    std::string ext = it->path().extension().string();
    for (size_t i = 0; i < ext.size(); ++i) {
      ext[i] = static_cast<char>(tolower(ext[i]));
    }
    if (IsImageExt(ext)) {
      imgFiles.push_back(it->path().string());
    }
  }
  for (auto& kv : texRecords) {
    TextureRecord& record = kv.second;
    if (!record.hasSource || record.state != TextureAssetState::SourceMissing) {
      continue;
    }
    for (size_t i = 0; i < imgFiles.size(); ++i) {
      uint64_t hash = 0;
      if (!Fnv1a64File(imgFiles[i], hash) || hash != record.sourceHash) {
        continue;
      }
      record.sourcePath = imgFiles[i];
      record.data.sourcePath = imgFiles[i];
      EncodeTextureFile(record.data, record.texPath);
      record.state = TextureAssetState::Ready;
      std::filesystem::path mp(record.texPath);
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
  DropMeshGpu(*record);
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
bool AssetDatabase::RenameTextureFile(const std::string& guid, const std::string& newFileName) {
  auto it = texRecords.find(guid);
  if (it == texRecords.end() || newFileName.empty()) {
    return false;
  }
  std::filesystem::path oldPath(it->second.texPath);
  std::filesystem::path target = oldPath.parent_path() / newFileName;
  if (target.extension().string() != ".kztex") {
    target.replace_extension(".kztex");
  }
  std::error_code ec;
  if (std::filesystem::exists(target, ec)) {
    return false;
  }
  std::filesystem::rename(oldPath, target, ec);
  if (ec) {
    return false;
  }
  it->second.texPath = target.string();
  return true;
}
bool AssetDatabase::DeleteTextureFile(const std::string& guid, bool force, std::vector<std::string>& blockedBy) {
  blockedBy.clear();
  auto it = texRecords.find(guid);
  if (it == texRecords.end()) {
    return false;
  }
  for (auto& kv : records) {
    for (size_t m = 0; m < kv.second.data.materials.size(); ++m) {
      if (kv.second.data.materials[m].albedoTexGuid == guid) {
        std::filesystem::path mp(kv.second.meshPath);
        blockedBy.push_back(mp.filename().string());
        break;
      }
    }
  }
  if (!blockedBy.empty() && !force) {
    return false;
  }
  DropTexGpu(it->second);
  std::error_code ec;
  std::filesystem::remove(it->second.texPath, ec);
  texRecords.erase(it);
  return true;
}
std::map<std::string, size_t> AssetDatabase::ComputeTexRefCounts() const {
  std::map<std::string, size_t> counts;
  for (auto& kv : records) {
    for (size_t m = 0; m < kv.second.data.materials.size(); ++m) {
      const std::string& tg = kv.second.data.materials[m].albedoTexGuid;
      if (!tg.empty()) {
        counts[tg]++;
      }
    }
  }
  return counts;
}
int AssetDatabase::SelectMipLevel(float dist, int mipCount) {
  if (mipCount <= 0) {
    return 0;
  }
  if (dist < 0.0f) {
    dist = 0.0f;
  }
  int want = mipCount;
  if (dist < 15.0f) {
    want = mipCount;
  } else if (dist < 40.0f) {
    want = mipCount > 1 ? mipCount - 1 : 1;
    if (want < 1) {
      want = 1;
    }
  } else if (dist < 100.0f) {
    want = 2;
  } else {
    want = 1;
  }
  if (want > mipCount) {
    want = mipCount;
  }
  if (want < 1) {
    want = 1;
  }
  return want;
}
void AssetDatabase::UpdateStreaming(const float cameraPos[3], const std::vector<MeshUse>& uses) {
  for (size_t u = 0; u < uses.size(); ++u) {
    auto mr = records.find(uses[u].meshGuid);
    if (mr == records.end() || !mr->second.loaded) {
      continue;
    }
    float dx = uses[u].pos[0] - cameraPos[0];
    float dy = uses[u].pos[1] - cameraPos[1];
    float dz = uses[u].pos[2] - cameraPos[2];
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    for (size_t m = 0; m < mr->second.data.materials.size(); ++m) {
      const std::string& tg = mr->second.data.materials[m].albedoTexGuid;
      if (tg.empty()) {
        continue;
      }
      auto tr = texRecords.find(tg);
      if (tr == texRecords.end() || !tr->second.loaded) {
        continue;
      }
      int want = SelectMipLevel(dist, static_cast<int>(tr->second.data.mips.size()));
      if (want > tr->second.selectedLevels) {
        tr->second.selectedLevels = want;
        for (int mip = tr->second.residentLevels; mip < want; ++mip) {
          bool queued = false;
          for (size_t q = 0; q < uploadQueue.size(); ++q) {
            if (uploadQueue[q].guid == tg && uploadQueue[q].mip == mip) {
              queued = true;
              break;
            }
          }
          if (!queued) {
            TexUpload up;
            up.guid = tg;
            up.mip = mip;
            uploadQueue.push_back(up);
          }
        }
      }
    }
  }
}
size_t AssetDatabase::DrainUploads(IRHI* rhi, size_t maxBytes) {
  if (rhi == nullptr || maxBytes == 0) {
    return 0;
  }
  size_t done = 0;
  size_t used = 0;
  while (!uploadQueue.empty()) {
    TexUpload up = uploadQueue.front();
    auto tr = texRecords.find(up.guid);
    if (tr == texRecords.end() || !tr->second.loaded) {
      uploadQueue.erase(uploadQueue.begin());
      continue;
    }
    TextureRecord& rec = tr->second;
    if (up.mip < 0 || up.mip >= static_cast<int>(rec.data.mips.size())) {
      uploadQueue.erase(uploadQueue.begin());
      continue;
    }
    if (rec.gpu == 0) {
      rec.gpu = rhi->CreateTexture2D(static_cast<int>(rec.data.width), static_cast<int>(rec.data.height), static_cast<int>(rec.data.mips.size()), ToRHIFormat(rec.data.format, rec.data.srgb));
      rec.residentLevels = 0;
      if (rec.gpu == 0) {
        uploadQueue.erase(uploadQueue.begin());
        continue;
      }
    }
    const TextureMipData& mip = rec.data.mips[static_cast<size_t>(up.mip)];
    size_t cost = mip.data.size();
    if (used + cost > maxBytes && done > 0) {
      break;
    }
    if (!rhi->UpdateTextureMip(rec.gpu, up.mip, static_cast<int>(mip.width), static_cast<int>(mip.height), mip.rowPitch, mip.data.data(), mip.data.size())) {
      uploadQueue.erase(uploadQueue.begin());
      continue;
    }
    used += cost;
    if (up.mip == rec.residentLevels) {
      rec.residentLevels = up.mip + 1;
    }
    uploadQueue.erase(uploadQueue.begin());
    ++done;
  }
  return done;
}
void AssetDatabase::SetGpuRHI(IRHI* rhi) {
  gpuRhi = rhi;
}
void AssetDatabase::DropMeshGpu(MeshRecord& record) {
  if (gpuRhi != nullptr) {
    if (record.gpuVB != 0) {
      gpuRhi->DestroyBuffer(record.gpuVB);
    }
    if (record.gpuIB != 0) {
      gpuRhi->DestroyBuffer(record.gpuIB);
    }
  }
  record.gpuVB = 0;
  record.gpuIB = 0;
  record.gpuCount = 0;
  record.gpuReady = false;
  record.stageInterleaved.clear();
  record.stageVBDone = 0;
  record.stageIBDone = 0;
}
void AssetDatabase::DropTexGpu(TextureRecord& record) {
  if (gpuRhi != nullptr && record.gpu != 0) {
    gpuRhi->DestroyTexture(record.gpu);
  }
  record.gpu = 0;
  record.residentLevels = 0;
}
bool AssetDatabase::EnsureMeshGpu(const std::string& guid, size_t maxBytes) {
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr || !record->loaded || gpuRhi == nullptr) {
    return false;
  }
  if (record->gpuReady) {
    return true;
  }
  if (maxBytes == 0) {
    return false;
  }
  size_t vertexCount = record->data.positions.size() / 3;
  if (vertexCount == 0 || record->data.indices.empty()) {
    return false;
  }
  size_t vbBytes = vertexCount * 32;
  size_t ibBytes = record->data.indices.size() * 4;
  if (record->gpuVB == 0) {
    record->stageInterleaved.clear();
    record->stageInterleaved.reserve(vertexCount * 8);
    for (size_t i = 0; i < vertexCount; ++i) {
      record->stageInterleaved.push_back(record->data.positions[i * 3 + 0]);
      record->stageInterleaved.push_back(record->data.positions[i * 3 + 1]);
      record->stageInterleaved.push_back(record->data.positions[i * 3 + 2]);
      record->stageInterleaved.push_back(record->data.normals[i * 3 + 0]);
      record->stageInterleaved.push_back(record->data.normals[i * 3 + 1]);
      record->stageInterleaved.push_back(record->data.normals[i * 3 + 2]);
      record->stageInterleaved.push_back(record->data.uvs[i * 2 + 0]);
      record->stageInterleaved.push_back(record->data.uvs[i * 2 + 1]);
    }
    RHIBuffer vb = gpuRhi->CreateBufferEmpty(static_cast<uint64_t>(vbBytes), 32, false);
    if (vb == 0) {
      record->stageInterleaved.clear();
      return false;
    }
    RHIBuffer ib = gpuRhi->CreateBufferEmpty(static_cast<uint64_t>(ibBytes), 4, true);
    if (ib == 0) {
      gpuRhi->DestroyBuffer(vb);
      record->stageInterleaved.clear();
      return false;
    }
    record->gpuVB = vb;
    record->gpuIB = ib;
    record->stageVBDone = 0;
    record->stageIBDone = 0;
  }
  size_t budget = maxBytes;
  const unsigned char* vbytes = reinterpret_cast<const unsigned char*>(record->stageInterleaved.data());
  while (budget > 0 && record->stageVBDone < vbBytes) {
    size_t chunk = vbBytes - record->stageVBDone;
    if (chunk > budget) {
      chunk = budget;
    }
    if (!gpuRhi->UpdateBufferRange(record->gpuVB, record->stageVBDone, vbytes + record->stageVBDone, chunk)) {
      return false;
    }
    record->stageVBDone += chunk;
    budget -= chunk;
  }
  const unsigned char* ibytes = reinterpret_cast<const unsigned char*>(record->data.indices.data());
  while (budget > 0 && record->stageIBDone < ibBytes) {
    size_t chunk = ibBytes - record->stageIBDone;
    if (chunk > budget) {
      chunk = budget;
    }
    if (!gpuRhi->UpdateBufferRange(record->gpuIB, record->stageIBDone, ibytes + record->stageIBDone, chunk)) {
      return false;
    }
    record->stageIBDone += chunk;
    budget -= chunk;
  }
  if (record->stageVBDone == vbBytes && record->stageIBDone == ibBytes) {
    record->gpuCount = static_cast<uint32_t>(record->data.indices.size());
    record->gpuReady = true;
    record->stageInterleaved.clear();
    return true;
  }
  return false;
}
bool AssetDatabase::SetSourcePath(const std::string& guid, const std::string& newSourcePath) {
  MeshRecord* record = FindByGuid(guid);
  if (record == nullptr || newSourcePath.empty()) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(newSourcePath, ec) || ec) {
    return false;
  }
  record->sourcePath = newSourcePath;
  record->data.sourcePath = newSourcePath;
  record->hasSource = true;
  record->data.hasSource = true;
  record->lastSeenSourceTime = 0;
  EncodeMeshFile(record->data, record->meshPath);
  RefreshRecordState(*record);
  return true;
}
bool AssetDatabase::SetTexSourcePath(const std::string& guid, const std::string& newSourcePath) {
  auto it = texRecords.find(guid);
  if (it == texRecords.end() || newSourcePath.empty()) {
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(newSourcePath, ec) || ec) {
    return false;
  }
  it->second.sourcePath = newSourcePath;
  it->second.data.sourcePath = newSourcePath;
  it->second.hasSource = true;
  it->second.data.hasSource = true;
  it->second.lastSeenSourceTime = 0;
  EncodeTextureFile(it->second.data, it->second.texPath);
  RefreshTexRecordState(it->second);
  return true;
}
}
