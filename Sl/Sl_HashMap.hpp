#ifndef SL_HASHMAP_H
#define SL_HASHMAP_H

#include "Sl_Defines.hpp"
#include "Sl_Array.hpp"

//// 0 = Not equals, 1.. = Equals

typedef Sl::u64(*Hasher_fn) (Sl::u64 seed, const void* key, Sl::usize key_len);

namespace Sl
{
    // * Hasher function is required to just hash the raw bytes
    // * Hash_fn in the template is optional function that will be called, when hashing is needed
    //  it will give callback to hasher, so you can decide what members and how you want to hash you class
    u64 hasher_fn_default(usize seed, const void* key, usize key_len);

    struct HashMapOptions
    {
        // Capacity of hashmap is always power of 2
        u32 initial_size = 32;
        float max_load = 0.80f;
        float grow_factor = 2.0f;
        Hasher_fn hasher = hasher_fn_default;
        usize seed = 0;
        // Optional. If set: it will allocate memory from this allocator for Array table
        Allocator* allocator = nullptr;
    };


    template<typename K, typename V, u64(*Hash_fn)(usize seed, const K& key, Hasher_fn callback) = nullptr>
    class HashMap {
        struct Info {
            u64 hash;
            usize index;
        };
        struct Entry {
            K key;
            V value;
            u64 hash;

            Entry(K&& k, V&& v) : key(std::move(k)), value(std::move(v)), hash(FREE_HASH) {}
        };
    public:
        static const u64 FREE_HASH        = 0;
        static const u64 DEAD_HASH        = 1;
        static const u64 FIRST_VALID_HASH = 2;
    public:
        Array<Entry> _table;
        size_t _count;
        Hasher_fn _hasher;
        usize _seed;
        float _max_load;
        float _grow_factor;

        HashMap(HashMapOptions opt = {}) noexcept
        {
            _count = 0;
            _hasher = opt.hasher;
            _seed = opt.seed;
            _max_load = opt.max_load;
            _grow_factor = opt.grow_factor;
            _table.set_allocator(opt.allocator);

            const auto new_capacity = is_power_of_two(opt.initial_size) ? opt.initial_size : next_power_of_two(opt.initial_size);
            _table.reserve(new_capacity);
            _table.set_count(new_capacity);
            for (auto& slot : _table) slot.hash = FREE_HASH;
        }

        bool is_empty() const noexcept { return _count == 0; }

        bool contains(const K& key) const noexcept { return get(key) != nullptr; }

        template<typename... Args>
        void insert(K&& key, Args&&... args) noexcept
        {
            if (++_count > _table.capacity() * _max_load) grow();

            auto capacity = _table.capacity();
            auto _hash = hash(key);
            auto _index = _hash & (capacity - 1);

            for (usize i = 0; i < capacity; ++i) {
               usize probe = (_index + i) % capacity;
               auto& slot = _table[probe];
               if (slot.hash < FIRST_VALID_HASH) {
                   ::new (&slot.value) V(std::forward<Args>(args)...);
                   slot.key = key;
                   slot.hash = _hash;
                   return;
               }
            }
            grow();
            insert(std::move(key), std::forward<Args>(args)...);
        }

        template<typename... Args>
        void insert(K& key, Args&&... args) noexcept
        {
            if (++_count > _table.capacity() * _max_load) grow();
            auto capacity = _table.capacity();
            auto _hash = hash(key);
            auto _index = _hash & (capacity - 1);

            for (usize i = 0; i < capacity; ++i) {
                usize probe = (_index + i) % capacity;
                auto& slot = _table[probe];
                if (slot.hash < FIRST_VALID_HASH) {
                    ::new (&slot.value) V(std::forward<Args>(args)...);
                    slot.key = key;
                    slot.hash = _hash;
                    return;
                }
            }
            grow();
            insert(std::move(key), std::forward<Args>(args)...);
        }

        V* operator[](const K& key) noexcept
        {
            return get(key);
        }

        V* get(const K& key) const noexcept
        {
            auto capacity = _table.capacity();
            auto _hash = hash(key);
            auto _index = _hash & (capacity - 1);

            for (usize i = 0; i < capacity; ++i) {
                usize probe = (_index + i) % capacity;
                auto& slot = _table[probe];
                if (slot.hash == FREE_HASH) return nullptr;

                if (slot.hash == _hash && slot.key == key)
                    return &slot.value;
            }
            return nullptr;
        }

