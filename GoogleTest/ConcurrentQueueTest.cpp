#include <ConcurrentQueue.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace
{
    class RecordingResource : public std::pmr::memory_resource
    {
    public:
        std::size_t allocations   = 0;
        std::size_t deallocations = 0;
        bool failAllocations      = false;

    private:
        struct Allocation
        {
            std::size_t bytes;
            std::size_t alignment;
        };

        void* do_allocate(std::size_t bytes, std::size_t alignment) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (failAllocations)
            {
                throw std::bad_alloc();
            }

            void* pointer = std::pmr::new_delete_resource()->allocate(bytes, alignment);
            mAllocations.emplace(pointer, Allocation{ bytes, alignment });
            ++allocations;
            return pointer;
        }

        void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override
        {
            std::lock_guard<std::mutex> lock(mMutex);
            const auto allocation = mAllocations.find(pointer);
            ASSERT_NE(allocation, mAllocations.end());
            EXPECT_EQ(bytes, allocation->second.bytes);
            EXPECT_EQ(alignment, allocation->second.alignment);
            std::pmr::new_delete_resource()->deallocate(pointer, allocation->second.bytes, allocation->second.alignment);
            mAllocations.erase(allocation);
            ++deallocations;
        }

        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
        {
            return this == &other;
        }

        std::mutex mMutex;
        std::unordered_map<void*, Allocation> mAllocations;
    };

    class ScopedQueueResource
    {
    public:
        explicit ScopedQueueResource(std::pmr::memory_resource& resource)
        {
            ec2s::setConcurrentQueueMemoryResource(resource);
        }

        ~ScopedQueueResource()
        {
            ec2s::setConcurrentQueueMemoryResource(*std::pmr::new_delete_resource());
        }
    };

    struct alignas(128) AlignedValue
    {
        explicit AlignedValue(int initialValue = 0)
            : value(initialValue)
        {
            EXPECT_EQ(reinterpret_cast<std::uintptr_t>(this) % alignof(AlignedValue), 0u);
        }

        AlignedValue(const AlignedValue& other)
            : AlignedValue(other.value)
        {
        }

        AlignedValue& operator=(const AlignedValue&) = default;

        int value;
    };
}  // namespace

TEST(ConcurrentQueueTest, EmptyAndSingleProducerFIFO)
{
    ec2s::ConcurrentQueue<int> queue;
    int result = 0;
    EXPECT_FALSE(queue.tryDequeue(result));

    for (int i = 0; i < 1000; ++i)
    {
        ASSERT_TRUE(queue.enqueue(i));
    }
    for (int i = 0; i < 1000; ++i)
    {
        ASSERT_TRUE(queue.tryDequeue(result));
        EXPECT_EQ(result, i);
    }
    EXPECT_FALSE(queue.tryDequeue(result));
}

TEST(ConcurrentQueueTest, MoveOnlyValues)
{
    ec2s::ConcurrentQueue<std::unique_ptr<int>> queue(1);
    auto value = std::make_unique<int>(42);
    ASSERT_TRUE(queue.enqueue(std::move(value)));
    EXPECT_EQ(value, nullptr);

    std::unique_ptr<int> result;
    ASSERT_TRUE(queue.tryDequeue(result));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, 42);
}

TEST(ConcurrentQueueTest, SharedResourceAndMatchingDeallocation)
{
    RecordingResource resource;
    ScopedQueueResource useResource(resource);
    {
        ec2s::ConcurrentQueue<int> integers(32);
        ASSERT_GT(resource.allocations, 0u);
        const auto initialAllocations = resource.allocations;
        ec2s::ConcurrentQueue<std::string> strings(32);
        EXPECT_GT(resource.allocations, initialAllocations);

        ASSERT_TRUE(integers.enqueue(42));
        ASSERT_TRUE(strings.enqueue(std::string(256, 'x')));
        // Leave both queues populated so their destruction releases live values.
    }
    EXPECT_EQ(resource.allocations, resource.deallocations);
}

TEST(ConcurrentQueueTest, OverAlignedValues)
{
    RecordingResource resource;
    ScopedQueueResource useResource(resource);
    {
        ec2s::ConcurrentQueue<AlignedValue> queue(32);
        const AlignedValue value(42);
        ASSERT_TRUE(queue.enqueue(value));
        AlignedValue result;
        ASSERT_TRUE(queue.tryDequeue(result));
        EXPECT_EQ(result.value, 42);
    }
    EXPECT_GT(resource.allocations, 0u);
    EXPECT_EQ(resource.allocations, resource.deallocations);
}

TEST(ConcurrentQueueTest, AllocationFailureReturnsFalse)
{
    ScopedQueueResource useResource(*std::pmr::null_memory_resource());
    ec2s::ConcurrentQueue<int> queue(32);
    EXPECT_FALSE(queue.enqueue(42));
    int result = 0;
    EXPECT_FALSE(queue.tryDequeue(result));
}

