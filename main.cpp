#include "include/EC2S.hpp"

#include <iostream>
#include <chrono>

void test();

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

    bool succeeded = true;

    registry.each<int>([&](int& e) { succeeded = (e == 2); });
    registry.each<double>([&](double& e) { succeeded = (e == 2.3); });
    registry.each<char>([&](char& e) { succeeded = (e == 'b'); });

    if (!succeeded)
    {
        std::cout << "failed at thread " << std::this_thread::get_id() << "\n";
    }
}

int main()
{
    constexpr int kTestNum = 1000;

    ec2s::JobSystem jobSystem;
    ec2s::Registry registry;

    std::mutex mut;

    const auto beforeTask = [&](std::vector<int>& test, int val) -> ec2s::Job
    {
#ifndef NDEBUG
        {
            std::unique_lock lock(mut);
            std::cout << "[before task] thread ID : " << std::this_thread::get_id() << " start\n";
        }
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        test.emplace_back(val);

#ifndef NDEBUG
        {
            std::unique_lock lock(mut);
            std::cout << "[before task] thread ID : " << std::this_thread::get_id() << " end\n";
        }
#endif

        co_return;
    };

    const auto task = [&](ec2s::JobSystem& jobSystem, std::vector<int>& test) -> ec2s::Job
    {
#ifndef NDEBUG
        {
            std::unique_lock lock(mut);
            std::cout << "[main task] thread ID : " << std::this_thread::get_id() << " start\n";
        }
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        co_await ec2s::waitAll(jobSystem, beforeTask(test, 1), beforeTask(test, 2), beforeTask(test, 3), beforeTask(test, 4));

        test.emplace_back(42);
        //heavyTask();
#ifndef NDEBUG
        {
            std::unique_lock lock(mut);
            std::cout << "[main task] thread ID : " << std::this_thread::get_id() << " end\n";
        }
#endif

        co_return;
    };

    std::chrono::high_resolution_clock::time_point start, end;

    // parallel
    {
        start = std::chrono::high_resolution_clock::now();
        //for (int i = 0; i < kTestNum; ++i)
        //{
        //    jobSystem.schedule(task(i));
        //}

        std::vector<int> test;
        jobSystem.submit(task(jobSystem, test));

        std::this_thread::sleep_for(std::chrono::milliseconds(3000));  // wait for all tasks to finish
        jobSystem.stop();

        for (auto e : test)
        {
            std::cout << e << "\n";
        }

        end = std::chrono::high_resolution_clock::now();

        const auto elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        std::cout << "parallel : " << elapsed << "[ms]\n";
    }
    std::cout << "\n\n";

    // serial
    //{
    //    start = std::chrono::high_resolution_clock::now();
    //    for (int i = 0; i < kTestNum; ++i)
    //    {
    //        task(i).resume();
    //    }
    //    end                = std::chrono::high_resolution_clock::now();
    //    const auto elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    //    std::cout << "serial : " << elapsed << "[ms]\n";
    //}

    return 0;
}
