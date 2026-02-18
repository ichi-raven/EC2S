/*****************************************************************/ /**
 * @file   TLSFAllocator.hpp
 * @brief  header file of TLSFAllocator class
 * 
 * @author ichi-raven
 * @date   February 2026
 *********************************************************************/

#include <cstddef>
#include <cstdint>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

#ifndef EC2S_TLSFALLOCATOR_HPP_
#define EC2S_TLSFALLOCATOR_HPP_

namespace ec2s
{
    template <uint32_t kSplitNum = 4>
    class TLSFAllocator;

    template <typename T, uint32_t kSplitNum = 4>
    class TLSFStdAllocator;

    template <uint32_t kSplitNum>
    class TLSFAllocator
    {
    private:
        // Header struct for BoundaryBlock
        class BoundaryBlockHeader
        {
            uint32_t size;

        public:
            BoundaryBlockHeader()
                : size()
            {
            }
            uint32_t getSize() const
            {
                return size;
            }
            void setSize(uint32_t size)
            {
                this->size = size;
            }
        };

        template <class Header = BoundaryBlockHeader, class EndTag = uint32_t>
        class BoundaryBlock
        {
            void writeEndTag()
            {
                new (reinterpret_cast<std::byte*>(this) + getBlockSize() - getEndTagSize()) EndTag(getBlockSize());
            }

            uint32_t getEndTagSize()
            {
                return sizeof(EndTag);
            }

        public:
            Header header;

            BoundaryBlock(uint32_t size)
            {
                header.setSize(size);
                writeEndTag();
            };

            void* getMemory()
            {
                return (std::byte*)this + sizeof(BoundaryBlock);
            }

            uint32_t getMemorySize() const
            {
                return header.getSize();
            }

            uint32_t getBlockSize()
            {
                return sizeof(BoundaryBlock) + header.getSize() + getEndTagSize();
            }

            BoundaryBlock* next()
            {
                return (BoundaryBlock*)((std::byte*)this + getBlockSize());
            }

            BoundaryBlock* prev()
            {
                std::uint32_t* preSize = (std::uint32_t*)((std::byte*)this - getEndTagSize());
                return (BoundaryBlock*)((std::byte*)this - *preSize);
            }

            void merge()
            {
                BoundaryBlock* nextBlock = next();

                uint32_t newSize = header.getSize() + getEndTagSize() + sizeof(BoundaryBlock) + nextBlock->getMemorySize();
                header.setSize(newSize);
                writeEndTag();
            }

            BoundaryBlock* split(uint32_t size)
            {
                uint32_t needSize = size + getEndTagSize() + sizeof(BoundaryBlock);
                if (needSize > header.getSize())
                {
                    return nullptr;
                }

                uint32_t newBlockMemSize = header.getSize() - needSize;

                header.setSize(size);
                writeEndTag();

                BoundaryBlock* newBlock = next();
                new (newBlock) BoundaryBlock(newBlockMemSize);

                return newBlock;
            }

            bool enableSplit(uint32_t size)
            {
                uint32_t needSize = size + getEndTagSize() + sizeof(BoundaryBlock);

                return needSize <= header.getSize();
            }
        };

        class TLSFBlockHeader : public BoundaryBlockHeader
        {
        public:
            TLSFBlockHeader* pre;
            TLSFBlockHeader* next;
            bool used;

            TLSFBlockHeader()
                : pre(nullptr)
                , next(nullptr)
                , used(false)
            {
            }
        };

    public:
        TLSFAllocator() = delete;

        TLSFAllocator(std::byte* mainMemory, uint32_t byteSize)
            : mMemory(mainMemory)
            , mMaxSize(byteSize - sizeof(TLSFBlockHeader) - sizeof(uint32_t))
            , mAllSize(byteSize)
            , mBlockArraySize((static_cast<uint32_t>(getMSB(mMaxSize)) - kSplitNum + 1) * (1ul << kSplitNum))
        {
            mBlockArray = new BoundaryBlock<TLSFBlockHeader>*[mBlockArraySize];
            clearAll();
        }

        ~TLSFAllocator()
        {
#ifndef NDEBUG
            for (size_t i = 0; i < mBlockArraySize; ++i)
            {
                if (mBlockArray[i] && reinterpret_cast<std::byte*>(mBlockArray[i]) >= mMemory && reinterpret_cast<std::byte*>(mBlockArray[i]) <= mMemory + mMaxSize)
                {
                    mBlockArray[i]->~BoundaryBlock();
                }
            }
#endif

            delete[] mBlockArray;
        }

