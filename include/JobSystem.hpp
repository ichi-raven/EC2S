/*****************************************************************/ /**
 * @file   JobSystem.hpp
 * @brief  header file of JobSystem class
 * 
 * @author ichi-raven
 * @date   April 2024
 *********************************************************************/

#ifndef EC2S_INCLUDE_JOBSYSTEM_HPP_
#define EC2S_INCLUDE_JOBSYSTEM_HPP_

#include "ConcurrentQueue.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <optional>
#include <semaphore>
#include <thread>
#include <utility>
#include <vector>
#include <functional>
#include <cassert>

namespace ec2s
{
    class Job
    {
    private:
        friend class ThreadPool;
        Job()  = default;
        ~Job() = default;

    public:
        Job(const Job&)            = delete;
        Job& operator=(const Job&) = delete;

        void addChild(Job&& child)
        {
            // Finish building the dependency graph before submitting any of its jobs.
            pChildren.push_back(&child);
            child.dependencyCount.fetch_add(1, std::memory_order_relaxed);
        }

    private:
        std::function<void()> f;
        // One dependency holds the job until registration of its descendants finishes.
        std::atomic_int dependencyCount{ 1 };
        std::vector<Job*> pChildren;
        std::atomic_bool isSubmitted{ false };
    };

    // Submitted jobs are owned by the pool. Call wait/stop/restart outside jobs,
    // and serialize stop/restart with each other and with submissions.
    class ThreadPool
    {
    public:
        // ThreadPool Public Methods
        explicit ThreadPool(std::optional<size_t> nThreads = std::nullopt)
        {
            if (!nThreads)
            {
                const auto hardwareThreads = std::thread::hardware_concurrency();
                nThreads                   = hardwareThreads > 1 ? hardwareThreads - 1 : 1;
            }

            mThreads.resize(std::max(static_cast<size_t>(1u), *nThreads));

            restart();
        }

        ~ThreadPool()
        {
            stop();
        }

        size_t size() const
        {
            return mThreads.size();
        }

        Job& createJob(const std::function<void()>& f)
        {
            Job* pJob = new Job();
            pJob->f   = f;
            return *pJob;
        }

        void submit(Job&& job)
        {
            assert(!mStop.load(std::memory_order_acquire));
            registerJob(&job);
        }

        void submit(const std::function<void()>& f)
        {
            submit(std::move(createJob(f)));
        }

        void wait()
        {
            auto remaining = mRemainingJobNum.load(std::memory_order_acquire);
            while (remaining != 0)
            {
                mRemainingJobNum.wait(remaining, std::memory_order_acquire);
                remaining = mRemainingJobNum.load(std::memory_order_acquire);
            }
        }

        void stop()
        {
            if (mStop.exchange(true, std::memory_order_acq_rel))
            {
                return;
            }

            wait();
            mAvailableJobs.release(static_cast<std::ptrdiff_t>(mThreads.size()));

            for (auto& thread : mThreads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }
        }

        void restart()
        {
            if (!mStop.exchange(false, std::memory_order_acq_rel))
            {
                return;
            }

            for (auto& thread : mThreads)
            {
                thread = std::thread(&ThreadPool::worker, this);
            }
        }

    private:
        void completeJob()
        {
            if (mRemainingJobNum.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                mRemainingJobNum.notify_all();
            }
        }

        void registerJob(Job* pJob)
        {
            // Count before publishing isSubmitted so another submitter cannot miss it.
            mRemainingJobNum.fetch_add(1, std::memory_order_relaxed);
            if (pJob->isSubmitted.exchange(true, std::memory_order_acq_rel))
            {
                completeJob();
                return;
            }

            for (auto* pChild : pJob->pChildren)
            {
                registerJob(pChild);
            }

            if (pJob->dependencyCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                scheduleJob(pJob);
            }
        }

        void scheduleJob(Job* pJob)
        {
            if (mReadyJobs.enqueue(pJob))
            {
                mAvailableJobs.release();
            }
            else
            {
                // Preserve the job and its dependencies if queue allocation fails.
                executeJob(pJob);
            }
        }

        void executeJob(Job* pJob)
        {
            pJob->f();
            for (auto* pChild : pJob->pChildren)
            {
                if (pChild->dependencyCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    scheduleJob(pChild);
                }
            }

            delete pJob;
            completeJob();
        }

        void worker()
        {
            while (true)
            {
                mAvailableJobs.acquire();
                Job* pJob;
                while (!mReadyJobs.try_dequeue(pJob))
                {
                    if (mStop.load(std::memory_order_acquire) && mRemainingJobNum.load(std::memory_order_acquire) == 0)
                    {
                        return;
                    }
                    std::this_thread::yield();
                }

                executeJob(pJob);
            }
        }

    private:
        // ThreadPool Private Members
        std::vector<std::thread> mThreads;
        ConcurrentQueue<Job*> mReadyJobs;
        std::counting_semaphore<> mAvailableJobs{ 0 };
        std::atomic_size_t mRemainingJobNum{ 0 };
        std::atomic_bool mStop{ true };
    };

    template <typename T>
    concept Job1DFunc = requires(const T& f, size_t x) {
        { f(x) };
    };

    template <typename T>
    concept Job1DChunkFunc = requires(const T& f, size_t start, size_t end) {
        { f(start, end) };
    };

    template <typename T>
    concept Job2DFunc = requires(const T& f, size_t x, size_t y) {
        { f(x, y) };
    };

