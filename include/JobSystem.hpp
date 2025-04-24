/*****************************************************************/ /**
 * @file   JobSystem.hpp
 * @brief  header file of JobSystem class
 * 
 * @author ichi-raven
 * @date   April 2024
 *********************************************************************/

#ifndef EC2S_INCLUDE_JOBSYSTEM_HPP_
#define EC2S_INCLUDE_JOBSYSTEM_HPP_

#include <coroutine>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <cassert>
#include <future>

#include <iostream>

namespace ec2s
{
    struct JobPromise;
    class JobSystem;
    template<typename... Args>
    class WaitAllAwaiter;

    /**
     * @brief  internal representation of Job
     */
    struct Job
    {
        enum class Priority : uint8_t
        {
            eUrgent = 0,
            eHigh   = 1,
            eNormal = 2,
            eLow    = 3,
        };

        using promise_type = JobPromise;

        using HandleType = std::coroutine_handle<promise_type>;

        Job(HandleType h, Priority priority = Priority::eNormal)
            : handle(h)
        {
        }

        bool operator<(const Job& another) const
        {
            return static_cast<std::underlying_type_t<Priority>>(this->priority) < static_cast<std::underlying_type_t<Priority>>(another.priority);
        }

        HandleType handle;
        Priority priority;
    };

    struct JobPromise
    {
        void set_continuation(std::coroutine_handle<> h)
        {
            continuation = h;
        }

        void set_on_complete(std::function<void()> fn)
        {
            on_complete = std::move(fn);
        }

        Job get_return_object()
        {
            std::cout << "get_return_object" << "\n";
            return Job(Job::HandleType::from_promise(*this));
        }

        std::suspend_always initial_suspend()
        {
            std::cout << "initial_suspend" << "\n";

            return std::suspend_always{};
        }

        auto final_suspend() noexcept
        {
            struct FinalAwaiter
            {
                bool await_ready() noexcept
                {
                    return false;
                }

                void await_suspend(Job::HandleType h) noexcept
                {
                    if (h.promise().on_complete)
                    {
                        h.promise().on_complete();  // when_all用
                    }
                    else if (h.promise().continuation)
                    {
                        h.promise().continuation.resume();  // 単独await
                    }
                }
                void await_resume() noexcept
                {
                }
            };
            return FinalAwaiter{};
        }

        void return_void()
        {
            std::cout << "return_void" << "\n";
        }

        void unhandled_exception()
        {
            std::cout << "unhandled_exception" << "\n";

            std::terminate();
        }

        std::coroutine_handle<> continuation;
        std::function<void()> on_complete = nullptr;
    };

    /**
     * @brief  job system for parallel execution of specified job(task)
     */
    class JobSystem
    {
    public:
        /** 
         * @brief  constructor
         *  
         * @param workerThreadNum number of tasks to be executed in parallel (default: -1, use all hardware threads)
         */
        JobSystem(std::optional<size_t> workerThreadNum = std::nullopt)
        {
            if (!workerThreadNum)
            {
                workerThreadNum = std::max(1u, std::thread::hardware_concurrency() - 1);
            }

            assert(workerThreadNum >= 1 || "workerThreadNum must be greater than 0");
            mWorkerThreads.resize(workerThreadNum.value());

            for (auto& thread : mWorkerThreads)
            {
                thread = std::jthread([this]() { workerLoop(); });
            }
        }

        /** 
         * @brief  destructor(stop all worker threads)
         *  
         */
        ~JobSystem()
        {
            // double check
            if (!mStop)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mStop)
                {
                    stop();
                }
            }
        }

        /** 
         * @brief  register the specified function as a job
         *  
         */
        void submit(Job&& job)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            mJobs.emplace(std::move(job));
            mConditionVariable.notify_one();
        }

        
        void stop()
        {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStop = true;
            }

            mConditionVariable.notify_all();
            
            for (auto& thread : mWorkerThreads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }
        }

        /** 
         * @brief  get the current number of worker threads
         *  
         * @return current number of worker threads
         */
        uint32_t getWorkerThreadNum() const
        {
            return static_cast<uint32_t>(mWorkerThreads.size());
        }

    private:
        void workerLoop()
        {
            std::optional<Job> job;

            while (true)
            {
                // exclusive lock (wait job)
                {
                    std::unique_lock<std::mutex> lock(mMutex);
                    mConditionVariable.wait(lock, [this] { return mStop || !mJobs.empty(); });

                    if (mStop && mJobs.empty())
                    {
                        return;
                    }

                    job = mJobs.top();
                    mJobs.pop();
                }

                // execute job
                if (job)
                {
                    job->handle.resume();
                }
            }
        }

    private:
        //! worker threads
        std::vector<std::jthread> mWorkerThreads;
        //! condition variable to control all worker threads
        std::condition_variable mConditionVariable;

        // async-------------
        //! mutex for all worker threads
        std::mutex mMutex;
        //! jobs queue
        std::priority_queue<Job> mJobs;
        //! flag indicating whether the system is stopped
        bool mStop;
        // ------------------
    };

    template <typename... Jobs>
    class WaitAllAwaiter
    {
    public:
        WaitAllAwaiter(JobSystem& jobSystem, Jobs&&... js)
            : mJobSystem(jobSystem)
            , mJobs(std::forward<Jobs>(js)...)
            , mCounter(sizeof...(Jobs))
        {
        }

        bool await_ready() const noexcept
        {
            return mCounter == 0;
        }

        void await_suspend(std::coroutine_handle<> h)
        {
            mContinuation = h;
            apply(
                [this](Job& job)
                {
                    // 各ジョブ完了時に this->resume_one() を呼ぶよう設定
                    job.handle.promise().set_on_complete([this] { this->resume_one(); });
                    mJobSystem.submit(std::move(job));  // ← スレッドプールで実行
                });
        }

        void await_resume() noexcept
        {
        }

    private:
        void resume_one()
        {
            if (mCounter.fetch_sub(1) == 1)
            {
                mContinuation.resume();  // 全部完了したら再開
            }
        }

        template <typename F>
        void apply(F&& f)
        {
            std::apply([&](auto&... job) { (f(job), ...); }, mJobs);
        }

        JobSystem& mJobSystem;
        std::tuple<Jobs...> mJobs;
        std::atomic<size_t> mCounter;
        std::coroutine_handle<> mContinuation;
    };


    template <typename... Jobs>
    auto static waitAll(JobSystem& jobSystem, Jobs&&... jobs)
    {
        return WaitAllAwaiter<Jobs...>(jobSystem, std::forward<Jobs>(jobs)...);
    }

}  // namespace ec2s

#endif
