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
                rtn = mFreedEntities.front();
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
            for (auto& [typeHash, pSparseSet] : mpComponentArrayPairs)
            {
                pSparseSet->remove(entity);
            }

            mFreedEntities.emplace(entity | (1ull << kEntitySlotShiftWidth));
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
        Component& get(Entity entity)
        {
            return std::any_cast<SparseSet<Component>&>(mComponentArrayMap[TypeHashGenerator::id<Component>()])[entity];
        }

        template<typename Component1, typename Component2, typename... OtherComponents>
        std::tuple<Component1, Component2, OtherComponents...> get(Entity entity)
        {
            // TODO
            assert(!"TODO");
        }

        template<typename T>
        const std::vector<Entity>& getEntities()
        {
            return std::any_cast<SparseSet<T>&>(mComponentArrayMap[TypeHashGenerator::id<T>()]).getDenseEntities();
        }

        std::size_t activeEntityNum() const
        {
            return mNextEntity - mFreedEntities.size();
        }

        template<typename T>
        std::size_t size()
        {
            return std::any_cast<SparseSet<T>&>(mComponentArrayMap[TypeHashGenerator::id<T>()]).size();
        }

        template<typename T, typename... Args>
        void add(Entity entity, Args... args)
        {
#ifndef NDEBUG
            const TypeHash hash = TypeHashGenerator::id<T>();
#else
            constexpr TypeHash hash = TypeHashGenerator::id<T>();
#endif

            auto&& itr = mComponentArrayMap.find(hash);
            if (itr == mComponentArrayMap.end())
            {
                itr = mComponentArrayMap.emplace(hash, SparseSet<T>()).first;
                mpComponentArrayPairs.emplace_back(hash, std::any_cast<SparseSet<T>>(&itr->second));
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
            ss.remove(entity);
        }

        template<typename T, typename Func, typename Traits::IsEligibleEachFunc<Func, T>* = nullptr >
        void each(Func func)
        {
            auto&& itr = mComponentArrayMap.find(TypeHashGenerator::id<T>());
            assert(itr != mComponentArrayMap.end());
            auto& ss = std::any_cast<SparseSet<T>&>(itr->second);
            ss.each(func);
        }

        template<typename T, typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, T>* = nullptr >
        void each(Func func)
        {
            auto&& itr = mComponentArrayMap.find(TypeHashGenerator::id<T>());
            assert(itr != mComponentArrayMap.end());
            auto& ss = std::any_cast<SparseSet<T>&>(itr->second);
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
            //assert(hasAllTypesCalled<Args...>() || !"component type which requested by view is not contained in this registry!");
            checkAndAddNewComponent<Args...>();
            return View<Args...>(std::any_cast<SparseSet<Args>&>(mComponentArrayMap[TypeHashGenerator::id<Args>()])...);
        }

        void dump()
        {
#ifndef NDEBUG
            for (const auto& pSS : mpComponentArrayPairs)
            {
                std::cerr << "type hash : " << pSS.first << "\n";
                pSS.second->dump();
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
        inline bool checkAndAddNewComponent()
        {
#ifndef NDEBUG
            const TypeHash hash = TypeHashGenerator::id<Head>();
#else
            constexpr TypeHash hash = TypeHashGenerator::id<Head>();
#endif

            if (!mComponentArrayMap.contains(hash))
            {
                auto&& itr = mComponentArrayMap.emplace(hash, SparseSet<Head>()).first;
                mpComponentArrayPairs.emplace_back(hash, std::any_cast<SparseSet<Head>>(&itr->second));
                return false;
            }

            if constexpr (sizeof...(Tail) > 0)
            {
                checkAndAddNewComponent<Tail...>();
            }

            return true;
        }


        Entity mNextEntity;
        std::queue<Entity> mFreedEntities;

        std::unordered_map<TypeHash, std::any> mComponentArrayMap;
        std::vector<std::pair<TypeHash, ISparseSet*>> mpComponentArrayPairs;
    };
}

#endif