        bool remove(const K& key) noexcept
        {
            auto capacity = _table.capacity();
            auto _hash = hash(key);
            auto _index = _hash & (capacity - 1);

            for (size_t i = 0; i < capacity; ++i) {
                usize probe = (_index + i) % capacity;
                Entry& slot = _table[probe];

                if (slot.hash == FREE_HASH) return false;
                if (slot.hash == _hash && slot.key == key) {
                    slot.hash = DEAD_HASH;
                    --_count;
                    slot.value.~V();
                    return true;
                }
            }
            return false;
        }

        template<typename Function>
        void forEach(Function&& func) const noexcept {
            for (size_t i = 0; i < _table.capacity(); ++i) {
                Entry& slot = _table[i];

                if (slot.hash >= FIRST_VALID_HASH) func(slot.key, slot.value);
            }
        }

        inline u64 hash(const K& key) const noexcept
        {
            u64 res = 0;
            IF_CONSTEXPR(Hash_fn != nullptr) {
                res = Hash_fn(_seed, key, _hasher);
            } else {
                res = _hasher(_seed, &key, sizeof(key));
            }
            if (res < FIRST_VALID_HASH) res += FIRST_VALID_HASH;
            return res;
        }

        void grow() noexcept
        {
            u64 new_capacity = _table.capacity() * _grow_factor;
            if (!is_power_of_two(new_capacity)) {
                new_capacity = next_power_of_two(new_capacity);
            }
            Array<Entry> new_table {};
        restart:
            new_table.reserve(new_capacity);
            new_table.set_count(new_capacity);
            for (auto& slot : new_table) slot.hash = FREE_HASH;

            _count = 0;
            for (usize i = 0; i < _table.capacity(); ++i) {
                auto& slot = _table[i];
                if (slot.hash >= FIRST_VALID_HASH) {
                    if (!insert_inner(new_table, std::move(slot.key), std::move(slot.value)))
                    {
                        new_table.set_count(0);
                        new_capacity = next_power_of_two(new_capacity * _grow_factor);
                        goto restart;
                    }
                    // ++_count;
                }
            }
            _table.cleanup();
            _table.set_data(new_table.data());
            _table.set_capacity(new_table.capacity());
            _table.set_count(new_table.count());
        }

        void clear() noexcept
        {
            for (usize i = _table.capacity(); i > 0; --i) {
                auto& slot = _table[i - 1];
                if (slot.hash >= FIRST_VALID_HASH) {
                    slot.hash = FREE_HASH;
                    slot.value.~V();
                }
            }
        }

        void cleanup() noexcept
        {
            clear();
            _table.set_count(0);
            _table.cleanup();
        }
    private:
        static constexpr bool is_power_of_two(usize n) noexcept
        {
            return n > 0 && (n & (n - 1)) == 0;
        }

        static constexpr usize next_power_of_two(usize n) noexcept
        {
            if (n <= 1) return 2;
            n--;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            n |= n >> 32;
            return n + 1;
        }
        bool insert_inner(Array<Entry>& table, K&& key, V&& value) noexcept
        {
            ++_count;
            ASSERT_TRUE(_count < table.capacity());

            auto capacity = table.capacity();
            auto _hash = hash(key);
            auto _index = _hash & (capacity - 1);
            for (usize i = 0; i < capacity; ++i) {
                usize probe = (_index + i) % capacity;
                auto& slot = table[probe];

                if (slot.hash < FIRST_VALID_HASH) {
                    slot.key = std::move(key);
                    slot.value = std::move(value);
                    slot.hash = _hash;
                    return true;
                }
            }
            return false;
        }
    };

} // namespace Sl
#endif // !SL_HASHMAP_H

#ifdef SL_IMPLEMENTATION
namespace Sl
{
    u64 hasher_fn_default(usize seed, const void* key, usize key_len)
    {
        const u64 prime = 1099511628211ull;
        u64 hash = 14695981039346656037ull;
        auto* bytes = (const u8*)key;

        hash ^= seed;
        hash *= prime;

        for (usize i = 0; i < key_len; ++i) {
            hash ^= (u64)bytes[i];
            hash *= prime;
        }

        return hash;
    }
} // namespace Sl
#endif
