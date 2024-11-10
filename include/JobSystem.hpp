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

    class JobSystem
    {
    private:
        struct Job
        {
            std::function<void()> task;
        };

    public:
        using JobHandle = Job*;

    public:
        JobSystem(const uint32_t workerThreadNum)
            : mStop(false)
        {
            assert(workerThreadNum >= 1 || "workerThreadNum must be greater than 0");

            mWorkerThreads.resize(workerThreadNum);

            restart();
        }

        ~JobSystem()
        {
            if (!mStop)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                if (!mStop)
                {
                    stop();
                }
            }
        }

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

        template <typename Func>
        JobHandle schedule(const Func f)
        {
            assert(!mStop || !"This JobSystem is currently stopped!");

            {
                std::lock_guard<std::mutex> lock(mMutex);
                return &(mJobs.emplace(Job{ .task = f }));
            }
        }

        template <typename Func>
        JobHandle exec(const Func f)
        {
            assert(!mStop || !"This JobSystem is currently stopped!");

            JobHandle handle;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                handle = &(mJobs.emplace(Job{ .task = f }));
            }

            mConditionVariable.notify_one();
            return handle;
        }

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

        void join()
        {
            stop();
            restart();
        }

        uint32_t getWorkerThreadNum() const
        {
            return static_cast<uint32_t>(mWorkerThreads.size());
        }

    private:
        std::vector<std::thread> mWorkerThreads;

        std::condition_variable mConditionVariable;

        // async-------------
        std::mutex mMutex;
        std::queue<Job> mJobs;
        bool mStop;
        // ------------------
    };

}  // namespace ec2s

#endif
