#include "include/EC2S.hpp"

void test();

using namespace ec2s;

void heavyTask()
{
    constexpr std::size_t kTestEntityNum = static_cast<std::size_t>(1e2);
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
    //test();

    ec2s::JobSystem jobSystem(4);
    ec2s::Registry registry;

    jobSystem.schedule(
        [&]()
        {
            std::cout << "thread ID : " << std::this_thread::get_id() << "\n";
            //std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            heavyTask();
            std::cout << std::this_thread::get_id() << "end\n";
        });
    jobSystem.schedule(
        [&]()
        {
            std::cout << "thread ID : " << std::this_thread::get_id() << "\n";
            //std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            heavyTask();
            std::cout << std::this_thread::get_id() << "end\n";
        });
    jobSystem.schedule(
        [&]()
        {
            std::cout << "thread ID : " << std::this_thread::get_id() << "\n";
            //std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            heavyTask();
            std::cout << std::this_thread::get_id() << "end\n";
        });
    jobSystem.schedule(
        [&]()
        {
            std::cout << "thread ID : " << std::this_thread::get_id() << "\n";
            //std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            heavyTask();
            std::cout << std::this_thread::get_id() << "end\n";
        });

    // additional
    jobSystem.schedule(
        [&]()
        {
            std::cout << "thread ID : " << std::this_thread::get_id() << "\n";
            //std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            heavyTask();
            std::cout << std::this_thread::get_id() << "end\n";
        });


    return 0;
}