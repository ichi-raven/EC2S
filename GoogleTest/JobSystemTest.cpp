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

//class JobSystemTest : public ::testing::Test
//{
//protected:
//
//protected:
//    void SetUp() override
//    {
//        // initialize by environments thread num
//        jobSystem = std::make_unique<ec2s::JobSystem>(std::thread::hardware_concurrency());
//    }
//
//    void TearDown() override
//    {
//        if (jobSystem)
//        {
//            jobSystem->stop();
//        }
//    }
//
//    std::unique_ptr<ec2s::JobSystem> jobSystem;
//
//    // helper function for testing
//    static void sleepRandomDuration()
//    {
//        static std::random_device rd;
//        static std::mt19937 gen(rd());
//        static std::uniform_int_distribution<> dis(1, 10);
//        std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
//    }
//};
//
//TEST_F(JobSystemTest, BasicFunctionality)
//{
//    std::atomic<int> counter{ 0 };
//    const int taskCount = 100;
//
//    for (int i = 0; i < taskCount; ++i)
//    {
//        jobSystem->exec([&counter]() { counter++; });
//    }
//
//    jobSystem->join();
//
//    EXPECT_EQ(counter, taskCount);
//}
//
//TEST_F(JobSystemTest, ParallelExecutionCorrectness)
//{
//    std::vector<int> results(1000, 0);
//    std::mutex resultsMutex;
//
//    for (int i = 0; i < 1000; ++i)
//    {
//        jobSystem->exec(
//            [i, &results, &resultsMutex]()
//            {
//                int result = i * 2;
//                std::lock_guard<std::mutex> lock(resultsMutex);
//                results[i] = result;
//            });
//    }
//
//    jobSystem->join();
//    
//    for (int i = 0; i < 1000; ++i)
//    {
//        EXPECT_EQ(results[i], i * 2);
//    }
//}
//
//// integration test with ECS registry
//TEST_F(JobSystemTest, ECSIntegration)
//{
//    const size_t entityCount = 10000;
//    ec2s::Registry registry;
//    std::vector<ec2s::Entity> entities(entityCount);
//
//    // initialize
//    for (size_t i = 0; i < entityCount; ++i)
//    {
//        entities[i] = registry.create();
//        registry.add<int>(entities[i], 1);
//        if (i % 2)
//        {
//            registry.add<double>(entities[i], 0.3);
//        }
//    }
//
//    std::atomic<int> processedEntities{ 0 };
//    const int batchSize = 1000;
//
//    // update component in parallel
//    for (size_t i = 0; i < entityCount; i += batchSize)
//    {
//        jobSystem->exec(
//            [&registry, &processedEntities, i, batchSize, entityCount]()
//            {
//                const size_t end = std::min(i + batchSize, entityCount);
//                for (size_t j = i; j < end; ++j)
//                {
//                    auto& intComp = registry.get<int>(static_cast<ec2s::Entity>(j));
//                    intComp += 1;
//                    processedEntities++;
//                }
//            });
//    }
//
//    jobSystem->join();
//    EXPECT_EQ(processedEntities, entityCount);
//
//    bool allCorrect = true;
//    registry.each<int>(
//        [&allCorrect](const int& value)
//        {
//            if (value != 2) allCorrect = false;
//        });
//    EXPECT_TRUE(allCorrect);
//}
//
//// check performance
//TEST_F(JobSystemTest, PerformanceBenchmark)
//{
//    const int iterations   = 10;
//    const size_t taskCount = 1000;
//
//    auto startParallel = std::chrono::high_resolution_clock::now();
//
//    for (size_t i = 0; i < taskCount; ++i)
//    {
//        jobSystem->exec(
//            []()
//            {
//                std::this_thread::sleep_for(std::chrono::microseconds(100));
//            });
//    }
//
//    auto endParallel      = std::chrono::high_resolution_clock::now();
//    auto parallelDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endParallel - startParallel).count();
//
//    auto startSerial = std::chrono::high_resolution_clock::now();
//
//    for (size_t i = 0; i < taskCount; ++i)
//    {
//        std::this_thread::sleep_for(std::chrono::microseconds(100));
//    }
//
//    auto endSerial      = std::chrono::high_resolution_clock::now();
//    auto serialDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endSerial - startSerial).count();
//
//    EXPECT_LT(parallelDuration, serialDuration);
//}
//
//TEST_F(JobSystemTest, ThreadCountConfiguration)
//{
//    // initialize JobSystem with various number of threads
//    std::vector<int> threadCounts = { 1, 2, 4, 8, 16 };
//
//    for (int count : threadCounts)
//    {
//        auto js = std::make_unique<ec2s::JobSystem>(count);
//        std::atomic<int> counter{ 0 };
//        const int taskCount = 100;
//
//        for (int i = 0; i < taskCount; ++i)
//        {
//            js->exec(
//                [&counter]()
//                {
//                    counter++;
//                    sleepRandomDuration();
//                });
//        }
//
//        js->stop();
//        EXPECT_EQ(counter, taskCount);
//    }
//}
//
//TEST_F(JobSystemTest, ErrorHandlingAndEdgeCases)
//{
//    EXPECT_NO_THROW(jobSystem->exec([]() {}));
//
//    std::atomic<int> exceptionCount{ 0 };
//    for (int i = 0; i < 100; ++i)
//    {
//        jobSystem->exec(
//            [&exceptionCount, &i]()
//            {
//                try
//                {
//                    if (i % 2 == 0)
//                    {
//                        throw std::runtime_error("Test exception");
//                    }
//                }
//                catch (...)
//                {
//                    exceptionCount++;
//                }
//            });
//    }
//
//    jobSystem->join();
//
//    EXPECT_GT(exceptionCount, 0);
//}
//
//TEST_F(JobSystemTest, LongRunningTasks)
//{
//    std::atomic<int> completedTasks{ 0 };
//    const int taskCount = 10;
//
//    auto startTime = std::chrono::high_resolution_clock::now();
//
//    for (int i = 0; i < taskCount; ++i)
//    {
//        jobSystem->exec(
//            [&completedTasks]()
//            {
//                // simulate long-running tasks
//                std::this_thread::sleep_for(std::chrono::milliseconds(100));
//                completedTasks++;
//            });
//    }
//
//    jobSystem->join();
//    auto endTime  = std::chrono::high_resolution_clock::now();
//    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
//
//    EXPECT_EQ(completedTasks, taskCount);
//}