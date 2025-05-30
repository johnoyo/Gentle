#pragma once

#include "EntityManager.h"

#include <immintrin.h>
#include <cstddef>
#include <cstdint>

namespace HBL2
{
    struct ComponentMaskAVX
    {
        alignas(32) uint64_t data[MASK_WORDS];

        ComponentMaskAVX() { clear(); }

        void clear()
        {
            for (size_t i = 0; i < MASK_WORDS; ++i)
            {
                data[i] = 0ULL;
            }
        }

        void set(Entity e)
        {
            size_t idx = e >> 6;
            uint64_t bit = 1ULL << (e & 63);
            data[idx] |= bit;
        }

        void reset(Entity e)
        {
            size_t idx = e >> 6;
            uint64_t bit = 1ULL << (e & 63);
            data[idx] &= ~bit;
        }

        bool test(Entity e) const
        {
            size_t idx = e >> 6;
            uint64_t bit = 1ULL << (e & 63);
            return (data[idx] & bit) != 0;
        }

        // Optimize mask comparison operations
        bool has_any(const ComponentMaskAVX& other) const
        {
            for (size_t i = 0; i < MASK_WORDS; i += 4)
            {
                __m256i a = _mm256_load_si256((__m256i*) & data[i]);
                __m256i b = _mm256_load_si256((__m256i*) & other.data[i]);
                __m256i c = _mm256_and_si256(a, b);

                // Quick check if any bits are set
                if (!_mm256_testz_si256(c, c))
                {
                    return true;
                }
            }

            return false;
        }

        void and_with(const ComponentMaskAVX& other)
        {
            for (size_t i = 0; i < MASK_WORDS; i += 4)
            {
                __m256i a = _mm256_load_si256((__m256i*) & data[i]);
                __m256i b = _mm256_load_si256((__m256i*) & other.data[i]);
                __m256i c = _mm256_and_si256(a, b);
                _mm256_store_si256((__m256i*) & data[i], c);
            }
        }

        void and_not_with(const ComponentMaskAVX& other)
        {
            for (size_t i = 0; i < MASK_WORDS; i += 4)
            {
                __m256i m = _mm256_load_si256((__m256i*) & data[i]);
                __m256i ex = _mm256_load_si256((__m256i*) & other.data[i]);
                __m256i res = _mm256_andnot_si256(ex, m);
                _mm256_store_si256((__m256i*) & data[i], res);
            }
        }

        void operator&=(const ComponentMaskAVX& other)
        {
            and_with(other);
        }

        void operator-=(ComponentMaskAVX const& other)
        {
            and_not_with(other);
        }

        // Count bits
        size_t count() const
        {
            size_t total = 0;
            for (size_t i = 0; i < MASK_WORDS; ++i)
            {
                total += _mm_popcnt_u64(data[i]);
            }

            return total;
        }

        // Find the first set bit, or MAX_ENTITIES if none
        Entity find_first() const
        {
            for (size_t wi = 0; wi < MASK_WORDS; ++wi)
            {
                uint64_t w = data[wi];
                if (w)
                {
                    // count trailing zeros to find LSB
#ifdef _MSC_VER
                    unsigned long tz;
                    _BitScanForward64(&tz, w);
#else
                    unsigned tz = __builtin_ctzll(w);
#endif
                    Entity e = static_cast<Entity>(wi * 64 + tz);
                    return (e < MAX_ENTITIES ? e : MAX_ENTITIES);
                }
            }
            return MAX_ENTITIES;
        }

        // Find the next set bit after 'prev', or MAX_ENTITIES if none
        Entity find_next(Entity prev) const
        {
            Entity next = prev + 1;
            if (next >= MAX_ENTITIES) return MAX_ENTITIES;

            size_t wi = next >> 6;
            uint64_t bit_off = next & 63;

            // Mask off bits below bit_off in the first word
            uint64_t w = data[wi] & (~0ULL << bit_off);
            if (w)
            {
#ifdef _MSC_VER
                unsigned long tz;
                _BitScanForward64(&tz, w);
#else
                unsigned tz = __builtin_ctzll(w);
#endif
                Entity e = static_cast<Entity>(wi * 64 + tz);
                return (e < MAX_ENTITIES ? e : MAX_ENTITIES);
            }

            // Scan remaining words
            for (size_t wj = wi + 1; wj < MASK_WORDS; ++wj)
            {
                uint64_t w2 = data[wj];
                if (w2)
                {
#ifdef _MSC_VER
                    unsigned long tz;
                    _BitScanForward64(&tz, w2);
#else
                    unsigned tz = __builtin_ctzll(w2);
#endif
                    Entity e = static_cast<Entity>(wj * 64 + tz);
                    return (e < MAX_ENTITIES ? e : MAX_ENTITIES);
                }
            }

            return MAX_ENTITIES;
        }

        struct Iterator
        {
            Iterator(const ComponentMaskAVX* m, Entity pos)
                : mask(m), current(pos) {
            }

            Entity operator*() const { return current; }

            Iterator& operator++()
            {
                current = mask->find_next(current);
                return *this;
            }

            bool operator==(const Iterator& other) const
            {
                return current == other.current;
            }

            bool operator!=(const Iterator& other) const
            {
                return !(*this == other);
            }

        private:
            const ComponentMaskAVX* mask;
            Entity current;
        };

        Iterator begin() const
        {
            return Iterator(this, find_first());
        }

        Iterator end() const
        {
            return Iterator(this, MAX_ENTITIES);
        }
    };
}