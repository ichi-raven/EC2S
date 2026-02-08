#include "include/EC2S.hpp"

#include <iostream>
#include <chrono>
#include <random>

using namespace ec2s;

void heavyTask()
{
    constexpr std::size_t kTestEntityNum = static_cast<std::size_t>(1e5);
    ec2s::Registry registry;
    std::vector<ec2s::Entity> entities(kTestEntityNum);

    for (std::size_t i = 0; i < kTestEntityNum; ++i)
    {
        entities[i] = registry.create();
        registry.add<int>(entities[i], 1);
        if (i % 2)
        {
            registry.add<double>(entities[i], 0.3);
        }
        else
        {
            registry.add<char>(entities[i], 'a');
        }
    }

    registry.each<int>([](int& e) { e += 1; });
    registry.each<double>([](double& e) { e += 2.; });
    registry.each<char>([](char& e) { e += 1; });

    registry.each<int, Exclude<double>>([]([[maybe_unused]] Entity entity, int& e) { e += 1; });

    // validation
    bool succeeded = true;

    registry.each<int, double>([&](int& e, double& e2) { succeeded &= (e == 2); });
    registry.each<int, char>([&](int& e, char& e2) { succeeded &= (e == 3); });

    registry.each<double>([&](double& e) { succeeded &= (e == 2.3); });
    registry.each<char>([&](char& e) { succeeded &= (e == 'b'); });

    if (!succeeded)
    {
        std::cout << "failed at thread " << std::this_thread::get_id() << "\n";
    }
}

void loadTest()
{
    constexpr int kTestTime = static_cast<std::size_t>(1e3);
    ThreadPool threadPool;

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kTestTime; ++i)
    {
        threadPool.submit(heavyTask);
    }
    threadPool.wait();
    auto end = std::chrono::steady_clock::now();
    std::cout << "parallel time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms\n";

    start = std::chrono::steady_clock::now();
    for (int i = 0; i < kTestTime; ++i)
    {
        heavyTask();
    }
    end = std::chrono::steady_clock::now();
    std::cout << "serial time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms\n";
}

void parallelTest()
{
    ThreadPool threadPool;

    auto sleepRandomMs = []
    {
        using namespace std::chrono_literals;

        static thread_local std::mt19937_64 rng{ std::random_device{}() };
        static thread_local std::uniform_int_distribution<int> dist(1, 30);

        std::this_thread::sleep_for(dist(rng) * 1ms);
    };

    // ordered parallel tasks with dependencies

    const auto checkTask1      = []() { std::cout << "called 1\n"; };
    const auto checkTask1a     = []() { std::cout << "called 1a\n"; };
    const auto checkTask2      = []() { std::cout << "called 2\n"; };
    const auto checkTask2a     = []() { std::cout << "called 2a\n"; };
    const auto checkTask2b     = []() { std::cout << "called 2b\n"; };
    const auto checkTask3      = []() { std::cout << "called 3\n"; };
    const auto checkTask4      = []() { std::cout << "called 4\n"; };
    const auto checkTask5      = []() { std::cout << "called 5\n"; };
    const auto independentTask = []() { std::cout << "independent\n"; };

    auto& job1  = threadPool.createJob(checkTask1);
    auto& job1a = threadPool.createJob(checkTask1a);
    auto& job2  = threadPool.createJob(checkTask2);
    auto& job2a = threadPool.createJob(checkTask2a);
    auto& job2b = threadPool.createJob(checkTask2b);
    auto& job3  = threadPool.createJob(checkTask3);
    auto& job4  = threadPool.createJob(checkTask4);
    auto& job5  = threadPool.createJob(checkTask5);

    job1.addChild(job2);
    job1a.addChild(job2);
    job1.addChild(job2a);
    job1.addChild(job2b);
    job2.addChild(job3);
    job2a.addChild(job3);
    job3.addChild(job4);
    job4.addChild(job5);

    std::cout << "start submitting all jobs...\n";

    // submit in random order with independent tasks
    threadPool.submit(independentTask);
    sleepRandomMs();
    threadPool.submit(job1);
    threadPool.submit(independentTask);
    sleepRandomMs();
    threadPool.submit(job1a);
    threadPool.submit(independentTask);
    threadPool.submit(independentTask);

    threadPool.wait();
    std::cout << "all jobs done\n";
}

void sortTest()
{
    constexpr std::size_t kTestEntityNum = static_cast<std::size_t>(1e2);
    std::vector<Entity> entities(kTestEntityNum);

    std::vector<int> groundTruth(kTestEntityNum);
    std::iota(groundTruth.begin(), groundTruth.end(), 0);
    std::vector<int> shuffled = groundTruth;
    std::shuffle(shuffled.begin(), shuffled.end(), std::mt19937_64{ std::random_device{}() });

    Registry registry;
    for (std::size_t i = 0; i < kTestEntityNum; ++i)
    {
        entities[i] = registry.create();
        registry.add<int>(entities[i], shuffled[i]);
    }

    registry.sort<int>([](const int& a, const int& b) { return a < b; });

    bool succeeded = true;

    // dense access check
    registry.each<int>(
        [&](const int& e)
        {
            static std::size_t index = 0;
            succeeded &= (e == groundTruth[index]);
            ++index;
        });

    // sparse access check
    for (std::size_t i = 0; i < kTestEntityNum; ++i)
    {
        const int& e = registry.get<int>(entities[i]);
        succeeded &= (e == static_cast<int>(i));

        if (!succeeded)
        {
            std::cout << "failed at entity " << entities[i] << "\n";
            std::cout << "expected " << static_cast<int>(i) << ", got " << e << "\n";
            break;
        }
    }

    if (succeeded)
    {
        std::cout << "sort test succeeded\n";
    }
    else
    {
        std::cout << "sort test failed\n";
    }
}

int main()
{
    loadTest();
    parallelTest();
    sortTest();

    return 0;
}
