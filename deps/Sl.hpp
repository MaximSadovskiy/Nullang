// Sl.h - STD library for ezbuild.hpp
#ifndef SL_CORE_H
#define SL_CORE_H

// Define SL_IMPLEMENTATION before including this file to include implementations of all files
//  #define SL_IMPLEMENTATION

// This file contains configuration macros, that you can change
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <functional>

// You can config this values
// By defining macro before including Sl.h
//  for example:
// # define SL_ALLOCATOR_MALLOC(size) my_custom_malloc(size)
// # include "Sl.h"
//
//--------------------------
// 0 == disable custom asserts for this library
#ifndef SL_ENABLE_ASSERT
#   define SL_ENABLE_ASSERT 1
#endif //SL_ENABLE_ASSERT

// If disabled Arrays and HashMaps will not take ownership and call destuctors when cleared
//   This is just a default value, you can still change it when creating an instance of Array/HashMap:
//  Array<YourType, false> arr = {};
//  arr.cleanup(); // <--- won't call destructor
// This feature is needed, if you don't care about ownership of the Type
#ifndef SL_ARRAY_CPP_COMPLIANT
#   define SL_ARRAY_CPP_COMPLIANT true
#endif // SL_ARRAY_CPP_COMPLIANT

#ifndef SL_ARRAY_REALLOC
#   define SL_ARRAY_REALLOC(ptr, size) realloc((ptr), (size))
#endif // SL_ARRAY_REALLOC
#ifndef SL_ARRAY_FREE
#   define SL_ARRAY_FREE(ptr) free((ptr))
#endif // SL_ARRAY_FREE
#ifndef SL_ARRAY_MEMCOPY
#   define SL_ARRAY_MEMCOPY(dst, src, size) memcpy((dst), (src), (size))
#endif // SL_ARRAY_MEMCOPY

// Set initial size of local array.
//  If size was excedeed, it will allocate more with ARRAY_REALLOC or Allocator
#ifndef SL_LOCAL_ARRAY_INIT_SIZE
#define SL_LOCAL_ARRAY_INIT_SIZE 16
#endif // SL_LOCAL_ARRAY_INIT_SIZE

#ifndef SL_ALLOCATOR_MALLOC
#   define SL_ALLOCATOR_MALLOC(size) malloc((size))
#endif // SL_ALLOCATOR_MALLOC
#ifndef SL_ALLOCATOR_FREE
#   define SL_ALLOCATOR_FREE(ptr) free((ptr))
#endif // SL_ALLOCATOR_FREE

#ifndef SL_GLOBAL_ALLOCATOR_INIT_SIZE
#   define SL_GLOBAL_ALLOCATOR_INIT_SIZE (1024 * 64)
#endif // SL_GLOBAL_ALLOCATOR_INIT_SIZE

#ifndef SL_ALLOCATOR_INIT_SIZE
#   define SL_ALLOCATOR_INIT_SIZE (1024 * 4)
#endif // SL_ALLOCATOR_INIT_SIZE

//---------------------------

#if defined(_MSC_VER)
#   define Sl_EXPORT __declspec(dllexport)
#   define Sl_IMPORT __declspec(dllimport)
#else
#   define Sl_EXPORT __attribute__((visibility("default")))
#   define Sl_IMPORT
#endif // _MSC_VER

#ifndef Sl_DEF
#define Sl_DEF
#endif // Sl_DEF

#ifndef SL_PRINTF_FORMATER
#   if defined(__GNUC__) || defined(__clang__)
#       define SL_PRINTF_FORMATER(fmt, args) __attribute__((format(printf, fmt, args)))
#   else
#       define SL_PRINTF_FORMATER(fmt, args) // msvc...
#   endif
#endif // SL_PRINTF_FORMATER

#ifndef UNREACHABLE
#define UNREACHABLE(str) do { fprintf(stderr, "%s:%d: [UNREACHABLE]: %s\n", __FILE__, __LINE__, (str)); DEBUG_BREAK(); exit(1); } while(0)
#endif // UNREACHABLE

#ifndef TODO
#define TODO(str) do { fprintf(stderr, "%s:%d: [TODO] %s\n", __FILE__, __LINE__, (str)); DEBUG_BREAK(); exit(1); } while(0)
#endif // TODO

#ifndef UNUSED
#define UNUSED(v) ((void)(v))
#endif // UNUSED

#ifndef ARRAY_LEN
#define ARRAY_LEN(a) (sizeof((a)) / sizeof((a)[0]))
#endif // ARRAY_LEN

#ifndef STR_LIT_SIZE // Without NULL
#define STR_LIT_SIZE(str_lit) (sizeof(str_lit) - sizeof((str_lit)[0]))
#endif // STR_LIT_SIZE

#ifndef DEFER_RETURN
#define DEFER_RETURN(res) do { result = res; goto end; } while(0)
#endif // DEFER_RETURN

#ifndef IF_CONSTEXPR
#   if __cplusplus >= 201703L
#       define IF_CONSTEXPR if constexpr
#   else
#       define IF_CONSTEXPR if
#   endif // __cplusplus >= 201703L
#endif // IF_CONSTEXPR

#ifndef SL_THREAD_LOCAL
#   if defined(_MSC_VER)
#       define SL_THREAD_LOCAL __declspec(thread)
#   elif defined(__GNUC__) || defined(__clang__)
#       if __STDC_VERSION__ >= 201112L || __cplusplus >= 201103L
#           define SL_THREAD_LOCAL thread_local
#       else
#           define SL_THREAD_LOCAL __thread
#       endif
#   else
#       warning "Cannot set SL_THREAD_LOCAL"
#       define SL_THREAD_LOCAL
#   endif
#endif // SL_THREAD_LOCAL

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif // MAX

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif // MIN

#ifndef SWAP
#define SWAP(a, b) do { auto temp___ = (a); (a) = (b); (b) = temp___; } while (0)
#endif // SWAP

#ifndef ALIGNMENT
#define ALIGNMENT(val, alignment) ((alignment) == 0 ? (val) : (((val) + (alignment) - 1) & ~((alignment) - 1)))
#endif // ALIGNMENT

#ifndef DISABLE
#define DISABLE if(0)
#endif // DISABLE

namespace Sl
{
    typedef uint8_t  u8;
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;
    typedef int8_t   s8;
    typedef int16_t  s16;
    typedef int32_t  s32;
    typedef int64_t  s64;
    typedef float    f32;
    typedef double   f64;
    typedef size_t   usize;
}

#if defined(_DEBUG)
    #if defined(_MSC_VER)
        extern void __cdecl __debugbreak(void);
        #define DEBUG_BREAK() __debugbreak()
    #elif ( (!defined(__NACL__)) && ((defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))) )
        #define DEBUG_BREAK() __asm__ __volatile__ ( "int $3\n\t" )
    #elif (defined(__GNUC__) || defined(__clang__)) && defined(__riscv)
        #define DEBUG_BREAK() __asm__ __volatile__ ( "ebreak\n\t" )
    #elif ( defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__)) )
        #define DEBUG_BREAK() __asm__ __volatile__ ( "brk #22\n\t" )
    #elif defined(__APPLE__) && defined(__arm__)
        #define DEBUG_BREAK() __asm__ __volatile__ ( "bkpt #22\n\t" )
    #elif defined(__386__) && defined(__WATCOMC__)
        #define DEBUG_BREAK() { _asm { int 0x03 } }
    #elif defined(HAVE_SIGNAL_H) && !defined(__WATCOMC__)
        #define DEBUG_BREAK() raise(SIGTRAP)
    #else
        #define DEBUG_BREAK()
    #endif
