/*****************************************************************//**
 * @file   Registry.hpp
 * @brief  header file of Registry class
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_REGISTRY_HPP_
#define EC2S_REGISTRY_HPP_

#include "SparseSet.hpp"
#include "View.hpp"
#include "Entity.hpp"
#include "StackAny.hpp"

//#include <any>
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
                rtn = mFreedEntities.front();
                mFreedEntities.pop();
            }

            if constexpr (sizeof...(Args) > 0)
            {
                createImpl<Args...>(rtn);
            }

            return rtn;
        }

        void destroy(const Entity entity)
        {
            for (auto& [typeHash, pSparseSet] : mpComponentArrayPairs)
            {
                pSparseSet->remove(entity);
            }

            mFreedEntities.emplace(static_cast<Entity>(entity | (1ull << kEntitySlotShiftWidth)));
        }

        void clear()
        {
            for (auto& [typeHash, pSparseSet] : mpComponentArrayPairs)
            {
                pSparseSet->clear();
            }

            std::queue<Entity> empty;
            std::swap(mFreedEntities, empty);
        }

        template<typename Component>
        Component& get(const Entity entity)
        {
            return mComponentArrayMap[TypeHasher::hash<Component>()].get<SparseSet<Component>>()[entity];
        }

        template<typename Component1, typename Component2, typename... OtherComponents>
        std::tuple<Component1, Component2, OtherComponents...> get(const Entity entity)
        {
            // TODO:
            assert(!"TODO");
        }

        template<typename T>
        const std::vector<Entity>& getEntities()
        {
            return mComponentArrayMap[TypeHasher::hash<T>()].get<SparseSet<T>>().getDenseEntities();
        }

        std::size_t activeEntityNum() const
        {
            return static_cast<std::size_t>(mNextEntity) - mFreedEntities.size();
        }

        template<typename T>
        std::size_t size()
        {
            if (!mComponentArrayMap.contains(TypeHasher::hash<T>()))
            {
                return 0;
            }
            return mComponentArrayMap[TypeHasher::hash<T>()].get<SparseSet<T>>().size();
        }

        template<typename T>
        bool contains(const Entity entity)
        {
            return mComponentArrayMap.contains(TypeHasher::hash<T>()) && mComponentArrayMap[TypeHasher::hash<T>()].get<SparseSet<T>>().contains(entity);
        }

        template<typename T, typename... Args>
        void add(const Entity entity, Args... args)
        {
#ifdef EC2S_CHECK_SYNONYM
            const TypeHash hash = TypeHasher::hash<T>();
#else
            constexpr TypeHash hash = TypeHasher::hash<T>();
#endif

            auto&& itr = mComponentArrayMap.find(hash);
            if (itr == mComponentArrayMap.end())
            {
                itr = mComponentArrayMap.emplace(hash, SparseSet<T>()).first;
                mpComponentArrayPairs.emplace_back(hash, &(itr->second.get<SparseSet<T>>()));
            }

            auto& ss = itr->second.get<SparseSet<T>>();
            ss.emplace(entity, args...);
        }

        template<typename T>
        void remove(const Entity entity)
        {
            auto&& itr = mComponentArrayMap.find(TypeHasher::hash<T>());
            if (itr == mComponentArrayMap.end())
            {
                return;
            }

            auto& ss = itr->second.get<SparseSet<T>>();
            ss.remove(entity);
        }

        template<typename T, typename Func, typename Traits::IsEligibleEachFunc<Func, T>* = nullptr >
        void each(Func func)
        {
            auto&& itr = mComponentArrayMap.find(TypeHasher::hash<T>());
            if (itr == mComponentArrayMap.end())
            {
                return;
            }
            //auto& ss = std::any_cast<SparseSet<T>&>(itr->second);
            auto& ss = itr->second.get<SparseSet<T>>();
            ss.each(func);
        }

        template<typename T, typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, T>* = nullptr >
        void each(Func func)
        {
            auto&& itr = mComponentArrayMap.find(TypeHasher::hash<T>());
            if (itr == mComponentArrayMap.end())
            {
                return;
            }

            auto& ss = itr->second.get<SparseSet<T>>();
            ss.each(func);
        }

        template<typename T, typename Func, typename std::enable_if_t<!std::is_invocable_v<Func, T&> && !std::is_invocable_v<Func, Entity, T&>>>
        void each(Func func)
        {
            static_assert(std::is_invocable_v<Func, T&> || std::is_invocable_v<Func, Entity, T&>, "ineligible Func type!");
        }

        template<typename Component1, typename Component2, typename... OtherComponents, typename Func>
        void each(Func func)
        {
            view<Component1, Component2, OtherComponents...>().each(func);
        }

        template<typename... Args>
        View<Args...> view()
        {
            checkAndAddNewComponent<Args...>();
            return View<Args...>(mComponentArrayMap[TypeHasher::hash<Args>()].get<SparseSet<Args>>()...);

        }

        void dump()
        {
#ifndef NDEBUG
            for (const auto& pSparseSet : mpComponentArrayPairs)
            {
                std::cerr << "type hash : " << pSparseSet.first << "\n";
                pSparseSet.second->dump();
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
        void checkAndAddNewComponent()
        {
#ifndef NDEBUG
            const TypeHash hash = TypeHasher::hash<Head>();
#else
            constexpr TypeHash hash = TypeHasher::hash<Head>();
#endif

            if (!mComponentArrayMap.contains(hash))
            {
                auto&& itr = mComponentArrayMap.emplace(hash, SparseSet<Head>()).first;
                mpComponentArrayPairs.emplace_back(hash, &(itr->second.get<SparseSet<Head>>()));
            }

            if constexpr (sizeof...(Tail) > 0)
            {
                checkAndAddNewComponent<Tail...>();
            }
        }


        Entity mNextEntity;
        std::queue<Entity> mFreedEntities;

        using Dummy_t = std::uint32_t;

        std::unordered_map<TypeHash, StackAny<sizeof(SparseSet<Dummy_t>)>> mComponentArrayMap;
        std::vector<std::pair<TypeHash, ISparseSet*>> mpComponentArrayPairs;
    };
}

#endif