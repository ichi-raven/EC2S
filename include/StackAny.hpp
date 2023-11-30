/*****************************************************************//**
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
	template<size_t kMemSize>
	class StackAny
	{
	private:
		class IDestructor
		{
		public:

			virtual ~IDestructor()
			{}

			virtual void invoke(void* const p) = 0;
		};

		template<typename T>
		class Destructor : public IDestructor
		{
		public:

			virtual ~Destructor()
			{}

			virtual void invoke(void* const p) override
			{
				reinterpret_cast<T*>(p)->~T();
			}
		};

	public:

		StackAny()
			: mpDestructor(nullptr)
			, mTypeHash(0)
			, mpMemory {}
		{

		}

		template<typename T>
		StackAny(const T& from)
		{
			static_assert(sizeof(T) <= kMemSize, "invalid type size!");
			new(mpMemory) T(from);
			mpDestructor = new Destructor<T>();
			mTypeHash = TypeHasher::hash<T>();
		}

		template<typename T>
		StackAny(const T&& from)
		{
			static_assert(sizeof(T) <= kMemSize, "invalid type size!");
			new(mpMemory) T(from);
			mpDestructor = new Destructor<T>();
			mTypeHash = TypeHasher::hash<T>();
		}

		~StackAny()
		{
			if (mpDestructor)
			{
				mpDestructor->invoke(mpMemory);
				delete mpDestructor;
			}
		}

		template<typename T>
		T& operator=(const T& from)
		{
			static_assert(sizeof(T) <= kMemSize, "invalid type size!");

			if (mpDestructor)
			{
				mpDestructor->invoke(mpMemory);
				delete mpDestructor;
			}

			new(mpMemory) T(from);
			mpDestructor = new Destructor<T>();
			mTypeHash = TypeHasher::hash<T>();

			return *(reinterpret_cast<T*>(mpMemory));
		}

		template<typename T>
		T& operator=(const T&& from)
		{
			static_assert(sizeof(T) <= kMemSize, "invalid type size!");

			if (mpDestructor)
			{
				mpDestructor->invoke(mpMemory);
				delete mpDestructor;
			}

			new(mpMemory) T(from);
			mpDestructor = new Destructor<T>();
			mTypeHash = TypeHasher::hash<T>();

			return *(reinterpret_cast<T*>(mpMemory));
		}

		template<typename T>
		T& get()
		{
			static_assert(sizeof(T) <= kMemSize, "invalid type size!");
			assert(mTypeHash == TypeHasher::hash<T>() || !"invalid cast!");

			return *(reinterpret_cast<T*>(mpMemory));
		}

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
		std::byte mpMemory[kMemSize];
		IDestructor* mpDestructor;
		std::size_t mTypeHash;
	};
}

#endif
