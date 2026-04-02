#ifndef SL_DEFINES_H
#define SL_DEFINES_H

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
#define MAX(x, y) (x) > (y) ? (x) : (y)
#endif // MAX

#ifndef MIN
#define MIN(x, y) (x) < (y) ? (x) : (y)
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

#endif // SL_DEFINES_H
