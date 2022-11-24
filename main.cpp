#include "include/EC2S.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>

void test();

using namespace ec2s;

struct Transform
{
    float pos[3];
    float vel[3];
    float acc[3];
};

struct Renderable
{
    uint32_t meshID;
};

struct Alive
{};

int main()
{
    test();

    return 0;
}