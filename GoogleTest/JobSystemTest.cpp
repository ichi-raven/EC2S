/*****************************************************************//**
 * @file   JobSystemTest.cpp
 * @brief  Test codes for JobSystem
 * 
 * @author ichi-raven
 * @date   November 2024
 *********************************************************************/

#include <gtest/gtest.h>
#include "../include/EC2S.hpp"
#include <chrono>
#include <atomic>
#include <random>

class JobSystemTest : public ::testing::Test
{
protected:

protected:
    void SetUp() override
    {
        // デフォルトのスレッド数で初期化
        jobSystem = std::make_unique<ec2s::JobSystem>(std::thread::hardware_concurrency());
    }

    void TearDown() override
    {
        if (jobSystem)
        {
            jobSystem->stop();
        }
    }

    std::unique_ptr<ec2s::JobSystem> jobSystem;

    // テスト用のヘルパー関数
    static void sleepRandomDuration()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(1, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
    }
};

// 基本的な機能テスト
TEST_F(JobSystemTest, BasicFunctionality)
{
    std::atomic<int> counter{ 0 };
    const int taskCount = 100;

    for (int i = 0; i < taskCount; ++i)
    {
        jobSystem->exec([&counter]() { counter++; });
    }

    jobSystem->stop();
    EXPECT_EQ(counter, taskCount);
}

// 並列処理の正確性テスト
TEST_F(JobSystemTest, ParallelExecutionCorrectness)
{
    std::vector<int> results(1000, 0);
    std::mutex resultsMutex;

    // 複数のタスクを同時に実行
    for (int i = 0; i < 1000; ++i)
    {
        jobSystem->exec(
            [i, &results, &resultsMutex]()
            {
                int result = i * 2;  // 何らかの計算
                std::lock_guard<std::mutex> lock(resultsMutex);
                results[i] = result;
            });
    }

    jobSystem->stop();

    // 結果の検証
    for (int i = 0; i < 1000; ++i)
    {
        EXPECT_EQ(results[i], i * 2);
    }
}

// ECSとの統合テスト
TEST_F(JobSystemTest, ECSIntegration)
{
    const size_t entityCount = 10000;
    ec2s::Registry registry;
    std::vector<ec2s::Entity> entities(entityCount);

    // エンティティとコンポーネントの初期化
    for (size_t i = 0; i < entityCount; ++i)
    {
        entities[i] = registry.create();
        registry.add<int>(entities[i], 1);
        if (i % 2)
        {
            registry.add<double>(entities[i], 0.3);
        }
    }

    std::atomic<int> processedEntities{ 0 };
    const int batchSize = 1000;

    // 並列処理でコンポーネントを更新
    for (size_t i = 0; i < entityCount; i += batchSize)
    {
        jobSystem->exec(
            [&registry, &processedEntities, i, batchSize, entityCount]()
            {
                const size_t end = std::min(i + batchSize, entityCount);
                for (size_t j = i; j < end; ++j)
                {
                    auto& intComp = registry.get<int>(static_cast<ec2s::Entity>(j));
                    intComp += 1;
                    processedEntities++;
                }
            });
    }

    jobSystem->stop();
    EXPECT_EQ(processedEntities, entityCount);

    // 結果の検証
    bool allCorrect = true;
    registry.each<int>(
        [&allCorrect](const int& value)
        {
            if (value != 2) allCorrect = false;
        });
    EXPECT_TRUE(allCorrect);
}

// パフォーマンステスト
TEST_F(JobSystemTest, PerformanceBenchmark)
{
    const int iterations   = 10;
    const size_t taskCount = 1000;

    // 並列実行の時間計測
    auto startParallel = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < taskCount; ++i)
    {
        jobSystem->exec(
            []()
            {
                // シミュレートされた作業負荷
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            });
    }

    jobSystem->stop();
    auto endParallel      = std::chrono::high_resolution_clock::now();
    auto parallelDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endParallel - startParallel).count();

    // 直列実行の時間計測
    auto startSerial = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < taskCount; ++i)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    auto endSerial      = std::chrono::high_resolution_clock::now();
    auto serialDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endSerial - startSerial).count();

    // 並列実行が直列実行より高速であることを確認
    EXPECT_LT(parallelDuration, serialDuration);
}

// スレッド数の設定テスト
TEST_F(JobSystemTest, ThreadCountConfiguration)
{
    // 様々なスレッド数でJobSystemを初期化
    std::vector<int> threadCounts = { 1, 2, 4, 8, 16 };

    for (int count : threadCounts)
    {
        auto js = std::make_unique<ec2s::JobSystem>(count);
        std::atomic<int> counter{ 0 };
        const int taskCount = 100;

        for (int i = 0; i < taskCount; ++i)
        {
            js->exec(
                [&counter]()
                {
                    counter++;
                    sleepRandomDuration();
                });
        }

        js->stop();
        EXPECT_EQ(counter, taskCount);
    }
}

// エラー処理とエッジケーステスト
TEST_F(JobSystemTest, ErrorHandlingAndEdgeCases)
{
    // 空のタスク
    EXPECT_NO_THROW(jobSystem->exec([]() {}));

    // 例外を投げるタスク
    std::atomic<int> exceptionCount{ 0 };
    for (int i = 0; i < 100; ++i)
    {
        jobSystem->exec(
            [&exceptionCount, &i]()
            {
                try
                {
                    if (i % 2 == 0)
                    {
                        throw std::runtime_error("Test exception");
                    }
                }
                catch (...)
                {
                    exceptionCount++;
                }
            });
    }

    jobSystem->stop();
    EXPECT_GT(exceptionCount, 0);

    // stopを複数回呼び出し
    EXPECT_NO_THROW(jobSystem->stop());
    EXPECT_NO_THROW(jobSystem->stop());
}

// 長時間実行タスクのテスト
TEST_F(JobSystemTest, LongRunningTasks)
{
    std::atomic<int> completedTasks{ 0 };
    const int taskCount = 10;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < taskCount; ++i)
    {
        jobSystem->exec(
            [&completedTasks]()
            {
                // 長時間実行タスクをシミュレート
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                completedTasks++;
            });
    }

    jobSystem->stop();

    auto endTime  = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    EXPECT_EQ(completedTasks, taskCount);
    EXPECT_LT(duration, taskCount / jobSystem->getWorkerThreadNum() + 100);  // マージンを持たせて300ms
}

// キャンセル処理のテスト（もし実装されている場合）
TEST_F(JobSystemTest, TaskCancellation)
{
    std::atomic<bool> taskStarted{ false };
    std::atomic<bool> taskCompleted{ false };

    jobSystem->exec(
        [&]()
        {
            taskStarted = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            taskCompleted = true;
        });

    // タスクが開始されるのを待つ
    while (!taskStarted)
    {
        std::this_thread::yield();
    }

    // 途中でJobSystemを停止
    jobSystem->stop();

    // タスクが完了していることを確認
    EXPECT_TRUE(taskCompleted);
}