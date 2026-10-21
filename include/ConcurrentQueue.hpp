#ifndef EC2S_CONCURRENTQUEUE_HPP_
#define EC2S_CONCURRENTQUEUE_HPP_

#include "third_party/concurrentqueue/concurrentqueue.h"

#include <cstddef>
#include <limits>
#include <memory_resource>
#include <new>
#include <utility>

namespace ec2s
{
    namespace detail
    {
        inline std::pmr::memory_resource* gpConcurrentQueueMemoryResource = std::pmr::new_delete_resource();

        struct alignas(std::max_align_t) ConcurrentQueueAllocation
        {
            std::size_t bytes;
        };

        struct ConcurrentQueuePmrTraits : moodycamel::ConcurrentQueueDefaultTraits
        {
            static void* malloc(std::size_t bytes)
            {
                if (bytes > std::numeric_limits<std::size_t>::max() - sizeof(ConcurrentQueueAllocation))
                {
                    return nullptr;
                }

                const auto allocationBytes = sizeof(ConcurrentQueueAllocation) + bytes;
                try
                {
                    void* memory = gpConcurrentQueueMemoryResource->allocate(allocationBytes, alignof(ConcurrentQueueAllocation));
                    if (memory == nullptr)
                    {
                        return nullptr;
                    }

                    auto* allocation = new (memory) ConcurrentQueueAllocation{ allocationBytes };
                    return allocation + 1;
                }
                catch (const std::bad_alloc&)
                {
                    return nullptr;
                }
            }

            static void free(void* pointer) noexcept
            {
                if (pointer == nullptr)
                {
                    return;
                }

                auto* allocation = static_cast<ConcurrentQueueAllocation*>(pointer) - 1;
                gpConcurrentQueueMemoryResource->deallocate(allocation, allocation->bytes, alignof(ConcurrentQueueAllocation));
            }
        };
    }  // namespace detail

    // Shared by all ConcurrentQueue<T>. Change only when no queues exist and
    // keep the resource alive until every queue has been destroyed.
    inline void setConcurrentQueueMemoryResource(std::pmr::memory_resource& resource)
    {
        detail::gpConcurrentQueueMemoryResource = &resource;
    }

    template <typename T>
    class ConcurrentQueue
    {
    public:
        using ProducerToken = moodycamel::ProducerToken;
        using ConsumerToken = moodycamel::ConsumerToken;

        ConcurrentQueue() = default;

        // Initial allocation estimate, not a maximum queue size.
        explicit ConcurrentQueue(std::size_t initialCapacity)
            : mQueue(initialCapacity)
        {
        }

        // Reuse tokens with this queue only, one thread at a time per token.
        // Keep the queue alive until its tokens have been destroyed.
        [[nodiscard]] ProducerToken makeProducerToken()
        {
            return ProducerToken(mQueue);
        }

        [[nodiscard]] ConsumerToken makeConsumerToken()
        {
            return ConsumerToken(mQueue);
        }

        [[nodiscard]] bool enqueue(const T& value)
        {
            return mQueue.enqueue(value);
        }

        [[nodiscard]] bool enqueue(T&& value)
        {
            return mQueue.enqueue(std::move(value));
        }

        // Token construction can fail to allocate; an invalid token returns false.
        [[nodiscard]] bool enqueue(const ProducerToken& token, const T& value)
        {
            return token.valid() && mQueue.enqueue(token, value);
        }

        [[nodiscard]] bool enqueue(const ProducerToken& token, T&& value)
        {
            return token.valid() && mQueue.enqueue(token, std::move(value));
        }

        [[nodiscard]] bool tryDequeue(T& result)
        {
            return mQueue.try_dequeue(result);
        }

        [[nodiscard]] bool tryDequeue(ConsumerToken& token, T& result)
        {
            return mQueue.try_dequeue(token, result);
        }

        [[nodiscard]] size_t sizeApprox() const
        {
            return mQueue.size_approx();
        }

    private:
        moodycamel::ConcurrentQueue<T, detail::ConcurrentQueuePmrTraits> mQueue;
    };
}  // namespace ec2s

#endif  // EC2S_CONCURRENTQUEUE_HPP_