TEST(ConcurrentQueueTest, AllocationFailurePreservesQueuedValues)
{
    RecordingResource resource;
    ScopedQueueResource useResource(resource);
    {
        ec2s::ConcurrentQueue<int> queue(32);
        for (int i = 0; i < 32; ++i)
        {
            ASSERT_TRUE(queue.enqueue(i));
        }
        resource.failAllocations = true;
        EXPECT_FALSE(queue.enqueue(32));
        for (int i = 0; i < 32; ++i)
        {
            int result = -1;
            ASSERT_TRUE(queue.tryDequeue(result));
            EXPECT_EQ(result, i);
        }
        resource.failAllocations = false;
        ASSERT_TRUE(queue.enqueue(42));
        int result = 0;
        ASSERT_TRUE(queue.tryDequeue(result));
        EXPECT_EQ(result, 42);
    }
    EXPECT_EQ(resource.allocations, resource.deallocations);
}

TEST(ConcurrentQueueTest, MultipleProducersAndConsumers)
{
    constexpr int producerCount = 4;
    constexpr int consumerCount = 4;
    constexpr int perProducer   = 10000;
    constexpr int total         = producerCount * perProducer;
    RecordingResource resource;
    ScopedQueueResource useResource(resource);
    {
        ec2s::ConcurrentQueue<int> queue(1024);
        std::vector<std::atomic<int>> seen(total);
        std::atomic<int> consumed{ 0 };
        std::atomic<int> enqueueFailures{ 0 };
        std::atomic<int> invalidValues{ 0 };
        std::barrier start(producerCount + consumerCount);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        std::vector<std::thread> threads;

        for (int producer = 0; producer < producerCount; ++producer)
        {
            threads.emplace_back(
                [&, producer]
                {
                    start.arrive_and_wait();
                    for (int i = 0; i < perProducer; ++i)
                    {
                        if (!queue.enqueue(producer * perProducer + i))
                        {
                            ++enqueueFailures;
                        }
                    }
                });
        }
        for (int consumer = 0; consumer < consumerCount; ++consumer)
        {
            threads.emplace_back(
                [&]
                {
                    start.arrive_and_wait();
                    while (consumed.load() < total && std::chrono::steady_clock::now() < deadline)
                    {
                        int result;
                        if (queue.tryDequeue(result))
                        {
                            if (result >= 0 && result < total)
                            {
                                ++seen[result];
                            }
                            else
                            {
                                ++invalidValues;
                            }
                            ++consumed;
                        }
                        else
                        {
                            std::this_thread::yield();
                        }
                    }
                });
        }
        for (auto& thread : threads)
        {
            thread.join();
        }

        EXPECT_EQ(enqueueFailures.load(), 0);
        EXPECT_EQ(invalidValues.load(), 0);
        EXPECT_EQ(consumed.load(), total);
        for (const auto& count : seen)
        {
            EXPECT_EQ(count.load(), 1);
        }
        int result;
        EXPECT_FALSE(queue.tryDequeue(result));
    }
    EXPECT_EQ(resource.allocations, resource.deallocations);
}

TEST(ConcurrentQueueTest, TokenAndTokenFreeOperationsCanBeMixed)
{
    RecordingResource resource;
    ScopedQueueResource useResource(resource);
    {
        ec2s::ConcurrentQueue<int> queue;
        auto producer = queue.makeProducerToken();
        auto consumer = queue.makeConsumerToken();
        ASSERT_TRUE(producer.valid());

        constexpr int perProducer = 1000;
        for (int i = 0; i < perProducer; ++i)
        {
            ASSERT_TRUE(queue.enqueue(producer, i));
            ASSERT_TRUE(queue.enqueue(perProducer + i));
        }

        // Each producer preserves FIFO, but there is no order across producers.
        int nextExplicit = 0;
        int nextImplicit = perProducer;
        for (int i = 0; i < 2 * perProducer; ++i)
        {
            int result = -1;
            ASSERT_TRUE(i % 2 == 0 ? queue.tryDequeue(consumer, result) : queue.tryDequeue(result));
            ASSERT_GE(result, 0);
            ASSERT_LT(result, 2 * perProducer);
            if (result < perProducer)
            {
                EXPECT_EQ(result, nextExplicit++);
            }
            else
            {
                EXPECT_EQ(result, nextImplicit++);
            }
        }
        EXPECT_EQ(nextExplicit, perProducer);
        EXPECT_EQ(nextImplicit, 2 * perProducer);
        int result = -1;
        EXPECT_FALSE(queue.tryDequeue(consumer, result));
        EXPECT_FALSE(queue.tryDequeue(result));
        EXPECT_EQ(result, -1);
    }
    EXPECT_GT(resource.allocations, 0u);
    EXPECT_EQ(resource.allocations, resource.deallocations);
}

