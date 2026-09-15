#include "kizuri/core/jobs/JobSystem.h"

#ifndef _WIN32
#error "KizuriCore JobSystem targets the Windows platform. Only MSVC/Windows runners are supported by the grid."
#endif

#include <windows.h>

#include <cstring>

#include "kizuri/core/memory/MemoryTracker.h"

namespace kizuri::core {

bool JobSystem::MpmcRing::Push(const QueueEntry& entry)
{
    size_t position = Tail.fetch_add(1, std::memory_order_acq_rel);
    QueueCell& cell = Cells[position & Mask];
    for (;;)
    {
        if (cell.Sequence.load(std::memory_order_acquire) == position)
        {
            break;
        }
        std::this_thread::yield();
    }
    cell.Data = entry;
    cell.Sequence.store(position + 1, std::memory_order_release);
    return true;
}

bool JobSystem::MpmcRing::TryPop(QueueEntry& out)
{
    for (;;)
    {
        size_t position = Head.load(std::memory_order_acquire);
        QueueCell& cell = Cells[position & Mask];
        size_t sequence = cell.Sequence.load(std::memory_order_acquire);
        if (sequence != position + 1)
        {
            return false;
        }
        if (Head.compare_exchange_weak(position,
                position + 1,
                std::memory_order_acq_rel,
                std::memory_order_relaxed))
        {
            out = cell.Data;
            cell.Sequence.store(position + Mask + 1, std::memory_order_release);
            return true;
        }
    }
}

bool JobSystem::MpmcRing::Prepare()
{
    if (Cells != nullptr)
    {
        return true;
    }
    Cells = static_cast<QueueCell*>(MemoryTracker::Instance().AllocateRaw(sizeof(QueueCell) * Capacity));
    if (Cells == nullptr)
    {
        return false;
    }
    for (size_t i = 0; i < Capacity; ++i)
    {
        new (&Cells[i]) QueueCell();
        Cells[i].Sequence.store(i, std::memory_order_relaxed);
    }
    return true;
}

void JobSystem::MpmcRing::Destroy()
{
    if (Cells != nullptr)
    {
        MemoryTracker::Instance().FreeRaw(Cells);
        Cells = nullptr;
    }
}

void JobSystem::ExecuteJob(const Job& job) noexcept
{
    if (job.IndexedHandler != nullptr)
    {
        job.IndexedHandler(job.UserData, job.ItemIndex);
    }
    else if (job.Handler != nullptr)
    {
        job.Handler(job.UserData);
    }
}

void JobSystem::CompleteEntry(const QueueEntry& entry) noexcept
{
    if (entry.Counter != nullptr)
    {
        entry.Counter->Decrement();
    }
    s_outstandingJobs.fetch_sub(1, std::memory_order_relaxed);
}

void JobSystem::RunJobInline(const QueueEntry& entry)
{
    ExecuteJob(entry.Job);
    CompleteEntry(entry);
}

void JobSystem::RunJobOnFiber(Worker& worker, const QueueEntry& entry)
{
    JobFiber* fiber = nullptr;
    if (worker.FreeCount > 0)
    {
        fiber = worker.FreeRing[--worker.FreeCount];
    }
    if (fiber == nullptr)
    {
        RunJobInline(entry);
        return;
    }

    fiber->Entry = entry;
    fiber->CommandPending = true;
    SetCurrentJobFiber(fiber);
    SwitchToFiber(fiber->NativeHandle);
    ClearCurrentJobFiber();

    if (fiber->ParkedWaiting)
    {
        fiber->ParkedWaiting = false;
        worker.WaitingQueue[worker.WaitingCount++] = fiber;
        return;
    }

    worker.FreeRing[worker.FreeCount++] = fiber;
}

void JobSystem::ServiceWaitingFibers(Worker& worker)
{
    for (uint32_t i = 0; i < worker.WaitingCount; ++i)
    {
        JobFiber* waiting = worker.WaitingQueue[i];
        if (waiting->WaitCounter == nullptr || !waiting->WaitCounter->IsComplete())
        {
            continue;
        }

        worker.WaitingQueue[i] = worker.WaitingQueue[worker.WaitingCount - 1];
        --worker.WaitingCount;
        --i;

        SetCurrentJobFiber(waiting);
        SwitchToFiber(waiting->NativeHandle);
        ClearCurrentJobFiber();

        if (waiting->ParkedWaiting)
        {
            waiting->ParkedWaiting = false;
            worker.WaitingQueue[worker.WaitingCount++] = waiting;
        }
        else
        {
            worker.FreeRing[worker.FreeCount++] = waiting;
        }
    }
}

void KIZURI_CALL JobSystem::FiberRoutine(void* parameter)
{
    JobFiber* fiber = static_cast<JobFiber*>(parameter);
    Worker* worker = fiber->Owner;
    while (true)
    {
        while (!fiber->CommandPending)
        {
            SwitchToFiber(worker->ThreadFiber);
        }
        fiber->CommandPending = false;

        const QueueEntry entry = fiber->Entry;
        SetCurrentJobFiber(fiber);
        ExecuteJob(entry.Job);
        ClearCurrentJobFiber();
        CompleteEntry(entry);

        fiber->WaitCounter = nullptr;
        SwitchToFiber(worker->ThreadFiber);
    }
}

void* JobSystem::WorkerThreadStart(Worker* worker)
{
    worker->ThreadFiber = ConvertThreadToFiber(nullptr);
    g_jobThreadFiber = worker->ThreadFiber;
    g_jobThreadIsWorker = true;

    for (uint32_t i = 0; i < FibersPerWorker; ++i)
    {
        JobFiber* fiber = worker->Fibers[i];
        fiber->NativeHandle = CreateFiber(0, &FiberRoutine, fiber);
    }

    while (true)
    {
        ServiceWaitingFibers(*worker);
        QueueEntry entry;
        if (s_queue->TryPop(entry))
        {
            RunJobOnFiber(*worker, entry);
            continue;
        }
        if (s_shutdownRequested.load(std::memory_order_acquire) &&
            s_outstandingJobs.load(std::memory_order_acquire) == 0)
        {
            break;
        }
        std::this_thread::yield();
    }

    ShutdownWorkerFibers(*worker);
    ConvertFiberToThread();
    g_jobThreadIsWorker = false;
    return nullptr;
}

void JobSystem::ShutdownWorkerFibers(Worker& worker)
{
    for (uint32_t i = 0; i < FibersPerWorker; ++i)
    {
        JobFiber* fiber = worker.Fibers[i];
        if (fiber->NativeHandle != nullptr)
        {
            DeleteFiber(fiber->NativeHandle);
            fiber->NativeHandle = nullptr;
        }
    }
    worker.ThreadFiber = nullptr;
}

bool JobSystem::Initialize(uint32_t workerCount)
{
    if (s_initialized.load(std::memory_order_acquire))
    {
        return true;
    }

    uint32_t clampedWorkers = workerCount == 0 ? 1 : workerCount;
    if (clampedWorkers > MaxWorkers)
    {
        clampedWorkers = MaxWorkers;
    }

    if (!s_queue->Prepare())
    {
        return false;
    }
    s_queue->ShutdownRequested.store(false, std::memory_order_relaxed);

    s_workerCount = clampedWorkers;
    s_outstandingJobs.store(0, std::memory_order_relaxed);
    s_shutdownRequested.store(false, std::memory_order_relaxed);

    for (uint32_t i = 0; i < s_workerCount; ++i)
    {
        Worker& worker = s_workers[i];
        worker.Index = i;
        worker.ThreadFiber = nullptr;
        worker.WaitingCount = 0;
        worker.Thread = std::thread();
        worker.FreeCount = FibersPerWorker;
        for (uint32_t f = 0; f < FibersPerWorker; ++f)
        {
            JobFiber* fiber = &s_fiberPool[i * FibersPerWorker + f];
            fiber->Owner = &worker;
            fiber->NativeHandle = nullptr;
            fiber->CommandPending = false;
            fiber->ParkedWaiting = false;
            fiber->WaitCounter = nullptr;
            worker.Fibers[f] = fiber;
            worker.FreeRing[f] = fiber;
        }
    }

    s_initialized.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < s_workerCount; ++i)
    {
        s_workers[i].Thread = std::thread(&JobSystem::WorkerThreadStart, &s_workers[i]);
    }
    return true;
}

