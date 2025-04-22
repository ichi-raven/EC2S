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

        struct promise_type;

        using HandleType = std::coroutine_handle<promise_type>;

        struct promise_type
        {
            auto get_return_object()
            {
                std::cout << "get_return_object" << "\n";
                return Job{ std::coroutine_handle<promise_type>::from_promise(*this) };
            }

            std::suspend_always initial_suspend()
            {
                std::cout << "initial_suspend" << "\n";

                return std::suspend_always{};
            }

            std::suspend_always final_suspend() noexcept
            {
                std::cout << "final_suspend" << "\n";

                return std::suspend_always{};
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
        };

        Job(HandleType h, Priority p = Priority::eNormal)
            : coro(h)
            , priority(p)
        {
        }

        Job(const Job&)            = delete;
        Job& operator=(const Job&) = delete;

        Job(Job&& other) noexcept
            : coro(other.coro)
            , priority(other.priority)
        {
            other.coro = nullptr;
        }

        Job& operator=(Job&& other) noexcept
        {
            this->coro     = other.coro;
            this->priority = other.priority;
            other.coro     = nullptr;

            return *this;
        }

        Job(const Job&& other) noexcept
            : coro(other.coro)
            , priority(other.priority)
        {
        }

        Job& operator=(const Job&& other) noexcept
        {
            this->coro     = other.coro;
            this->priority = other.priority;

            return *this;
        }

        void resume()
        {
            std::cout << "check resume" << "\n";

            if (coro && !coro.done())
            {
                std::cout << "resume" << "\n";
                coro.resume();
            }
        }

        bool done() const
        {
            std::cout << "check done" << "\n";
            return coro.done();
        }

        // Awaiter‚ð•Ô‚·
        auto operator co_await()
        {
            struct Awaiter
            {
                HandleType coro;

                bool await_ready() const noexcept
                {
                    std::cout << "await_ready" << "\n";
                    return false;
                }

                void await_suspend(HandleType awaiting) const noexcept
                {
                    std::cout << "await_suspend" << "\n";
                    auto& promise = awaiting.promise();
                }

                void await_resume() const noexcept
                {
                    std::cout << "await_resume" << "\n";
                }
            };

            return Awaiter{ coro };
        }

        bool operator<(const Job& another) const
        {
            return static_cast<std::underlying_type_t<Priority>>(this->priority) < static_cast<std::underlying_type_t<Priority>>(another.priority);
        }

        HandleType coro;
        Priority priority;
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
            : mStop(false)
        {
            if (!workerThreadNum)
            {
                workerThreadNum = std::max(1u, std::thread::hardware_concurrency() - 1);
            }

            assert(workerThreadNum >= 1 || "workerThreadNum must be greater than 0");
            mWorkerThreads.resize(workerThreadNum.value());

            restart();
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
         * @brief  restart the stopped JobSystem (all worker threads)
         *  
         */
        void restart()
        {
            if (!mStop)
            {
                stop();
            }

            {
                std::lock_guard<std::mutex> lock(mMutex);
                mStop = false;
            }

            for (auto& thread : mWorkerThreads)
            {
                thread = std::thread([this]() { workerLoop(); });
            }
        }

        /** 
         * @brief  register the specified function as a job
         *  
         */
        void submit(Job&& job)
        {
            assert(!mStop || !"This JobSystem is currently stopped!");
            if (!mStop)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mJobs.emplace(std::move(job));
            }

            mConditionVariable.notify_one();
        }

        /** 
         * @brief  stop all worker threads
         *  
         */
        void stop()
        {
            {
                std::unique_lock<std::mutex> lock(mMutex);
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
         * @brief  join all worker threads once and restart them
         *  
         */
        void join()
        {
            stop();
            restart();
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

                    job = std::move(mJobs.top());
                    mJobs.pop();
                }

                // execute job
                if (job)
                {
                    while (!job->done())
                    {
                        job->resume();
                    }
                }
            }
        }

    private:
        //! worker threads
        std::vector<std::thread> mWorkerThreads;
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

}  // namespace ec2s

#endif