        std::byte* allocate(uint32_t size)
        {
            if (size < (1ul << kSplitNum))
            {
                // too small size
                size = 1ul << kSplitNum;
            }

            if (size > mMaxSize)
            {
                assert(!"requested size is over max size!");
                return nullptr;
            }

            const uint32_t FLI = getMSB(size);
            uint32_t SLI       = getSecondLevel(size, FLI, kSplitNum);

            BoundaryBlock<TLSFBlockHeader>* target = mBlockArray[getBlockArrayIndex(FLI, SLI)];

            TLSFBlockHeader* header = nullptr;
            if (target)
            {
                header = &target->header;

                for (size_t i = 0; header->next && header->used; ++i)
                {
                    header = header->next;
                }
            }

            if (!target || !header || header->used)
            {
                target = reinterpret_cast<BoundaryBlock<TLSFBlockHeader>*>(header);

                uint32_t freeListBit = 0;
                for (uint32_t i = 0; i < (1ul << kSplitNum); ++i)
                {
                    if (mBlockArray[getBlockArrayIndex(FLI, i)])
                    {
                        TLSFBlockHeader* h = &mBlockArray[getBlockArrayIndex(FLI, i)]->header;
                        for (size_t index = 0; h->next && h->used; ++index)
                        {
                            h = h->next;
                        }

                        if (!h->used)
                        {
                            freeListBit |= 1 << i;
                        }
                    }
                }

                uint32_t newSLI = getFreeListSLI(SLI, freeListBit);
                if (newSLI == -1)
                {
                    uint32_t newFLI = getFreeListFLI(FLI, mAllFLI);

                    if (newFLI == -1)
                    {
                        assert(!"failed to allocate!");
                        return nullptr;
                    }

                    for (int i = (1 << kSplitNum) - 1; i >= 0; --i)
                    {
                        auto* biggerBlock = mBlockArray[getBlockArrayIndex(newFLI, i)];

                        if (biggerBlock && !biggerBlock->header.used)
                        {
                            if (!biggerBlock->enableSplit(size))
                            {
                                assert(!"falied to split biggest block!");
                                return nullptr;
                            }

                            unregisterFLI(biggerBlock->getMemorySize());

                            mBlockArray[getBlockArrayIndex(newFLI, i)] = nullptr;

                            auto* splitted = biggerBlock->split(size);

                            mBlockArray[getBlockArrayIndex(FLI, SLI)] = biggerBlock;

                            uint32_t splittedFLI = getMSB(splitted->getMemorySize());
                            uint32_t splittedSLI = getSecondLevel(splitted->getMemorySize(), splittedFLI, kSplitNum);
                            if (!mBlockArray[(newFLI - kSplitNum) * (1 << kSplitNum) + i])
                            {
                                mBlockArray[getBlockArrayIndex(splittedFLI, splittedSLI)] = splitted;
                                splitted->header.used                                     = false;
                            }
                            else
                            {
                                auto* header         = &mBlockArray[getBlockArrayIndex(newFLI, i)]->header;
                                auto* splittedHeader = &splitted->header;
                                header->next         = splittedHeader;
                                splittedHeader->pre  = header;
                                splittedHeader->next = nullptr;
                            }

                            registerFLI(splitted->getMemorySize());
                            break;
                        }
                    }
                }
                else
                {
                    SLI = newSLI;
                }

                target = mBlockArray[getBlockArrayIndex(FLI, SLI)];
            }

            if (!target || target->header.used)
            {
                // failed to allocate
                return nullptr;
            }

            target->header.used = true;
            return reinterpret_cast<std::byte*>(target->getMemory());
        }

        template <typename T>
        T* allocate(uint32_t num)
        {
            return reinterpret_cast<T*>(allocate(sizeof(T) * num));
        }

