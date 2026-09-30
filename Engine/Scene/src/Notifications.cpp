#include "Kizuri/Notifications.h"
namespace Kizuri {
NotificationCenter::NotificationCenter()
  : lifetime(4.0)
  , nextSeq(1) {
}
void NotificationCenter::SetLifetime(double seconds) {
  lifetime = seconds < 0.0 ? 0.0 : seconds;
}
double NotificationCenter::Lifetime() const {
  return lifetime;
}
void NotificationCenter::Notify(LogLevel level, const std::string& text) {
  Notification n;
  n.level = level;
  n.text = text;
  n.age = 0.0;
  n.seq = nextSeq++;
  active.push_back(n);
  history.push_back(n);
  if (history.size() > 20) {
    history.erase(history.begin());
  }
}
void NotificationCenter::Update(double dt) {
  if (dt < 0.0) {
    dt = 0.0;
  }
  for (size_t i = 0; i < active.size();) {
    active[i].age += dt;
    if (active[i].age >= lifetime) {
      active.erase(active.begin() + i);
    } else {
      ++i;
    }
  }
}
std::vector<Notification> NotificationCenter::Active() const {
  return active;
}
std::vector<Notification> NotificationCenter::History() const {
  return history;
}
size_t NotificationCenter::ActiveCount() const {
  return active.size();
}
size_t NotificationCenter::HistoryCount() const {
  return history.size();
}
void NotificationCenter::Clear() {
  active.clear();
  history.clear();
}
}
