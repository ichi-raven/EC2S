#include "include/EC2S.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>

void test();

using namespace ec2s;

int main()
{
    test();

    Entity sparseEntity = 0;

    //std::cerr << std::hex << (((((sparseEntity & kEntitySlotMask) >> kEntitySlotShiftWidth) + 1) << kEntitySlotShiftWidth) | 1) << "\n";

    ec2s::Registry registry;

    return 0;
}