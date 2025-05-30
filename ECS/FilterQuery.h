#pragma once

#include "IComponentStorage.h"
#include "ExcludeQuery.h"

namespace HBL2
{
    template<typename... Components>
    class FilterQuery
    {
    public:
        FilterQuery(Span<IComponentStorage*> allStorages, uint32_t entityCount, StaticArray<IComponentStorage*, sizeof...(Components)> storages)
            :m_AllStorages(allStorages), m_EntityCount(entityCount), m_Storages(storages)
        {
        }

        template<typename... ExcludeTypes>
        ExcludeQuery<IncludeWrapper<Components...>, ExcludeWrapper<ExcludeTypes...>> Exclude()
        {
            return ExcludeQuery<IncludeWrapper<Components...>, ExcludeWrapper<ExcludeTypes...>>
                (m_JointMask, m_EntityCount, m_Storages, { EnsureArray<std::remove_const_t<ExcludeTypes>>()...});
        }

        FilterQuery& ForEach(std::function<void(Components&...)>&& func)
        {
            m_Function = std::move(func);
            return *this;
        }

        void Run()
        {
            ForEachRunImpl(std::index_sequence_for<Components...>{});
        }

        void Schedule()
        {
        }

        void Dispatch()
        {
            ForEachDispatchImpl(std::index_sequence_for<Components...>{});
        }

    private:
        template<size_t... Indices>
        void ForEachRunImpl(std::index_sequence<Indices...>)
        {
            constexpr size_t Arity = sizeof...(Components);

            // Find min count
            std::array<size_t, Arity> counts = { m_Storages[Indices]->Indices().Size()... };
            size_t minCount = counts[0];
            size_t minIdx = 0;
            for (size_t i = 1; i < Arity; ++i)
            {
                if (counts[i] < minCount)
                {
                    minCount = counts[i];
                    minIdx = i;
                }
            }

            bool lowEntiyCount = (m_EntityCount <= 1000);
            bool mediumEntiyCountLowDensity = (m_EntityCount > 1000 && m_EntityCount <= 10000 && minCount <= 1500);
            bool mediumHightEntiyCountLowDensity = (m_EntityCount > 10000 && m_EntityCount <= 20000 && minCount <= 3000);

            if (lowEntiyCount || mediumEntiyCountLowDensity || mediumHightEntiyCountLowDensity)// || hightEntiyCountLowDensity)   /// <<< low‐density branch
            {
                for (Entity e : m_Storages[minIdx]->Indices())
                {
                    // test each other array via Has()
                    bool ok = (m_Storages[Indices]->Has(e) && ...);
                    if (!ok) continue;

                    // unpack components in index order
                    m_Function((Components&)*((Components*)(m_Storages[Indices]->Get(e)))...);
                }
            }
            else
            {
                // Compute joint mask by ANDing all masks once
                m_JointMask = m_Storages[0]->Mask();
                for (size_t i = 1; i < sizeof...(Components); ++i)
                {
                    m_JointMask &= m_Storages[i]->Mask();
                }

                for (Entity e : m_JointMask)
                {
                    m_Function((Components&)*((Components*)(m_Storages[Indices]->Get(e)))...);
                }
            }
        }

        template<size_t... Indices>
        void ForEachDispatchImpl(std::index_sequence<Indices...>)
        {
            // Compute joint mask by ANDing all masks once
            m_JointMask = m_Storages[0]->Mask();
            for (size_t i = 1; i < sizeof...(Components); ++i)
            {
                m_JointMask &= m_Storages[i]->Mask();
            }

            // Check entity count to decide execution strategy
            uint32_t entityCount = m_JointMask.count();

            JobContext ctx;

            // Use the specialized ECS dispatch
            JobSystem::Get().DispatchQuery<Components...>(
                ctx,
                m_JointMask,
                m_Storages,
                std::max(32u, entityCount / (JobSystem::Get().GetThreadCount() * 4)), // Dynamic group size
                m_Function,
                std::make_index_sequence<sizeof...(Components)>{}
            );

            // Wait for completion
            JobSystem::Get().Wait(ctx);
        }

        template<typename T>
        IComponentStorage* EnsureArray()
        {
            uint8_t id = ComponentTypeID::Get<T>();
            if (!m_AllStorages[id])
            {
                m_AllStorages[id] = (IComponentStorage*)new SparseComponentStorage<T>();
            }
            return m_AllStorages[id];
        }

    private:
        StaticArray<IComponentStorage*, sizeof...(Components)> m_Storages;
        Span<IComponentStorage*> m_AllStorages;
        ComponentMaskAVX m_JointMask;
        std::function<void(Components&...)> m_Function;
        uint32_t m_EntityCount = 0;
    };
}