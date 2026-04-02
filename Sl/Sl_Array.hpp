#ifndef SL_ARRAY_H
#define SL_ARRAY_H

#include "Sl_Defines.hpp"
#include <utility>

namespace Sl
{
    struct Allocator;
    void memory_copy(void* dest, usize dest_size, const void* src, usize src_size) noexcept;
    void memory_zero(void* src, usize size) noexcept;

    template <typename T>
    struct Array
    {
    protected:
        T* _data;
        usize _capacity;
        usize _count;
        Allocator* _allocator; // Optional, if set: it will allocate memory from this allocator istead of SL_ARRAY_REALLOC
        float grow_rate;
    public:
        static const usize INVALID_INDEX = -1;

        Array(Allocator* allocator = nullptr, float grow_rate = 2.0)
        {
            this->_allocator = allocator;
            this->_data = nullptr;
            this->_capacity = 0;
            this->_count = 0;
            this->grow_rate = grow_rate;
        }

        template<typename... Args>
        void push(Args&&... args) noexcept
        {
            resize(_count + 1);
            ::new (_data + _count++) T(std::forward<Args>(args)...);
        }

        void push_many(const T* new_data, usize new_count) noexcept
        {
            ASSERT(new_data != nullptr, "Cannot push null array");
            if (new_data == nullptr) return;

            resize(this->_count + new_count);
            for (usize i = 0; i < new_count; ++i)
                _data[this->_count++] = new_data[i];
        }

        T* data() const { return _data; };
        usize capacity() const { return _capacity; };
        usize count() const { return _count; };
        Allocator* allocator() const { return _allocator; }
        void set_allocator(Allocator* allocator)
        {
            cleanup();
            _allocator = allocator;
        }

        T& get(usize index) noexcept
        {
            ASSERT(_count > index, "Index out of range");
            return _data[index];
        }

        T& get_unsafe(usize index) noexcept
        {
            return _data[index];
        }

        void remove_unordered(usize index) noexcept
        {
            ASSERT(_count > 0 && index <= _count, "Index out of range");
            if (_count == 0 || index >= _count) return;

            // I don't know what's std::vector devs were smoking
            if (index != _count - 1) {
                std::swap(_data[index], _data[--_count]);
            } else {
                _data[index].~T();
                --_count;
            }
        }

        template<typename Function>
        void forEach(Function&& func) noexcept {
            for (usize i = 0; i < _count; ++i) {
                func(_data[i]);
            }
        }

        template<typename Function>
        void forEachIndexed(Function&& func) noexcept {
            for (usize i = 0; i < _count; ++i) {
                func(i, _data[i]);
            }
        }

        T& first() noexcept { return get(0); }
        T& last() noexcept { return get(_count - 1); }
        bool is_empty() noexcept { return _count < 1; }
        bool is_valid_index(usize index) const noexcept { return index < _capacity; }

        usize find_first(T val) const
        {
            for (usize i = 0; i < _count; ++i) {
                if (_data[i] == val) return i;
            }
            return INVALID_INDEX;
        }
        usize find_last(T val) const
        {
            usize index = INVALID_INDEX;
            for (usize i = 0; i < _count; ++i) {
                if (_data[i] == val) index = i;
            }
            return index;
        }

        template<typename... Args>
        Array& operator<<(Args&&... args) noexcept { push(std::forward<Args>(args)...); return *this; }
        bool contains(T val) const { return find_first(val) != INVALID_INDEX; }
        T& operator[](usize index) noexcept { return get(index); }
        T* begin() noexcept { return _data; }
        T* end() noexcept { return _data + _count; }

        T&& pop() noexcept
        {
            if (_count < 1) {
                ASSERT(_count > 0, "Cannot pop from empty array");
                return std::move(_data[_count]);
            }

            // _data[_count - 1].~T();
            _count -= 1;
            return std::move(_data[_count]);
        }

