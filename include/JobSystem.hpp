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
            std::function<void()> mTask;

            void execute()
            {
                mTask();
            }
        };

    public:
        JobSystem(const uint32_t workerThreadNum)
            : mStop(false)
        {
            assert(workerThreadNum >= 1 || "workerThreadNum must be greater than 0");

            mWorkerThreads.resize(workerThreadNum);
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
                                mConditionVariable.wait(lock, [this] { return !mJobs.empty() || mStop; });

                                if (mStop)
                                {
                                    return;
                                }

                                job = std::move(mJobs.front());
                                mJobs.pop();
                            }

                            job.execute();
                        }
                    });
            }
        }

        ~JobSystem()
        {
            stop();
        }

        template <typename Func>
        void schedule(const Func f)
        {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mJobs.emplace(Job{ .mTask = f });
            }

            mConditionVariable.notify_one();
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

    private:
        std::vector<std::thread> mWorkerThreads;
        std::queue<Job> mJobs;
        std::condition_variable mConditionVariable;
        std::mutex mMutex;
        bool mStop;
    };

}  // namespace ec2s

#endif
