#pragma once

#include "ComponentArray.h"

namespace HBL2
{
	template<typename... Components>
	class AdvancedQuery
	{
	public:
		AdvancedQuery(ComponentArray<std::remove_const_t<Components>>&... arrays)
			: m_Arrays{ (ComponentArrayBase*)&arrays... }
		{
			// Compute joint mask by ANDing all masks once
			m_JointMask = m_Arrays[0]->Mask();
			for (size_t i = 1; i < sizeof...(Components); ++i)
			{
				m_JointMask &= m_Arrays[i]->Mask();
			}
		}

		void ForEach(std::function<void(Components&...)>&& func)
		{
			ForEachImpl(func, std::index_sequence_for<Components...>{});
		}

		void ForEach(std::function<void(Entity, Components&...)>&& func)
		{
			ForEachWithEntityImpl(func, std::index_sequence_for<Components...>{});
		}

		ComponentMask::Iterator begin() const { return m_JointMask.begin(); }
		ComponentMask::Iterator end() const { return m_JointMask.end(); }

	private:
		template<size_t... Indices>
		void ForEachImpl(std::function<void(Components&...)>& func, std::index_sequence<Indices...>)
		{
			for (Entity e : m_JointMask)
			{
				func((Components&)(((ComponentArray<std::remove_const_t<Components>>*)(m_Arrays[Indices]))->Get(e))...);
			}
		}

		// Helper method to expand the parameter pack with entity
		template<size_t... Indices>
		void ForEachWithEntityImpl(std::function<void(Entity, Components&...)>& func, std::index_sequence<Indices...>)
		{
			for (Entity e : m_JointMask)
			{
				func(e, (Components&)(((ComponentArray<std::remove_const_t<Components>>*)(m_Arrays[Indices]))->Get(e))...);
			}
		}

		template<typename... Cs>
		void FillDeps(SystemEntry& info) {
			((
				// compute the ComponentTypeID only once per type
				[&] {
					auto id = ComponentTypeID::Get<std::remove_const_t<Cs>>();
					if constexpr (std::is_const_v<Cs>)
					{
						info.ReadMask.set(id);
					}
					else
					{
						info.WriteMask.set(id);
					}
				}()
			), ...);
		}

	private:
		std::array<ComponentArrayBase*, sizeof...(Components)> m_Arrays;
		ComponentMask m_JointMask;
	};

	template<typename Component>
	class BasicQuery
	{
	public:
		BasicQuery(ComponentArray<std::remove_const_t<Component>>& array)
			: m_Array{ (ComponentArrayBase*)&array }
		{

		}

		void ForEach(std::function<void(Component&)>&& func)
		{
			ComponentArray<std::remove_const_t<Component>>& array = *(ComponentArray<std::remove_const_t<Component>>*)(m_Array);

			for (Component& c : array)
			{
				func(c);
			}
		}

		void ForEach(std::function<void(Entity, Component&)>&& func)
		{
			ComponentArray<std::remove_const_t<Component>>& array = *(ComponentArray<std::remove_const_t<Component>>*)(m_Array);

			uint32_t index = 0;
			for (Component& c : array)
			{
				func(array.GetEntity(index++), c);
			}
		}

	private:
		template<typename... Cs>
		void FillDeps(SystemEntry& info) {
			((
				// compute the ComponentTypeID only once per type
				[&] {
					constexpr auto id = ComponentTypeID::Get<std::remove_const_t<Cs>>();
					if constexpr (std::is_const_v<Cs>)
					{
						info.ReadMask.set(id);
					}
					else
					{
						info.WriteMask.set(id);
					}
				}()
			), ...);
		}

	private:
		ComponentArrayBase* m_Array = nullptr;
	};
}