void JobSystem::Shutdown()
{
    if (!s_initialized.load(std::memory_order_acquire))
    {
        return;
    }

    DrainQueueUntilEmpty();

    s_shutdownRequested.store(true, std::memory_order_release);
    s_queue->ShutdownRequested.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < s_workerCount; ++i)
    {
        if (s_workers[i].Thread.joinable())
        {
            s_workers[i].Thread.join();
        }
        s_workers[i].Thread = std::thread();
    }

    for (uint32_t i = 0; i < s_workerCount; ++i)
    {
        Worker& worker = s_workers[i];
        worker.WaitingCount = 0;
        worker.FreeCount = 0;
        for (uint32_t f = 0; f < FibersPerWorker; ++f)
        {
            JobFiber* fiber = &s_fiberPool[i * FibersPerWorker + f];
            fiber->Owner = nullptr;
            worker.Fibers[f] = nullptr;
            worker.FreeRing[f] = nullptr;
        }
    }

    s_workerCount = 0;
    s_queue->Destroy();
    s_initialized.store(false, std::memory_order_release);
}

void JobSystem::Enqueue(const Job& job)
{
    if (!s_initialized.load(std::memory_order_acquire) || s_shutdownRequested.load(std::memory_order_acquire))
    {
        return;
    }
    QueueEntry entry;
    entry.Job = job;
    entry.Counter = nullptr;
    s_outstandingJobs.fetch_add(1, std::memory_order_relaxed);
    s_queue->Push(entry);
}

