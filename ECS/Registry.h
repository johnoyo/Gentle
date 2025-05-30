#pragma once

#include "EntityManager.h"
#include "IComponentStorage.h"
#include "SparseComponentStorage.h"

#include "ViewQuery.h"
#include "FilterQuery.h"

namespace HBL2
{
    class Registry
    {
    public:
        Entity CreateEntity()
        {
            m_EntityCount++;
            return m_Entities.Create();
        }
        void DestroyEntity(Entity e)
        {
            m_EntityCount--;
            m_Entities.Destroy(e);

            for (size_t i = 0; i < ComponentTypeID::GetCount(); ++i)
            {
                m_Storages[i]->Remove(e);
            }
        }
        uint32_t GetEntityCount() const { return m_EntityCount; }

        template<typename T, typename TStorage>
        void SetStorageType()
        {
            uint8_t id = ComponentTypeID::Get<T>();

            if (m_Storages[id])
            {
                m_Storages[id]->Clear();
            }

            m_Storages[id] = (IComponentStorage*)new TStorage();
        }

        template<typename T>
        T& AddComponent(Entity e, T&& comp = {})
        {
            IComponentStorage* arr = EnsureArray<T>();
            void* ptr = arr->Add(e);
            HBL2_CORE_ASSERT(ptr != nullptr, "Error while adding component!");
            return *new(ptr) T(std::forward<T>(comp));
        }

        template<typename T, typename... Args>
        T& EmplaceComponent(Entity e, Args&&... args)
        {
            IComponentStorage* arr = EnsureArray<T>();
            void* mem = arr->Add(e);
            HBL2_CORE_ASSERT(mem != nullptr, "Error while emplacing component!");
            return *(new(mem) T(std::forward<Args>(args)...));
        }

        template<typename T>
        T& GetComponent(Entity e)
        {
            IComponentStorage* arr = EnsureArray<T>();
            return *(T*)arr->Get(e);
        }

        template<typename T>
        bool HasComponent(Entity e)
        {
            IComponentStorage* arr = EnsureArray<T>();
            return arr->Has(e);
        }

        template<typename T>
        void RemoveComponent(Entity e)
        {
            IComponentStorage* arr = EnsureArray<T>();

            if (!arr->Has(e))
            {
                return;
            }

            arr->Remove(e);
        }

        template<typename Component>
        ViewQuery<Component> Filter()
        {
            return ViewQuery<Component>(EnsureArray<std::remove_const_t<Component>>());
        }

        template<typename... Components> requires (sizeof...(Components) > 1)
        FilterQuery<Components...> Filter()
        {
            return FilterQuery<Components...>({ m_Storages, MAX_COMPONENT_TYPES }, m_EntityCount, { EnsureArray<std::remove_const_t<Components>>()... });
        }

        void Clear()
        {
            m_Entities.Clear();
            m_EntityCount = 0;

            for (int i = 0; i < MAX_COMPONENT_TYPES; i++)
            {
                if (m_Storages[i])
                {
                    m_Storages[i]->Clear();
                }
            }
        }

    private:
        template<typename T>
        IComponentStorage* EnsureArray()
        {
            uint8_t id = ComponentTypeID::Get<T>();
            if (!m_Storages[id])
            {
                m_Storages[id] = (IComponentStorage*)new SparseComponentStorage<T>();
            }
            return m_Storages[id];
        }

        EntityManager m_Entities;
        uint32_t m_EntityCount = 0;
        IComponentStorage* m_Storages[MAX_COMPONENT_TYPES] = { nullptr };
    };
}