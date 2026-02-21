
// ============================================================
// ArenaAllocator Tests (generic tests)
// ============================================================

#include <ArenaAllocator.hpp>

#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <cstring>
#include <numeric>

using namespace ec2s;

class ArenaAllocatorTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kMemorySize = 1024 * 1024;
    std::vector<std::byte> memory;
    std::unique_ptr<ArenaAllocator<>> allocator;

    void SetUp() override
    {
        memory.resize(kMemorySize);
        allocator = std::make_unique<ArenaAllocator<>>(memory.data(), kMemorySize);
    }
};

// ------------------------------------------------------------
// Basic allocation
// ------------------------------------------------------------

TEST_F(ArenaAllocatorTest, AllocateSequentially)
{
    void* p1 = allocator->allocate(128);
    void* p2 = allocator->allocate(256);

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);
}

// ------------------------------------------------------------
// Linear growth test
// ------------------------------------------------------------

TEST_F(ArenaAllocatorTest, SequentialMemoryGrowth)
{
    void* p1 = allocator->allocate(128);
    void* p2 = allocator->allocate(128);

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);

    // Arena allocator should grow forward in memory
    EXPECT_LT(p1, p2);
}

// ------------------------------------------------------------
// Reset behavior
// ------------------------------------------------------------

TEST_F(ArenaAllocatorTest, ResetAllocator)
{
    void* p1 = allocator->allocate(256);
    ASSERT_NE(p1, nullptr);

    allocator->reset();

    void* p2 = allocator->allocate(256);
    ASSERT_NE(p2, nullptr);

    // After reset, address should be reused
    EXPECT_EQ(p1, p2);
}

// ------------------------------------------------------------
// Exhaustion test
// ------------------------------------------------------------

TEST_F(ArenaAllocatorTest, ExhaustMemory)
{
    std::vector<void*> ptrs;

    while (true)
    {
        void* p = allocator->allocate(4096);
        if (!p) break;
        ptrs.push_back(p);
    }

    EXPECT_FALSE(ptrs.empty());
}

// ------------------------------------------------------------
// ArenaMemoryResource test
// ------------------------------------------------------------

TEST_F(ArenaAllocatorTest, UseWithPMRVector)
{
    ArenaMemoryResource<> memResource(*allocator);
    std::pmr::vector<int> vec(&memResource);
    for (int i = 0; i < 1000; ++i)
    {
        vec.push_back(i);
    }
    EXPECT_EQ(vec.size(), 1000u);
    int sum = std::accumulate(vec.begin(), vec.end(), 0);
    EXPECT_EQ(sum, 999 * 1000 / 2);
}
