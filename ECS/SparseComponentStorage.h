#pragma once

#include "IComponentStorage.h"

namespace HBL2
{
    static constexpr size_t PAGE_SIZE = 2048; // 1KB pages (adjust based on benchmarking)
    static constexpr size_t PAGE_MASK = PAGE_SIZE - 1; // 0x3FF for masking
    static constexpr size_t PAGE_SHIFT = 11; // log2(1024) = 10 for shifting
    static constexpr Entity NO_ENTITY = UINT32_MAX;
    static constexpr uint32_t INDEX_BITS = 20;
    static constexpr uint32_t INDEX_MASK = (1u << INDEX_BITS) - 1; // 0xFFFF
    static constexpr uint32_t VERSION_SHIFT = INDEX_BITS;

    template<typename T>
    class SparseComponentStorage : IComponentStorage
    {
    public:
        SparseComponentStorage() = default;

        virtual void* Add(Entity e) override
        {
            T comp{};

            auto idx = static_cast<uint32_t>(packed.size());
            packed.push_back(comp);
            indices.push_back(e);
            mask.set(e);

            EnsurePage(e);
            auto& iv = (*sparsePages[e >> PAGE_SHIFT])[e & PAGE_MASK];
            iv = PackIndexVersion(idx, UnpackVersion(iv));

            return &packed.back();
        }

        virtual void Remove(Entity e) override
        {
            HBL2_CORE_ASSERT(Has(e), "Entity does not have requested component.");

            // Lookup packed index & bump version
            uint32_t& iv = (*sparsePages[e >> PAGE_SHIFT])[e & PAGE_MASK];
            uint32_t old = iv;
            size_t idx = UnpackIndex(old);
            uint32_t ver = UnpackVersion(old) + 1;

            // Swap‐remove in packed *and* indices
            Entity lastEntity = indices.back();
            packed[idx] = std::move(packed.back());
            indices[idx] = lastEntity;         // keep indices aligned

            packed.pop_back();
            indices.pop_back();                        // *pop* from indices

            // Update sparse entry for the swapped entity
            auto& siv = (*sparsePages[lastEntity >> PAGE_SHIFT])[lastEntity & PAGE_MASK];
            siv = PackIndexVersion(uint32_t(idx), UnpackVersion(siv));

            // Tombstone the removed slot and bump its version
            iv = PackIndexVersion(INDEX_MASK, ver);

            // Clear the bit
            mask.reset(e);
        }

        virtual bool Has(Entity e) override
        {
            size_t p = e >> PAGE_SHIFT;
            if (p >= sparsePages.size() || !sparsePages[p]) return false;
            uint32_t iv = (*sparsePages[p])[e & PAGE_MASK];
            return UnpackIndex(iv) != INDEX_MASK;  // only 0xFFFF is invalid now
        }

        virtual void* Get(Entity e) override
        {
            HBL2_CORE_ASSERT(Has(e), "Entity does not have requested component.");
            uint32_t iv = (*sparsePages[e >> PAGE_SHIFT])[e & PAGE_MASK];
            return &packed[UnpackIndex(iv)];
        }

        virtual ComponentMaskAVX& Mask() const override { return const_cast<ComponentMaskAVX&>(mask); }

        virtual const Span<const Entity> Indices() const override
        {
            return indices;
        }

        virtual void IterateRaw(TrampolineFunction<void, void*>& callback) const override
        {
            for (size_t i = 0, n = packed.size(); i < n; ++i)
            {
                callback((void*)&packed[i]);
            }
        }

        virtual void Clear() override
        {
            packed.clear();
            indices.clear();

            for (auto* page : sparsePages)
            {
                delete page;
            }

            sparsePages.clear();
        }

    private:
        void EnsurePage(Entity e)
        {
            size_t p = e >> PAGE_SHIFT;
            if (p >= sparsePages.size())
            {
                sparsePages.resize(p + 1, nullptr);
            }
            if (!sparsePages[p])
            {
                sparsePages[p] = new std::array<uint32_t, PAGE_SIZE>();
                sparsePages[p]->fill(PackIndexVersion(INDEX_MASK, 0));
            }
        }

    private:
        // Combines packed index (upper bits) and version (lower bits) into one 32-bit value. Adjust bit‐widths as needed.
        inline uint32_t PackIndexVersion(uint32_t idx, uint32_t ver)
        {
            // clamp idx so that any out‑of‑range becomes our tombstone
            uint32_t safeIdx = (idx > INDEX_MASK) ? INDEX_MASK : idx;
            return (ver << VERSION_SHIFT) | safeIdx;
        }
        inline uint32_t UnpackIndex(uint32_t iv) { return  iv & INDEX_MASK; }
        inline uint32_t UnpackVersion(uint32_t iv) { return iv >> VERSION_SHIFT; }

    private:
        ComponentMaskAVX mask;
        std::vector<T> packed;
        std::vector<Entity> indices;
        std::vector<std::array<uint32_t, PAGE_SIZE>*> sparsePages;
    };
}