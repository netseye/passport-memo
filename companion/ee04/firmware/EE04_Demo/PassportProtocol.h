#pragma once
#include <array>
#include <cstdint>
#include <string>

// Four outstanding challenges, consumed only after an authenticated write persists.
class PassportChallenges {
 public:
  void reset() { for (auto& value : values_) value.clear(); }
  void issue(const std::string& value, uint32_t now) {
    slot_ = (slot_ + 1) % values_.size();
    values_[slot_] = value;
    times_[slot_] = now;
  }
  int match(const std::string& value, uint32_t now) const {
    if (value.size() != 32) return -1;
    for (size_t i = 0; i < values_.size(); ++i)
      if (values_[i] == value && uint32_t(now - times_[i]) < 30000)
        return int(i);
    return -1;
  }
  void consume(int slot) {
    if (slot >= 0 && size_t(slot) < values_.size()) values_[slot].clear();
  }
 private:
  std::array<std::string, 4> values_;
  std::array<uint32_t, 4> times_{};
  size_t slot_ = 0;
};
inline std::string passportCanonical(const std::string& nonce, uint32_t id,
                                     bool done, const std::string& text) {
  return "memo-v1\n" + nonce + "\n" + std::to_string(id) + "\n" +
         (done ? "1" : "0") + "\n" + text;
}
