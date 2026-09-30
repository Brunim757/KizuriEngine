#include "Kizuri/Autosave.h"
#include "Kizuri/Scene.h"
#include "Kizuri/SceneSerializer.h"
#include <filesystem>
#include <system_error>
namespace Kizuri {
std::string RecoveryPathFor(const std::string& mainPath, const std::string& untitledDir) {
  if (mainPath.empty()) {
    std::filesystem::path p(untitledDir);
    p /= "KizuriUntitled.autosave.kzscene";
    return p.string();
  }
  return mainPath + ".autosave.kzscene";
}
bool ShouldOfferRecovery(const std::string& mainPath, const std::string& recoveryPath) {
  std::error_code ec;
  if (!std::filesystem::exists(recoveryPath, ec) || ec) {
    return false;
  }
  if (mainPath.empty()) {
    return true;
  }
  if (!std::filesystem::exists(mainPath, ec) || ec) {
    return true;
  }
  std::filesystem::file_time_type recTime = std::filesystem::last_write_time(recoveryPath, ec);
  if (ec) {
    return false;
  }
  std::filesystem::file_time_type mainTime = std::filesystem::last_write_time(mainPath, ec);
  if (ec) {
    return false;
  }
  return recTime > mainTime;
}
bool DeleteRecoveryFile(const std::string& recoveryPath) {
  if (recoveryPath.empty()) {
    return false;
  }
  std::error_code ec;
  return std::filesystem::remove(recoveryPath, ec) && !ec;
}
AutosaveManager::AutosaveManager()
  : interval(60.0)
  , elapsed(0.0) {
}
void AutosaveManager::SetInterval(double seconds) {
  interval = seconds < 0.0 ? 0.0 : seconds;
  elapsed = 0.0;
}
double AutosaveManager::Interval() const {
  return interval;
}
void AutosaveManager::ResetTimer() {
  elapsed = 0.0;
}
bool AutosaveManager::Update(double dt, const Scene& scene, const std::string& recoveryPath) {
  if (interval <= 0.0 || recoveryPath.empty()) {
    return false;
  }
  if (dt < 0.0) {
    dt = 0.0;
  }
  elapsed += dt;
  if (elapsed < interval) {
    return false;
  }
  elapsed = 0.0;
  if (!scene.IsDirty()) {
    return false;
  }
  return SaveSceneToFile(scene, recoveryPath);
}
}
