#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>

#include "kizuri/core/jobs/JobCounter.h"
#include "kizuri/core/jobs/JobSystem.h"

namespace {

constexpr uint32_t TestJobCount = 20000;

std::atomic<uint64_t> g_executedJobs{ 0 };
std::atomic<uint64_t> g_fiberExecutedJobs{ 0 };
std::atomic<uint64_t> g_parallelSum{ 0 };

void CountJob(void* userData)
{
    uint64_t* counter = static_cast<uint64_t*>(userData);
    ++*counter;
    if (kizuri::core::JobSystem::IsWorkerThread())
    {
        g_fiberExecutedJobs.fetch_add(1);
    }
}

void SumIndexed(void* userData, uint32_t index)
{
    std::atomic<uint64_t>* accumulator = static_cast<std::atomic<uint64_t>*>(userData);
    accumulator->fetch_add(index);
}

} // namespace

TEST_CASE("JobSystem smoke test completes every job", "[jobs]")
{
    REQUIRE(kizuri::core::JobSystem::Initialize(4));

    uint64_t executed = 0;
    kizuri::core::JobCounter counter;
    kizuri::core::Job job;
    job.Handler = &CountJob;
    job.UserData = &executed;
    for (uint32_t i = 0; i < TestJobCount; ++i)
    {
        kizuri::core::JobSystem::EnqueueWithCounter(job, counter);
    }
    kizuri::core::JobSystem::WaitForCounter(counter);

    REQUIRE(executed == TestJobCount);
    REQUIRE(counter.IsComplete());
    REQUIRE(g_fiberExecutedJobs.load() > 0);
}

TEST_CASE("JobSystem WaitAll waits for a batch", "[jobs]")
{
    REQUIRE(kizuri::core::JobSystem::Initialize(4));

    uint64_t executed = 0;
    kizuri::core::Job job;
    job.Handler = &CountJob;
    job.UserData = &executed;

    kizuri::core::Job jobs[64];
    for (uint32_t i = 0; i < 64; ++i)
    {
        jobs[i] = job;
    }
    kizuri::core::JobSystem::WaitAll(jobs, 64);
    REQUIRE(executed == 64);
}

TEST_CASE("JobSystem ParallelFor accumulates exact sum", "[jobs]")
{
    REQUIRE(kizuri::core::JobSystem::Initialize(4));

    std::atomic<uint64_t> accumulator{ 0 };
    const uint32_t itemCount = 5000;
    kizuri::core::JobSystem::ParallelFor(itemCount, &SumIndexed, &accumulator);

    const uint64_t expectedSum = static_cast<uint64_t>(itemCount) * (itemCount - 1) / 2;
    REQUIRE(accumulator.load() == expectedSum);
}

TEST_CASE("JobSystem drains every queued job on shutdown", "[jobs]")
{
    REQUIRE(kizuri::core::JobSystem::Initialize(2));

    uint64_t executed = 0;
    kizuri::core::Job job;
    job.Handler = &CountJob;
    job.UserData = &executed;
    for (uint32_t i = 0; i < 100; ++i)
    {
        kizuri::core::JobSystem::Enqueue(job);
    }
    kizuri::core::JobSystem::DrainQueueUntilEmpty();
    REQUIRE(executed == 100);

    kizuri::core::JobSystem::Shutdown();
    REQUIRE_FALSE(kizuri::core::JobSystem::IsInitialized());
}