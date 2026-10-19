/*****************************************************************//**
 * @file   JobSystemTest.cpp
 * @brief  Test codes for JobSystem
 * 
 * @author ichi-raven
 * @date   November 2024
 *********************************************************************/

#include <gtest/gtest.h>
#include <atomic>
#include <vector>
#include <chrono>
#include <thread>
#include <numeric>
#include <mutex>
#include <future>
#include <memory>
#include <semaphore>
#include <barrier>
#include <latch>

#include <JobSystem.hpp>

using namespace ec2s;

// ------------------------------------------------------------
// basic job execution
// ------------------------------------------------------------

TEST(ThreadPoolBasic, SingleJobExecution)
{
    ThreadPool pool(2);

    std::atomic<bool> executed = false;

    pool.submit([&]() { executed = true; });

    pool.wait();

    EXPECT_TRUE(executed.load());
}

TEST(ThreadPoolBasic, MultipleJobsExecution)
{
    ThreadPool pool(4);

    constexpr int N          = 100;
    std::atomic<int> counter = 0;

    for (int i = 0; i < N; ++i)
    {
        pool.submit([&]() { counter.fetch_add(1); });
    }

    pool.wait();

    EXPECT_EQ(counter.load(), N);
}

// ------------------------------------------------------------
// parallel execution speedup
// ------------------------------------------------------------

TEST(ThreadPoolConcurrency, ParallelExecutionSpeedup)
{
    ThreadPool pool(4);

    constexpr int N          = 4;
    std::atomic<int> counter = 0;

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i)
    {
        pool.submit(
            [&]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                counter.fetch_add(1);
            });
    }

    pool.wait();

    auto end     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(counter.load(), N);
    EXPECT_LT(elapsed, 800);  // ~800ms in serial
}

// ------------------------------------------------------------
// Job dependency test
// ------------------------------------------------------------

