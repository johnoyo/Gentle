#pragma once

#include "Core\Allocators.h"
#include "IComponentStorage.h"

namespace HBL2
{
	template<typename T, size_t N>
	class SmallComponentStorage : IComponentStorage
	{
		static_assert(N <= 64, "SmallComponentStorage holds maximum 64 components!");
	public:
		virtual void* Add(Entity e) override
		{
			// Add new component (default initialized)
			if (m_Size < N)
			{
				m_Entities.Add(e);
				m_Components.Add(T{});
				m_Mask.set(e);
				return &m_Components[m_Size++];
			}

			// Error - exceeded capacity
			HBL2_CORE_ASSERT(false, "SmallComponentStorage capacity exceeded");
			return nullptr;
		}

		virtual void Remove(Entity e) override
		{
			for (size_t i = 0; i < m_Size; ++i)
			{
				if (m_Entities[i] == e)
				{
					// Swap with last element and pop (avoid shifting elements)
					if (i < m_Size - 1)
					{
						m_Components[i] = std::move(m_Components[m_Size - 1]);
						m_Entities[i] = m_Entities[m_Size - 1];
					}

					m_Components.Pop();
					m_Entities.Pop();
					m_Mask.reset(e);
					m_Size--;
					return;
				}
			}
		}

		virtual void* Get(Entity e) override
		{
			for (size_t i = 0; i < m_Size; ++i)
			{
				if (m_Entities[i] == e)
				{
					return &m_Components[i];
				}
			}

			HBL2_CORE_ASSERT(false, "Entity not found in SmallComponentStorage");
			return nullptr;
		}

		virtual bool Has(Entity e) override
		{
			if (!m_Mask.test(e))
			{
				return false;
			}

			for (size_t i = 0; i < m_Size; ++i)
			{
				if (m_Entities[i] == e)
				{
					return true;
				}
			}

			return false;
		}

		virtual ComponentMaskAVX& Mask() const override
		{
			return const_cast<ComponentMaskAVX&>(m_Mask);
		}

		virtual const Span<const Entity> Indices() const override
		{
			return { m_Entities.Data(), m_Size };
		}

		virtual void Clear() override
		{
			m_Components.Clear();
			m_Entities.Clear();
			m_Mask.clear();
			m_Size = 0;
		}

		virtual void IterateRaw(TrampolineFunction<void, void*>& callback) const override
		{
			for (size_t i = 0; i < m_Size; ++i)
			{
				callback((void*)&m_Components[i]);
			}
		}

	private:
		DynamicArray<T, BinAllocator> m_Components = MakeDynamicArray<T>(&Allocator::Scene, N);
		DynamicArray<Entity, BinAllocator> m_Entities = MakeDynamicArray<Entity>(&Allocator::Scene, N);
		ComponentMaskAVX m_Mask;
		uint32_t m_Size;
	};
}