#pragma once

#include "IComponentStorage.h"

namespace HBL2
{
	template<typename T>
	class SingletonComponentStorage : IComponentStorage
	{
	public:
		virtual void* Add(Entity e) override
		{
			if (m_Entity == UINT32_MAX)
			{
				m_Entity = e;
				return &m_Component;
			}

			return nullptr;
		}

		virtual void Remove(Entity e) override
		{
			if (m_Entity == e)
			{
				m_Entity = UINT32_MAX;
			}
		}

		virtual void* Get(Entity e) override
		{
			if (m_Entity == e)
			{
				return &m_Component;
			}

			return nullptr;
		}

		virtual bool Has(Entity e) override
		{
			if (m_Entity == e)
			{
				return true;
			}
			return false;
		}

		virtual ComponentMaskAVX& Mask() const override
		{
			return const_cast<ComponentMaskAVX&>(m_Mask);
		}

		virtual const Span<const Entity> Indices() const override
		{
			return { &m_Entity, 1 };
		}

		virtual void Clear() override { m_Entity = UINT32_MAX; }

		virtual void IterateRaw(TrampolineFunction<void, void*>& callback) const override
		{
			callback((void*)&m_Component);
		}

	private:
		T m_Component;
		Entity m_Entity = UINT32_MAX;
		ComponentMaskAVX m_Mask;
	};
}