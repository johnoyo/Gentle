#pragma once

#include "EntityManager.h"
#include "ComponentArray.h"
#include "Sceduler.h"
#include "Query.h"

#include <typeinfo>

namespace HBL2
{
    class Registry
    {
    public:
        ~Registry()
        {
            for (size_t i = 0; i < ComponentTypeID::GetCount(); ++i)
            {
                delete m_Arrays[i];
            }
        }

        // Create / destroy entities
        Entity CreateEntity()
        {
            m_EntityCount++;
            return m_Entities.Create();
        }
        void DestroyEntity(Entity e)
        {
            m_EntityCount--;
            m_Entities.Destroy(e);
            // TODO: optionally remove from all arrays:
            for (size_t i = 0; i < ComponentTypeID::GetCount(); ++i)
            {
                m_Arrays[i]->Mask().reset(e);
            }
        }
        uint32_t GetEntityCount() const { return m_EntityCount; }

        // Add / remove by typeID
        template<typename T>
        void AddComponent(Entity e, const T& comp)
        {
            ComponentArray<T>& arr = EnsureArray<T>();
            arr.Add(e, comp);
        }
        template<typename T>
        bool HasComponent(Entity e)
        {
            ComponentArray<T>& arr = EnsureArray<T>();
            return arr.Has(e);
        }
        template<typename T>
        void RemoveComponent(Entity e)
        {
            ComponentArray<T>& arr = EnsureArray<T>();

            if (!arr.Has(e))
            {
                return;
            }

            arr.Remove(e);
        }
        
        template<typename... Components>
        AdvancedQuery<Components...> Group()
        {
            return AdvancedQuery<Components...>(EnsureArray<std::remove_const_t<Components>>()...);
        }

        template<typename Component>
        BasicQuery<Component> View()
        {
            return BasicQuery<Component>(EnsureArray<std::remove_const_t<Component>>());
        }

        template<typename A>
        void Run(std::function<void(A&)> func)
        {
            ComponentArray<std::remove_const_t<A>>& arrA = EnsureArray<std::remove_const_t<A>>();

            for (A& a : arrA)
            {
                func(a);
            }
        }

        template<typename A>
        void Schedule(std::function<void(A&)> func)
        {
            SystemEntry entry;
            FillDeps<A>(entry);

            entry.Task = [this, func]()
            {
                this->Run<A>(func);
            };

            m_Sceduler.Register(entry);
        }

        template<typename A>
        void Run(std::function<void(Entity e, A&)> func)
        {
            ComponentArray<std::remove_const_t<A>>& arrA = EnsureArray<std::remove_const_t<A>>();
            uint32_t index = 0;
            for (A& a : arrA)
            {
                func(arrA.GetEntity(index++), a);
            }
        }

        template<typename A>
        void Schedule(std::function<void(Entity e, A&)> func)
        {
            SystemEntry entry;
            FillDeps<A>(entry);

            entry.Task = [this, func]()
            {
                this->Run<A>(func);
            };

            m_Sceduler.Register(entry);
        }

        template<typename A, typename B>
        void Run(std::function<void(A&, B&)> func)
        {
            ComponentArray<std::remove_const_t<A>>& arrA = EnsureArray<std::remove_const_t<A>>();
            ComponentArray<std::remove_const_t<B>>& arrB = EnsureArray<std::remove_const_t<B>>();

            // SIMD‐optimized AND
            ComponentMask joint = arrA.Mask();    // copy
            joint &= arrB.Mask();

            for (Entity e : joint)
            {
                func((A&)arrA.Get(e), (B&)arrB.Get(e));
            }
        }

        template<typename A, typename B>
        void Schedule(std::function<void(A&, B&)> func)
        {
            SystemEntry entry;
            FillDeps<A>(entry);
            FillDeps<B>(entry);

            entry.Task = [this, func]()
            {
                this->Run<A, B>(func);
            };

            m_Sceduler.Register(entry);
        }

        template<typename A, typename B>
        void Run(std::function<void(Entity e, A&, B&)> func)
        {
            ComponentArray<std::remove_const_t<A>>& arrA = EnsureArray<std::remove_const_t<A>>();
            ComponentArray<std::remove_const_t<B>>& arrB = EnsureArray<std::remove_const_t<B>>();

            // SIMD‐optimized AND
            ComponentMask joint = arrA.Mask();    // copy
            joint &= arrB.Mask();

            for (Entity e : joint)
            {
                func(e, arrA.Get(e), arrB.Get(e));
            }
        }

        template<typename A, typename B>
        void Schedule(std::function<void(Entity e, A&, B&)> func)
        {
            SystemEntry entry;
            FillDeps<A>(entry);
            FillDeps<B>(entry);

            entry.Task = [this, func]()
            {
                this->Run<A, B>(func);
            };

            m_Sceduler.Register(entry);
        }

        template<typename A, typename B, typename C>
        void Run(std::function<void(A&, B&, C&)> func)
        {
            ComponentArray<std::remove_const_t<A>>& arrA = EnsureArray<std::remove_const_t<A>>();
            ComponentArray<std::remove_const_t<B>>& arrB = EnsureArray<std::remove_const_t<B>>();
            ComponentArray<std::remove_const_t<C>>& arrC = EnsureArray<std::remove_const_t<C>>();

            // SIMD‐optimized AND
            ComponentMask joint = arrA.Mask();    // copy
            joint &= arrB.Mask();
            joint &= arrC.Mask();

            for (Entity e : joint)
            {
                func(arrA.Get(e), arrB.Get(e), arrC.Get(e));
            }
        }

        template<typename A, typename B, typename C>
        void Run(std::function<void(Entity e, A&, B&, C&)> func)
        {
            ComponentArray<std::remove_const_t<A>>& arrA = EnsureArray<std::remove_const_t<A>>();
            ComponentArray<std::remove_const_t<B>>& arrB = EnsureArray<std::remove_const_t<B>>();
            ComponentArray<std::remove_const_t<C>>& arrC = EnsureArray<std::remove_const_t<C>>();

            // SIMD‐optimized AND
            ComponentMask joint = arrA.Mask();    // copy
            joint &= arrB.Mask();
            joint &= arrC.Mask();

            for (Entity e : joint)
            {
                func(e, arrA.Get(e), arrB.Get(e), arrC.Get(e));
            }
        }

        void ExecuteScheduledSystems()
        {
            m_Sceduler.RunAll();
        }

    private:
        template<typename T>
        ComponentArray<T>& EnsureArray()
        {
            uint8_t id = ComponentTypeID::Get<T>();
            if (!m_Arrays[id])
            {
                m_Arrays[id] = (ComponentArrayBase*)new ComponentArray<T>();
            }
            return *(ComponentArray<T>*)(m_Arrays[id]);
        }

        template<typename C>
        void FillDeps(SystemEntry& info)
        {
            uint8_t id = ComponentTypeID::Get<std::remove_const_t<C>>();
            if constexpr (std::is_const_v<C>)
            {
                info.ReadMask.set(id);
            }
            else
            {
                info.WriteMask.set(id);
            }
        }

    private:
        Sceduler m_Sceduler;
        EntityManager m_Entities;
        uint32_t m_EntityCount = 0;
        ComponentArrayBase* m_Arrays[MAX_COMPONENT_TYPES] = { nullptr };
    };
}