#else
    #define DEBUG_BREAK()
#endif //_DEBUG

#define ASSERT(condition, text)\
    Sl::assert_default_(!!(condition), __FILE__, __LINE__, (text))

#define ASSERT_DEBUG(condition)\
    Sl::assert_debug_(!!(condition), __FILE__, __LINE__, #condition)

#define ASSERT_TRUE(condition)\
    Sl::assert_default_(!!(condition), __FILE__, __LINE__, "Expression \"" #condition "\" was false, but expected to be true.")

#define ASSERT_FALSE(condition)\
    Sl::assert_default_(!(condition), __FILE__, __LINE__, "Expression \"" #condition "\" was true, but expected to be false.")

#define ASSERT_EQUALS(condition1, condition2)\
    Sl::assert_default_((condition1) == (condition2), __FILE__, __LINE__, "Expression \"" #condition1 " == " #condition2 "\" was false.")

#define ASSERT_NOT_NULL(condition)\
    Sl::assert_default_((condition), __FILE__, __LINE__, "Expression \"" #condition "\" was null.")

namespace Sl
{
    struct Allocator;
    void memory_set(void* src, u8 data, usize size) noexcept;
    void memory_zero(void* src, usize size) noexcept;
    void memory_copy(void* dest, usize dest_size, const void* src, usize src_size) noexcept;
    ptrdiff_t memory_compare(const void* ptr1, usize ptr1_size, const void* ptr2, usize ptr2_size) noexcept;
    bool memory_equals(const void* ptr1, usize ptr1_size, const void* ptr2, usize ptr2_size) noexcept;
    usize memory_strlen(const char* ptr) noexcept;
    usize memory_wstrlen(const wchar_t* ptr) noexcept; // returns size in bytes
    void* memory_format(Allocator& allocator, usize& size_out, const char* format, ...) noexcept SL_PRINTF_FORMATER(3, 4);
    void* memory_format(Allocator& allocator, usize& size_out, const char* format, va_list args) noexcept;
    void* memory_duplicate(Allocator& allocator, const void* ptr) noexcept; // also puts NULL at the end
    void* memory_duplicate(Allocator& allocator, const void* ptr, usize size) noexcept;

    // Use macros ASSERT and ASSERT_DEBUG
    void assert_default_(bool condition, const char* file_name, int line, const char* text) noexcept;
    void assert_debug_(bool condition, const char* file_name, int line, const char* text) noexcept;

    // Base class for snapshot type erasure (used by Allocator virtual interface)
    struct SnapshotBase
    {
        virtual ~SnapshotBase() = default;
    };

    // Template snapshot wrapper - use Snapshot<YourData> instead of inheriting
    template<typename T>
    struct Snapshot : SnapshotBase
    {
        T data;
    };

    // If you want to use custom allocator, you must extend from this base class
    //  or you can use already implemented ones below
    struct Allocator
    {
        virtual ~Allocator() = default;

        virtual void* allocate(usize size, u16 alignment = sizeof(void*)) = 0;
        virtual void* reallocate(void* ptr, usize old_size, usize new_size) = 0;
        virtual SnapshotBase* snapshot() = 0;
        virtual void rewind(SnapshotBase* snapshot) = 0;
        virtual void reset() = 0;
        virtual void cleanup() = 0;
        virtual void display_content() = 0;
    };

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
        bool is_valid_index(usize index) const noexcept { return index < _count; }

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
        usize size_of_t() const noexcept { return ALIGNMENT(sizeof(T), alignof(T)); }

        T&& pop() noexcept
        {
            ASSERT(_count > 0, "Cannot pop from empty array");
            return std::move(_data[--_count]);
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
                ASSERT_DEBUG(_data != nullptr);
                _capacity = needed_capacity;
            } else {
                const auto needed_capacity_bytes = needed_capacity * ALIGNMENT(sizeof(T), alignof(T));
                T* new_data;
                if (_allocator)
                    new_data = (T*)_allocator->allocate(needed_capacity_bytes);
                else
                    new_data = (T*)SL_ARRAY_REALLOC(nullptr, needed_capacity_bytes);
                ASSERT_DEBUG(new_data != nullptr);
                const auto new_count = needed_capacity < _count ? needed_capacity : _count;
                // Relocate bitwise, matching resize()'s realloc model: these
                // arrays hold relocatable values (heap pointers stay valid),
                // and T need not be move-assignable.
                if (new_count > 0)
                    SL_ARRAY_MEMCOPY(new_data, _data, new_count * sizeof(T));
                for (usize i = 0; i < new_count; ++i)
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

        // Array& operator=(Array<T> arr) = delete;
        // Array& operator=(const Array<T>&& arr) = delete;
        // Array& operator=(const Array& arr) = delete;
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

        bool is_valid_index(usize index) const noexcept { return index < _count; }
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

        T&& pop() noexcept
        {
            ASSERT(_count > 0, "Cannot pop from empty array");
            return std::move(_data[--_count]);
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
        usize size_of_t() const noexcept { return ALIGNMENT(sizeof(T), alignof(T)); }
    };

    Allocator* get_global_allocator() noexcept;
    void set_global_allocator(Allocator& allocator) noexcept;
    // Deallocates global allocator. You don't need to call it before calling set_global_allocator(), it will be done for you
    void cleanup_global_allocator() noexcept;
    // Allocates some memory from the global allocator. 0 alignment equals no alignment
    void* temp_alloc(usize size, u16 alignment = sizeof(void*)) noexcept;
    // Make a "snapshot" of current state of the global allocator,
    //  in order to restore it later (basically deallocates used resources) by calling temp_end(snapshot);
    SnapshotBase* temp_begin() noexcept;
    // Restores state of the global allocator of when temp_begin() was called
    //  (!!! This doesnt actually free's any resources)
    void temp_end(SnapshotBase* snapshot) noexcept;
    // Resets (but not free's!!) global allocator
    //  Only recomended if you know that you not use resources allocated by global allocator.
    //  Some ezbuild.h functions uses global_allocator, for example to strdup file names.
    // Better do:
    //   SnapshotBase* snapshot = temp_begin();
    //   {
    //     // do some temporary allocations...
    //   }
    //   temp_end(snapshot);
    //
    // Or:
    //   {
    //      ScopedAllocator temp; // will call temp_end() at the end of the scope automatically
    //      // do some temporary allocations...
    //   }
    void temp_reset() noexcept;

    // Saves state of the local/global allocator in the Constructor and restores state at the Destructor.
    struct ScopedAllocator
    {
        Allocator* _allocator_ref;
        SnapshotBase* _snapshot;

        ScopedAllocator(Allocator& allocator)
        {
            _allocator_ref = &allocator;
            _snapshot = _allocator_ref->snapshot();
        }
        ScopedAllocator()
        {
            _allocator_ref = get_global_allocator();
            _snapshot = _allocator_ref->snapshot();
        }
        ~ScopedAllocator()
        {
            _allocator_ref->rewind(_snapshot);
        }
    };

    struct ArenaSnapshotData
    {
        usize region_index = 0;
        usize index = 0;
    };
    using ArenaSnapshot = Snapshot<ArenaSnapshotData>;

    struct ArenaRegion
    {
        char* data = nullptr;
        usize cursor = 0;
        usize capacity = 0;
    };

    struct ArenaAllocator : Allocator
    {
    private:
        Array<ArenaRegion> _regions;
        usize _current_region_index;
        usize _region_size;
    public:
        ArenaAllocator(usize default_region_size = SL_ALLOCATOR_INIT_SIZE)
        {
            _region_size = default_region_size;
            _current_region_index = 0;
        }

        inline Array<ArenaRegion>& regions() { return _regions; }
        inline usize current_region_index() { return _current_region_index; }
        inline usize region_size() { return _region_size; }

        void* allocate(usize size, u16 alignment = sizeof(void*)) override;
        void* reallocate(void* ptr, usize old_size, usize new_size) override;
        SnapshotBase* snapshot() override;
        void rewind(SnapshotBase* snapshot) override;
        void reset() override;
        void cleanup() override;
        void display_content() override;
    };

    struct LinearSnapshotData
    {
        usize index = 0;
    };
    using LinearSnapshot = Snapshot<LinearSnapshotData>;
    struct LinearAllocator : Allocator
    {
        char* data;
        usize cursor;
        usize total_size;

        LinearAllocator(usize total_size = SL_ALLOCATOR_INIT_SIZE)
        {
            this->data = nullptr;
            this->cursor = 0;
            this->total_size = total_size;
        }

        void* allocate(usize size, u16 alignment = sizeof(void*)) override;
        void* reallocate(void* ptr, usize old_size, usize new_size) override;
        SnapshotBase* snapshot() override;
        void rewind(SnapshotBase* snapshot) override;
        void reset() override;
        void cleanup() override;
        void display_content() override;
    };

    struct StackSnapshotData
    {
        void* current = nullptr;
    };
    using StackSnapshot = Snapshot<StackSnapshotData>;
    struct StackAllocator : Allocator
    {
        void* begin;
        void* end;
        void* current;
        usize total_size;

        StackAllocator(usize total_size = SL_ALLOCATOR_INIT_SIZE)
        {
            this->begin = nullptr;
            this->current = nullptr;
            this->end = nullptr;
            this->total_size = total_size;
        }
        void* allocate(usize size, u16 alignment = sizeof(void*)) override;
        void* reallocate(void* ptr, usize old_size, usize new_size) override;
        SnapshotBase* snapshot() override;
        void rewind(SnapshotBase* snapshot) override;
        void reset() override;
        void cleanup() override;
        void display_content() override;
    };

    typedef struct PoolChunk {
        struct PoolChunk* next;
    } PoolChunk;
    struct PoolSnapshotData
    {
        PoolChunk* current_chunk = nullptr;
    };
    using PoolSnapshot = Snapshot<PoolSnapshotData>;
    struct PoolAllocator : Allocator
    {
        PoolChunk* root;
        PoolChunk* pool;
        usize chunk_count;
        usize chunk_size;

        PoolAllocator(usize chunk_count = 32, usize chunk_size = SL_ALLOCATOR_INIT_SIZE / 32)
        {
            this->root = nullptr;
            this->pool = nullptr;
            this->chunk_count = chunk_count;
            this->chunk_size = chunk_size;
        }

        void* allocate(usize size, u16 alignment = sizeof(void*)) override; // Alignment is ignore in this one
        void* reallocate(void* ptr, usize old_size, usize new_size) override;
        SnapshotBase* snapshot() override;
        void rewind(SnapshotBase* snapshot) override;
        void reset() override;
        void cleanup() override;
        void display_content() override;
    };

    // 0 = Not equals, 1.. = Equals
    typedef Sl::u64(*Hasher_fn) (Sl::u64 seed, const void* key, Sl::usize key_len);

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
                    slot.key = std::move(key);
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

        V* get(const K& key) noexcept
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
            Array<Entry> new_table {_table.allocator()};
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
            for (usize i = 0; i < _table.capacity(); ++i) {
                auto& slot = _table[i];
                if (slot.hash >= FIRST_VALID_HASH) {
                    slot.hash = FREE_HASH;
                    slot.value.~V();
                }
            }
            _count = 0;
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

    enum LogLevel {
        LOG_EMPTY = 0, // EMPTY = doesn't print it's level (but others do)
        LOG_TRACE,
        LOG_INFO,
        LOG_WARNING,
        LOG_ERROR,
        LOG_DISABLE
    };

    // call this functions
    void log(const char* const format, ...) SL_PRINTF_FORMATER(1, 2); // LOG_EMPTY
    void log_level(LogLevel level, const char* const format, ...) SL_PRINTF_FORMATER(2, 3);
    void log_trace(const char* const format, ...) SL_PRINTF_FORMATER(1, 2);
    void log_info(const char* const format, ...) SL_PRINTF_FORMATER(1, 2);
    void log_warning(const char* const format, ...) SL_PRINTF_FORMATER(1, 2);
    void log_error(const char* const format, ...) SL_PRINTF_FORMATER(1, 2);

    typedef void (*Logger_handler)(LogLevel level, const char* const format, va_list args);

    void log_set_current(Logger_handler log);
    Logger_handler log_get_current();

    // Here's already implemented log function.
    // You can still create a custom one, and then set it using: log_set_current(your_log_function)
    void logger_default(LogLevel level, const char* const format, va_list args);
    void logger_colored(LogLevel level, const char* const format, va_list args);
    void logger_muted(LogLevel level, const char* const format, va_list args);

    // Useful for changing log for current scope, for example: if you want to mute logger.
    struct ScopedLogger
    {
        ScopedLogger(Logger_handler new_logger) {
            saved_logger = log_get_current();
            log_set_current(new_logger);
        }
        ~ScopedLogger() {
            log_set_current(saved_logger);
        }
        Logger_handler saved_logger;
    };

    #define SV_LIT(str_lit) StrView(str_lit, sizeof(str_lit) - sizeof(str_lit[0]), true, false)
    #define SV_FORMAT "%.*s"
    #define SV_ARG(sv) (int)((sv).size), (sv).data
    struct StrView
    {
        const char* data;
        usize size;
        u16 is_null_terminated;
        u16 is_wide;
        u32 reserved;

        static const usize INVALID_INDEX = -1;

        StrView(const char* str, usize size, bool is_null_terminated, bool is_wide);
        StrView(const char* str, usize size);
        StrView(const char* str);
        StrView(const wchar_t* wstr);
        StrView sub_view(usize start_index, usize size);
        StrView chop_left(usize count) noexcept;
        StrView chop_right(usize count) noexcept;
        StrView chop_left_by_delimeter(StrView delim) noexcept;
        StrView chop_right_by_delimeter(StrView delim) noexcept;
        usize trim() noexcept;
        usize trim_left() noexcept;
        usize trim_right() noexcept;
        usize trim_left_char(char ch) noexcept;
        usize trim_right_char(char ch) noexcept;
        bool equals(StrView sv) const noexcept;
        bool starts_with(StrView prefix) const noexcept;
        bool ends_with(StrView suffix) const noexcept;
        bool contains(char ch) const noexcept;
        bool contains(StrView word) const noexcept;
        bool split_by_char(LocalArray<StrView>& array_out, char character) const noexcept;
        usize find_first(char ch) const noexcept;
        usize find_first(StrView word) const noexcept;
        usize find_first_until(char ch, char until) const noexcept;
        usize find_last(char ch) const noexcept;
        usize find_last(StrView word) const noexcept;
        usize find_last_until(char ch, char until) const noexcept;
        usize find_first_of_chars(StrView chars) const noexcept;
        usize find_last_of_chars(StrView chars) const noexcept;
        s32 compare(StrView sv) const noexcept;
        char first() const noexcept;
        char last() const noexcept;
        usize at(usize index) const noexcept;
        bool is_empty() const noexcept;
        bool is_valid_index(usize index) const noexcept;
        bool contains_non_ascii_char() const noexcept;
        bool operator==(const StrView& right) const noexcept;
        bool operator!=(const StrView& right) const noexcept;
        static u64 hash(usize seed, const StrView& key, Hasher_fn callback);
    };

    #define SB_FORMAT "%.*s"
    #define SB_ARG(sv) (int)((sv).count()), (sv).data()
    struct StrBuilder : Array<char>
    {
        StrBuilder(Allocator* allocator = nullptr)
        {
            _allocator = allocator;
        }

        StrBuilder& append(char ch) noexcept;
        StrBuilder& append(const char* str, usize size) noexcept;
        StrBuilder& append(StrView str) noexcept;
        StrBuilder& append_escaped(StrView command, bool force_escape = false); // Usefull for paths
        StrBuilder& append_null(bool update_count = true) noexcept;
        StrBuilder& appendf(const char* format, ...) noexcept SL_PRINTF_FORMATER(2, 3);
        StrBuilder& align(u16 alignment) noexcept;
        StrBuilder& reset() noexcept;
        StrBuilder& cleanup() noexcept;
        Allocator* current_allocator() const noexcept;
        char* to_cstring_alloc(Allocator* allocator = nullptr) const noexcept;
        StrView to_string_view(bool is_null_terminated = false, bool is_wide = false) const noexcept;
        StrBuilder& copy_from(const StrBuilder& other) noexcept;
        StrBuilder& operator<<(StrView str) noexcept; // calls append_escaped()
        StrBuilder& operator<<(const char* str) noexcept; // calls append_escaped()
        StrBuilder& operator<<(char val) noexcept;
        StrBuilder& operator<<(int val) noexcept;
        StrBuilder& operator<<(long val) noexcept;
        StrBuilder& operator<<(long long val) noexcept;
        StrBuilder& operator<<(unsigned val) noexcept;
        StrBuilder& operator<<(unsigned long val) noexcept;
        StrBuilder& operator<<(unsigned long long val) noexcept;
        StrBuilder& operator<<(double val) noexcept;
        bool operator==(const StrBuilder& other) noexcept;
        static u64 hash(usize seed, const StrBuilder& key, Hasher_fn callback);
    };

    // Example:
    //    auto relPtr  = REL_PTR(int, arr, 123);
    //    auto origPtr = relPtr[arr]; // returns 123
    #define REL_PTR(RelativeT, base, ptr) Sl::RelPtr<decltype(base), decltype(ptr), RelativeT>((base), (ptr));

    // RelativeT must be signed numeric type, since you want to know offset from left and right
	template <typename BasePtrT, typename PtrT, typename RelativeT = int32_t>
	struct RelPtr
	{
		RelativeT _relative_index = 0;
		RelPtr(const BasePtrT base_ptr, const PtrT ptr)
		{
    		const ptrdiff_t offset = ((ptrdiff_t)ptr) - ((ptrdiff_t)base_ptr);

            const auto bits = sizeof(RelativeT) * 8 - 1;
           	const s64 max_offset = ((1ll << (bits - 1)) - 1) * 2;
    		const s64 min_offset = ((1LL << bits));
    		ASSERT(offset <= max_offset && offset >= min_offset,
    		    "Offset exceded max size of relative_index type, change RelativeT to a bigger size");

            _relative_index = offset;
		}

		PtrT to_normal(const BasePtrT base_ptr) noexcept
		{
			return (PtrT) ((ptrdiff_t) base_ptr + (ptrdiff_t)_relative_index);
		}

		PtrT operator[](const BasePtrT base_ptr) noexcept
		{
    		return to_normal(base_ptr);
		}
	};

    class Defer__ {
    public:
        using FuncPtr = std::function<void()>;

        Defer__(FuncPtr func) : func_(func) {}
        ~Defer__() noexcept { func_(); }
        Defer__& operator=(const Defer__&) = delete;
        Defer__(const Defer__&) = delete;
    private:
        FuncPtr func_;
    };

    #ifndef DEFER_CONCAT
    #  define DEFER_CONCAT(a, b) a ## b
    #endif // !DEFER_CONCAT
    #ifndef DEFER_CONCAT_EXPAND
    #  define DEFER_CONCAT_EXPAND(a, b) DEFER_CONCAT(a,b)
    #endif // !DEFER_CONCAT_EXPAND
    #define defer(code) Sl::Defer__ DEFER_CONCAT_EXPAND(defer,__COUNTER__)([&](){code ;});
} // namespace Sl

#ifdef SL_IMPLEMENTATION
#include <new>
namespace Sl
{
    static SL_THREAD_LOCAL Allocator* _global_alloc = nullptr;
    static SL_THREAD_LOCAL Logger_handler logger_handler = logger_default;

    Allocator* get_global_allocator() noexcept
    {
        if (_global_alloc == nullptr) {
            static SL_THREAD_LOCAL ArenaAllocator _default_global_alloc(SL_GLOBAL_ALLOCATOR_INIT_SIZE);
            _global_alloc = &_default_global_alloc;
        }
        return _global_alloc;
    }

    void set_global_allocator(Allocator& new_allocator) noexcept
    {
        get_global_allocator()->cleanup();
        _global_alloc = &new_allocator;
    }

    void cleanup_global_allocator() noexcept
    {
        get_global_allocator()->cleanup();
    }

    void* temp_alloc(usize size, u16 alignment) noexcept
    {
        return get_global_allocator()->allocate(size, alignment);
    }

    SnapshotBase* temp_begin() noexcept
    {
        return get_global_allocator()->snapshot();
    }

    void temp_end(SnapshotBase* snapshot) noexcept
    {
        get_global_allocator()->rewind(snapshot);
    }

    void temp_reset() noexcept
    {
        get_global_allocator()->reset();
    }

    void memory_copy(void* dest, usize dest_size, const void* src, usize src_size) noexcept;

    void log(const char* const format, ...);

    void* LinearAllocator::allocate(usize size, u16 alignment)
    {
        size = ALIGNMENT(size, alignment);
        if (data == nullptr) {
            if (total_size == 0) total_size = SL_GLOBAL_ALLOCATOR_INIT_SIZE;
            const auto allocated_size = MAX(total_size, size);
            this->total_size = allocated_size;
            this->data = (char*)SL_ALLOCATOR_MALLOC(allocated_size);
            ASSERT_DEBUG(data != nullptr);
        }
        ASSERT(cursor <= total_size, "Cursor went past total size (possible memory corruption)");

        if (cursor + size > total_size)
            return (void*)NULL;

        void* new_ptr = data + cursor;
        cursor += size;
        return new_ptr;
    }

    void* LinearAllocator::reallocate(void* ptr, usize old_size, usize new_size)
    {
        void* new_ptr = allocate(new_size);
        if (ptr == nullptr) return new_ptr;

        memory_copy(new_ptr, new_size, ptr, old_size);
        return new_ptr;
    }

    void LinearAllocator::reset()
    {
        cursor = 0;
    }

    void LinearAllocator::cleanup()
    {
        SL_ALLOCATOR_FREE(data);
        data = nullptr;
        cursor = 0;
        total_size = 0;
    }

    void LinearAllocator::rewind(SnapshotBase* _snapshot)
    {
        if (_snapshot == nullptr) return;
        LinearSnapshot* snapshot = static_cast<LinearSnapshot*>(_snapshot);
        this->cursor = snapshot->data.index;
    }

    SnapshotBase* LinearAllocator::snapshot()
    {
        auto current_cursor = cursor;
        auto* snapshot = (LinearSnapshot*) temp_alloc(sizeof(LinearSnapshot));
        ASSERT_NOT_NULL(snapshot);
        ::new (snapshot) LinearSnapshot;

        snapshot->data.index = current_cursor;
        return snapshot;
    }

    void LinearAllocator::display_content()
    {
        log("|---------------------\n");
        log("|Linear allocator:\n");
        log("|---------------------\n");
        log("|  capacity: %zu\n", total_size);
        log("|  cursor: %zu\n", cursor);
        log("|---------------------\n");
    }

    void* StackAllocator::allocate(usize size, u16 alignment)
    {
        size = ALIGNMENT(size, alignment);
        if (begin == nullptr) {
            if (total_size < 1) total_size = SL_GLOBAL_ALLOCATOR_INIT_SIZE;
            begin = (char*)SL_ALLOCATOR_MALLOC(total_size);
            this->current = this->begin;
            this->end = ((char*)this->begin) + total_size;
        }

        if ((char*)current + size > end)
            return nullptr;

        void* ptr = (void*)current;
        current = (char*)current + size;
        return ptr;
    }

    void* StackAllocator::reallocate(void* ptr, usize old_size, usize new_size)
    {
        void* new_ptr = allocate(new_size);
        if (ptr == nullptr) return new_ptr;

        memory_copy(new_ptr, new_size, ptr, old_size);
        return new_ptr;
    }

    SnapshotBase* StackAllocator::snapshot()
    {
        if (this->begin == nullptr || this->end == nullptr)  // initilize memory if empty
            allocate(0, 0);

        auto* current_ptr = this->current;
        auto* snapshot = (StackSnapshot*) temp_alloc(sizeof(StackSnapshot));
        ASSERT_NOT_NULL(snapshot);
        ::new (snapshot) StackSnapshot;

        if (current_ptr == nullptr) current_ptr = this->begin;
        snapshot->data.current = current_ptr;
        return snapshot;
    }

    void StackAllocator::rewind(SnapshotBase* _snapshot)
    {
        if (_snapshot == nullptr) return;
        StackSnapshot* snapshot = static_cast<StackSnapshot*>(_snapshot);

        ASSERT_TRUE(snapshot->data.current >= (void*)this->begin);
        ASSERT_TRUE(snapshot->data.current <= (void*)this->end);
        this->current = snapshot->data.current;
    }

    void StackAllocator::reset()
    {
        current = begin;
    }

    void StackAllocator::cleanup()
    {
        SL_ALLOCATOR_FREE(begin);
        begin = nullptr;
        end = nullptr;
        current = nullptr;
    }

    void StackAllocator::display_content()
    {
        log("|---------------------\n");
        log("|Stack allocator:\n");
        log("|---------------------\n");
        log("|  current: %p\n", current);
        log("|  begin: %p\n", begin);
        log("|  end: %p\n", end);
        log("|---------------------\n");
    }

    void* PoolAllocator::allocate(usize size, u16 alignment)
    {
        UNUSED(alignment);
        if (this->root == nullptr) {
            const usize total_size = (usize)chunk_count * (usize)chunk_size;

            if(this->chunk_count < 1) this->chunk_count = 32;
            if(this->chunk_size < 1) this->chunk_size = SL_GLOBAL_ALLOCATOR_INIT_SIZE / 32;
            this->pool = nullptr;
            this->root = (PoolChunk*)SL_ALLOCATOR_MALLOC(total_size);
            ASSERT_DEBUG(this->root != nullptr);

            reset();
        }
        ASSERT(chunk_size >= size, "Requested size bigger than chunk size of Pool");
        ASSERT(chunk_count > 0, "Chunk count must be bigger than 0");
        ASSERT(chunk_size > 0, "Chunk size must be bigger than 0");

        if (pool == NULL
            || size > chunk_size
            || chunk_count == 0
            || chunk_size == 0
        )
            return (void*)NULL;

        PoolChunk* ptr = pool;
        pool = ptr->next;
        return ptr;
    }

    void* PoolAllocator::reallocate(void* ptr, usize old_size, usize new_size)
    {
        void* new_ptr = allocate(new_size);
        if (ptr == nullptr) return new_ptr;

        memory_copy(new_ptr, new_size, ptr, old_size);
        return new_ptr;
    }

    SnapshotBase* PoolAllocator::snapshot()
    {
        auto* current_chunk = this->pool;
        auto* snapshot = (PoolSnapshot*) temp_alloc(sizeof(PoolSnapshot));
        ASSERT_NOT_NULL(snapshot);
        ::new (snapshot) PoolSnapshot;
        if (current_chunk == nullptr) {
            if (this->root == nullptr) allocate(0, 0); // initilize memory
            current_chunk = this->root;
        }

        snapshot->data.current_chunk = current_chunk;
        return snapshot;
    }

    void PoolAllocator::rewind(SnapshotBase* _snapshot)
    {
        if (_snapshot == nullptr) return;

        PoolSnapshot* snapshot = static_cast<PoolSnapshot*>(_snapshot);
        this->pool = snapshot->data.current_chunk;
    }

    void PoolAllocator::reset()
    {
        const usize total_size = chunk_count * chunk_size;
        ASSERT(chunk_count > 0, "Chunk count must be bigger than 0");
        ASSERT(chunk_size > 0, "Chunk size must be bigger than 0");

        pool = root;
        PoolChunk* current_block = pool;

        for (usize i = chunk_size; i < total_size; i += chunk_size)
        {
            current_block->next = (PoolChunk*)((char*)root + i);
            current_block = current_block->next;
        }
        current_block->next = NULL;
    }

    void PoolAllocator::cleanup()
    {
        SL_ALLOCATOR_FREE(root);
        root = nullptr;
        pool = nullptr;
    }

    void PoolAllocator::display_content()
    {
        // @TODO not really useful, must traverse all chunks
        log("|---------------------\n");
        log("|Pool allocator:\n");
        log("|---------------------\n");
        log("|  chunk_count: %zu\n", chunk_count);
        log("|  chunk_size: %zu\n", chunk_size);
        log("|  current_chunk: %p\n", pool);
        log("|  root_chunk: %p\n", root);
        log("|---------------------\n");
    }

    void* ArenaAllocator::allocate(usize size, u16 alignment)
    {
        const usize aligned_size = ALIGNMENT(size, alignment);

        ArenaRegion* region = NULL;
        bool found_region = false;
        for (usize i = this->_current_region_index; i < _regions.count(); ++i) {
            region = &_regions.get(i);
            if (region->cursor + aligned_size <= region->capacity || region->data == NULL) {
                found_region = true;
                this->_current_region_index = i;
                break;
            }
        }
        if (!found_region) {
            _regions.push(ArenaRegion{});
            region = &_regions.last();
            this->_current_region_index = _regions.count() - 1;
        }

        if (region->data == NULL) {
            const usize allocated_size = MAX(aligned_size, _region_size);
            region->cursor = 0;
            region->capacity = allocated_size;
            region->data = (char*)SL_ALLOCATOR_MALLOC(allocated_size);
            ASSERT_DEBUG(region->data != nullptr);
        }
        void* new_ptr = (void*)(region->data + region->cursor);
        region->cursor += aligned_size;
        return new_ptr;
    }

    void* ArenaAllocator::reallocate(void* ptr, usize old_size, usize new_size)
    {
        void* new_ptr = allocate(new_size);
        if (ptr == nullptr) return new_ptr;

        memory_copy(new_ptr, new_size, ptr, old_size);
        return new_ptr;
    }

    void ArenaAllocator::reset()
    {
        for (auto& region : _regions) {
            region.cursor = 0;
        }
        _current_region_index = 0;
    }

    void ArenaAllocator::cleanup()
    {
        for (auto& region : _regions) {
            SL_ALLOCATOR_FREE(region.data);
        }
        _regions.cleanup();
        _current_region_index = 0;
    }

    void ArenaAllocator::rewind(SnapshotBase* _snapshot)
    {
        if (_snapshot == nullptr) return;
        ArenaSnapshot* snapshot = static_cast<ArenaSnapshot*>(_snapshot);

        if (snapshot->data.region_index < _regions.count()) {
            auto& region = _regions[snapshot->data.region_index];
            region.cursor = snapshot->data.index;
            _current_region_index = snapshot->data.region_index;
            for (usize i = snapshot->data.region_index + 1; i < _regions.count(); ++i)
                _regions[i].cursor = 0;
        }
    }

    SnapshotBase* ArenaAllocator::snapshot()
    {
        if (_regions.count() < 1) allocate(0, 0); // initilize regions if empty

        const auto current_index = _regions[_current_region_index].cursor;
        ArenaSnapshot* snapshot = (ArenaSnapshot*) temp_alloc(sizeof(ArenaSnapshot), alignof(ArenaSnapshot));
        ASSERT_NOT_NULL(snapshot);
        ::new (snapshot) ArenaSnapshot;
        snapshot->data.region_index = _current_region_index;
        snapshot->data.index = current_index;
        return snapshot;
    }

    void ArenaAllocator::display_content()
    {
        u32 iter = 0;
        log("|---------------------\n");
        log("|Arena allocator:\n");
        log("|---------------------\n");
        for (auto& region : _regions) {
            log("|-region %d", iter++);
            if (iter - 1 == _current_region_index) log(" <--");
            log("\n");
            log("|  capacity: %zu\n", region.capacity);
            log("|  cursor: %zu\n", region.cursor);
            log("|---------------------\n");
        }
    }

        StrBuilder& StrBuilder::append(const char* str, usize size) noexcept
    {
        if (size > 0 && str) {
            push_many(str, size);
        }
        return *this;
    }

    StrBuilder& StrBuilder::append(char ch) noexcept
    {
        append(&ch, sizeof(ch));
        return *this;
    }

    StrBuilder& StrBuilder::append_null(bool update_count) noexcept
    {
        append("\0", 1);
        if (!update_count) _count -= 1;
        return *this;
    }

    StrBuilder& StrBuilder::append(StrView str) noexcept
    {
        if (str.data) {
            append(str.data, str.size);
        }
        return *this;
    }

    StrBuilder& StrBuilder::append_escaped(StrView command, bool force_escape)
    {
        if (command.size == 0) return *this;
        if (!force_escape && command.find_first_of_chars(" \t\n\v\"") == StrView::INVALID_INDEX) {
            // no need to quote
            append(command.data, command.size);
        } else {
            usize backslashes = 0;
            append("\"", 1);
            for (usize i = 0; i < command.size; ++i) {
                char x = command.data[i];
                if (x == '\\') {
                    backslashes += 1;
                } else {
                    if (x == '\"') {
                        for (usize k = 0; k < 1+backslashes; ++k) {
                            append('\\');
                        }
                    }
                    backslashes = 0;
                }
                append(x);
            }
            for (usize i = 0; i < backslashes; ++i) {
                append('\\');
            }
            append('"');
        }
        return *this;
    }

    StrBuilder& StrBuilder::align(u16 alignment) noexcept
    {
        usize aligned_size = ALIGNMENT(_count, alignment) - _count;
        for (; aligned_size > 0; --aligned_size) {
            append_null();
        }
        return *this;
    }

    StrBuilder& StrBuilder::reset() noexcept
    {
        Array::clear();
        return *this;
    }

    StrBuilder& StrBuilder::cleanup() noexcept
    {
        Array::cleanup();
        return *this;
    }

    StrBuilder& StrBuilder::appendf(const char* format, ...) noexcept
    {
        if (!format) {
            ASSERT_DEBUG(format);
            return *this;
        }

        va_list args;
        va_start(args, format);
        va_list args_copy;
        va_copy(args_copy, args);
        auto size = vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);
        if (size > 0) {
            if (_allocator) {
                auto* str = (char*)_allocator->allocate(size+1);
                va_copy(args_copy, args);
                vsnprintf(str, size+1, format, args_copy);
                va_end(args_copy);
                append(str, static_cast<usize>(size));
            } else {
                char buf[4096];
                va_copy(args_copy, args);
                vsnprintf(buf, sizeof(buf), format, args_copy);
                va_end(args_copy);
                append(buf, static_cast<usize>(size));
            }
        }
        va_end(args);
        return *this;
    }

    Allocator* StrBuilder::current_allocator() const noexcept
    {
        return _allocator;
    }

    char* StrBuilder::to_cstring_alloc(Allocator* allocator) const noexcept
    {
        usize size = _count * sizeof(char);
        char* str;
        if (allocator == nullptr) allocator = _allocator;
        if (allocator) str = (char*)allocator->allocate(size + 1);
        else return nullptr;
        ASSERT_DEBUG(str);

        if (_data && size > 0)
            memory_copy(str, size, _data, size);
        str[size] = '\0';
        return str;
    }

    Sl::StrView StrBuilder::to_string_view(bool is_null_terminated, bool is_wide) const noexcept
    {
        return Sl::StrView(_data, _count * sizeof(char), is_null_terminated, is_wide);
    }

    StrBuilder& StrBuilder::operator<<(StrView str) noexcept
    {
        append_escaped(str);
        return *this;
    }

    StrBuilder& StrBuilder::operator<<(const char* str) noexcept
    {
        append_escaped(str);
        return *this;
    }

    StrBuilder& StrBuilder::operator<<(char val) noexcept
    {
        append(val);
        return *this;
    }
    StrBuilder& StrBuilder::operator<<(int val) noexcept
    {
        char buf[64];
        sprintf(buf, "%d", val);
        append(buf);
        return *this;
    }
    StrBuilder& StrBuilder::operator<<(long val) noexcept
    {
        char buf[64];
        sprintf(buf, "%ld", val);
        append(buf);
        return *this;
    }
    StrBuilder& StrBuilder::operator<<(long long val) noexcept
    {
        char buf[128];
        sprintf(buf, "%lld", val);
        append(buf);
        return *this;
    }
    StrBuilder& StrBuilder::operator<<(unsigned val) noexcept
    {
        char buf[64];
        sprintf(buf, "%u", val);
        append(buf);
        return *this;
    }
    StrBuilder& StrBuilder::operator<<(unsigned long val) noexcept
    {
        char buf[64];
        sprintf(buf, "%lu", val);
        append(buf);
        return *this;
    }
    StrBuilder& StrBuilder::operator<<(unsigned long long val) noexcept
    {
        char buf[64];
        sprintf(buf, "%llu", val);
        append(buf);
        return *this;
    }
    StrBuilder& StrBuilder::operator<<(double val) noexcept
    {
        char buf[128];
        sprintf(buf, "%.17g", val);
        append(buf);
        return *this;
    }
    StrBuilder& StrBuilder::copy_from(const StrBuilder& other) noexcept
    {
        resize(other._capacity);
        _count = other._count;
        memory_copy((void*)_data, _count, (void*)other._data, other._count);
        return *this;
    }
    bool StrBuilder::operator==(const StrBuilder& other) noexcept
    {
        if (_count != other._count) return false;
        return memory_equals((void*)_data, _count, (void*)other._data, other._count);
    }
    u64 StrBuilder::hash(usize seed, const StrBuilder& key, Hasher_fn callback)
    {
        ASSERT(callback, "Hasher function could not be null");
        return callback(seed, key.data(), key.count());
    }

    void assert_default_(bool condition, const char* file_name, int line, const char* text) noexcept
    {
    #if SL_ENABLE_ASSERT == 1
        if (!condition)
        {
            fprintf(stderr, "%s:%d: [ASSERT] %s\n", file_name, line, text);
            DEBUG_BREAK();
            exit(1);
        }
    #else
        (void)condition; (void)file_name; (void)line; (void)text;
    #endif // SL_ENABLE_ASSERT == 1
    }

    void assert_debug_(bool condition, const char* file_name, int line, const char* text) noexcept
    {
    #if defined(_DEBUG) && SL_ENABLE_ASSERT == 1
        if (!condition)
        {
            fprintf(stderr, "%s:%d: [ASSERT] %s\n", file_name, line, text);
            DEBUG_BREAK();
        }
    #else
        (void)(condition); (void)(file_name); (void)(line); (void)(text);
    #endif // _DEBUG && SL_ENABLE_ASSERT == 1
    }

    bool memory_equals(const void* ptr1, usize ptr1_size, const void* ptr2, usize ptr2_size) noexcept
    {
        return memory_compare(ptr1, ptr1_size, ptr2, ptr2_size) == 0;
    }

    ptrdiff_t memory_compare(const void* ptr1, usize ptr1_size, const void* ptr2, usize ptr2_size) noexcept
    {
        ASSERT_NOT_NULL(ptr1);
        ASSERT_NOT_NULL(ptr2);

        if (ptr1_size != ptr2_size) return ((ptrdiff_t) ptr1_size - (ptrdiff_t)ptr2_size);

        return memcmp(ptr1, ptr2, ptr2_size);
    }

    void memory_copy(void* dest, usize dest_size, const void* src, usize src_size) noexcept
    {
        ASSERT_NOT_NULL(src);
        ASSERT_NOT_NULL(dest);
        if (src == nullptr || dest == nullptr) return;

        const usize copy_size = MIN(dest_size, src_size);
        memcpy(dest, src, copy_size);
    }

    void memory_zero(void* src, usize size) noexcept
    {
        memory_set(src, 0, size);
    }

    void memory_set(void* src, u8 data, usize size) noexcept
    {
        ASSERT_NOT_NULL(src);
        if (src == nullptr) return;

        for (usize i = 0; i < size; ++i)
            ((char*) src)[i] = static_cast<char>(data);
    }

    usize memory_strlen(const char* ptr) noexcept
    {
        if (ptr == nullptr) return 0;

        usize len = 0;
        while (ptr[len] != '\0') { len +=1; }
        return len;
    }

    usize memory_wstrlen(const wchar_t* ptr_) noexcept
    {
        const char* ptr = (const char*) ptr_;
        if (ptr == nullptr) return 0;

        usize len = 0;
        while (true) {
            bool is_null = true;
            for (usize i = 0; i < sizeof(wchar_t); ++i)
            {
                if (ptr[len + i] != '\0') {
                    is_null = false;
                    break;
                }
            }
            if (is_null) break;
            len += sizeof(wchar_t);
        }
        return len;
    }

    void* memory_format(Allocator& allocator, usize& size_out, const char* format, va_list args) noexcept
    {
        void* ptr = nullptr;
        if (format == nullptr) return ptr;

        va_list args_copy;
        va_copy(args_copy, args);
            size_out = vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);
        if (size_out > 0) {
            ptr = allocator.allocate(size_out + 1);
            size_out = vsnprintf((char*) ptr, size_out + 1, format, args);
        }
        return ptr;
    }

    void* memory_format(Allocator& allocator, usize& size_out, const char* format, ...) noexcept
    {
        va_list args;
        va_start(args, format);
            void* ptr = memory_format(allocator, size_out, format, args);
        va_end(args);
        return ptr;
    }

    void* memory_duplicate(Allocator& allocator, const void* ptr, usize size) noexcept
    {
        if (ptr == nullptr) return nullptr;

        void* new_ptr = allocator.allocate(size + 1);
        memory_copy(new_ptr, size, ptr, size);
        ((char*)new_ptr)[size] = '\0';
        return new_ptr;
    }

    void* memory_duplicate(Allocator& allocator, const void* ptr) noexcept
    {
        if (ptr == nullptr) return nullptr;
        return memory_duplicate(allocator, ptr, memory_strlen((const char*)ptr));
    }

    void log_level(LogLevel level, const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(level, format, args);
        va_end(args);
    }

    void log_set_current(Logger_handler log)
    {
        logger_handler = log;
    }

    Logger_handler log_get_current()
    {
        return logger_handler;
    }

    void logger_default(LogLevel level, const char* const format, va_list args)
    {
        switch (level)
        {
            case LOG_TRACE:
                fprintf(stdout, "[TRACE] ");
                break;
            case LOG_INFO:
                fprintf(stdout, "[INFO] ");
                break;
            case LOG_WARNING:
                fprintf(stdout, "[WARNING] ");
                break;
            case LOG_ERROR:
                fprintf(stdout, "[ERROR] ");
                break;
            case LOG_EMPTY: break;
            case LOG_DISABLE: return;
            default: UNREACHABLE("logger_default");
        }
        vfprintf(stdout, format, args);
    }

    void logger_colored(LogLevel level, const char* const format, va_list args)
    {
        switch (level)
        {
            case LOG_TRACE:
                fprintf(stdout, "\x1b[36m[TRACE] ");
                break;
            case LOG_INFO:
                fprintf(stdout, "\x1b[94m[INFO] ");
                break;
            case LOG_WARNING:
                fprintf(stdout, "\x1b[33m[WARNING] ");
                break;
            case LOG_ERROR:
                fprintf(stdout, "\x1b[31m[ERROR] ");
                break;
            case LOG_EMPTY: break;
            case LOG_DISABLE: return;
            default: UNREACHABLE("logger_colored");
        }

        vfprintf(stdout, format, args);
        printf("\x1b[0m");
    }

    void logger_muted(LogLevel level, const char* const format, va_list args)
    {
        UNUSED(level);
        UNUSED(format);
        UNUSED(args);
    }

    void log(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_EMPTY, format, args);
        va_end(args);
    }
    void log_trace(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_TRACE, format, args);
        va_end(args);
    }
    void log_info(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_INFO, format, args);
        va_end(args);
    }
    void log_warning(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_WARNING, format, args);
        va_end(args);
    }
    void log_error(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_ERROR, format, args);
        va_end(args);
    }

    StrView::StrView(const char* str, usize size)
        : data(str), size(size), is_null_terminated(false), is_wide(false), reserved(0)
    {
    }

    StrView::StrView(const char* str, usize size, bool is_null_terminated, bool is_wide)
        : data(str), size(size), is_null_terminated(is_null_terminated), is_wide(is_wide), reserved(0)
    {
    }

    StrView::StrView(const char* str)
        : data(str), is_null_terminated(true), is_wide(false), reserved(0)
    {
        ASSERT_NOT_NULL(str);
        this->size = memory_strlen(str);
    }

    StrView::StrView(const wchar_t* wstr)
        : is_null_terminated(true), is_wide(true), reserved(0)
    {
        ASSERT_NOT_NULL(wstr);
        this->data = (const char*) wstr;
        this->size = memory_wstrlen(wstr);
    }

    bool StrView::operator==(const StrView& right) const noexcept
    {
        return equals(right);
    }

    bool StrView::operator!=(const StrView& right) const noexcept
    {
        return !equals(right);
    }

    usize StrView::find_first(char ch) const noexcept
    {
        for (usize i = 0; i < size; ++i) {
            if (data[i] == ch) return i;
        }
        return INVALID_INDEX;
    }

    usize StrView::find_first_until(char ch, char until) const noexcept
    {
        for (usize i = 0; i < size; ++i) {
            if (data[i] == ch) return i;
            if (data[i] == until) break;
        }
        return INVALID_INDEX;
    }

    usize StrView::find_last(char ch) const noexcept
    {
        usize index = INVALID_INDEX;
        for (usize i = 0; i < size; ++i) {
            if (data[i] == ch)
                index = i;
        }
        return index;
    }

    usize StrView::find_last_until(char ch, char until) const noexcept
    {
        usize index = INVALID_INDEX;
        for (usize i = 0; i < size; ++i) {
            if (this->data[i] == until)
                break;
            if (this->data[i] == ch)
                index = i;
        }
        return index;
    }

    bool StrView::split_by_char(LocalArray<StrView>& array_out, char character) const noexcept
    {
        array_out.clear();
        usize start = 0;

        for (usize i = 0; i <= size; ++i) {
            if (i == size || data[i] == character) {
                if (i > start) {
                    array_out.push(StrView{data + start, i - start});
                }
                start = i + 1;
            }
        }

        return array_out.count() > 0;
    }


    usize StrView::find_first(StrView word) const noexcept
    {
        if (word.size == 0 || word.size > this->size)
            return INVALID_INDEX;

        const usize limit = this->size - word.size + 1;

        for (usize i = 0; i < limit; ++i) {
            bool match = true;
            for (usize j = 0; j < word.size; ++j) {
                if (this->data[i + j] != word.data[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return i;
            }
        }

        return INVALID_INDEX;
    }

    usize StrView::find_last(StrView word) const noexcept
    {
        if (word.size == 0 || word.size > this->size)
            return INVALID_INDEX;


        usize latest_match = INVALID_INDEX;
        const usize limit = this->size - word.size + 1;

        for (usize i = 0; i < limit; ++i) {
            bool match = true;
            for (usize j = 0; j < word.size; ++j) {
                if (this->data[i + j] != word.data[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                latest_match = i;
            }
        }

        return latest_match;
    }

    usize StrView::find_first_of_chars(StrView chars) const noexcept
    {
        for (usize i = 0; i < size; ++i) {
            for (usize j = 0; j < chars.size; ++j) {
                if (data[i] == chars.data[j]) {
                    return i;
                }
            }
        }
        return INVALID_INDEX;
    }

    usize StrView::find_last_of_chars(StrView chars) const noexcept
    {
        usize index = INVALID_INDEX;
        for (usize i = 0; i < size; ++i) {
            for (usize j = 0; j < chars.size; ++j) {
                if (data[i] == chars.data[j]) {
                    index = i;
                }
            }
        }
        return index;
    }

    bool StrView::starts_with(StrView prefix) const noexcept
    {
        if (prefix.size > size)
            return false;

        return memory_equals(data, prefix.size, prefix.data, prefix.size);
    }

    bool StrView::ends_with(StrView suffix) const noexcept
    {
        if (suffix.size > size)
            return false;

        const usize offset_size = (size - suffix.size);
        return memory_equals(data + offset_size, size - offset_size, suffix.data, suffix.size);
    }

    bool StrView::contains(char ch) const noexcept
    {
        return find_first(ch) != INVALID_INDEX;
    }

    bool StrView::contains(StrView word) const noexcept
    {
        return find_first(word) != INVALID_INDEX;
    }

    usize StrView::trim_left_char(char ch) noexcept
    {
        usize trimed_count = 0;
        while (size > 0 && *data == ch)
        {
            ++trimed_count;
            ++data;
            --size;
        }
        return trimed_count;
    }

    usize StrView::trim_right_char(char ch) noexcept
    {
        usize trimed_count = 0;
        while (size > 0 && *(data + size - 1) == ch) {
            ++trimed_count;
            --size;
        }
        if (trimed_count > 0) is_null_terminated = false;
        return trimed_count;
    }

    usize StrView::trim_left() noexcept
    {
        return trim_left_char(' ');
    }

    usize StrView::trim_right() noexcept
    {
        return trim_right_char(' ');
    }

    usize StrView::trim() noexcept
    {
        usize total = 0;
        total += trim_left();
        total += trim_right();
        return total;
    }

    s32 StrView::compare(StrView sv) const noexcept
    {
        return memory_compare(data, size, sv.data, sv.size);
    }

    bool StrView::equals(StrView sv) const noexcept
    {
        return compare(sv) == 0;
    }

    usize StrView::at(usize index) const noexcept
    {
        if (index >= size)
            return INVALID_INDEX;
        return static_cast<usize>(*(data + index));
    }

    StrView StrView::sub_view(usize start_index, usize end_index)
    {
        ASSERT_TRUE(start_index <= end_index && end_index <= size);
        StrView sub{data + start_index, end_index - start_index, false, (bool)is_wide};
        return sub;
    }

    StrView StrView::chop_left(usize count) noexcept
    {
        ASSERT_TRUE(size >= count);
        StrView chopped{data, count, false, (bool)is_wide};
        data += count;
        size -= count;
        return chopped;
    }

    StrView StrView::chop_right(usize count) noexcept
    {
        ASSERT_TRUE(size >= count);
        StrView chopped{data + (size - count), count, (bool)is_null_terminated, (bool)is_wide};
        if (count > 0) {
            is_null_terminated = false;
            size -= count;
        }
        return chopped;
    }

    StrView StrView::chop_left_by_delimeter(StrView delim) noexcept
    {
        ASSERT_TRUE(!delim.is_empty());

        usize pos = find_first(delim);
        if (pos == INVALID_INDEX) {
            StrView copy = *this;
            this->data = nullptr;
            this->size = 0;
            return copy;
        }

        StrView chopped{data, pos + delim.size, false, (bool)is_wide};
        data += pos + delim.size;
        size -= pos + delim.size;
        return chopped;
    }

    StrView StrView::chop_right_by_delimeter(StrView delim) noexcept
    {
        ASSERT_TRUE(!delim.is_empty());

        usize pos = find_last(delim);
        if (pos == INVALID_INDEX) {
            StrView copy = *this;
            memory_zero(this, sizeof(*this));
            return copy;
        }

        StrView chopped{data + pos, size - pos, false, (bool)is_wide};
        is_null_terminated = false;
        size = pos;
        return chopped;
    }

    bool StrView::is_empty() const noexcept
    {
        return size < 1;
    }

    bool StrView::is_valid_index(usize index) const noexcept
    {
        return index < size;
    }

    char StrView::first() const noexcept
    {
        if (size < 1) return '\0';
        return data[0];
    }

    char StrView::last() const noexcept
    {
        if (size < 1) return '\0';
        return data[size - 1];
    }

    bool StrView::contains_non_ascii_char() const noexcept
    {
        for (usize i = 0; i < size; ++i) {
            if (static_cast<u8>(data[i]) > 0x7F)
                return true;
        }
        return false;
    }

    u64 StrView::hash(usize seed, const StrView& key, Hasher_fn callback)
    {
        ASSERT(callback, "Hasher function could not be null");
        return callback(seed, key.data, key.size);
    }

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
#endif // !SL_IMPLEMENTATION

#endif // !SL_CORE_H