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
    struct JobSystem;
    struct JobList;

    struct JobPromise
    {
        friend class JobSystem;
        friend class SuspendJob;
        friend class FinalizeJob;

        Job get_return_object()
        {
            std::cout << "get_return_object" << "\n";
            return Job::from_promise(*this);
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

    private:
        JobSystem* pJobSystem = nullptr;
        JobList* pJobList     = nullptr;
    };

    struct SuspendJob
    {

        constexpr bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<JobPromise> handle) noexcept
        {
            std::cout << "await_suspend" << "\n";
            auto& promise = handle.promise();

            auto& jobList = *promise.pJobList;

            jobList.add(promise.get_return_object());

            

            {
                // --- Eager Workers ---
                //
                // Eagerly try to fetch & execute the next task from the front of the
                // scheduler queue -
                // We do this so that multiple threads can share the
                // scheduling workload.
                //
                // But we can also disable that, so that there is only one thread
                // that does the scheduling, and removing elements from the
                // queue.

                coroutine_handle_t c = task_list->pop_task();

                if (c)
                {
                    assert(!c.done() && "task must not be done");
                    c();
                }
            }
        }

        void await_resume() noexcept
        {
            std::cout << "await_resume" << "\n";
        }
    };

    /**
     * @brief  internal representation of Job
     */
    struct Job : std::coroutine_handle<JobPromise>
    {
        friend class JobSystem;

        enum class Priority : uint8_t
        {
            eUrgent = 0,
            eHigh   = 1,
            eNormal = 2,
            eLow    = 3,
        };

        using promise_type = ::JobPromise;

        using HandleType = std::coroutine_handle<promise_type>;

        Job(HandleType h, Priority p = Priority::eNormal)
            : handle(h)
            , priority(p)
        {
        }

        Job(const Job&)            = delete;
        Job& operator=(const Job&) = delete;

        Job(Job&& other) noexcept
            : handle(other.handle)
            , priority(other.priority)
        {
            other.handle = nullptr;
        }

        Job& operator=(Job&& other) noexcept
        {
            this->handle   = other.handle;
            this->priority = other.priority;
            other.handle   = nullptr;

            return *this;
        }

        Job(const Job&& other) noexcept
            : handle(other.handle)
            , priority(other.priority)
        {
        }

        Job& operator=(const Job&& other) noexcept
        {
            this->handle   = other.handle;
            this->priority = other.priority;

            return *this;
        }

        void resume()
        {
            std::cout << "check resume" << "\n";

            if (handle && !handle.done())
            {
                std::cout << "resume" << "\n";
                handle.resume();
            }
        }

        bool done() const
        {
            std::cout << "check done" << "\n";
            return handle.done();
        }

        bool operator<(const Job& another) const
        {
            return static_cast<std::underlying_type_t<Priority>>(this->priority) < static_cast<std::underlying_type_t<Priority>>(another.priority);
        }

        HandleType handle;
        Priority priority;
    };

    class JobList
    {
    public:
        JobList() = default;
        JobList(Job&& job)
        {
            mJobs.emplace_back(std::move(job));
        }

        JobList(const JobList&)            = delete;
        JobList& operator=(const JobList&) = delete;
        JobList(JobList&&)                 = default;
        JobList& operator=(JobList&&)      = default;
        ~JobList()
        {
            for (auto& job : mJobs)
            {
                if (job.handle)
                {
                    job.handle.destroy();
                }
            }
        }

        void add(Job&& job)
        {
            mJobs.emplace_back(std::move(job));
        }

        inline std::size_t getJobCount()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mJobs.size();
        }

    private:
        std::mutex mMutex;
        std::vector<Job> mJobs;
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