        // Does not shrink, if needed: use reserve()
        void resize(usize needed_capacity) noexcept
        {
            if (needed_capacity > _capacity)
            {
                auto old_capacity = _capacity;
                if (_capacity == 0) _capacity = 32;
                while (_capacity < needed_capacity) _capacity *= grow_rate;

                if (_allocator)
                    _data = (T*)_allocator->reallocate(_data, old_capacity * ALIGNMENT(sizeof(T), alignof(T)), _capacity * ALIGNMENT(sizeof(T), alignof(T)));
                else
                    _data = (T*)SL_ARRAY_REALLOC(_data, _capacity * ALIGNMENT(sizeof(T), alignof(T)));
                ASSERT_DEBUG(_data != nullptr);
            }
        }

        void reserve(usize needed_capacity) noexcept
        {
            if (needed_capacity > _capacity) {
                if (_allocator)
                    _data = (T*)_allocator->reallocate(_data, _capacity * ALIGNMENT(sizeof(T), alignof(T)), needed_capacity * ALIGNMENT(sizeof(T), alignof(T)));
                else
                    _data = (T*)SL_ARRAY_REALLOC(_data, needed_capacity * ALIGNMENT(sizeof(T), alignof(T)));
                _capacity = needed_capacity;
                ASSERT_DEBUG(_data != nullptr);
                for (usize i = _count; i < _capacity; ++i)
                    ::new (_data + i) T(static_cast<T&&>(_data[i]));
            } else {
                const auto needed_capacity_bytes = needed_capacity * ALIGNMENT(sizeof(T), alignof(T));
                T* new_data;
                if (_allocator)
                    new_data = (T*)_allocator->allocate(needed_capacity_bytes);
                else
                    new_data = (T*)SL_ARRAY_REALLOC(nullptr, needed_capacity_bytes);
                ASSERT_DEBUG(new_data != nullptr);
                const auto new_count = needed_capacity < _count ? needed_capacity : _count;
                for (usize i = 0; i < new_count; ++i)
                    new_data[i] = std::move(_data[i]);
                for (usize i = new_count; i < _count; ++i)
                    _data[i].~T();
                if (!_allocator) SL_ARRAY_FREE(_data);
                _data = new_data;
                _capacity = needed_capacity;
                _count = new_count;
            }
        }

        void set_data(T* data)
        { // Use it, if you know what you are doing
            _data = data;
        }

        void set_count(usize count)
        { // Use it, if you know what you are doing
            ASSERT(count <= _capacity, "Count cannot be bigger than allocated capacity");
            _count = count;
        }

        void set_capacity(usize capacity)
        { // Use it, if you know what you are doing
            _capacity = capacity;
        }

        void reverse() noexcept
        {
            if (_count < 2) return;

            usize left = 0;
            usize right = _count - 1;

            while (left < right) {
                std::swap(_data[left], _data[right]);
                ++left;
                --right;
            }
        }

        void memzero() noexcept
        {
            if (_data) memory_zero(_data, _capacity * ALIGNMENT(sizeof(T), alignof(T)));
        }

        void clear() noexcept
        {
            for (usize i = 0; i < _count; ++i) {
                _data[i].~T();
            }
            _count = 0;
        }

        void cleanup() noexcept
        {
            clear();
            if (!_allocator) SL_ARRAY_FREE(_data);
            _data = nullptr;
            _capacity = 0;
        }

        Array& operator=(Array& arr) noexcept
        {
            if ((void*) this != (void*) &arr) {
                cleanup();
                this->_data = arr._data;
                this->_count = arr._count;
                this->_capacity = arr._capacity;
                arr._data = nullptr;
                arr._count = 0;
                arr._capacity = 0;
            }
            return *this;
        }

        // Array& operator=(Array&& arr) noexcept
        // {
        //     return *this = arr;
        // }

