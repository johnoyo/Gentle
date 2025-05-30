#pragma once

#include "ComponentMask.h"
#include "SparseFlatBitmap3L.h"

#include <cstdint>
#include <vector>
#include <cassert>

namespace HBL2
{
    constexpr uint32_t MAX_COMPONENT_TYPES = 64;
    using ComponentMask = SparseFlatBitmap3L;

    struct ComponentTypeID
    {
        template<typename T>
        static uint8_t Get()
        {
            static const uint8_t id = s_Counter++;
            return id;
        }

        static uint8_t GetCount()
        {
            return s_Counter;
        }

    private:
        static inline uint8_t s_Counter = 0;
    };

    struct ComponentArrayBase
    {
        virtual ~ComponentArrayBase() = default;
        virtual ComponentMask& Mask() = 0;
    };

    template<typename T>
    class ComponentArray : ComponentArrayBase
    {
    public:
        ComponentArray() { m_Reverse.resize(MAX_ENTITIES, uint32_t(-1)); }

        void Add(Entity e, const T& comp)
        {
            uint32_t idx = m_Packed.size();
            m_Packed.push_back(comp);
            m_Indices.push_back(e);
            m_Reverse[e] = idx;
            m_Mask.set(e);
        }

        void Remove(Entity e)
        {
            uint32_t idx = m_Reverse[e];
            uint32_t last = m_Packed.size() - 1;

            // swap‑remove
            std::swap(m_Packed[idx], m_Packed[last]);
            std::swap(m_Indices[idx], m_Indices[last]);
            m_Reverse[m_Indices[idx]] = idx;

            m_Packed.pop_back();
            m_Indices.pop_back();
            m_Mask.reset(e);
        }

        T& Get(Entity e)
        {
            return m_Packed[m_Reverse[e]];
        }

        bool Has(Entity e) const { return m_Mask.test(e); }

        ComponentMask& Mask() override
        {
            return m_Mask;
        }

        std::vector<T>::iterator begin() { return m_Packed.begin(); }
        std::vector<T>::iterator end() { return m_Packed.end(); }

    private:
        Entity GetEntity(uint32_t idx)
        {
            return m_Indices[idx];
        }

    private:
        std::vector<T> m_Packed;
        std::vector<Entity> m_Indices;
        std::vector<uint32_t> m_Reverse;
        ComponentMask m_Mask;

        friend class Registry;
    };
}