void JobSystem::EnqueueWithCounter(const Job& job, JobCounter& counter)
{
    if (!s_initialized.load(std::memory_order_acquire) || s_shutdownRequested.load(std::memory_order_acquire))
    {
        return;
    }
    counter.Increment();
    QueueEntry entry;
    entry.Job = job;
    entry.Counter = &counter;
    s_outstandingJobs.fetch_add(1, std::memory_order_relaxed);
    s_queue->Push(entry);
}

void JobSystem::EnqueueWithCounter(const Job* jobs, uint32_t count, JobCounter& counter)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        EnqueueWithCounter(jobs[i], counter);
    }
}

void JobSystem::WaitAll(const Job* jobs, uint32_t count)
{
    if (count == 0)
    {
        return;
    }
    JobCounter counter;
    EnqueueWithCounter(jobs, count, counter);
    WaitForCounter(counter);
}

void JobSystem::ParallelFor(uint32_t itemCount, IndexedJobHandler handler, void* userData)
{
    if (itemCount == 0)
    {
        return;
    }
    if (itemCount <= InlineBatchLimit || !s_initialized.load(std::memory_order_acquire))
    {
        for (uint32_t i = 0; i < itemCount; ++i)
        {
            handler(userData, i);
        }
        return;
    }

    JobCounter counter;
    counter.Increment(itemCount);
    QueueEntry entry;
    entry.Job.IndexedHandler = handler;
    entry.Job.UserData = userData;
    entry.Counter = &counter;
    for (uint32_t i = 0; i < itemCount; ++i)
    {
        entry.Job.ItemIndex = i;
        s_outstandingJobs.fetch_add(1, std::memory_order_relaxed);
        s_queue->Push(entry);
    }
    WaitForCounter(counter);
}

void JobSystem::WaitForCounter(const JobCounter& counter)
{
    if (g_jobThreadIsWorker && CurrentJobFiber() != nullptr)
    {
        JobFiber* fiber = CurrentJobFiber();
        while (!counter.IsComplete())
        {
            fiber->WaitCounter = &counter;
            fiber->ParkedWaiting = true;
            SwitchToFiber(CurrentThreadFiber());
            fiber->ParkedWaiting = false;
        }
        fiber->WaitCounter = nullptr;
        return;
    }

    while (!counter.IsComplete())
    {
        QueueEntry entry;
        if (s_queue->TryPop(entry))
        {
            RunJobInline(entry);
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

void JobSystem::DrainQueueUntilEmpty()
{
    if (s_queue == nullptr)
    {
        return;
    }
    while (s_outstandingJobs.load(std::memory_order_acquire) > 0)
    {
        QueueEntry entry;
        if (s_queue->TryPop(entry))
        {
            RunJobInline(entry);
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

} // namespace kizuri::core