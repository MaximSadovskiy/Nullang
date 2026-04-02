#ifndef SL_ASSERTS_H
#define SL_ASSERTS_H

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
    void assert_default_(bool condition, const char* file_name, int line, const char* text) noexcept;
    void assert_debug_(bool condition, const char* file_name, int line, const char* text) noexcept;
} // namespace Sl
#endif // !SL_ASSERTS_H

#if defined(SL_IMPLEMENTATION)
namespace Sl
{
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
} // namespace Sl

#endif // !SL_IMPLEMENTATION
