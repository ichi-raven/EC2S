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
#include <queue>
#include <functional>
#include <cassert>

namespace ec2s
{
    /**
     * @brief  job system for parallel execution of specified job(task)
     */
    class JobSystem
    {
    private:
        /**
         * @brief  internal representation of Job
         */
        struct Job
        {
            //! function actually executed
            std::function<void()> task;
        };

    public:
        //! Job handle expression
        using JobHandle = Job*;

    public:
        /** 
         * @brief  constructor
         *  
         * @param workerThreadNum number of tasks to be executed in parallel
         */
        JobSystem(const uint32_t workerThreadNum)
            : mStop(false)
        {
            assert(workerThreadNum >= 1 || "workerThreadNum must be greater than 0");

            mWorkerThreads.resize(workerThreadNum);

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
                            Job job;
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

                            job.task();
                        }
                    });
            }
        }

        /** 
         * @brief  register the specified function as a job
         *  
         * @tparam Func type of specified function
         * @param f specified function
         * @return registered job's handle
         */
        template <typename Func>
        JobHandle schedule(const Func f)
        {
            assert(!mStop || !"This JobSystem is currently stopped!");
            if (!mStop)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return &(mJobs.emplace(Job{ .task = f }));
            }
        }

        /** 
         * @brief  immediately executes the specified function as a job in a worker thread
         *  
         * @tparam Func type of specified function
         * @param f specified function
         * @return executing job's handle
         */
        template <typename Func>
        JobHandle exec(const Func f)
        {
            assert(!mStop || !"This JobSystem is currently stopped!");

            if (mStop)
            {
                return nullptr;
            }

            JobHandle handle;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                handle = &(mJobs.emplace(Job{ .task = f }));
            }

            mConditionVariable.notify_one();
            return handle;
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
        std::queue<Job> mJobs;
        //! flag indicating whether the system is stopped
        bool mStop;
        // ------------------
    };

}  // namespace ec2s

#endif
