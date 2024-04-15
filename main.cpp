#include "include/EC2S.hpp"

void test();

using namespace ec2s;

int main()
{
    //test();

    ec2s::JobSystem jobSystem(8);
    ec2s::Registry registry;
    constexpr int kTestEntityNum = static_cast<int>(1e5);

    jobSystem.schedule(
        [&]()
        {
            std::cout << "thread ID : " << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        });
    jobSystem.schedule(
        [&]()
        {
            std::cout << "thread ID : " << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        });
    jobSystem.schedule(
        [&]()
        {
            std::cout << "thread ID : " << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        });
    jobSystem.schedule(
        [&]()
        {
            std::cout << "thread ID : " << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(7000));

    return 0;
}