/*****************************************************************/ /**
 * @file   Registry.hpp
 * @brief  header file of Registry class
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_REGISTRY_HPP_
#define EC2S_REGISTRY_HPP_

#include "Condition.hpp"
#include "Entity.hpp"
#include "Group.hpp"
#include "StackAny.hpp"
#include "SparseSet.hpp"
#include "View.hpp"

#include <cassert>
#include <queue>
#include <unordered_map>

#ifndef NDEBUG
#include <sstream>
#endif

namespace ec2s
{
    template <typename T>
    class Group;

    /**
     * @brief  class that ties all components to Entity and executes System
     */
    class Registry
    {
        template <typename T>
        friend class Group;

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
                rtn = spinEntitySlot(mFreedEntities.front());
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

            mFreedEntities.emplace(entity);
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
         * @return entity to get Component
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
        template <typename T, typename ComponentAllocator = std::allocator<T>, typename... Args>
            requires Concepts::AllocatorConcept<ComponentAllocator>
        T& add(const Entity entity, Args... args)
        {
#ifdef EC2S_CHECK_SYNONYM
            const TypeHash hash = TypeHasher::hash<T>();
#else
            constexpr TypeHash hash = TypeHasher::hash<T>();
#endif

            auto&& itr = mComponentArrayMap.find(hash);
            if (itr == mComponentArrayMap.end())
            {
                itr = mComponentArrayMap.emplace(hash, SparseSet<T, ComponentAllocator>()).first;
                mpComponentArrayPairs.emplace_back(hash, &(itr->second.get<SparseSet<T>>()));
            }

            auto& ss = itr->second.get<SparseSet<T>>();

            ss.emplace(entity, args...);

            // TODO: group check and update
            if (auto&& groupItr = mpGroupMap.find(hash); groupItr != mpGroupMap.end())
            {
                groupItr->second->notifyAddedEntity(entity);
            }

            return ss[entity];
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
            constexpr auto hash = TypeHasher::hash<T>();

            auto&& itr = mComponentArrayMap.find(hash);
            if (itr == mComponentArrayMap.end())
            {
                return;
            }

            auto& ss = itr->second.get<SparseSet<T>>();

            // TODO: group check and update
            if (auto&& groupItr = mpGroupMap.find(hash); groupItr != mpGroupMap.end())
            {
                groupItr->second->notifyRemovedEntity(entity);
            }

            ss.remove(entity);
        }

        template <typename T, typename Predicate>
            requires Concepts::Predicate<Predicate, T>
        bool sort(Predicate predicate)
        {
            constexpr auto hash = TypeHasher::hash<T>();

            // check group
            if (mpGroupMap.contains(hash))
            {
                // cannot sort Component included in Group
                return false;
            }

            auto&& itr = mComponentArrayMap.find(hash);
            if (itr == mComponentArrayMap.end())
            {
                return false;
            }

            auto& ss = itr->second.get<SparseSet<T>>();
            ss.sort(predicate);
            return true;
        }

        /** 
         * @brief  execute the specified function on all components of the specified type (system in ECS)
         *  
         * @tparam T component type
         * @tparam Func function type
         * @param func system function
         */
        template <typename T, typename Func>
            requires Concepts::Invocable<Func, T>
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
         * @param func system function
         */
        template <typename T, typename Func>
            requires Concepts::InvocableWithEntity<Func, T>
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
        auto view() -> auto
        {
            checkAndAddNewComponent<Args...>();

            auto include = std::tuple_cat(TupleType<Args>{}...);
            auto exclude = std::tuple_cat(ExcludeTupleType<Args>{}...);

            static_assert(std::tuple_size_v<decltype(include)> > 0, "View must include at least one Component type!");

            iterateTupleAndAssignSparseSet(include);
            iterateTupleAndAssignSparseSet(exclude);

            return View<decltype(include), decltype(exclude)>(include, exclude);
        }

        template <typename... Args>
        auto group() -> std::optional<Group<decltype(std::tuple_cat(TupleType<Args>{}...))>>
        {
            checkAndAddNewComponent<Args...>();

            auto include = std::tuple_cat(TupleType<Args>{}...);
            static_assert(std::tuple_size_v<decltype(include)> >= 2, "Group must include at least two Component type!");

            if (!checkGroup<Args...>())
            {
                // Group with the same Component types already exists
                return std::nullopt;
            }

            iterateTupleAndAssignSparseSet(include);
            // TODO: Exclusion
            // TODO: Handling events for adding/removing components of the same type

            return std::optional<Group<decltype(include)>>{ std::in_place, *this, include };
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
            // skip Exclude declaration
            if constexpr (ExcludeType<Head>)
            {
                if constexpr (sizeof...(Tail) > 0)
                {
                    checkAndAddNewComponent<Tail...>();
                }

                return;
            }

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

        template <typename Head, typename... Tail>
        bool checkGroup()
        {
#ifdef EC2S_CHECK_SYNONYM
            const TypeHash hash = TypeHasher::hash<Head>();
#else
            constexpr TypeHash hash = TypeHasher::hash<Head>();
#endif
            // skip Exclude declaration
            if constexpr (ExcludeType<Head>)
            {
                if constexpr (sizeof...(Tail) > 0)
                {
                    return checkGroup<Tail...>();
                }

                return true;
            }

            if (mpGroupMap.contains(hash))
            {
                return false;
            }

            if constexpr (sizeof...(Tail) > 0)
            {
                return checkGroup<Tail...>();
            }

            return true;
        }

        void registerGroup(const TypeHash hash, IGroup* pGroup)
        {
            mpGroupMap.emplace(hash, pGroup);
        }

        /** 
         * @brief iterate through the tuple and assign SparseSet pointers to each element
         * @warn if specified Component type is not registered, nullptr will be assigned
         *  
         */
        template <size_t N = 0, typename TupleType>
        void iterateTupleAndAssignSparseSet(TupleType& t)
        {
            if constexpr (N < std::tuple_size<TupleType>::value)
            {
                using ComponentType     = std::remove_pointer_t<std::tuple_element_t<N, TupleType>>::ComponentType;
                constexpr TypeHash hash = TypeHasher::hash<ComponentType>();
                if (mComponentArrayMap.contains(hash))
                {
                    std::get<N>(t) = &(mComponentArrayMap[hash].get<SparseSet<ComponentType>>());
                }
                else
                {
                    std::get<N>(t) = nullptr;
                }

                iterateTupleAndAssignSparseSet<N + 1>(t);
            }
        }

        /** 
         * @brief  register Group to registry
         *  
         */
        template <typename TupleType, size_t N = 0>
        void iterateTupleAndRegisterGroup(IGroup* pIGroup)
        {
            if constexpr (N < std::tuple_size<TupleType>::value)
            {
                using ComponentType     = std::remove_pointer_t<std::tuple_element_t<N, TupleType>>::ComponentType;
                constexpr TypeHash hash = TypeHasher::hash<ComponentType>();
                mpGroupMap.emplace(hash, pIGroup);
                iterateTupleAndRegisterGroup<TupleType, N + 1>(pIGroup);
            }
        }

        /** 
         * @brief  remove Group from registry
         *  
         */
        template <typename TupleType, size_t N = 0>
        void iterateTupleAndRemoveGroup()
        {
            if constexpr (N < std::tuple_size<TupleType>::value)
            {
                using ComponentType     = std::remove_pointer_t<std::tuple_element_t<N, TupleType>>::ComponentType;
                constexpr TypeHash hash = TypeHasher::hash<ComponentType>();
                mpGroupMap.erase(hash);
                iterateTupleAndRemoveGroup<TupleType, N + 1>();
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
        //! maps a Group for each Component type tuple to the type hash of the Component type
        std::unordered_map<TypeHash, IGroup*> mpGroupMap;
        //! pair of SparseSet and Component type hash for each Component type (same as mComponentArrayMap)
        std::vector<std::pair<TypeHash, ISparseSet*>> mpComponentArrayPairs;
    };
}  // namespace ec2s

#endif