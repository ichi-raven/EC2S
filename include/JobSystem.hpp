/*****************************************************************/ /**
 * @file   JobSystem.hpp
 * @brief  header file of JobSystem class
 * 
 * @author ichi-raven
 * @date   April 2024
 *********************************************************************/

#ifndef EC2S_INCLUDE_JOBSYSTEM_HPP_
#define EC2S_INCLUDE_JOBSYSTEM_HPP_

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <functional>
#include <cassert>
#include <unordered_set>

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
            child.isChild = true;
            pChildren.push_back(&child);
            child.dependencyCount.fetch_add(1, std::memory_order_relaxed);
        }

    private:
        std::function<void()> f;
        Job* pNext{ nullptr };
        std::atomic_int dependencyCount{ 0 };
        std::vector<Job*> pChildren;
        bool isChild{ false };
    };

    class ThreadPool
    {
    public:
        // ThreadPool Public Methods
        explicit ThreadPool(std::optional<size_t> nThreads = std::nullopt)
            : mStop(false)
            , mpJobHead(nullptr)
            , mRemainingJobNum(0)
        {
            if (!nThreads)
            {
                nThreads = std::thread::hardware_concurrency() - 1;
            }

            mThreads.resize(std::max(static_cast<size_t>(1u), *nThreads));

            restart();
        }

        ~ThreadPool()
        {
            stop();

            // Clean up remaining jobs
            while (mpJobHead != nullptr)
            {
                Job* pJob = mpJobHead;
                mpJobHead = pJob->pNext;
                delete pJob;
            }
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
            Job* pJob = &job;
            if (mpJobHead == nullptr)
            {
                mpJobHead = pJob;
            }
            else
            {
                pJob->pNext = mpJobHead;
                mpJobHead   = pJob;
            }

            int childJobs               = 0;
            const auto countAllChildren = [&childJobs, this](Job* pJob, const auto& func) -> void
            {
                for (auto* pChild : pJob->pChildren)
                {
                    if (mpSubmittedJobs.contains(pChild))
                    {
                        continue;
                    }

                    mpSubmittedJobs.insert(pChild);
                    ++childJobs;
                    func(pChild, func);
                }
            };
            countAllChildren(&job, countAllChildren);

            mRemainingJobNum.fetch_add(1 + childJobs, std::memory_order_relaxed);

            cv.notify_one();
        }

        void submit(const std::function<void()>& f)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            Job* pJob   = new Job();
            pJob->f     = f;
            pJob->pNext = nullptr;

            if (mpJobHead == nullptr)
            {
                mpJobHead = pJob;
            }
            else
            {
                pJob->pNext = mpJobHead;
                mpJobHead   = pJob;
            }

            mRemainingJobNum.fetch_add(1, std::memory_order_relaxed);

            cv.notify_one();
        }

        void wait()
        {
            while (mpJobHead != nullptr)
            {
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mpJobHead == nullptr)
                    {
                        break;  // All jobs are processed
                    }
                }

                std::this_thread::yield();
            }

            while (mRemainingJobNum.load(std::memory_order_acquire) != 0)
            {
                std::this_thread::yield();  // Wait until all jobs are done
            }

            mRemainingJobNum.store(0, std::memory_order_relaxed);
            mpSubmittedJobs.clear();
        }

        void stop()
        {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStop = true;
            }

            cv.notify_all();

            while (true)
            {
                // Double check
                if (mpJobHead == nullptr)
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mpJobHead == nullptr)
                    {
                        break;  // All jobs are processed
                    }
                }
            }

            for (auto& thread : mThreads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }

            mRemainingJobNum.store(0, std::memory_order_relaxed);
            mpSubmittedJobs.clear();
            mStop = false;
        }

        void restart()
        {
            std::lock_guard<std::mutex> lock(mMutex);
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
                cv.wait(lock, [this] { return mStop || mpJobHead != nullptr; });

                if (mStop && mpJobHead == nullptr)
                {
                    return;
                }

                Job* pJob = mpJobHead;
                if (pJob == nullptr)
                {
                    continue;
                }

                mpJobHead = pJob->pNext;

                lock.unlock();

                pJob->f();

                mRemainingJobNum.fetch_sub(1, std::memory_order_relaxed);

                for (auto* pChild : pJob->pChildren)
                {
                    if (pChild->dependencyCount.fetch_sub(1) == 1)
                    {
                        std::lock_guard<std::mutex> lock(mMutex);
                        pChild->pNext = mpJobHead;
                        mpJobHead     = pChild;
                        cv.notify_one();
                    }
                }

                delete pJob;
            }
        }

    private:
        // ThreadPool Private Members
        std::vector<std::thread> mThreads;
        mutable std::mutex mMutex;
        std::atomic_int mRemainingJobNum;
        std::unordered_set<Job*> mpSubmittedJobs;
        bool mStop;
        Job* mpJobHead;
        std::condition_variable cv;
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
            threadPool.submit([&jobFunc]() { jobFunc(now, now + chunkSize); });

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
