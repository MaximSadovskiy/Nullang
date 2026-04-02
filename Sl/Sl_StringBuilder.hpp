#ifndef SL_STRINGBUILDER_H
#define SL_STRINGBUILDER_H

#include "Sl_Defines.hpp"
#include "Sl_Array.hpp"

#define SB_FORMAT "%.*s"
#define SB_ARG(sv) (int)((sv).count()), (sv).data()

typedef Sl::u64(*Hasher_fn) (Sl::u64 seed, const void* key, Sl::usize key_len);

namespace Sl
{
    struct StrView;
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
} // namespace Sl
#endif // !SL_STRINGBUILDER_H

#if defined(SL_IMPLEMENTATION)
namespace Sl
{
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
        if (_allocator) {
            auto size = vsnprintf(nullptr, 0, format, args);
            if (size > 0) {
                auto* str = (char*)_allocator->allocate(size);
                append(str, static_cast<usize>(size));
            }
        } else {
            char buf[4096];
            auto size = vsnprintf(buf, sizeof(buf), format, args);
            if (size > 0) {
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
        char buf[64];
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
} // namespace Sl
#endif // !SL_IMPLEMENTATION