TEST(ThreadPoolDependency, ChildRunsAfterParent)
{
    ThreadPool pool(2);

    std::vector<int> order;
    std::mutex m;

    auto& parent = pool.createJob(
        [&]()
        {
            std::lock_guard<std::mutex> lock(m);
            order.push_back(1);
        });

    auto& child = pool.createJob(
        [&]()
        {
            std::lock_guard<std::mutex> lock(m);
            order.push_back(2);
        });

    parent.addChild(std::move(child));

    pool.submit(std::move(parent));
    pool.wait();

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

// ------------------------------------------------------------
// stop / restart
// ------------------------------------------------------------

TEST(ThreadPoolLifecycle, StopAndRestart)
{
    ThreadPool pool(2);

    std::atomic<int> counter = 0;

    pool.submit([&]() { counter++; });
    pool.wait();

    pool.stop();
    pool.restart();

    pool.submit([&]() { counter++; });
    pool.wait();

    EXPECT_EQ(counter.load(), 2);
}

// ------------------------------------------------------------
// parallelFor (1D)
// ------------------------------------------------------------

TEST(ParallelForTest, Basic1D)
{
    ThreadPool pool(4);

    constexpr int N = 1000;
    std::vector<int> values(N, 0);

    parallelFor(0, N, [&](size_t i) { values[i] = 1; }, pool);

    int sum = std::accumulate(values.begin(), values.end(), 0);
    EXPECT_EQ(sum, N);
}

// ------------------------------------------------------------
// parallelForChunk
// ------------------------------------------------------------

TEST(ParallelForTest, Chunk1D)
{
    ThreadPool pool(4);

    constexpr int N = 1000;
    std::vector<int> values(N, 0);

    parallelForChunk(
        0, N,
        [&](size_t start, size_t end)
        {
            for (size_t i = start; i < end; ++i)
            {
                values[i] = 1;
            }
        },
        pool);

    int sum = std::accumulate(values.begin(), values.end(), 0);
    EXPECT_EQ(sum, N);
}

// ------------------------------------------------------------
// parallelFor2D
// ------------------------------------------------------------

TEST(ParallelForTest, Basic2D)
{
    ThreadPool pool(4);

    constexpr int X = 50;
    constexpr int Y = 50;

    std::atomic<int> counter = 0;

    parallelFor2D({ 0, 0 }, { X, Y }, [&](size_t x, size_t y) { counter.fetch_add(1); }, pool);

    EXPECT_EQ(counter.load(), X * Y);
}

// ------------------------------------------------------------
// parallelFor2DChunk
// ------------------------------------------------------------

TEST(ParallelForTest, Chunk2D)
{
    ThreadPool pool(4);

    constexpr int X = 30;
    constexpr int Y = 20;

    std::atomic<int> counter = 0;

    parallelFor2DChunk(
        { 0, 0 }, { X, Y },
        [&](std::pair<size_t, size_t> start, std::pair<size_t, size_t> end)
        {
            for (size_t x = start.first; x < end.first; ++x)
            {
                for (size_t y = start.second; y < end.second; ++y)
                {
                    counter.fetch_add(1);
                }
            }
        },
        pool);

    EXPECT_EQ(counter.load(), X * Y);
}

// ------------------------------------------------------------
// boundary conditions for parallelFor
// ------------------------------------------------------------

TEST(ParallelForEdgeCases, ZeroRange)
{
    ThreadPool pool(4);

    std::atomic<int> counter = 0;

    parallelFor(0, 0, [&](size_t) { counter++; }, pool);

    EXPECT_EQ(counter.load(), 0);
}

// ------------------------------------------------------------
// Stress test
// ------------------------------------------------------------

TEST(ThreadPoolStress, ManySmallTasks)
{
    ThreadPool pool(8);

    constexpr int N          = 10000;
    std::atomic<int> counter = 0;

    for (int i = 0; i < N; ++i)
    {
        pool.submit([&]() { counter.fetch_add(1); });
    }

    pool.wait();

    EXPECT_EQ(counter.load(), N);
}

TEST(ThreadPoolCompletion, WaitIncludesJobDestruction)
{
    ThreadPool pool(1);
    std::binary_semaphore run{ 0 };
    std::binary_semaphore destroying{ 0 };
    std::binary_semaphore releaseDestruction{ 0 };
    std::atomic<bool> destroyed{ false };

    auto resource = std::shared_ptr<int>(new int, [&](int* value)
    {
        destroying.release();
        releaseDestruction.acquire();
        delete value;
        destroyed = true;
    });
    pool.submit([resource, &run] { run.acquire(); });
    resource.reset();
    run.release();
    destroying.acquire();

    std::promise<void> completed;
    auto completion = completed.get_future();
    std::thread waiter([&]
    {
        pool.wait();
        completed.set_value();
    });

    EXPECT_EQ(completion.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    releaseDestruction.release();
    waiter.join();
    EXPECT_TRUE(destroyed.load());
    pool.stop();
}

TEST(ThreadPoolLifecycle, StopKeepsWorkersForDependentJobs)
{
    ThreadPool pool(2);
    std::binary_semaphore parentStarted{ 0 };
    std::binary_semaphore releaseParent{ 0 };
    std::counting_semaphore<2> childrenStarted{ 0 };
    std::counting_semaphore<2> releaseChildren{ 0 };
    std::atomic<int> executed{ 0 };

    auto& parent = pool.createJob([&]
    {
        parentStarted.release();
        releaseParent.acquire();
    });
    const auto childTask = [&]
    {
        childrenStarted.release();
        releaseChildren.acquire();
        ++executed;
    };
    auto& first = pool.createJob(childTask);
    auto& second = pool.createJob(childTask);
    parent.addChild(std::move(first));
    parent.addChild(std::move(second));
    pool.submit(std::move(parent));
    parentStarted.acquire();

    std::promise<void> stopped;
    auto completion = stopped.get_future();
    std::thread stopper([&]
    {
        pool.stop();
        stopped.set_value();
    });
    EXPECT_EQ(completion.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    releaseParent.release();

    const bool firstStarted = childrenStarted.try_acquire_for(std::chrono::seconds(1));
    const bool secondStarted = childrenStarted.try_acquire_for(std::chrono::seconds(1));
    releaseChildren.release(2);
    stopper.join();

    EXPECT_TRUE(firstStarted);
    EXPECT_TRUE(secondStarted);
    EXPECT_EQ(executed.load(), 2);
}

TEST(ThreadPoolDependency, SharedDescendantsAcrossSubmissions)
{
    ThreadPool pool(4);
    int firstValue = 0;
    int secondValue = 0;
    int result = 0;
    auto& first = pool.createJob([&] { firstValue = 20; });
    auto& second = pool.createJob([&] { secondValue = 22; });
    auto& child = pool.createJob([&] { result = firstValue + secondValue; });
    first.addChild(std::move(child));
    second.addChild(std::move(child));

    pool.submit(std::move(first));
    pool.submit(std::move(second));
    pool.wait();

    EXPECT_EQ(result, 42);
}

TEST(ThreadPoolCompletion, RepeatedGraphsWhileAnotherJobIsRunning)
{
    ThreadPool pool(2);
    std::binary_semaphore blockerStarted{ 0 };
    std::binary_semaphore releaseBlocker{ 0 };
    std::binary_semaphore childCompleted{ 0 };
    pool.submit([&]
    {
        blockerStarted.release();
        releaseBlocker.acquire();
    });
    blockerStarted.acquire();

    for (int i = 0; i < 256; ++i)
    {
        auto& parent = pool.createJob([] {});
        auto& child = pool.createJob([&] { childCompleted.release(); });
        parent.addChild(std::move(child));
        pool.submit(std::move(parent));
        childCompleted.acquire();
    }

    std::promise<void> completed;
    auto completion = completed.get_future();
    std::thread waiter([&]
    {
        pool.wait();
        completed.set_value();
    });
    EXPECT_EQ(completion.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    releaseBlocker.release();
    const auto status = completion.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(status, std::future_status::ready);
    pool.stop();
    waiter.join();
}

TEST(ThreadPoolConcurrency, ConcurrentSubmissionsExecuteExactlyOnce)
{
    ThreadPool pool(4);
    constexpr int producerCount = 4;
    constexpr int perProducer   = 5000;
    std::vector<std::atomic<int>> executions(producerCount * perProducer);
    std::barrier start(producerCount);
    std::vector<std::thread> producers;
    for (int producer = 0; producer < producerCount; ++producer)
    {
        producers.emplace_back(
            [&, producer]
            {
                start.arrive_and_wait();
                for (int i = 0; i < perProducer; ++i)
                {
                    const int index = producer * perProducer + i;
                    pool.submit([&, index] { ++executions[index]; });
                }
            });
    }
    for (auto& producer : producers)
    {
        producer.join();
    }
    pool.wait();
    for (const auto& count : executions)
    {
        EXPECT_EQ(count.load(), 1);
    }
}

TEST(ThreadPoolDependency, ConcurrentRootsShareDescendants)
{
    ThreadPool pool(4);
    constexpr int parentCount = 4;
    constexpr int chainLength = 256;
    std::vector<int> values(parentCount, 0);
    std::vector<int> executions(chainLength, 0);
    std::vector<Job*> parents;
    std::vector<Job*> chain;
    int result = 0;

    for (int i = 0; i < parentCount; ++i)
    {
        parents.push_back(&pool.createJob([&, i] { values[i] = i + 1; }));
    }
    for (int i = 0; i < chainLength; ++i)
    {
        chain.push_back(&pool.createJob(
            [&, i]
            {
                ++executions[i];
                if (i == 0)
                {
                    result = std::accumulate(values.begin(), values.end(), 0);
                }
            }));
    }
    for (auto* parent : parents)
    {
        parent->addChild(std::move(*chain.front()));
    }
    for (int i = 1; i < chainLength; ++i)
    {
        chain[i - 1]->addChild(std::move(*chain[i]));
    }

    std::barrier start(parentCount);
    std::vector<std::thread> producers;
    for (auto* parent : parents)
    {
        producers.emplace_back(
            [&, parent]
            {
                start.arrive_and_wait();
                pool.submit(std::move(*parent));
            });
    }
    for (auto& producer : producers)
    {
        producer.join();
    }
    pool.wait();
    EXPECT_EQ(result, 10);
    for (const auto count : executions)
    {
        EXPECT_EQ(count, 1);
    }
}

TEST(ThreadPoolCompletion, MultipleWaitersObserveResults)
{
    ThreadPool pool(2);
    std::binary_semaphore started{ 0 };
    std::binary_semaphore releaseJob{ 0 };
    int result = 0;
    pool.submit(
        [&]
        {
            started.release();
            releaseJob.acquire();
            result = 42;
        });
    started.acquire();

    std::latch waiting(4);
    std::vector<std::thread> waiters;
    for (int i = 0; i < 4; ++i)
    {
        waiters.emplace_back(
            [&]
            {
                waiting.count_down();
                pool.wait();
                EXPECT_EQ(result, 42);
            });
    }
    waiting.wait();
    releaseJob.release();
    for (auto& waiter : waiters)
    {
        waiter.join();
    }
}

TEST(ThreadPoolLifecycle, RepeatedStopAndRestartDrainsJobs)
{
    ThreadPool pool(4);
    std::atomic<int> executed{ 0 };
    for (int cycle = 0; cycle < 16; ++cycle)
    {
        pool.restart();
        for (int i = 0; i < 100; ++i)
        {
            pool.submit([&] { ++executed; });
        }
        pool.stop();
        EXPECT_EQ(executed.load(), (cycle + 1) * 100);
        pool.stop();
        pool.restart();
    }
}

namespace
{
    class JobQueueResourceScope
    {
    public:
        explicit JobQueueResourceScope(std::pmr::memory_resource& resource)
        {
            setConcurrentQueueMemoryResource(resource);
        }

        ~JobQueueResourceScope()
        {
            setConcurrentQueueMemoryResource(*std::pmr::new_delete_resource());
        }
    };

    class FailingJobQueueResource : public std::pmr::memory_resource
    {
    public:
        std::atomic<bool> fail{ false };
        std::atomic<int> failures{ 0 };

    private:
        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            if (fail.load())
            {
                ++failures;
                throw std::bad_alloc();
            }
            return std::pmr::new_delete_resource()->allocate(bytes, alignment);
        }

        void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override
        {
            std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
        }

        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
        {
            return this == &other;
        }
    };
}  // namespace

TEST(ThreadPoolAllocationFailure, RootRunsOnSubmittingThread)
{
    JobQueueResourceScope resource(*std::pmr::null_memory_resource());
    ThreadPool pool(2);
    std::thread::id executedOn;
    pool.submit([&] { executedOn = std::this_thread::get_id(); });
    EXPECT_EQ(executedOn, std::this_thread::get_id());
    pool.wait();
    pool.stop();
}

TEST(ThreadPoolAllocationFailure, ChildRunsOnParentWorker)
{
    FailingJobQueueResource resource;
    JobQueueResourceScope useResource(resource);
    ThreadPool pool(2);
    std::binary_semaphore parentStarted{ 0 };
    std::binary_semaphore releaseParent{ 0 };
    std::thread::id parentThread;
    std::thread::id childThread;
    auto& parent = pool.createJob(
        [&]
        {
            parentThread = std::this_thread::get_id();
            parentStarted.release();
            releaseParent.acquire();
        });
    auto& child = pool.createJob([&] { childThread = std::this_thread::get_id(); });
    parent.addChild(std::move(child));
    pool.submit(std::move(parent));
    parentStarted.acquire();
    resource.fail = true;
    releaseParent.release();
    pool.wait();

    EXPECT_GT(resource.failures.load(), 0);
    EXPECT_NE(parentThread, std::this_thread::get_id());
    EXPECT_EQ(childThread, parentThread);
}

TEST(ThreadPoolAllocationFailure, SharedDescendantsCompleteExactlyOnce)
{
    JobQueueResourceScope resource(*std::pmr::null_memory_resource());
    ThreadPool pool(2);
    int executions  = 0;
    int firstValue  = 0;
    int secondValue = 0;
    int result      = 0;
    auto& first     = pool.createJob([&] { firstValue = 20; });
    auto& second    = pool.createJob([&] { secondValue = 22; });
    auto& child     = pool.createJob(
        [&]
        {
            ++executions;
            result = firstValue + secondValue;
        });
    first.addChild(std::move(child));
    second.addChild(std::move(child));
    pool.submit(std::move(first));
    pool.submit(std::move(second));
    pool.wait();

    EXPECT_EQ(result, 42);
    EXPECT_EQ(executions, 1);
    pool.stop();
}
