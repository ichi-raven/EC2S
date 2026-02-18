#include <gtest/gtest.h>
#include <vector>
#include <memory>
#include <cstring>
#include <numeric>

#include <TLSFAllocator.hpp>

using namespace ec2s;

// ============================================================
// TLSFAllocator Tests
// ============================================================

class TLSFAllocatorTest : public ::testing::Test
{
protected:
    static constexpr uint32_t kMemorySize = 1024 * 1024;  // 1 MB
    std::vector<std::byte> memory;
    std::unique_ptr<TLSFAllocator<4>> allocator;

    void SetUp() override
    {
        memory.resize(kMemorySize);
        allocator = std::make_unique<TLSFAllocator<>>(memory.data(), kMemorySize);
    }
};

// ------------------------------------------------------------
// Basic allocation
// ------------------------------------------------------------

TEST_F(TLSFAllocatorTest, AllocateAndDeallocate)
{
    auto* ptr = allocator->allocate(128);
    ASSERT_NE(ptr, nullptr);

    bool result = allocator->deallocate(ptr);
    EXPECT_TRUE(result);
}

// ------------------------------------------------------------
// Multiple allocations
// ------------------------------------------------------------

TEST_F(TLSFAllocatorTest, MultipleAllocations)
{
    std::vector<void*> ptrs;

    for (int i = 0; i < 100; ++i)
    {
        void* p = allocator->allocate(64);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }

    for (auto* p : ptrs)
    {
        EXPECT_TRUE(allocator->deallocate(p));
    }
}

// ------------------------------------------------------------
// Large allocation near capacity
// ------------------------------------------------------------

TEST_F(TLSFAllocatorTest, LargeAllocation)
{
    auto* ptr = allocator->allocate(512 * 1024);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(allocator->deallocate(ptr));
}

// ------------------------------------------------------------
// Split behavior
// ------------------------------------------------------------

TEST_F(TLSFAllocatorTest, SplitBlock)
{
    void* p1 = allocator->allocate(256);
    ASSERT_NE(p1, nullptr);

    void* p2 = allocator->allocate(256);
    ASSERT_NE(p2, nullptr);

    EXPECT_TRUE(allocator->deallocate(p1));
    EXPECT_TRUE(allocator->deallocate(p2));
}

// ------------------------------------------------------------
// Merge behavior
// ------------------------------------------------------------

TEST_F(TLSFAllocatorTest, MergeBlocks)
{
    void* p1 = allocator->allocate(256);
    void* p2 = allocator->allocate(256);

    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);

    allocator->deallocate(p1);
    allocator->deallocate(p2);

    // After merge, a large allocation should succeed
    void* large = allocator->allocate(512);
    ASSERT_NE(large, nullptr);

    EXPECT_TRUE(allocator->deallocate(large));
}

// ------------------------------------------------------------
// Exhaustion test
// ------------------------------------------------------------

TEST_F(TLSFAllocatorTest, ExhaustMemory)
{
    std::vector<void*> ptrs;

    while (true)
    {
        void* p = allocator->allocate(4096);
        if (p == nullptr)
        {
            break;
        }
        ptrs.push_back(p);
    }

    EXPECT_FALSE(ptrs.empty());

    for (auto* p : ptrs)
    {
        allocator->deallocate(p);
    }
}

// ------------------------------------------------------------
// Typed allocation
// ------------------------------------------------------------

TEST_F(TLSFAllocatorTest, TypedAllocation)
{
    int* arr = allocator->allocate<int>(100);
    ASSERT_NE(arr, nullptr);

    for (int i = 0; i < 100; ++i)
    {
        arr[i] = i;
    }

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(arr[i], i);
    }

    EXPECT_TRUE(allocator->deallocate(arr));
}

// ============================================================
// TLSFStdAllocator Tests
// ============================================================

TEST(TLSFStdAllocatorTest, UseWithVector)
{
    constexpr uint32_t kMemorySize = 1024 * 1024;
    std::vector<std::byte> memory(kMemorySize);
    TLSFAllocator<> backend(memory.data(), kMemorySize);

    TLSFStdAllocator<int> alloc(backend);

    std::vector<int, TLSFStdAllocator<int>> vec(alloc);

    for (int i = 0; i < 1000; ++i)
    {
        vec.push_back(i);
    }

    EXPECT_EQ(vec.size(), 1000u);

    int sum = std::accumulate(vec.begin(), vec.end(), 0);
    EXPECT_EQ(sum, 999 * 1000 / 2);
}
