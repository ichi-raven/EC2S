#include "include/EC2S.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>

void test();

using namespace ec2s;

int main()
{
    test();

    /*ec2s::Registry registry;

    for (int i = 0; i < 10; ++i)
    {
        registry.get<int>(registry.create<int>()) = i;
    }

    registry.each<int>([&](Entity entity, int a) 
        { 
            std::cout << a << "\n";
            registry.destroy(entity);
        });

    std::cout << "second\n";

    registry.each<int>([&](Entity entity, int a)
        {
            std::cout << a << "\n";
        });*/

    return 0;
}