    template <typename T>
    concept Job2DChunkFunc = requires(const T& f, std::pair<size_t, size_t> start, std::pair<size_t, size_t> end) {
        { f(start, end) };
    };

    template <Job1DFunc JobFunc>  // Constrain by concept
    void parallelFor(uint32_t start, uint32_t end, const JobFunc& jobFunc, ThreadPool& threadPool)
    {
        assert(threadPool.size() > 0);
        assert(end >= start);

        constexpr uint32_t kInvalidRem = 0xFFFFFFFF;
        const uint32_t range           = end - start;
        const uint32_t chunkSize       = range / threadPool.size();
        const uint32_t remainder       = range % threadPool.size();
        const uint32_t remStart        = end - remainder;

        for (uint32_t i = 0; i < threadPool.size(); ++i)
        {
            const uint32_t chunkStart = start + i * chunkSize;
            const uint32_t chunkEnd   = chunkStart + chunkSize;
            const uint32_t rem        = (i < remainder ? remStart + i : kInvalidRem);

            if (chunkEnd == chunkStart && rem == kInvalidRem)
            {
                break;
            }

            threadPool.submit(
                [chunkStart, chunkEnd, rem, &jobFunc]()
                {
                    for (uint32_t j = chunkStart; j < chunkEnd; ++j)
                    {
                        jobFunc(j);
                    }

                    if (rem == kInvalidRem)
                    {
                        return;
                    }

                    jobFunc(rem);
                });
        }

        threadPool.wait();
    }

    template <Job1DChunkFunc JobFunc>  // Constrain by concept
    void parallelForChunk(uint32_t start, uint32_t end, const JobFunc& jobFunc, ThreadPool& threadPool)
    {
        assert(threadPool.size() > 0);
        assert(end >= start);

        const uint32_t range     = end - start;
        const uint32_t remainder = range % threadPool.size();

        uint32_t now = start;

        for (uint32_t i = 0; i < threadPool.size(); ++i)
        {
            const uint32_t chunkSize = range / threadPool.size() + (i < remainder ? 1 : 0);
            threadPool.submit([=, &jobFunc]() { jobFunc(now, now + chunkSize); });

            now += chunkSize;
        }

        threadPool.wait();
    }

    template <Job2DFunc JobFunc>  // Constrain by concept
    void parallelFor2D(const std::pair<uint32_t, uint32_t> start, const std::pair<uint32_t, uint32_t> end, const JobFunc& jobFunc, ThreadPool& threadPool)
    {
        assert(threadPool.size() > 0);
        assert(end.first >= start.first && end.second >= start.second);

        constexpr uint32_t kInvalidRem = 0xFFFFFFFF;
        const uint32_t rangeX          = end.first - start.first;
        const uint32_t chunkSizeX      = rangeX / threadPool.size();
        const uint32_t remainderX      = rangeX % threadPool.size();
        const uint32_t remStart        = end.first - remainderX;

        for (uint32_t i = 0; i < threadPool.size(); ++i)
        {
            const uint32_t chunkStartX = start.first + i * chunkSizeX;
            const uint32_t chunkEndX   = chunkStartX + chunkSizeX;
            const uint32_t rem         = (i < remainderX ? remStart + i : kInvalidRem);

            if (chunkEndX == chunkStartX && rem == kInvalidRem)
            {
                // prevent unnecessary thread creation
                break;
            }

            threadPool.submit(
                [chunkStartX, chunkEndX, rem, start, end, &jobFunc]()
                {
                    for (uint32_t j = chunkStartX; j < chunkEndX; ++j)
                    {
                        for (uint32_t k = start.second; k < end.second; ++k)
                        {
                            jobFunc(j, k);
                        }
                    }

                    if (rem == kInvalidRem)
                    {
                        return;
                    }

                    for (uint32_t k = start.second; k < end.second; ++k)
                    {
                        jobFunc(rem, k);
                    }
                });
        }

        threadPool.wait();
    }

    template <Job2DChunkFunc JobFunc>  // Constrain by concept
    void parallelFor2DChunk(const std::pair<uint32_t, uint32_t> start, const std::pair<uint32_t, uint32_t> end, const JobFunc& jobFunc, ThreadPool& threadPool)
    {
        assert(threadPool.size() > 0);
        assert(end.first >= start.first && end.second >= start.second);

        const uint32_t rangeX = end.first - start.first;
        const uint32_t rangeY = end.second - start.second;

        if (rangeX > rangeY)  // landscape
        {
            const uint32_t remainderX = rangeX % threadPool.size();
            uint32_t nowX             = start.first;
            for (uint32_t i = 0; i < threadPool.size(); ++i)
            {
                const uint32_t chunkSizeX = rangeX / threadPool.size() + (i < remainderX ? 1 : 0);
                threadPool.submit([nowX, chunkSizeX, start, end, &jobFunc]() { jobFunc({ nowX, start.second }, { nowX + chunkSizeX, end.second }); });
                nowX += chunkSizeX;
            }
        }
        else  // portrait
        {
            const uint32_t remainderY = rangeY % threadPool.size();
            uint32_t nowY             = start.second;
            for (uint32_t i = 0; i < threadPool.size(); ++i)
            {
                const uint32_t chunkSizeY = rangeY / threadPool.size() + (i < remainderY ? 1 : 0);
                threadPool.submit([nowY, chunkSizeY, start, end, &jobFunc]() { jobFunc({ start.first, nowY }, { end.first, nowY + chunkSizeY }); });
                nowY += chunkSizeY;
            }
        }

        threadPool.wait();
    }

}  // namespace ec2s

#endif
