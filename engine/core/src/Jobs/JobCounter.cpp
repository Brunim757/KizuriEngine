#include "kizuri/core/jobs/JobCounter.h"

namespace kizuri::core {

void JobCounter::Increment(int32_t amount) noexcept
{
    remaining_.fetch_add(amount, std::memory_order_relaxed);
}

void JobCounter::Decrement() noexcept
{
    remaining_.fetch_sub(1, std::memory_order_acq_rel);
}

bool JobCounter::IsComplete() const noexcept
{
    return remaining_.load(std::memory_order_acquire) == 0;
}

} // namespace kizuri::core