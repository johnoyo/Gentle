#pragma once

#include <array>
#include <cstdint>
#include <cassert>

namespace HBL2
{
    constexpr size_t L2_BITS = 64;
    constexpr size_t L1_GROUPS = 64;
    constexpr size_t L0_BLOCKS = 64;

    class SparseFlatBitmap3L
    {
    public:
        void set(uint32_t entity)
        {
            auto [b, g, i] = split(entity);
            assert(b < L0_BLOCKS && g < L1_GROUPS && i < L2_BITS);
            uint64_t bit = 1ULL << i;
            l2[b][g] |= bit;
            l1[b] |= (1ULL << g);
            l0 |= (1ULL << b);
        }

        void reset(uint32_t entity)
        {
            auto [b, g, i] = split(entity);
            assert(b < L0_BLOCKS && g < L1_GROUPS && i < L2_BITS);
            uint64_t bit = 1ULL << i;
            if ((l2[b][g] & bit) == 0) return;

            l2[b][g] &= ~bit;
            if (l2[b][g] == 0)
            {
                l1[b] &= ~(1ULL << g);
                if (l1[b] == 0)
                {
                    l0 &= ~(1ULL << b);
                }
            }
        }

        bool test(uint32_t entity) const
        {
            auto [b, g, i] = split(entity);
            if (b >= L0_BLOCKS || g >= L1_GROUPS || i >= L2_BITS) return false;
            return (l2[b][g] & (1ULL << i)) != 0;
        }

        bool any() const
        {
            return l0 != 0;
        }

        void clear()
        {
            l0 = 0;
            l1.fill(0);
            for (auto& block : l2)
            {
                block.fill(0);
            }
        }

        void operator&=(const SparseFlatBitmap3L& other)
        {
            l0 &= other.l0;

            for (size_t b = 0; b < L0_BLOCKS; ++b)
            {
                l1[b] &= other.l1[b];

                // Process 64 groups per block, each group is 64 bits (uint64_t)
                // Use AVX2 to process 4 uint64_t (256 bits) at once
                for (size_t g = 0; g < L1_GROUPS; g += 4)
                {
                    // Load 4 uint64_t from this bitmap and other bitmap
                    __m256i v1 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&l2[b][g]));
                    __m256i v2 = _mm256_load_si256(reinterpret_cast<const __m256i*>(&other.l2[b][g]));

                    // Bitwise AND
                    __m256i vand = _mm256_and_si256(v1, v2);

                    // Store result back
                    _mm256_store_si256(reinterpret_cast<__m256i*>(&l2[b][g]), vand);
                }
            }
        }

        // Iterator optimized with bit scanning
        struct Iterator
        {
            const SparseFlatBitmap3L& map;
            uint32_t current_entity = 0;
            size_t b = 0, g = 0;
            uint64_t bits = 0;

            Iterator(const SparseFlatBitmap3L& m, bool end = false) : map(m), b(0), g(0)
            {
                if (end)
                {
                    current_entity = max_entity;
                }
                else
                {
                    bits = map.l2[b][g]; // Initialize bits here!
                    advance_to_next();
                }
            }

            uint32_t operator*() const { return current_entity; }

            Iterator& operator++()
            {
                // Clear current bit
                bits &= bits - 1;
                advance_to_next();
                return *this;
            }

            bool operator!=(const Iterator& other) const
            {
                return current_entity != other.current_entity;
            }

        private:
            void advance_to_next()
            {
                while (true)
                {
                    if (bits != 0)
                    {
                        int bit_index = bit_scan_forward(bits);
                        current_entity = static_cast<uint32_t>((b << 12) | (g << 6) | bit_index);
                        return;
                    }
                    // Move to next group
                    ++g;
                    if (g >= L1_GROUPS)
                    {
                        g = 0;
                        ++b;
                        if (b >= L0_BLOCKS)
                        {
                            current_entity = max_entity;
                            return;
                        }
                    }
                    bits = map.l2[b][g];
                }
            }
        };

        Iterator begin() const { return Iterator(*this); }
        Iterator end() const { return Iterator(*this, true); }

    private:
        static constexpr uint32_t max_entity = L0_BLOCKS * L1_GROUPS * L2_BITS;

        alignas(64) std::array<std::array<uint64_t, L1_GROUPS>, L0_BLOCKS> l2{};
        alignas(64) std::array<uint64_t, L0_BLOCKS> l1{};
        alignas(64) uint64_t l0 = 0;

        static constexpr std::tuple<uint32_t, uint32_t, uint32_t> split(uint32_t entity)
        {
            uint32_t i = entity & 0x3F;
            uint32_t g = (entity >> 6) & 0x3F;
            uint32_t b = entity >> 12;
            return { b, g, i };
        }

        static inline int bit_scan_forward(uint64_t x)
        {
            assert(x != 0);
#if defined(_MSC_VER)
            unsigned long index;
            _BitScanForward64(&index, x);
            return static_cast<int>(index);
#else
            return __builtin_ctzll(x);
#endif
        }
    };
}
