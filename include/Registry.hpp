/*****************************************************************/ /**
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

#include <unordered_map>
#include <queue>
#include <cassert>

#ifndef NDEBUG
#include <string>
#include <iostream>
#include <sstream>
#endif

namespace ec2s
{
    /**
     * @brief  class that ties all components to Entity and executes System
     */
    class Registry
    {
    public:
        /** 
         * @brief  constructor
         *  
         */
        Registry()
            : mNextEntity(0)
        {
        }

        /** 
         * @brief  destructor
         *  
         */
        ~Registry()
        {
        }

        /** 
         * @brief create new entity
         * 
         * @tparam Args component types owned by the Entity to be created
         * @return created Entity
         */
        template <typename... Args>
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

        /** 
         * @brief  destroy specified Entity
         *  
         * @param entity Entity to be destroyed
         */
        void destroy(const Entity entity)
        {
            for (auto& [typeHash, pSparseSet] : mpComponentArrayPairs)
            {
                pSparseSet->remove(entity);
            }

            mFreedEntities.emplace(static_cast<Entity>(entity | (1ull << kEntitySlotShiftWidth)));
        }

        /** 
         * @brief  clear all entities
         *  
         */
        void clear()
        {
            for (auto& [typeHash, pSparseSet] : mpComponentArrayPairs)
            {
                pSparseSet->clear();
            }

            std::queue<Entity> empty;
            std::swap(mFreedEntities, empty);
        }

        /** 
         * @brief  obtains the reference of specified Component of the specified Entity
         *  
         * @param entity Entity to get Component
         * @return ntity to get Component
         */
        template <typename Component>
        Component& get(const Entity entity)
        {
            return mComponentArrayMap[TypeHasher::hash<Component>()].get<SparseSet<Component>>()[entity];
        }

        /** 
         * @brief  obtains all Entities with the specified Component
         *  
         * @return 
         */
        template <typename T>
        const std::vector<Entity>& getEntities()
        {
            return mComponentArrayMap[TypeHasher::hash<T>()].get<SparseSet<T>>().getDenseEntities();
        }

        /** 
         * @brief  get the number of Entities currently active (created and not destroyed)
         *  
         * @return the number of Entities currently active
         */
        std::size_t activeEntityNum() const
        {
            return static_cast<std::size_t>(mNextEntity) - mFreedEntities.size();
        }

        /** 
         * @brief  returns the size(num) of the specified Component
         *  
         * @tparam T component type
         * @return the size(num) of the specified Component
         */
        template <typename T>
        std::size_t size()
        {
            if (!mComponentArrayMap.contains(TypeHasher::hash<T>()))
            {
                return 0;
            }
            return mComponentArrayMap[TypeHasher::hash<T>()].get<SparseSet<T>>().size();
        }

        /** 
         * @brief  checks if the specified Entity has been included
         *  
         * @param entity entity to be checked 
         * @return whether the entity has been included
         */
        template <typename T>
        bool contains(const Entity entity)
        {
            return mComponentArrayMap.contains(TypeHasher::hash<T>()) && mComponentArrayMap[TypeHasher::hash<T>()].get<SparseSet<T>>().contains(entity);
        }

        /** 
         * @brief  add the specified Component to the specified Entity
         *  
         * @tparam T component type
         * @tparam Args types of arguments forwarded to the Component constructor
         * @param entity Entity to add Component
         * @param ...args arguments forwarded to the Component constructor
         */
        template <typename T, typename... Args>
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

        /** 
         * @brief  removes a component of a specified type from a specified Entity
         *  
         * @tparam T component type
         * @param entity Entity to remove Component
         */
        template <typename T>
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

        /** 
         * @brief  execute the specified function on all components of the specified type (system in ECS)
         *  
         * @tparam T component type
         * @tparam Func function type
         * @tparam IsEligibleEachFunc Trait to determine if the Func type is correctly callable for the specified Component type
         * @param func system function
         */
        template <typename T, typename Func, typename Traits::IsEligibleEachFunc<Func, T>* = nullptr>
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

        /** 
         * @brief  system that takes Entity as its first argument
         *  
         * @tparam Func function type
         * @tparam IsEligibleEachFunc Trait to determine if the Func type and Entity is correctly callable for the specified Component type
         * @param func system function
         */
        template <typename T, typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, T>* = nullptr>
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

        /** 
         * @brief  called when an illegal Func type is passed to each()
         *  
         * @param func
         */
        template <typename T, typename Func, typename std::enable_if_t<!std::is_invocable_v<Func, T&> && !std::is_invocable_v<Func, Entity, T&>>>
        void each(Func func)
        {
            static_assert(std::is_invocable_v<Func, T&> || std::is_invocable_v<Func, Entity, T&>, "ineligible Func type!");
        }

        /** 
         * @brief   Simultaneous each for multiple Component types 
         * @detail  create View internally
         * 
         * @param func system function
         */
        template <typename Component1, typename Component2, typename... OtherComponents, typename Func>
        void each(Func func)
        {
            view<Component1, Component2, OtherComponents...>().each(func);
        }

        /** 
         * @brief  create a View from specified component types
         *  
         * @tparam Args component types
         * @return created View
         */
        template <typename... Args>
        View<Args...> view()
        {
            checkAndAddNewComponent<Args...>();
            return View<Args...>(mComponentArrayMap[TypeHasher::hash<Args>()].get<SparseSet<Args>>()...);
        }

        template<typename T>
        SparseSet<T>& getSparseSet()
        {
            return mComponentArrayMap[TypeHasher::hash<T>()].get<SparseSet<T>>();
        }

        /** 
         * @brief  dump whole SparseSets internals
         * @return dumped result string
         */
        std::string dump()
        {
#ifndef NDEBUG
            std::ostringstream oss;

            for (const auto& pSparseSet : mpComponentArrayPairs)
            {
                oss << "type hash : " << pSparseSet.first << "\n";
                oss << pSparseSet.second->dump();
                oss << "\n";
            }

            return oss.str();
#endif
            return "";
        }

    private:
        /** 
         * @brief internal implementation for expanding template arguments of create() 
         *  
         * @param entity
         */
        template <typename Head, typename... Tail>
        void createImpl(Entity entity)
        {
            add<Head>(entity);

            if constexpr (sizeof...(Tail) > 0)
            {
                createImpl<Tail...>(entity);
            }
        }

        /** 
         * @brief  check if there is a SparseSet of the specified Component type and if not, add it
         *  
         */
        template <typename Head, typename... Tail>
        void checkAndAddNewComponent()
        {
#ifdef EC2S_CHECK_SYNONYM
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

        //! Entity to be created next
        Entity mNextEntity;
        //! destroyed Entity
        std::queue<Entity> mFreedEntities;

        //! Dummy type for calculating the size of SparseSet (meaningless)
        using Dummy_t = std::uint32_t;
        //! maps a SparseSet for each Component type to the type hash of the Component type
        std::unordered_map<TypeHash, StackAny<sizeof(SparseSet<Dummy_t>)>> mComponentArrayMap;
        //! pair of SparseSet and Component type hash for each Component type (same as mComponentArrayMap)
        std::vector<std::pair<TypeHash, ISparseSet*>> mpComponentArrayPairs;
    };
}  // namespace ec2s

#endif