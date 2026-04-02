#ifndef SL_STRINGVIEW_H
#define SL_STRINGVIEW_H

#define SV_LIT(str_lit) StrView(str_lit, sizeof(str_lit) - sizeof(str_lit[0]), true, false)
#define SV_FORMAT "%.*s"
#define SV_ARG(sv) (int)((sv).size), (sv).data

#include "Sl_Defines.hpp"
#include "Sl_Array.hpp"

typedef Sl::u64(*Hasher_fn) (Sl::u64 seed, const void* key, Sl::usize key_len);

namespace Sl
{
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
} // namespace Sl
#endif // !SL_STRINGVIEW_H

#ifdef SL_IMPLEMENTATION
namespace Sl
{
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
            // End of string or found delimiter
            if (i == size || data[i] == character) {
                if (i > start) {
                    array_out.push(data + start, i - start);
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
            memory_zero(this, sizeof(*this));
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

} // namespace Sl

#endif // !SL_IMPLEMENTATION
