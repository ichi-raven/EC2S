/*****************************************************************/ /**
 * @file   LockFreeQueue.hpp
 * @brief  header file of LockFreeQueue class
 * 
 * @author ichi-raven
 * @date   March 2026
 *********************************************************************/
#ifndef EC2S_LOCKFREEQUEUE_HPP_
#define EC2S_LOCKFREEQUEUE_HPP_

#include "TaggedPointer.hpp"

#include <atomic>
#include <memory_resource>

namespace ec2s
{
    template <typename T>
    class LockFreeQueue
    {
    public:
        LockFreeQueue()
            : mpMemoryResource(nullptr)
        {
            Node* pDummy = allocate(T(), std::byte(42));
            mpHead.store(TaggedPointer(pDummy));
            mpTail.store(TaggedPointer(pDummy));
        }

        LockFreeQueue(std::pmr::memory_resource* const pMemoryResource)
            : mpMemoryResource(pMemoryResource)
        {
            Node* pDummy = allocate(T(), std::byte(0));
            mpHead.store(TaggedPointer(pDummy));
            mpTail.store(TaggedPointer(pDummy));
        }

        LockFreeQueue(const LockFreeQueue&)            = delete;
        LockFreeQueue& operator=(const LockFreeQueue&) = delete;

        void push(const T& value)
        {
            Node* pNewNode = allocate(value, stepTag(mpTail.load().getTag()));

            while (true)
            {
                auto tail      = mpTail.load();
                Node* tailNode = tail.getPointer<Node>();

                auto next = tailNode->pNext.load();

                if (next.getPointer<Node>() == nullptr)
                {
                    // tail is really the last node
                    TaggedPointer<> newNext(pNewNode, stepTag(next.getTag()));

                    if (tailNode->pNext.compare_exchange_weak(next, newNext))
                    {
                        // try to swing tail
                        TaggedPointer<> newTail(pNewNode, stepTag(tail.getTag()));
                        mpTail.compare_exchange_weak(tail, newTail);
                        return;
                    }
                }
                else
                {
                    // tail is behind, advance it
                    TaggedPointer<> newTail(next.getPointer<Node>(), stepTag(tail.getTag()));
                    mpTail.compare_exchange_weak(tail, newTail);
                }
            }
        }

        bool pop(T& result)
        {
            while (true)
            {
                auto head = mpHead.load();
                auto tail = mpTail.load();

                Node* pHeadNode = head.getPointer<Node>();
                Node* pNext     = pHeadNode->pNext.load().getPointer<Node>();

                if (!pNext)
                {
                    // queue is empty
                    return false;
                }

                if (head == tail)
                {
                    mpTail.compare_exchange_weak(tail, TaggedPointer(pNext));
                    continue;
                }

                result = pNext->value;

                if (mpHead.compare_exchange_weak(head, TaggedPointer(pNext)))
                {
                    if (mpMemoryResource)
                    {
                        mpMemoryResource->deallocate(pHeadNode, sizeof(Node));
                    }
                    else
                    {
                        delete pHeadNode;
                    }

                    return true;
                }
            }
        }

    private:
        struct Node
        {
            T value;
            std::atomic<TaggedPointer<>> pNext;
        };

        inline Node* allocate(const T& value, const std::byte tag)
        {
            if (mpMemoryResource)
            {
                Node* pNode = static_cast<Node*>(mpMemoryResource->allocate(sizeof(Node)));
                new (pNode) Node{ value, TaggedPointer(nullptr, tag) };
                return pNode;
            }
            else
            {
                return new Node{ value, TaggedPointer(nullptr, tag) };
            }
        }

        inline void deallocate(Node* pNode)
        {
            if (mpMemoryResource)
            {
                mpMemoryResource->deallocate(pNode, sizeof(Node));
            }
            else
            {
                delete pNode;
            }
        }
        
        inline std::byte stepTag(const std::byte tag)
        {
            return static_cast<std::byte>((uint8_t(tag) + 1) % (1 << TaggedPointer<>::kTagBits));
        }

        std::atomic<TaggedPointer<>> mpHead;
        std::atomic<TaggedPointer<>> mpTail;
        std::pmr::memory_resource* mpMemoryResource;
    };
}  // namespace ec2s

#endif  // EC2S_LOCKFREEQUEUE_HPP_