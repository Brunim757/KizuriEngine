#pragma once
#include "Kizuri/Log.h"
#include <string>
#include <vector>
namespace Kizuri {
struct Notification {
  LogLevel level;
  std::string text;
  double age;
  uint64_t seq;
};
class NotificationCenter {
public:
  NotificationCenter();
  void SetLifetime(double seconds);
  double Lifetime() const;
  void Notify(LogLevel level, const std::string& text);
  void Update(double dt);
  std::vector<Notification> Active() const;
  std::vector<Notification> History() const;
  size_t ActiveCount() const;
  size_t HistoryCount() const;
  void Clear();
private:
  std::vector<Notification> active;
  std::vector<Notification> history;
  double lifetime;
  uint64_t nextSeq;
};
}
