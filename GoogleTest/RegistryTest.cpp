/*****************************************************************//**
 * @file   test.cpp
 * @brief  Test codes for Registry
 * 
 * @author ichi-raven
 * @date   November 2024
 *********************************************************************/

#include <gtest/gtest.h>
#include "../include/EC2S.hpp"

// Component structures for testing
struct TestCompA
{
    TestCompA()
        : value(0)
    {
    }
    TestCompA(int v)
        : value(v)
    {
    }
    int value;
};

struct TestCompB
{
    TestCompB()
        : value(0.0)
    {
    }
    TestCompB(double v)
        : value(v)
    {
    }
    double value;
};

struct TestCompC
{
    TestCompC()
        : value('x')
    {
    }
    TestCompC(char v)
        : value(v)
    {
    }
    char value;
};

class EC2STest : public ::testing::Test
{
protected:
    ec2s::Registry registry;
};

// Entity Creation and Destruction Tests
TEST_F(EC2STest, EntityCreationAndDestruction)
{
    // Basic creation
    auto entity = registry.create();
    EXPECT_GT(registry.activeEntityNum(), 0);

    // Destruction
    registry.destroy(entity);
    EXPECT_EQ(registry.activeEntityNum(), 0);

    // Multiple entities
    std::vector<ec2s::Entity> entities;
    const size_t entityCount = 1000;

    for (size_t i = 0; i < entityCount; ++i)
    {
        entities.push_back(registry.create());
    }
    EXPECT_EQ(registry.activeEntityNum(), entityCount);

    // Destroy all
    for (auto e : entities)
    {
        registry.destroy(e);
    }
    EXPECT_EQ(registry.activeEntityNum(), 0);
}

// Component Management Tests
TEST_F(EC2STest, ComponentManagement)
{
    auto entity = registry.create();

    // Add components
    registry.add<TestCompA>(entity, 42);
    registry.add<TestCompB>(entity, 3.14);

    // Check component values
    EXPECT_EQ(registry.get<TestCompA>(entity).value, 42);
    EXPECT_EQ(registry.get<TestCompB>(entity).value, 3.14);

    // Check component counts
    EXPECT_EQ(registry.size<TestCompA>(), 1);
    EXPECT_EQ(registry.size<TestCompB>(), 1);

    // Modify component
    registry.get<TestCompA>(entity).value = 100;
    EXPECT_EQ(registry.get<TestCompA>(entity).value, 100);
}

// View Tests
TEST_F(EC2STest, ViewOperations)
{
    const size_t entityCount = 100;
    std::vector<ec2s::Entity> entities;

    // Create entities with different component combinations
    for (size_t i = 0; i < entityCount; ++i)
    {
        auto entity = registry.create();
        entities.push_back(entity);

        registry.add<TestCompA>(entity, static_cast<int>(i));

        if (i % 2 == 0)
        {
            registry.add<TestCompB>(entity, static_cast<double>(i));
        }
        if (i % 3 == 0)
        {
            registry.add<TestCompC>(entity, static_cast<char>('a' + (i % 26)));
        }
    }

    // Test view of single component
    {
        size_t count = 0;
        registry.each<TestCompA>([&count](const TestCompA& a) { count++; });
        EXPECT_EQ(count, entityCount);
    }

    // Test view of two components
    {
        size_t count = 0;
        auto view    = registry.view<TestCompA, TestCompB>();
        view.each([&count](const TestCompA& a, const TestCompB& b) { count++; });
        EXPECT_EQ(count, entityCount / 2);  // Only entities with both A and B
    }
}

// Edge Cases Tests
TEST_F(EC2STest, EdgeCases)
{
    auto entity = registry.create();

    // Double component addition
    registry.add<TestCompA>(entity, 1);
    EXPECT_NO_THROW(registry.add<TestCompA>(entity, 2));

    // Get non-existent component
    EXPECT_ANY_THROW(registry.get<TestCompB>(entity));

    // Destroy non-existent entity
    ec2s::Entity invalidEntity = static_cast<ec2s::Entity>(-1);
    EXPECT_NO_THROW(registry.destroy(invalidEntity));
    
    // Empty view operations (untestable due to C++ macro specifications)
    //EXPECT_ANY_THROW(registry.view<TestCompB, TestCompC>());
}

// Performance Tests
TEST_F(EC2STest, Performance)
{
    constexpr size_t entityCount = 100000;
    std::vector<ec2s::Entity> entities;

    // Measure creation time
    auto startCreate = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < entityCount; ++i)
    {
        auto entity = registry.create();
        entities.push_back(entity);
        registry.add<TestCompA>(entity, 1);
        if (i % 2 == 0)
        {
            registry.add<TestCompB>(entity, 0.5);
        }
    }
    auto endCreate  = std::chrono::high_resolution_clock::now();
    auto createTime = std::chrono::duration_cast<std::chrono::milliseconds>(endCreate - startCreate).count();
    EXPECT_LT(createTime, 1000);  // Should complete within 1 second

    // Measure iteration time
    auto startIter = std::chrono::high_resolution_clock::now();
    registry.each<TestCompA>([](TestCompA& a) { a.value += 1; });
    auto endIter  = std::chrono::high_resolution_clock::now();
    auto iterTime = std::chrono::duration_cast<std::chrono::milliseconds>(endIter - startIter).count();
    EXPECT_LT(iterTime, 100);  // Should complete within 100ms
}

// Component Deletion Tests
TEST_F(EC2STest, ComponentDeletion)
{
    auto entity = registry.create();

    registry.add<TestCompA>(entity, 1);
    registry.add<TestCompB>(entity, 2.0);
    registry.add<TestCompC>(entity, 'a');

    EXPECT_EQ(registry.size<TestCompA>(), 1);
    EXPECT_EQ(registry.size<TestCompB>(), 1);
    EXPECT_EQ(registry.size<TestCompC>(), 1);

    // Delete individual components
    registry.remove<TestCompB>(entity);
    EXPECT_EQ(registry.size<TestCompB>(), 0);
}