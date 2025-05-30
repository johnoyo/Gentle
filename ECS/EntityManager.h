#pragma once

#include <vector>
#include <atomic>

namespace HBL2
{
    constexpr uint32_t MAX_COMPONENT_TYPES = 128;
    constexpr uint32_t MAX_ENTITIES = 262144;
    constexpr uint32_t MASK_WORDS = (MAX_ENTITIES + 63) / 64;
    using Entity = uint32_t;

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

    class EntityManager
    {
    public:
        Entity Create()
        {
            if (!m_FreeList.empty())
            {
                Entity e = m_FreeList.back();
                m_FreeList.pop_back();
                return e;
            }
            return m_NextId++;
        }

        void Destroy(Entity e)
        {
            m_FreeList.push_back(e);
        }

        void Clear()
        {
            m_FreeList.clear();
            m_NextId.store(0);
        }

    private:
        std::vector<Entity> m_FreeList;
        std::atomic<uint32_t> m_NextId{ 0 };
    };
}