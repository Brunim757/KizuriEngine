#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include "kizuri/core/CoreTypes.h"
#include "kizuri/core/jobs/JobCounter.h"

namespace kizuri::core {

using JobHandler = void (*)(void* userData);
using IndexedJobHandler = void (*)(void* userData, uint32_t index);

struct Job
{
    JobHandler Handler = nullptr;
    IndexedJobHandler IndexedHandler = nullptr;
    void* UserData = nullptr;
    uint32_t ItemIndex = 0;
};

struct QueueEntry
{
    Job Job;
    JobCounter* Counter = nullptr;
};

inline thread_local bool g_jobThreadIsWorker = false;
inline thread_local void* g_jobCurrentFiber = nullptr;
inline thread_local void* g_jobThreadFiber = nullptr;

class JobSystem {
public:
    static constexpr uint32_t MaxWorkers = 16;
    static constexpr uint32_t FibersPerWorker = 4;
    static constexpr uint32_t JobQueueCapacity = 1u << 14;
    static constexpr uint32_t InlineBatchLimit = 128;

    static bool Initialize(uint32_t workerCount);
    static void Shutdown();
    static bool IsInitialized() { return s_initialized.load(std::memory_order_acquire); }
    static uint32_t WorkerCount() { return s_workerCount; }

    static void Enqueue(const Job& job);
    static void EnqueueWithCounter(const Job& job, JobCounter& counter);
    static void EnqueueWithCounter(const Job* jobs, uint32_t count, JobCounter& counter);

    static void ParallelFor(uint32_t itemCount, IndexedJobHandler handler, void* userData);
    static void WaitAll(const Job* jobs, uint32_t count);
    static void WaitForCounter(const JobCounter& counter);

    static bool IsWorkerThread() { return g_jobThreadIsWorker; }
    static void DrainQueueUntilEmpty();

private:
    struct QueueCell
    {
        std::atomic<size_t> Sequence{ 0 };
        QueueEntry Data{};
    };

    struct MpmcRing
    {
        static constexpr size_t Capacity = JobQueueCapacity;
        static constexpr size_t Mask = Capacity - 1;

        std::atomic<size_t> Head{ 0 };
        std::atomic<size_t> Tail{ 0 };
        std::atomic<bool> ShutdownRequested{ false };
        QueueCell* Cells = nullptr;

        bool Push(const QueueEntry& entry);
        bool TryPop(QueueEntry& out);
        bool Prepare();
        void Destroy();
    };

    struct JobFiber
    {
        void* NativeHandle = nullptr;
        Worker* Owner = nullptr;
        QueueEntry Entry{};
        const JobCounter* WaitCounter = nullptr;
        bool ParkedWaiting = false;
        bool CommandPending = false;
    };

    struct Worker
    {
        std::thread Thread{};
        void* ThreadFiber = nullptr;
        JobFiber* Fibers[FibersPerWorker]{};
        JobFiber* FreeRing[FibersPerWorker]{};
        JobFiber* WaitingQueue[FibersPerWorker]{};
        uint32_t FreeCount = 0;
        uint32_t WaitingCount = 0;
        uint32_t Index = 0;
    };

    static void* WorkerThreadStart(Worker* worker);
    static void KIZURI_CALL FiberRoutine(void* parameter);
    static JobFiber* CurrentJobFiber() { return static_cast<JobFiber*>(g_jobCurrentFiber); }
    static void SetCurrentJobFiber(JobFiber* fiber) { g_jobCurrentFiber = fiber; }
    static void ClearCurrentJobFiber() { g_jobCurrentFiber = nullptr; }
    static void* CurrentThreadFiber() { return g_jobThreadFiber; }
    static void ExecuteJob(const Job& job) noexcept;
    static void CompleteEntry(const QueueEntry& entry) noexcept;
    static void RunJobOnFiber(Worker& worker, const QueueEntry& entry);
    static void RunJobInline(const QueueEntry& entry);
    static void ServiceWaitingFibers(Worker& worker);
    static void ShutdownWorkerFibers(Worker& worker);

    inline static std::atomic<bool> s_initialized{ false };
    inline static std::atomic<bool> s_shutdownRequested{ false };
    inline static std::atomic<uint64_t> s_outstandingJobs{ 0 };
    inline static Worker s_workers[MaxWorkers]{};
    inline static JobFiber s_fiberPool[MaxWorkers * FibersPerWorker]{};
    inline static MpmcRing s_queueInstance{};
    inline static MpmcRing* s_queue = &s_queueInstance;
    inline static uint32_t s_workerCount = 0;
};

} // namespace kizuri::core