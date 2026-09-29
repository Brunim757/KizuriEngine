#pragma once
namespace Kizuri {
class OriginRebaser {
public:
  OriginRebaser();
  void SetOrigin(double x, double y, double z);
  void GetOrigin(double& x, double& y, double& z) const;
  void GetLastShift(double& x, double& y, double& z) const;
  void Rebase(double px, double py, double pz);
  bool RebaseIfNeeded(double px, double py, double pz, double threshold);
  void WorldToLocal(double wx, double wy, double wz, float& lx, float& ly, float& lz) const;
  void LocalToWorld(float lx, float ly, float lz, double& wx, double& wy, double& wz) const;
  double DistanceToOrigin(double px, double py, double pz) const;
private:
  double ox;
  double oy;
  double oz;
  double sx;
  double sy;
  double sz;
};
}
