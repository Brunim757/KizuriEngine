#pragma once
#include <string>
namespace Kizuri {
class Scene;
std::string RecoveryPathFor(const std::string& mainPath, const std::string& untitledDir);
bool ShouldOfferRecovery(const std::string& mainPath, const std::string& recoveryPath);
bool DeleteRecoveryFile(const std::string& recoveryPath);
class AutosaveManager {
public:
  AutosaveManager();
  void SetInterval(double seconds);
  double Interval() const;
  void ResetTimer();
  bool Update(double dt, const Scene& scene, const std::string& recoveryPath);
private:
  double interval;
  double elapsed;
};
}
