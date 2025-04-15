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

namespace ec2s
{
    struct JobPromise
    {
        Job get_return_object()
        {
            return Job{};
        }
        std::suspend_never initial_suspend()
        {
            return {};
        }
        std::suspend_always final_suspend() noexcept
        {
            return {};
        }
        void return_void()
        {
        }
        void unhandled_exception()
        {
        }
    };

    /**
     * @brief  internal representation of Job
     */
    struct Job : std::coroutine_handle<JobPromise>
    {
        using promise_type = JobPromise;

        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> h)
        {
            continuation = h;
        }

        void await_resume()
        {
        }

        std::coroutine_handle<> continuation;
    };

    /**
     * @brief  job system for parallel execution of specified job(task)
     */
    class JobSystem
    {
    private:
    public:
        //! Job handle expression
        using JobHandle = Job*;

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
                workerThreadNum = static_cast<int>(std::thread::hardware_concurrency());
            }

            assert(workerThreadNum >= 1 || "workerThreadNum must be greater than 0");
            mWorkerThreads.resize(static_cast<size_t>(workerThreadNum.value()));

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
            for (auto& thread : mWorkerThreads)
            {
                thread = std::thread(
                    [this]()
                    {
                        while (true)
                        {
                            std::coroutine_handle<> job;
                            {
                                std::unique_lock<std::mutex> lock(mMutex);
                                mConditionVariable.wait(lock, [this] { return mStop || !mJobs.empty(); });

                                if (mStop && mJobs.empty())
                                {
                                    return;
                                }

                                job = std::move(mJobs.front());
                                mJobs.pop();
                            }

                            job.resume();
                        }
                    });
            }
        }

        /** 
         * @brief  register the specified function as a job
         *  
         */
        JobHandle schedule(const Job& job)
        {
            assert(!mStop || !"This JobSystem is currently stopped!");
            if (!mStop)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mJobs.emplace(job);
            }
        }

        /** 
         * @brief  execute all currently registered jobs
         *  
         */
        void exec()
        {
            assert(!mStop || !"This JobSystem is currently stopped!");

            uint32_t jobNum = 0;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                jobNum = static_cast<uint32_t>(mJobs.size());
            }

            mConditionVariable.notify_all();

            if (jobNum > mWorkerThreads.size())
            {
                for (int i = 0; i < jobNum - mWorkerThreads.size(); ++i)
                {
                    mConditionVariable.notify_one();
                }
            }
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
                thread.join();
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
        //! worker threads
        std::vector<std::thread> mWorkerThreads;
        //! condition variable to control all worker threads
        std::condition_variable mConditionVariable;

        // async-------------
        //! mutex for all worker threads
        std::mutex mMutex;
        //! jobs queue
        std::queue<std::coroutine_handle<>> mJobs;
        //! flag indicating whether the system is stopped
        bool mStop;
        // ------------------
    };

}  // namespace ec2s

#endif
