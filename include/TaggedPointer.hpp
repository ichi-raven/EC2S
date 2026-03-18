/*****************************************************************/ /**
 * @file   TaggedPointer.hpp
 * @brief  header file of TaggedPointer class
 * 
 * @author ichi-raven
 * @date   March 2026
 *********************************************************************/
#ifndef EC2S_TAGGEDPOINTER_HPP_
#define EC2S_TAGGEDPOINTER_HPP_

#include <cstdint>
#include <cstddef>
#include <cassert>

namespace ec2s
{
    template <uint32_t kPointerBits = 57>
    class TaggedPointer
    {
    public:
        constexpr static uint32_t kTotalBits    = 64;
        constexpr static uint32_t kTagBits      = 64 - kPointerBits;
        constexpr static uintptr_t kPointerMask = (1ULL << kPointerBits) - 1;
        constexpr static uintptr_t kTagMask     = ~kPointerMask;

    public:
        TaggedPointer()
            : mpData(0)
        {
        }
        
        TaggedPointer(void* ptr, const std::byte tag = std::byte(0))
        {
            setPointer(ptr);
            setTag(tag);
        }

        bool operator==(const TaggedPointer& other) const
        {
            return mpData == other.mpData;
        }

        void setPointer(void* ptr)
        {
            mpData = reinterpret_cast<uintptr_t>(ptr) & kPointerMask;
        }

        template <typename T>
        T* getPointer() const
        {
            return reinterpret_cast<T*>(mpData & kPointerMask);
        }

        void setTag(const std::byte tag)
        {
            assert(static_cast<uint32_t>(tag) < (1ULL << kTagBits) || !"Tag value exceeds the maximum allowed by the available tag bits!");

            mpData = (mpData & kPointerMask) | (static_cast<uintptr_t>(tag) << kPointerBits);
        }

        std::byte getTag() const
        {
            return static_cast<std::byte>((mpData & kTagMask) >> kPointerBits);
        }

    private:
        uintptr_t mpData;
    };
}  // namespace ec2s

#endif  // EC2S_TAGGEDPOINTER_HPP_