#ifndef SL_BASE_H
#define SL_BASE_H


#include "Sl_Defines.hpp"
#include <stdarg.h>

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
} // namespace Sl
#endif // !SL_BASE_H

#if defined(SL_IMPLEMENTATION)
namespace Sl
{
    struct StrView;
    struct StrBuilder;

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
} // namespace Sl

#endif // !SL_IMPLEMENTATION