TEST(ConcurrentQueueTest, TokensSupportMoveOnlyValues)
{
    ec2s::ConcurrentQueue<std::unique_ptr<int>> queue;
    auto producer = queue.makeProducerToken();
    auto consumer = queue.makeConsumerToken();
    ASSERT_TRUE(producer.valid());

    auto value = std::make_unique<int>(42);
    ASSERT_TRUE(queue.enqueue(producer, std::move(value)));
    EXPECT_EQ(value, nullptr);

    std::unique_ptr<int> result;
    ASSERT_TRUE(queue.tryDequeue(consumer, result));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(*result, 42);
    EXPECT_FALSE(queue.tryDequeue(consumer, result));
}

TEST(ConcurrentQueueTest, ProducerTokenAllocationFailurePreservesValues)
{
    RecordingResource resource;
    ScopedQueueResource useResource(resource);
    {
        ec2s::ConcurrentQueue<std::string> queue;
        resource.failAllocations = true;
        auto producer            = queue.makeProducerToken();
        auto consumer            = queue.makeConsumerToken();
        EXPECT_FALSE(producer.valid());

        const std::string copied(256, 'c');
        std::string moved(256, 'm');
        EXPECT_FALSE(queue.enqueue(producer, copied));
        EXPECT_FALSE(queue.enqueue(producer, std::move(moved)));
        EXPECT_EQ(copied, std::string(256, 'c'));
        EXPECT_EQ(moved, std::string(256, 'm'));
        std::string result = "unchanged";
        EXPECT_FALSE(queue.tryDequeue(consumer, result));
        EXPECT_EQ(result, "unchanged");

        resource.failAllocations = false;
        auto recovered           = queue.makeProducerToken();
        ASSERT_TRUE(recovered.valid());
        ASSERT_TRUE(queue.enqueue(recovered, copied));
        ASSERT_TRUE(queue.enqueue(recovered, std::move(moved)));
        ASSERT_TRUE(queue.tryDequeue(consumer, result));
        EXPECT_EQ(result, copied);
        ASSERT_TRUE(queue.tryDequeue(consumer, result));
        EXPECT_EQ(result, std::string(256, 'm'));
        EXPECT_FALSE(queue.tryDequeue(consumer, result));
    }
    EXPECT_EQ(resource.allocations, resource.deallocations);
}

TEST(ConcurrentQueueTest, MultipleProducersAndConsumersWithTokens)
{
    constexpr int producerCount = 4;
    constexpr int consumerCount = 4;
    constexpr int perProducer   = 10000;
    constexpr int total         = producerCount * perProducer;
    RecordingResource resource;
    ScopedQueueResource useResource(resource);
    {
        ec2s::ConcurrentQueue<int> queue(1024);
        std::vector<std::atomic<int>> seen(total);
        std::atomic<int> consumed{ 0 };
        std::atomic<int> enqueueFailures{ 0 };
        std::atomic<int> invalidValues{ 0 };
        std::barrier start(producerCount + consumerCount);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        std::vector<std::thread> threads;

        for (int producer = 0; producer < producerCount; ++producer)
        {
            threads.emplace_back(
                [&, producer]
                {
                    auto token = queue.makeProducerToken();
                    EXPECT_TRUE(token.valid());
                    start.arrive_and_wait();
                    for (int i = 0; i < perProducer; ++i)
                    {
                        if (!queue.enqueue(token, producer * perProducer + i))
                        {
                            ++enqueueFailures;
                        }
                    }
                });
        }
        for (int consumer = 0; consumer < consumerCount; ++consumer)
        {
            threads.emplace_back(
                [&]
                {
                    auto token = queue.makeConsumerToken();
                    start.arrive_and_wait();
                    while (consumed.load() < total && std::chrono::steady_clock::now() < deadline)
                    {
                        int result;
                        if (queue.tryDequeue(token, result))
                        {
                            if (result >= 0 && result < total)
                            {
                                ++seen[result];
                            }
                            else
                            {
                                ++invalidValues;
                            }
                            ++consumed;
                        }
                        else
                        {
                            std::this_thread::yield();
                        }
                    }
                });
        }
        for (auto& thread : threads)
        {
            thread.join();
        }

        EXPECT_EQ(enqueueFailures.load(), 0);
        EXPECT_EQ(invalidValues.load(), 0);
        EXPECT_EQ(consumed.load(), total);
        for (const auto& count : seen)
        {
            EXPECT_EQ(count.load(), 1);
        }
        auto consumer = queue.makeConsumerToken();
        int result;
        EXPECT_FALSE(queue.tryDequeue(consumer, result));
    }
    EXPECT_EQ(resource.allocations, resource.deallocations);
}
