/*****************************************************************/ /**
 * @file   ArenaAllocator.hpp
 * @brief  header file of ArenaAllocator class
 * 
 * @author ichi-raven
 * @date   February 2026
 *********************************************************************/
#ifndef EC2S_ARENA_ALLOCATOR_HPP_
#define EC2S_ARENA_ALLOCATOR_HPP_

#include <algorithm>
#include <cstddef>
#include <memory_resource>

#define EC2S_DEFAULT_CACHE_LINE_SIZE 64

namespace ec2s
{
    template <size_t kBlockSize = 256>
    class alignas(EC2S_DEFAULT_CACHE_LINE_SIZE) ArenaAllocator
    {
    public:
        ArenaAllocator(const size_t byteSize) noexcept
            : mpHead(nullptr)
            , mpExternalMemory(nullptr)
            , mExternalMemorySize(0)
        {
            reallocate(byteSize);
        }

        ArenaAllocator(void* pMemory, const size_t memoryByteSize) noexcept
            : mpHead(nullptr)
            , mpExternalMemory(pMemory)
            , mExternalMemorySize(memoryByteSize)
            , mExternalMemoryCurrentSize(memoryByteSize)
        {
            reallocate(memoryByteSize);
        }

        ArenaAllocator(const ArenaAllocator&)            = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        ArenaAllocator(ArenaAllocator&& other) noexcept
        {
            if (this != &other)
            {
                while (mpHead)
                {
                    delete[] mpHead->pArena;
                    void* tmp = mpHead;
                    mpHead    = mpHead->pNext;
                    delete tmp;
                }
            }

            mpHead       = other.mpHead;
            other.mpHead = nullptr;
        }

        ArenaAllocator& operator=(ArenaAllocator&& other) noexcept
        {
            if (this != &other)
            {
                while (mpHead)
                {
                    delete[] mpHead->pArena;
                    void* tmp = mpHead;
                    mpHead    = mpHead->pNext;
                    delete tmp;
                }
            }

            mpHead       = other.mpHead;
            other.mpHead = nullptr;
            return *this;
        }

        ~ArenaAllocator() noexcept
        {
            while (mpHead)
            {
                if (!mpExternalMemory)
                {
                    delete[] mpHead->pArena;
                }

                void* tmp = mpHead;
                mpHead    = mpHead->pNext;
                delete tmp;
            }
        }

        void* allocate(const size_t numBytes) noexcept
        {
            if (mpHead->pCurrentPtr + numBytes > mpHead->pArena + mpHead->size)
            {
                if (!reallocate(std::max(kBlockSize, numBytes)))
                {
                    // failed to allocate new block
                    return nullptr;
                }
            }
            void* rtn = mpHead->pCurrentPtr;
            mpHead->pCurrentPtr += numBytes;
            return rtn;
        }

        template <typename T>
        T* allocate(const size_t num = 1) noexcept
        {
            const size_t allocSize = sizeof(T) * num;

            return reinterpret_cast<T*>(allocate(allocSize));
        }

        void reset()
        {
            auto* pHead = mpHead;
            while (pHead)
            {
                pHead->pCurrentPtr = pHead->pArena;
                pHead              = pHead->pNext;
            }
        }

    private:

        bool reallocate(const size_t newBlockSize) noexcept
        {
            auto* prevHead = mpHead;

            if (mpExternalMemory)
            {
                mpHead = new (std::nothrow) MemoryBlock();
                if (mpHead == nullptr || newBlockSize > mExternalMemoryCurrentSize)
                {
                    // failed to allocate new block
                    mpHead = prevHead;
                    return false;
                }

                mExternalMemoryCurrentSize -= newBlockSize;
                mpHead->pArena = static_cast<std::byte*>(mpExternalMemory);
            }
            else
            {
                mpHead         = new (std::nothrow) MemoryBlock();
                mpHead->pArena = new (std::nothrow) std::byte[newBlockSize];

                if (mpHead == nullptr || mpHead->pArena == nullptr)
                {
                    // failed to allocate new block
                    mpHead = prevHead;
                    return false;
                }
            }

            mpHead->pCurrentPtr = mpHead->pArena;
            mpHead->size        = newBlockSize;
            mpHead->pNext       = prevHead;
            return true;
        }

        struct MemoryBlock
        {
            std::byte* pArena;
            std::byte* pCurrentPtr;
            size_t size;
            MemoryBlock* pNext = nullptr;
        };

        MemoryBlock* mpHead;
        void* mpExternalMemory;
        size_t mExternalMemorySize;
        size_t mExternalMemoryCurrentSize;
    };

    template <size_t kBlockSize = 256>
    struct ArenaMemoryResource : public std::pmr::memory_resource
    {
        using size_type       = std::size_t;
        using difference_type = std::ptrdiff_t;

        ArenaMemoryResource() = delete;

        ArenaMemoryResource(ArenaAllocator<kBlockSize>& allocator)
            : mEngine(allocator)
        {
        }

        ArenaMemoryResource(const ArenaMemoryResource&)           = delete;
        ArenaMemoryResource& operator=(const ArenaMemoryResource&) = delete;

        void* do_allocate(const std::size_t bytes, const std::size_t alignment) override
        {
            if (alignment > alignof(std::max_align_t))
            {
                throw std::bad_alloc();
            }

            return mEngine.allocate(bytes);
        }

        void do_deallocate(void* p, std::size_t bytes, [[maybe_unused]] std::size_t alignment) override
        {
            // do nothing
            //mEngine.deallocate(p);
        }

        bool do_is_equal(const memory_resource& other) const noexcept override
        {
            return this == &other;
        }

    private:
        ArenaAllocator<kBlockSize>& mEngine;
    };

}  // namespace ec2s

#endif