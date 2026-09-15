#pragma once

#include <atomic>
#include <cstdint>

namespace kizuri::core {

class JobCounter {
public:
    JobCounter() = default;
    JobCounter(const JobCounter&) = delete;
    JobCounter& operator=(const JobCounter&) = delete;

    void Increment(int32_t amount = 1) noexcept;
    void Decrement() noexcept;
    bool IsComplete() const noexcept;
    int32_t Remaining() const noexcept { return remaining_.load(std::memory_order_acquire); }

private:
    std::atomic<int32_t> remaining_{ 0 };
};

} // namespace kizuri::core