        bool deallocate(void* address)
        {
            if (!address)
            {
                assert(!"invalid free address!");
                return false;
            }

            BoundaryBlock<TLSFBlockHeader>* pBlock = nullptr;
            {
                auto* p = reinterpret_cast<std::byte*>(address);
                pBlock  = reinterpret_cast<BoundaryBlock<TLSFBlockHeader>*>(p - sizeof(TLSFBlockHeader));
            }

            pBlock->header.used = false;
            assert(reinterpret_cast<TLSFBlockHeader*>(pBlock) == &pBlock->header);

            removeBlockFromList(pBlock);

            const auto* right      = reinterpret_cast<std::byte*>(pBlock->next());
            const auto* left       = reinterpret_cast<std::byte*>(pBlock->prev());
            const bool isRightFree = right && right >= mMemory && right <= (mMemory + mAllSize) && !(pBlock->next()->header.used) && pBlock->next()->getMemorySize();
            const bool isLeftFree  = left && left <= (mMemory + mAllSize) && left >= mMemory && !(pBlock->prev()->header.used) && pBlock->prev()->getMemorySize();

            if (isRightFree)
            {
                removeBlockFromList(pBlock->next());

                pBlock->merge();
            }

            if (isLeftFree)
            {
                removeBlockFromList(pBlock->prev(), pBlock);

                pBlock = pBlock->prev();
                pBlock->merge();
            }

            const uint32_t FLI = getMSB(pBlock->getMemorySize());
            const uint32_t SLI = getSecondLevel(pBlock->getMemorySize(), FLI, kSplitNum);

#ifndef NDEBUG
            {
                auto* p = reinterpret_cast<std::byte*>(mBlockArray[getBlockArrayIndex(FLI, SLI)]);
                assert(!p || !(p < mMemory || p > mMemory + mMaxSize) || !"invalid memory!!!");
            }
#endif

            if (!mBlockArray[getBlockArrayIndex(FLI, SLI)])
            {
                mBlockArray[getBlockArrayIndex(FLI, SLI)] = pBlock;
            }
            else
            {
                auto* header    = &mBlockArray[getBlockArrayIndex(FLI, SLI)]->header;
                auto* newHeader = &pBlock->header;
                header->next    = newHeader;
                newHeader->pre  = header;
                newHeader->next = nullptr;
            }

            registerFLI(pBlock->getMemorySize());

            return true;
        }

        void clearAll()
        {
            for (size_t i = 0; i < mBlockArraySize; ++i)
            {
                mBlockArray[i] = nullptr;
            }

            BoundaryBlock<TLSFBlockHeader>* block                = new (mMemory) BoundaryBlock<TLSFBlockHeader>(mMaxSize);
            mBlockArray[getBlockArrayIndex(getMSB(mMaxSize), 0)] = block;
            mAllFLI                                              = 1 << getMSB(mMaxSize);
        }

        void dump()
        {
#ifndef NDEBUG
            const auto maxFLI = getMSB(mMaxSize);
            const auto maxSLI = 1 << kSplitNum;

            std::cerr << "----------------dump-----------------\n";
            for (size_t fli = 0; fli <= maxFLI; ++fli)
            {
                for (size_t sli = 0; sli < maxSLI; ++sli)
                {
                    std::cerr << (1ul << fli) + (1ul << fli) / (1ul << kSplitNum) * sli << "(" << fli << " , " << sli << ") : ";
                    auto* p = reinterpret_cast<std::byte*>(mBlockArray[getBlockArrayIndex(fli, sli)]);
                    if (!p)
                    {
                        std::cerr << "null\n";
                    }
                    else if (p < mMemory || p >= mMemory + mAllSize)
                    {
                        std::cerr << "invalid\n";
                    }
                    else
                    {
                        std::cerr << "found\n";
                    }
                }
            }
            std::cerr << "----------------end of dump-----------------\n";
#endif
        }

    private:
        inline uint32_t getMSB(uint32_t data) const
        {
            if (data == 0)
            {
                return 0;
            }

            uint32_t index = 0;
            for (; data != 1; ++index)
            {
                data = data >> 1;
            }

            return index;
        }

        inline uint32_t getLSB(uint32_t data) const
        {
            uint32_t index = 0;
            for (; data % 2 == 0; ++index)
            {
                data = data >> 1;
            }

            return index;
        }

        inline uint32_t getSecondLevel(uint32_t size, uint32_t MSB, uint32_t N) const
        {
            const uint32_t mask = (1 << MSB) - 1;  // 1000 0000 -> 0111 1111

            const uint32_t rs = MSB - N;

            return (size & mask) >> rs;
        }

        inline uint32_t getFreeListSLI(uint32_t mySLI, uint32_t freeListBit)
        {
            uint32_t myBit = 0xffffffff << mySLI;

            uint32_t enableListBit = freeListBit & myBit;

            if (enableListBit == 0)
            {
                return -1;
            }

            return getLSB(enableListBit);
        }

