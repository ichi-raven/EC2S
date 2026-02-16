/*****************************************************************/ /**
 * @file   ArenaAllocator.hpp
 * @brief  header file of ArenaAllocator class
 * 
 * @author ichi-raven
 * @date   February 2026
 *********************************************************************/
#ifndef EC2S_ARENA_ALLOCATOR_HPP_
#define EC2S_ARENA_ALLOCATOR_HPP_

#include <cstddef>

namespace ec2s
{
    template <size_t BlockSize>
    class ArenaAllocator
    {
    public:
        ArenaAllocator()
        {
            mpArena      = new std::byte[BlockSize];
            mpCurrentPtr = mpArena;
        }

        ArenaAllocator(std::byte* pArena)
            : mpArena(pArena)
            , mpCurrentPtr(pArena)
        {
        }

        ArenaAllocator(const ArenaAllocator&)            = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        ArenaAllocator(ArenaAllocator&& other) noexcept
            : mpArena(other.mpArena)
            , mpCurrentPtr(other.mpCurrentPtr)
        {
            other.mpArena      = nullptr;
            other.mpCurrentPtr = nullptr;
        }

        ArenaAllocator& operator=(ArenaAllocator&& other)
        {
            if (this != &other)
            {
                delete[] mpArena;
                mpArena      = other.mpArena;
                mpCurrentPtr = other.mpCurrentPtr;
            }
            other.mpArena      = nullptr;
            other.mpCurrentPtr = nullptr;
            return *this;
        }

        ~ArenaAllocator()
        {
            delete[] mpArena;
        }

        template <typename T>
        T* allocate(const size_t num = 1) noexcept
        {
            if (mpCurrentPtr + sizeof(T) * num > mpArena + BlockSize)
            {
                return nullptr;  // not enough space
            }

            T* p = reinterpret_cast<T*>(mpCurrentPtr);
            mpCurrentPtr += sizeof(T) * num;
            return p;
        }

        //void deallocate(void* ptr) noexcept
        //{
        //    // do nothing, memory will be released when the allocator is destroyed
        //}

    private:
        std::byte* mpArena;
        std::byte* mpCurrentPtr;
    };

}  // namespace ec2s

#endif