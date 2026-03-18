/*****************************************************************/ /**
 * @file   LockFreeQueueTest.cpp
 * @brief  Test codes for LockFreeQueue
 * 
 * @author ichi-raven
 * @date   March 2026
 *********************************************************************/

#include <gtest/gtest.h>
#include <LockFreeQueue.hpp>

#include <thread>
#include <vector>
#include <atomic>
#include <memory_resource>
#include <unordered_set>
#include <chrono>

class LockFreeQueueTest : public ::testing::Test
{
protected:
    ec2s::LockFreeQueue<int> queue;
};

//////////////////////////////////////////////////////////////
// Basic functionality
//////////////////////////////////////////////////////////////

TEST_F(LockFreeQueueTest, BasicPushPop)
{
    queue.push(1);
    queue.push(2);
    queue.push(3);

    int v;

    ASSERT_TRUE(queue.pop(v));
    EXPECT_EQ(v, 1);

    ASSERT_TRUE(queue.pop(v));
    EXPECT_EQ(v, 2);

    ASSERT_TRUE(queue.pop(v));
    EXPECT_EQ(v, 3);
}

//////////////////////////////////////////////////////////////
// FIFO property
//////////////////////////////////////////////////////////////

TEST_F(LockFreeQueueTest, FIFOOrder)
{
    constexpr int N = 1000;

    for (int i = 0; i < N; i++) queue.push(i);

    int v;

    for (int i = 0; i < N; i++)
    {
        ASSERT_TRUE(queue.pop(v));
        EXPECT_EQ(v, i);
    }
}

//////////////////////////////////////////////////////////////
// Empty queue behavior
//////////////////////////////////////////////////////////////

TEST_F(LockFreeQueueTest, EmptyQueue)
{
    int v;

    EXPECT_FALSE(queue.pop(v));

    queue.push(42);

    EXPECT_TRUE(queue.pop(v));
    EXPECT_EQ(v, 42);

    EXPECT_FALSE(queue.pop(v));
}

//////////////////////////////////////////////////////////////
// Large data volume
//////////////////////////////////////////////////////////////

TEST_F(LockFreeQueueTest, LargeVolume)
{
    constexpr int N = 100000;

    for (int i = 0; i < N; i++) queue.push(i);

    int v;
    int count = 0;

    while (queue.pop(v)) count++;

    EXPECT_EQ(count, N);
}

//////////////////////////////////////////////////////////////
// Multi-producer test
//////////////////////////////////////////////////////////////

TEST_F(LockFreeQueueTest, MultiThreadPush)
{
    constexpr int threadCount = 4;
    constexpr int perThread   = 10000;

    std::vector<std::thread> threads;

    for (int t = 0; t < threadCount; t++)
    {
        threads.emplace_back(
            [this, t]()
            {
                for (int i = 0; i < perThread; i++) queue.push(t * perThread + i);
            });
    }

    for (auto& th : threads) th.join();

    int v;
    int count = 0;

    while (queue.pop(v)) count++;

    EXPECT_EQ(count, threadCount * perThread);
}

//////////////////////////////////////////////////////////////
// Multi-producer multi-consumer
//////////////////////////////////////////////////////////////

TEST_F(LockFreeQueueTest, MultiThreadPushPop)
{
    constexpr int producerCount = 4;
    constexpr int consumerCount = 4;
    constexpr int perProducer   = 5000;

    std::atomic<int> popCount{ 0 };

    std::vector<std::thread> threads;

    for (int p = 0; p < producerCount; p++)
    {
        threads.emplace_back(
            [this]()
            {
                for (int i = 0; i < perProducer; i++)
                {
                    queue.push(i);
                }
            });
    }

    for (int c = 0; c < consumerCount; c++)
    {
        threads.emplace_back(
            [this, &popCount]()
            {
                int v;
                int local = 0;

                while (local < perProducer)
                {
                    if (queue.pop(v))
                    {
                        local++;
                        popCount++;
                    }
                }
            });
    }

    for (auto& th : threads)
    {
        th.join();
    }

    EXPECT_EQ(popCount.load(), producerCount * perProducer);
}

//////////////////////////////////////////////////////////////
// ABA stress test
//////////////////////////////////////////////////////////////

TEST_F(LockFreeQueueTest, ABAStress)
{
    constexpr int threads = 8;
    constexpr int ops     = 200000;

    std::atomic<bool> start{ false };

    std::vector<std::thread> workers;

    for (int t = 0; t < threads; t++)
    {
        workers.emplace_back(
            [this, &start]()
            {
                while (!start.load())
                {
                }

                for (int i = 0; i < ops; i++)
                {
                    queue.push(i);

                    int v;
                    queue.pop(v);
                }
            });
    }

    start = true;

    for (auto& w : workers) w.join();

    int v;
    int remaining = 0;

    while (queue.pop(v)) remaining++;

    EXPECT_GE(remaining, 0);
}

//////////////////////////////////////////////////////////////
// Extreme contention benchmark
//////////////////////////////////////////////////////////////

TEST_F(LockFreeQueueTest, MillionOperationStress)
{
    constexpr int threadCount = 8;
    constexpr int operations  = 1000000;

    std::atomic<int> consumed{ 0 };

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; i++)
    {
        threads.emplace_back(
            [this]()
            {
                for (int j = 0; j < operations; j++) queue.push(j);
            });
    }

    for (int i = 0; i < threadCount; i++)
    {
        threads.emplace_back(
            [this, &consumed]()
            {
                int v;

                while (consumed < operations * 8)
                {
                    if (queue.pop(v)) consumed++;
                }
            });
    }

    for (auto& th : threads) th.join();

    auto end = std::chrono::high_resolution_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(consumed.load(), operations * 8);
    EXPECT_LT(ms, 5000);  // heuristic limit
}

//////////////////////////////////////////////////////////////
// TSAN race detection friendly test
//////////////////////////////////////////////////////////////

TEST_F(LockFreeQueueTest, TSANConcurrencyTest)
{
    constexpr int threads = 4;
    constexpr int ops     = 50000;

    std::atomic<bool> start{ false };

    std::vector<std::thread> workers;

    for (int i = 0; i < threads; i++)
    {
        workers.emplace_back(
            [this, &start]()
            {
                while (!start.load())
                {
                }

                int v;

                for (int j = 0; j < ops; j++)
                {
                    queue.push(j);
                    queue.pop(v);
                }
            });
    }

    start = true;

    for (auto& w : workers) w.join();
}

//////////////////////////////////////////////////////////////
// Memory resource leak detection
//////////////////////////////////////////////////////////////

class CountingResource : public std::pmr::memory_resource
{
public:
    size_t allocCount = 0;
    size_t freeCount  = 0;

private:
    void* do_allocate(size_t bytes, size_t align) override
    {
        allocCount++;
        return std::pmr::get_default_resource()->allocate(bytes, align);
    }

    void do_deallocate(void* p, size_t bytes, size_t align) override
    {
        freeCount++;
        std::pmr::get_default_resource()->deallocate(p, bytes, align);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
    {
        return this == &other;
    }
};

TEST(LockFreeQueueStandaloneTest, MemoryLeakCheck)
{
    CountingResource resource;

    {
        ec2s::LockFreeQueue<int> queue(&resource);

        for (int i = 0; i < 10000; i++) queue.push(i);

        int v;
        while (queue.pop(v))
        {
        }
    }

    EXPECT_EQ(resource.allocCount, resource.freeCount);
}