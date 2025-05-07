#pragma once

#include <vector>
#include <atomic>

namespace HBL2
{
    constexpr uint32_t MAX_ENTITIES = 262144;
    constexpr uint32_t MASK_WORDS = (MAX_ENTITIES + 63) / 64;
    using Entity = uint32_t;

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

    private:
        std::vector<Entity> m_FreeList;
        std::atomic<uint32_t> m_NextId{ 0 };
    };
}