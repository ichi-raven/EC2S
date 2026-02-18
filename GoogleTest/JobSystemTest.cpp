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