        inline uint32_t getFreeListFLI(uint32_t myFLI, uint32_t globalFLI)
        {
            uint32_t myBit        = 0xffffffff << myFLI;
            uint32_t enableFLIBit = globalFLI & myBit;
            if (enableFLIBit == 0)
            {
                return -1;
            }

            return getMSB(enableFLIBit);
        }

        inline size_t getBlockArrayIndex(const uint32_t FLI, const uint32_t SLI) const
        {
            return (FLI - kSplitNum) * (1 << kSplitNum) + SLI;
        }

        inline void registerFLI(const uint32_t memorySize)
        {
            mAllFLI |= (1 << getMSB(memorySize));
        }

        inline void unregisterFLI(const uint32_t memorySize)
        {
            mAllFLI &= ~(1 << getMSB(memorySize));
        }

        inline void removeBlockFromList(BoundaryBlock<TLSFBlockHeader>* pBlock, BoundaryBlock<TLSFBlockHeader>* pRight = nullptr)
        {
            if (!pBlock->header.next && !pBlock->header.pre)
            {
                unregisterFLI(pBlock->getMemorySize());
                const auto FLI                            = getMSB(pBlock->getMemorySize());
                const auto SLI                            = getSecondLevel(pBlock->getMemorySize(), FLI, kSplitNum);
                mBlockArray[getBlockArrayIndex(FLI, SLI)] = nullptr;
            }
            else if (pBlock->header.pre)
            {
                pBlock->header.pre->next = pBlock->header.next;
                if (pBlock->header.next)
                {
                    pBlock->header.next->pre = pBlock->header.pre;
                }
            }
            else if (pBlock->header.next)
            {
                pBlock->header.next->pre = nullptr;
                const auto FLI           = getMSB(pBlock->getMemorySize());
                const auto SLI           = getSecondLevel(pBlock->getMemorySize(), FLI, kSplitNum);
                if (pRight)
                {
                    mBlockArray[getBlockArrayIndex(FLI, SLI)] = reinterpret_cast<BoundaryBlock<TLSFBlockHeader>*>(pRight->header.next);
                }
                else
                {
                    mBlockArray[getBlockArrayIndex(FLI, SLI)] = reinterpret_cast<BoundaryBlock<TLSFBlockHeader>*>(pBlock->header.next);
                }
            }
            else
            {
                assert(!"invalid");
            }
        }

        BoundaryBlock<TLSFBlockHeader>** mBlockArray;
        std::byte* mMemory;
        const uint32_t mMaxSize;
        const uint32_t mAllSize;
        const uint32_t mBlockArraySize;
        uint32_t mAllFLI;
    };

    template <typename T, uint32_t kSplitNum>
    class TLSFStdAllocator
    {
    public:
        using value_type      = T;
        using pointer         = T*;
        using const_pointer   = const T*;
        using reference       = T&;
        using const_reference = const T&;
        using size_type       = std::size_t;
        using difference_type = std::ptrdiff_t;

        template <typename U>
        struct rebind
        {
            using other = TLSFStdAllocator<U, kSplitNum>;
        };

        explicit TLSFStdAllocator(TLSFAllocator<kSplitNum>& engine) noexcept
            : mEngine(&engine)
        {
        }

        template <typename U>
        TLSFStdAllocator(const TLSFStdAllocator<U, kSplitNum>& other) noexcept
            : mEngine(other.mEngine)
        {
        }

        T* allocate(std::size_t n)
        {
            if (n == 0) return nullptr;

            void* p = mEngine->allocate(static_cast<uint32_t>(n * sizeof(T)));

            if (!p)
            {
                throw std::bad_alloc();
            }
            return static_cast<T*>(p);
        }

        void deallocate(T* p, std::size_t n) noexcept
        {
            if (p)
            {
                mEngine->deallocate(p);
            }
        }

        template <typename U>
        bool operator==(const TLSFStdAllocator<U, kSplitNum>& other) const noexcept
        {
            return mEngine == other.mEngine;
        }

        template <typename U>
        bool operator!=(const TLSFStdAllocator<U, kSplitNum>& other) const noexcept
        {
            return !(*this == other);
        }

        TLSFAllocator<kSplitNum>* mEngine;
    };
}  // namespace ec2s
#endif