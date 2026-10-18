/*****************************************************************/ /**
 * @file   JobSystem.hpp
 * @brief  header file of JobSystem class
 * 
 * @author ichi-raven
 * @date   April 2024
 *********************************************************************/

#ifndef EC2S_INCLUDE_JOBSYSTEM_HPP_
#define EC2S_INCLUDE_JOBSYSTEM_HPP_

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
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

        Job(const Job&) = delete;
        Job& operator=(const Job&) = delete;

        void addChild(Job&& child)
        {
            // Finish building the dependency graph before submitting any of its jobs.
            pChildren.push_back(&child);
            child.dependencyCount.fetch_add(1, std::memory_order_relaxed);
        }

    private:
        std::function<void()> f;
        Job* pNext{ nullptr };
        std::atomic_int dependencyCount{ 0 };
        std::vector<Job*> pChildren;
        bool isSubmitted{ false };
    };

    // Submitted jobs are owned by the pool. Call wait/stop/restart outside worker jobs,
    // and serialize stop/restart with each other and with submissions.
    class ThreadPool
    {
    public:
        // ThreadPool Public Methods
        explicit ThreadPool(std::optional<size_t> nThreads = std::nullopt)
            : mRemainingJobNum(0)
            , mStop(true)
            , mpJobHead(nullptr)
        {
            if (!nThreads)
            {
                const auto hardwareThreads = std::thread::hardware_concurrency();
                nThreads = hardwareThreads > 1 ? hardwareThreads - 1 : 1;
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
            Job* pJob   = new Job();
            pJob->f     = f;
            pJob->pNext = nullptr;
            return *pJob;
        }

        void submit(Job&& job)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            assert(!mStop);
            if (job.isSubmitted)
            {
                return;
            }

            const auto registerJob = [this](Job* pJob, const auto& func) -> void
            {
                if (pJob->isSubmitted)
                {
                    return;
                }

                pJob->isSubmitted = true;
                ++mRemainingJobNum;
                for (auto* pChild : pJob->pChildren)
                {
                    func(pChild, func);
                }
            };
            registerJob(&job, registerJob);

            if (job.dependencyCount.load(std::memory_order_relaxed) == 0)
            {
                job.pNext = mpJobHead;
                mpJobHead = &job;
                mCv.notify_one();
            }
        }

        void submit(const std::function<void()>& f)
        {
            submit(std::move(createJob(f)));
        }

        void wait()
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCvCompleted.wait(lock, [this] { return mRemainingJobNum == 0; });
        }

        void stop()
        {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (mStop)
                {
                    return;
                }
                mStop = true;
            }

            mCv.notify_all();

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
            std::lock_guard<std::mutex> lock(mMutex);

            if (!mStop)
            {
                return;
            }

            mStop = false;

            for (auto& thread : mThreads)
            {
                thread = std::thread(&ThreadPool::worker, this);
            }
        }

    private:
        void worker()
        {
            while (true)
            {
                std::unique_lock<std::mutex> lock(mMutex);
                mCv.wait(lock, [this] { return mpJobHead != nullptr || (mStop && mRemainingJobNum == 0); });

                if (mpJobHead == nullptr)
                {
                    return;
                }

                Job* pJob = mpJobHead;
                mpJobHead = pJob->pNext;

                lock.unlock();

                pJob->f();

                lock.lock();
                for (auto* pChild : pJob->pChildren)
                {
                    if (pChild->dependencyCount.fetch_sub(1) == 1)
                    {
                        pChild->pNext = mpJobHead;
                        mpJobHead     = pChild;
                        mCv.notify_one();
                    }
                }
                lock.unlock();

                delete pJob;

                lock.lock();
                if (--mRemainingJobNum == 0)
                {
                    mCvCompleted.notify_all();
                    mCv.notify_all();
                }
            }
        }

    private:
        // ThreadPool Private Members
        std::vector<std::thread> mThreads;
        mutable std::mutex mMutex;
        size_t mRemainingJobNum;
        bool mStop;
        Job* mpJobHead;
        std::condition_variable mCv;
        std::condition_variable mCvCompleted;
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
