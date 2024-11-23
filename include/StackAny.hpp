/*****************************************************************/ /**
 * @file   StackAny.hpp
 * @brief  header file of StackAny class
 *
 * @author ichi-raven
 * @date   January 2023
 *********************************************************************/
#ifndef EC2S_STACKANY_HPP_
#define EC2S_STACKANY_HPP_

#include <cstddef>
#include <cassert>

#include "TypeHash.hpp"

namespace ec2s
{
    /**
     * @brief  Any type built on a fixed amount of memory on the Stack (only for SparseSet)
     * @details because runtime type information and any_cast are not used, the get() is faster than std::any
     * 
     * @tparam kMemSize size of the instance stored in this StackAny
     */
    template <size_t kMemSize>
    class StackAny
    {
    private:
        /**
         * @brief  interface type to call destructor of stored type
         */
        class IDestructor
        {
        public:
            /** 
             * @brief  destructor (itself)
             *  
             */
            virtual ~IDestructor()
            {
            }

            /** 
             * @brief  call destructors according to the type of the child
             *  
             * @param p pointer of the type of the child
             */
            virtual void invoke(void* const p) = 0;
        };

        /**
         * @brief  concrete type to call destructor of stored type
         * 
         * @tparam T type to be called destructor
         */
        template <typename T>
        class Destructor : public IDestructor
        {
        public:
            /** 
             * @brief  destructor (itself)
             *  
             */
            virtual ~Destructor()
            {
            }

            /** 
             * @brief  call destructors according to the type T
             *  
             * @param p pointer of the type T
             */
            virtual void invoke(void* const p) override
            {
                reinterpret_cast<T*>(p)->~T();
            }
        };

    public:
        /** 
         * @brief  constructor
         *  
         */
        StackAny()
            : mpDestructor(nullptr)
            , mTypeHash(0)
            , mpMemory{}
        {
        }

        /** 
         * @brief  constructor from value to be stored (lvalue ver)
         *  
         * @param from value to be stored 
         */
        template <typename T>
        StackAny(const T& from)
        {
            static_assert(sizeof(T) <= kMemSize, "invalid type size!");
            new (mpMemory) T(from);
            mpDestructor = new Destructor<T>();
            mTypeHash    = TypeHasher::hash<T>();
        }

        /** 
         * @brief  constructor from value to be stored (rvalue ver)
         *  
         * @param from value to be stored 
         */
        template <typename T>
        StackAny(const T&& from)
        {
            static_assert(sizeof(T) <= kMemSize, "invalid type size!");
            new (mpMemory) T(from);
            mpDestructor = new Destructor<T>();
            mTypeHash    = TypeHasher::hash<T>();
        }

        /** 
         * @brief  destructor
         *  
         */
        ~StackAny()
        {
            if (mpDestructor)
            {
                mpDestructor->invoke(mpMemory);
                delete mpDestructor;
            }
        }

        /** 
         * @brief  operator overloading for assigning values (lvalue ver)
         *  
         * @tparam T value type
         * @param from value
         * @return reference of value
         */
        template <typename T>
        T& operator=(const T& from)
        {
            static_assert(sizeof(T) <= kMemSize, "invalid type size!");

            if (mpDestructor)
            {
                mpDestructor->invoke(mpMemory);
                delete mpDestructor;
            }

            new (mpMemory) T(from);
            mpDestructor = new Destructor<T>();
            mTypeHash    = TypeHasher::hash<T>();

            return *(reinterpret_cast<T*>(mpMemory));
        }

        /** 
         * @brief  operator overloading for assigning values (rvalue ver)
         *  
         * @tparam T value type
         * @param from value
         * @return reference of value
         */
        template <typename T>
        T& operator=(const T&& from)
        {
            static_assert(sizeof(T) <= kMemSize, "invalid type size!");

            if (mpDestructor)
            {
                mpDestructor->invoke(mpMemory);
                delete mpDestructor;
            }

            new (mpMemory) T(from);
            mpDestructor = new Destructor<T>();
            mTypeHash    = TypeHasher::hash<T>();

            return *(reinterpret_cast<T*>(mpMemory));
        }

        /** 
         * @brief get (cast) the stored value 
         *  
         * @tparam T type to be obtained as
         * @return stored value casted to T
         */
        template <typename T>
        T& get()
        {
            static_assert(sizeof(T) <= kMemSize, "invalid type size!");

            if (TypeHasher::hash<T>() != mTypeHash)
            {
                throw std::exception("invalid type cast (StackAny)!");
            }

            return *(reinterpret_cast<T*>(mpMemory));
        }

        /** 
         * @brief  delete and reset stored values and information
         *  
         */
        void reset()
        {
            if (!mpDestructor)
            {
                return;
            }

            mpDestructor->invoke(mpMemory);
            delete mpDestructor;
            mpDestructor = nullptr;
        }

    private:
        //! stack memory area to store values
        std::byte mpMemory[kMemSize];
        //! pointer to call destructor
        IDestructor* mpDestructor;
        //! type hash of the stored type
        std::size_t mTypeHash;
    };
}  // namespace ec2s

#endif
