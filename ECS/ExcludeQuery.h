#pragma once

#include "IComponentStorage.h"

#include <utility>

namespace HBL2
{
	template<typename... T>
	struct IncludeWrapper {};

	template<typename... T>
	struct ExcludeWrapper {};

	template<typename Includes, typename Excludes>
	class ExcludeQuery;

	template<typename... IncludeTypes, typename... ExcludeTypes>
	class ExcludeQuery<IncludeWrapper<IncludeTypes...>, ExcludeWrapper<ExcludeTypes...>>
	{
	public:
		ExcludeQuery(ComponentMaskAVX& queryMask, uint32_t entityCount, Span<IComponentStorage*> includes, StaticArray<IComponentStorage*, sizeof...(ExcludeTypes)> excludes)
			: m_JointMask(queryMask), m_EntityCount(entityCount), m_Include(includes), m_Exclude(excludes)
		{
		}

		ExcludeQuery& ForEach(std::function<void(IncludeTypes&...)>&& func)
		{
			m_Function = std::move(func);
			return *this;
		}

		void Run()
		{
			ForEachRunImpl(std::index_sequence_for<IncludeTypes...>{});
		}

		void Schedule()
		{

		}

		void Dispatch()
		{
            ForEachDispatchImpl(std::index_sequence_for<IncludeTypes...>{});
		}

	private:
        template<size_t... Indices>
        void ForEachRunImpl(std::index_sequence<Indices...>)
        {
            constexpr size_t Arity = sizeof...(IncludeTypes);

            // Gather counts for all include components
            std::array<size_t, Arity> counts = { m_Include[Indices]->Indices().Size()... };
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

            bool lowEntityCount = (m_EntityCount <= 1000);
            bool mediumEntityCountLowDensity = (m_EntityCount > 1000 && m_EntityCount <= 10000 && minCount <= 1500);
            bool mediumHighEntityCountLowDensity = (m_EntityCount > 10000 && m_EntityCount <= 20000 && minCount <= 3000);

            if (lowEntityCount || mediumEntityCountLowDensity || mediumHighEntityCountLowDensity)
            {
                for (Entity e : m_Include[minIdx]->Indices())
                {
                    bool ok = (m_Include[Indices]->Has(e) && ...);
                    if (!ok) continue;

                    bool exclude = false;
                    for (size_t i = 0; i < sizeof...(ExcludeTypes); ++i)
                    {
                        if (m_Exclude[i]->Has(e))
                        {
                            exclude = true;
                            break;
                        }
                    }
                    if (exclude) continue;

                    m_Function((IncludeTypes&)*((IncludeTypes*)(m_Include[Indices]->Get(e)))...);
                }
            }
            else
            {
                // Build ANDed mask
                m_JointMask = m_Include[0]->Mask();
                for (size_t i = 1; i < sizeof...(IncludeTypes); ++i)
                {
                    m_JointMask &= m_Include[i]->Mask();
                }

                // Exclude masks
                for (size_t i = 0; i < sizeof...(ExcludeTypes); ++i)
                {
                    m_JointMask -= m_Exclude[i]->Mask();
                }

                for (Entity e : m_JointMask)
                {
                    m_Function((IncludeTypes&)*((IncludeTypes*)(m_Include[Indices]->Get(e)))...);
                }
            }
        }

        template<size_t... Indices>
        void ForEachDispatchImpl(std::index_sequence<Indices...>)
        {
            // Build ANDed mask
            m_JointMask = m_Include[0]->Mask();
            for (size_t i = 1; i < sizeof...(IncludeTypes); ++i)
            {
                m_JointMask &= m_Include[i]->Mask();
            }

            // Exclude masks
            for (size_t i = 0; i < sizeof...(ExcludeTypes); ++i)
            {
                m_JointMask -= m_Exclude[i]->Mask();
            }

            // Check entity count to decide execution strategy
            uint32_t entityCount = m_JointMask.count();

            JobContext ctx;

            // Use the specialized ECS dispatch
            JobSystem::Get().DispatchQuery<IncludeTypes...>(
                ctx,
                m_JointMask,
                m_Include,
                std::max(32u, entityCount / (JobSystem::Get().GetThreadCount() * 4)), // Dynamic group size
                m_Function,
                std::make_index_sequence<sizeof...(IncludeTypes)>{}
            );

            // Wait for completion
            JobSystem::Get().Wait(ctx);
        }

	private:
		Span<IComponentStorage*> m_Include;
		StaticArray<IComponentStorage*, sizeof...(ExcludeTypes)> m_Exclude;
		std::function<void(IncludeTypes&...)> m_Function;
		ComponentMaskAVX& m_JointMask;
		uint32_t m_EntityCount;
	};
}