        // Array& operator=(Array<T> arr) = delete;
        // Array& operator=(const Array<T>&& arr) = delete;
        // Array& operator=(const Array& arr) = delete;
    // ----------------------------------------------
    // Const functions
    // ----------------------------------------------
        T& get(usize index) const noexcept
        {
            ASSERT(_count > index, "Index out of range");
            return _data[index];
        }
        T& get_unsafe(usize index) const noexcept
        {
            return _data[index];
        }

        T& operator[](usize index) const noexcept { return get(index); }
        const T* begin() const noexcept { return _data; }
        const T* end() const noexcept { return _data + _count; }

        T& first() const noexcept { return get(0); }
        T& last() const noexcept { return get(_count - 1); }

        template<typename Function>
        void forEach(Function&& func) const noexcept {
            for (usize i = 0; i < _count; ++i) {
                func(_data[i]);
            }
        }

        template<typename Function>
        void forEachIndexed(Function&& func) const noexcept {
            for (usize i = 0; i < _count; ++i) {
                func(i, _data[i]);
            }
        }
    };

    // * Uses SL_LOCAL_ARRAY_INIT_SIZE stack storage initially.
    // * This allows to speed it up on small scale sizes.
    // * Allocates heap/custom allocator when exceeded.
    static_assert(SL_LOCAL_ARRAY_INIT_SIZE > 0ll, "Provide correct size");

    template<typename T>
    class LocalArray
    {
    private:
        T* _data;
        alignas(alignof(T)) u8 _storage[SL_LOCAL_ARRAY_INIT_SIZE * sizeof(T)];
        usize _count;
        usize _allocated_capacity;
        bool _is_heap_allocated; // True if allocated with SL_ARRAY_REALLOC or Allocator
        Allocator* _allocator; // Optional, if set: it will allocate memory from this allocator istead of SL_ARRAY_REALLOC
    public:
        LocalArray(Allocator* allocator = nullptr)
        {
            _data = (T*) &_storage;
            _count = 0;
            _allocated_capacity = 0;
            _is_heap_allocated = false;
            _allocator = allocator;
        }

        inline T* data() { return _data; }
        inline usize allocated_capacity() const { return _allocated_capacity; }
        inline bool is_heap_allocated() const { return _is_heap_allocated; }
        inline usize count() const { return _count; }

        inline void set_count(usize count) // Use it, if you know what you doing
        {
            ASSERT(!is_heap_allocated() || count <= _allocated_capacity,
                "Count cannot be bigger than allocated count");
            ASSERT(is_heap_allocated() || count <= SL_LOCAL_ARRAY_INIT_SIZE,
                "Count cannot be bigger than storage amount");
            _count = count;
        }

        template<typename... Args>
        void push(Args&&... args) noexcept
        {
            if (_count + 1 > SL_LOCAL_ARRAY_INIT_SIZE && !is_heap_allocated()) {
                 _is_heap_allocated = true;
                _allocated_capacity = SL_LOCAL_ARRAY_INIT_SIZE * 2;
                T* new_data;
                if (_allocator)
                    new_data = (T*)_allocator->allocate(size_of_t() * _allocated_capacity);
                else
                    new_data = (T*)SL_ARRAY_REALLOC(nullptr, size_of_t() * _allocated_capacity);
                memory_copy(new_data, size_of_t() * SL_LOCAL_ARRAY_INIT_SIZE, _data, size_of_t() * SL_LOCAL_ARRAY_INIT_SIZE);
                _data = new_data;
            } else if (_count + 1 > _allocated_capacity && is_heap_allocated()) {
                resize(_count + 1);
            }
            ::new (_data + _count++) T(std::forward<Args>(args)...);
        }

        T& get(usize index) noexcept
        {
            ASSERT(_count > index, "Index out of range");
            return _data[index];
        }

        T& get_unsafe(usize index) noexcept
        {
            return _data[index];
        }
        T& operator[](usize index) noexcept { return get(index); }

