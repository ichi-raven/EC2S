#ifndef EC2S_REGISTRY_HPP_
#define EC2S_REGISTRY_HPP_

#include "SparseSet.hpp"
#include "View.hpp"
#include "Entity.hpp"

#include <any>
#include <unordered_map>
#include <queue>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

namespace ec2s
{
    class Registry
    {
    public:

        Registry()
            : mNextEntity(0)
        {}

        ~Registry()
        {}

        template<typename... Args>
        Entity create()
        {
            Entity rtn = 0;

            if (mFreedEntities.empty())
            {
                rtn = mNextEntity++;
            }
            else
            {
                rtn = mFreedEntities.front() & kEntityIndexMask;
                mFreedEntities.pop();
            }

            if constexpr (sizeof...(Args) > 0)
            {
                createImpl<Args...>(rtn);
            }

            return rtn;
        }

        void destroy(Entity entity)
        {
            for (auto& pSparseSet : mpComponentArrays)
            {
                pSparseSet->remove(static_cast<std::size_t>(entity & kEntityIndexMask));
            }

            mFreedEntities.emplace((entity & kEntityIndexMask) + (1ull << kEntitySlotShiftWidth));
        }

        template<typename T>
        T& get(Entity entity)
        {
            return std::any_cast<SparseSet<T>&>(mComponentArrayMap[TypeHashGenerator::id<T>()])[static_cast<std::size_t>(entity & kEntityIndexMask)];
        }

        template<typename T, typename... Args>
        void add(Entity entity, Args... args)
        {
            auto&& itr = mComponentArrayMap.find(TypeHashGenerator::id<T>());
            if (itr == mComponentArrayMap.end())
            {
                itr = mComponentArrayMap.emplace(TypeHashGenerator::id<T>(), SparseSet<T>()).first;
                mpComponentArrays.emplace_back(std::any_cast<SparseSet<T>>(&itr->second));
            }

            auto& ss = std::any_cast<SparseSet<T>&>(itr->second);
            ss.emplace(entity, args...);
        }

        template<typename T>
        void remove(Entity entity)
        {
            auto&& itr = mComponentArrayMap.find(TypeHashGenerator::id<T>());
            assert(itr != mComponentArrayMap.end());
            auto& ss = std::any_cast<SparseSet<T>&>(itr->second);
            ss.remove(static_cast<std::size_t>(entity & kEntityIndexMask));
        }

        template<typename T, typename Func>
        void each(Func func)
        {
            auto&& itr = mComponentArrayMap.find(TypeHashGenerator::id<T>());
            assert(itr != mComponentArrayMap.end());
            auto& ss = std::any_cast<SparseSet<T>&>(itr->second);
            ss.each(func);
        }

        template<typename... Args>
        View<Args...> view()
        {
            assert(hasAllTypesCalled<Args...>() || !"component type which requested by view is not contained in this registry!");
            return View<Args...>(std::any_cast<SparseSet<Args>&>(mComponentArrayMap[TypeHashGenerator::id<Args>()])...);
        }

        void dump()
        {
#ifndef NDEBUG
            for (const auto& pSS : mpComponentArrays)
            {
                std::cerr << "type hash : " << pSS->getPackedTypeHash() << "\n";
                pSS->dump();
                std::cerr << "\n";
            }
#endif
        }

    private:
        template<typename Head, typename... Tail>
        void createImpl(Entity entity)
        {
            add<Head>(entity);

            if constexpr (sizeof...(Tail) > 0)
            {
                createImpl<Tail...>(entity);
            }
        }

        template<typename Head, typename... Tail>
        bool hasAllTypesCalled()
        {
            if (!mComponentArrayMap.contains(TypeHashGenerator::id<Head>()))
            {
                return false;
            }

            if constexpr (sizeof...(Tail) > 0)
            {
                hasAllTypesCalled<Tail...>();
            }

            return true;
        }


        Entity mNextEntity;
        std::queue<Entity> mFreedEntities;

        std::unordered_map<TypeHash, std::any> mComponentArrayMap;
        std::vector<ISparseSet*> mpComponentArrays;
    };
}

#endif