        bool is_valid_index(usize index) const noexcept { return index < _allocated_capacity; }
        bool is_empty() noexcept { return _count < 1; }
        T& first() noexcept { return get(0); }
        T& last() noexcept { return get(_count - 1); }
        T* begin() noexcept { return _data; }
        T* end() noexcept { return _data + _count; }

        // Does not shrink
        void resize(usize needed_capacity) noexcept
        {
            if (needed_capacity > _allocated_capacity)
            {
                auto old_capacity = _allocated_capacity;
                if (_allocated_capacity == 0) _allocated_capacity = 32;
                while (_allocated_capacity < needed_capacity) _allocated_capacity *= 2;

                if (_allocator)
                    _data = (T*)_allocator->reallocate(_data, old_capacity * size_of_t(), _allocated_capacity * size_of_t());
                else
                    _data = (T*)SL_ARRAY_REALLOC(_data, _allocated_capacity * size_of_t());
                ASSERT_DEBUG(_data != nullptr);
            }
        }

        void pop() noexcept
        {
            ASSERT(_count > 0, "Cannot pop from empty array");
            if (_count < 1) return;
            _data[--_count].~T();
        }

        void remove_unordered(usize index) noexcept
        {
            ASSERT(_count > 0 && index <= _count, "Index out of range");
            if (_count == 0 || index >= _count) return;

            if (index != _count - 1) {
                std::swap(_data[index], _data[--_count]);
            } else {
                _data[index].~T();
                --_count;
            }
        }

        void reverse() noexcept
        {
            if (_count < 2) return;

            usize left = 0;
            usize right = _count - 1;

            while (left < right) {
                std::swap(_data[left], _data[right]);
                ++left;
                --right;
            }
        }

        template<typename Function>
        void forEach(Function&& func) noexcept {
            for (usize i = 0; i < _count; ++i) {
                func(_data[i]);
            }
        }

        template<typename Function>
        void forEachIndexed(Function&& func) noexcept {
            for (usize i = 0; i < _count; ++i) {
                func(i, _data[i]);
            }
        }

        void clear() noexcept
        {
            for (usize i = 0; i < _count; ++i) {
                _data[i].~T();
            }
            _count = 0;
        }

        void cleanup() noexcept
        {
            clear();
            if (is_heap_allocated()) {
                if (_allocator == nullptr) SL_ARRAY_FREE(_data);
                _data = (T*) &_storage;
                _is_heap_allocated = false;
            }
        }

        void memzero() noexcept
        {
            ASSERT(_data != nullptr, "Data cannot be null, since it must point to _storage or allocated block");
            usize size = size_of_t();
            if (is_heap_allocated())
                size *= _allocated_capacity;
            else
                size *= SL_LOCAL_ARRAY_INIT_SIZE;
            memory_zero(_data, size);
        }
    // ----------------------------------------------
    // Const functions
    // ----------------------------------------------
        T& get(usize index) const noexcept
        {
            ASSERT(_count > index, "Index out of range");
            return _data[index];
        }
        T& get_unsafe(usize index) const noexcept
        {
            return _data[index];
        }

        T& operator[](usize index) const noexcept { return get(index); }
        const T* begin() const noexcept { return _data; }
        const T* end() const noexcept { return _data + _count; }

        T& first() const noexcept { return get(0); }
        T& last() const noexcept { return get(_count - 1); }

        template<typename Function>
        void forEach(Function&& func) const noexcept {
            for (usize i = 0; i < _count; ++i) {
                func(_data[i]);
            }
        }

        template<typename Function>
        void forEachIndexed(Function&& func) const noexcept {
            for (usize i = 0; i < _count; ++i) {
                func(i, _data[i]);
            }
        }
        usize size_of_t() const noexcept { return ALIGNMENT(sizeof(T), alignof(T)); }
    };
} // namescape Sl
#endif // SL_ARRAY_H
