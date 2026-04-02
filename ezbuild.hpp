// You can config this values, by defining macro, before including this file
//
// This will include implementation of this library to the current translation unit
//  #define EZBUILD_IMPLEMENTATION
//
// This will enable debug info of builder script and also will show trace of building commands
//  #define EZBUILD_DEBUG
//
// This will disable automatically setting console to UTF-8 on Windows (Does nothing on other OS..)
//  #define EZBUILD_DONT_SET_CONSOLE
//
// This will controll max size of error message buffer (If you don't understand, best to leave it default)
//  #define EZBUILD_ERROR_MESSAGE_SIZE (size)

#if defined(EZBUILD_IMPLEMENTATION)
#   define SL_IMPLEMENTATION
#   include "Sl/Sl.hpp"
#   undef SL_IMPLEMENTATION
#endif //EZBUILD_IMPLEMENTATION

#ifndef EZBUILD_H
#define EZBUILD_H

#include "Sl/Sl.hpp"

#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#else
#   include <fcntl.h>
#   include <errno.h>
#   include <string.h>
#   include <unistd.h>
#   include <dirent.h>
#   include <sys/types.h>
#   include <sys/stat.h>
#   include <sys/wait.h>
#endif // _WIN32

#ifndef EZBUILD_ERROR_MESSAGE_SIZE
#   define EZBUILD_ERROR_MESSAGE_SIZE (1024 * 8)
#endif // !EZBUILD_ERROR_MESSAGE_SIZE

namespace Sl
{
    #ifdef _WIN32
        typedef HANDLE ProcessID;
        typedef HANDLE ThreadId;
        typedef HANDLE ProcessDescriptor;
        typedef HANDLE FileHandle;
        typedef FILETIME FileTimeUnit;
        #define INVALID_PROCESS INVALID_HANDLE_VALUE
        #define INVALID_FILE_HANDLE static_cast<FileHandle>(INVALID_HANDLE_VALUE)
    #else
        typedef int ProcessID;
        typedef int ThreadId;
        typedef int ProcessDescriptor;
        typedef int FileHandle;
        typedef time_t FileTimeUnit;
        #define INVALID_PROCESS -1
        #define INVALID_FILE_HANDLE static_cast<FileHandle>(-1)
    #endif // _WIN32

    struct Process;
    struct Processes;
    struct SystemInfo;
    struct ExecutableOptions;
    struct CmdOptions;
    struct FileTime;
    struct FileEntry;

    enum class FlagsFile
    {
        FILE_OPEN_READ       = 1 << 0,
        FILE_OPEN_WRITE      = 1 << 1,
        FILE_OPEN_EXECUTE    = 1 << 2,
        FILE_OPEN_READ_WRITE = FILE_OPEN_READ | FILE_OPEN_WRITE,
        FILE_OPEN_ALL        = FILE_OPEN_READ | FILE_OPEN_WRITE | FILE_OPEN_EXECUTE,
    };
    s32 operator|(FlagsFile a, FlagsFile b);
    s32 operator&(FlagsFile a, FlagsFile b);
    FlagsFile& operator|=(FlagsFile& a, FlagsFile b);

    enum class FlagsWarning
    {
        NONE = 0,
        NORMAL,      // -Wall or /W3 in msvc, because its shits a lot of useless warnings
        FATAL,   // -Wfatal-errors, fatal means it will stop compilation at the first error (MSVC can't do that)
        ALL,         // -Wall or /W4 in msvc
        ALL_FATAL,   // -Wall -Wfatal-errors
        EXTRA,       // -Wall -Wextra
        EXTRA_FATAL, // -Wall -Wextra -Wfatal-errors
        VERBOSE,     // -Wall -Wextra -Wpedantic
        EnumSize
    };

    enum class FlagsOptimization
    {
        NONE = 0,
        BASIC,     // -O1
        NORMAL,    // -O2
        SIZE,      // -Os
        SPEED,     // -Ofast
        ALL,       // -O3
        EnumSize
    };

    enum class FlagsSTD
    {
        NONE = 0,
        C99,       // -std=c99
        C11,       // -std=c11
        CPP14,     // -std=c++14
        C17,       // -std=c17
        CPP20,     // -std=c++20
        CPPLatest, // -std=c++2x..
        C2X,       // -std=c2x
        EnumSize
    };

    enum class FlagsExecutableType
    {
        EXECUTABLE = 0,
        STATIC_LIB,
        DYNAMIC_LIB,
        EnumSize
    };

    enum class FlagsCompiler
    {
        UNKNOWN = 0,
        MSVC,
        CLANG,
        GCC,
        EnumSize
    };

    enum class FlagsSystem
    {
        UNKNOWN = 0,
        WINDOWS,
        MACOS,
        LINUX,
        BSD,
        ANDROID,
        EnumSize
    };

    enum class FileType
    {
        NORMAL = 0,
        DIRECTORY,
        SYMLINK,
        OTHER,
        EnumSize
    };

    enum Result
    {
        SL_ERROR = -1,
        SL_FALSE = 0,
        SL_TRUE  = 1,
    };

    // Checks if build script needs to be rebuilt and reruns it
    #define rebuild_itself_force(options, argc, argv, ...) Sl::rebuild_itself_args(true, (options), (argc), (argv), __FILE__, __VA_ARGS__, NULL)
    #define rebuild_itself(options, argc, argv, ...) Sl::rebuild_itself_args(false, (options), (argc), (argv), __FILE__, __VA_ARGS__, NULL)
    void rebuild_itself_args(bool force, ExecutableOptions options, int argc, char **argv, const char *source_path, ...);
    // If build script was rebuilt last time - returns true
    bool was_script_rebuilt(int argc, char** argv);
    // Compares time of depency files with provided file. If true, that means file needs to be rebuilt. Useful for non C/C++ builds.
    Result file_needs_rebuilt(StrView file, LocalArray<StrView>& dependency_files);
    // Must-have for incremental builds. This function checks depencies of C/C++ file by itself (for example #include "...").
    Result file_needs_rebuilt_cpp(StrView obj, StrView src_file, StrView output_folder = "", HashMap<StrView, FileTimeUnit, StrView::hash>* memoization = nullptr);
    // Checks if provided argument is supported for current compiler
    bool is_flag_supported_cpp(StrView expected_flag);
    // Returns all supported flags for current compiler
    bool get_supported_flags(Array<StrView>& flags_out);
    bool create_folder(StrView folder, bool return_error_if_folder_exist = false);
    bool delete_folder(StrView folder);
    bool is_file_exists(StrView file);
    bool create_file(StrView file, FileHandle& handle_out, bool return_error_if_file_exist = false, FlagsFile flags = FlagsFile::FILE_OPEN_WRITE);
    bool open_file(StrView file, FileHandle& handle_out, FlagsFile flags = FlagsFile::FILE_OPEN_READ);
    // Doesn't check for existence of file
    bool delete_file(StrView file);
    bool write_to_file(StrView file, const char* data, usize size);
    bool write_to_file(FileHandle file_handle, const char* data, usize size);
    bool rename_file(StrView from, StrView to);
    bool close_file(FileHandle file_handle);
    bool get_file_time(FileHandle file_handle, FileTime& file_time_out);
    bool get_file_size(FileHandle file_handle, usize& file_size_out);
    s32 compare_file_time(FileTimeUnit file_time1, FileTimeUnit file_time2);
    bool read_folder(StrView folder_path, Array<FileEntry>& files_out);
    bool read_entire_file(StrView file_path, StrBuilder& buffer);
    bool read_entire_file(FileHandle file_handle, StrBuilder& buffer);
    bool read_dependencies(StrView depency_path, Array<StrView>& depencies_out, StrView output_folder = "");
    SystemInfo get_system_info();
    usize get_last_error_code();
    const char* get_error_message();
    FlagsCompiler get_compiler();
    const char* get_compiler_name(FlagsCompiler compiler = get_compiler(), bool is_cpp = true);
    FlagsSystem get_system();
    const char* get_system_name(FlagsSystem system = get_system());

    struct ExecutableOptions
    {
        bool is_cpp = true; // compile with C++ or C compiler
        bool incremental_build = true;
        bool debug = false;
        FlagsOptimization optimize = FlagsOptimization::NONE;
        FlagsWarning warnings = FlagsWarning::NONE;
        FlagsSTD std = FlagsSTD::NONE;
    };
    struct CmdOptions
    {
        bool reset_command = true;
        bool print_command = true;
        bool wait_command = true; // does nothing if async is not NULL
        Processes* async = nullptr;
        ProcessDescriptor* stdin_desc = nullptr;
        ProcessDescriptor* stdout_desc = nullptr;
        ProcessDescriptor* stderr_desc = nullptr;
    };
    // Main object of this library, it has two uses:
    //  1) You can use it, to run system processes
    //  2) You can build C/C++ files with it
    struct Cmd : StrBuilder {
        Cmd() {}
        template<typename... Args>
        Cmd(Args... args) {
            push(args...);
        }
        ~Cmd() {
            StrBuilder::cleanup();
            source_paths.cleanup();
            source_files.cleanup();
            source_files_output.cleanup();
            link_libraries.cleanup();
            link_libraries_paths.cleanup();
            linker_flags.cleanup();
            custom_flags.cleanup();
            custom_arguments.cleanup();
            defines.cleanup();
        }

        // This will push escaped strings to internal buffer, ultimately creating a "command"
        template<typename... Args>
        void push(Args... args) {
            auto process = [&](auto&& arg) {
                *this << std::forward<decltype(arg)>(arg);
                append(' ');
            };
            int expand_args[] = {0, (process(args), 0)...};
            UNUSED(expand_args);
        }
        // Execute current command, which in return gives you a Process struct
        Process execute(CmdOptions opt = {});
        // Print current command
        void print();
        // Trim current command
        void trim();
        // Clear internal buffer and other resources (It does not cleanup itself)
        void clear();

    // This functions are responsible for С/C++ build
    //-------------------------------------------------------------------
        // Starts the build, appending some commands to internal buffer
        void start_build(ExecutableOptions opt = {});
        // End the build, flushing all the commands
        bool end_build(bool run, bool force_rebuilt = false);
    // Funtions down below can be called in between start_build() and end_build() in order to configure build steps
        // Set output file name to provided one
        void output_file(StrView file, bool contains_ext = false);
        // Sets the folder, where all temporary files will be generated (optional, by default ".build")
        void output_folder(StrView folder);
        // Add source file to build step
        void add_source_file(StrView file);
        // Add source files from folder (This will add all .c and .cpp files from provided folder)
        bool include_sources_from_folder(StrView folder_path);
        // Add include search path
        void add_include_path(StrView path);
        // Add define
        void add_define(StrView define_str);
        // Add library file to build step
        void link_library(StrView lib);
        // Add library search path
        void add_library_path(StrView path);
        // Add custom compiler flag (Would not be checked for validity)
        void add_cpp_flag(StrView flag);
        // Add custom linker flag (Would not be checked for validity)
        void add_linker_flag(StrView flag);
        // Add custom argument when running already built executable
        void add_run_argument(StrView arg);
    // This functions is used during build step, they are internal, not meant to used directly.
    // But they can be useful, if you need some sophisticated build step outside of provided ones.
        // Pushes output flag into internal buffer
        void push_flag_output(FlagsCompiler compiler, bool output_to_obj = false);
        // Pushes debug flag into internal buffer
        void push_flag_debug(FlagsCompiler compiler);
        // Pushes optimization flag into internal buffer
        void push_flag_optimization(FlagsOptimization optimization, FlagsCompiler compiler);
        // Pushes warning flag into internal buffer
        void push_flag_warning(FlagsWarning warning, FlagsCompiler compiler);
        // Pushes std flag into internal buffer
        void push_flag_std(FlagsSTD std, bool is_cpp, FlagsCompiler compiler);
        void link_common_win_libraries();
        void append_libraries();
        void append_libraries_paths();
        void append_defines();
        void append_custom_flags();
        void append_linker_flags(FlagsCompiler compiler);
        void append_output_name(FlagsCompiler compiler, bool append_flag = true);
        void build_tree_of_folders(StrView file);
    public:
        Array<StrView> source_paths = {};
        Array<StrView> source_files = {};
        Array<StrView> source_files_output = {};
        Array<StrView> link_libraries = {};
        Array<StrView> link_libraries_paths = {};
        Array<StrView> linker_flags = {};
        Array<StrView> custom_flags = {};
        Array<StrView> custom_arguments = {};
        Array<StrView> defines = {};
        StrView        output_name = {"a", 1, true, false};
        StrView        _output_folder = {".build", 6, true, false};
        bool           output_contains_ext = false;
        bool           incremental_build = true;
        u32            max_concurent_procceses = 0;
    };

    struct FileTime
    {
        FileTimeUnit creation_time;
        FileTimeUnit last_access_time;
        FileTimeUnit last_write_time;
    };

    struct FileEntry
    {
        StrView name; // String allocated by global allocator
        FileType type;

        FileEntry(StrView name, FileType type = FileType::NORMAL)
            : name(name), type(type)
        {}

        const char* get_type_name() const;
    };

    struct Process
    {
        ProcessID id;
        ProcessID threadId;
        bool done;
        bool error_happened;

        Process()
            : id(INVALID_PROCESS), threadId(INVALID_PROCESS), done(false), error_happened(false)
        {}
        Process(ProcessID id, ThreadId threadId = INVALID_PROCESS)
            : id(id), threadId(threadId), done(false), error_happened(false)
        {}

        bool wait();
    };

    struct Processes : Array<Process>
    {
        ~Processes() {
            wait_all();
        }
        bool wait_all(bool clear_array = true);
    };

    struct SystemInfo
    {
        usize number_of_processors = 0;
        usize page_size = 0;
        usize available_memory = 0;
        usize total_memory = 0;
    };
} // namespace Sl
#endif // EZBUILD_H

#if defined(EZBUILD_IMPLEMENTATION)
namespace Sl
{
    SystemInfo get_system_info()
    {
        SystemInfo info = {};
    #ifdef _WIN32
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        info.number_of_processors = sysInfo.dwNumberOfProcessors;
        info.page_size            = sysInfo.dwPageSize;
        MEMORYSTATUSEX memInfo = {};
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        info.available_memory = memInfo.ullAvailPhys;
        info.total_memory = memInfo.ullTotalPhys;
    #else
        long phys_pages           = sysconf(_SC_PHYS_PAGES);
        long avphys_pages         = sysconf(_SC_AVPHYS_PAGES);
        info.number_of_processors = sysconf(_SC_NPROCESSORS_ONLN);
        info.page_size            = sysconf(_SC_PAGESIZE);
        info.total_memory         = phys_pages > 0 ? (usize)phys_pages * info.page_size : 0;
        info.available_memory     = avphys_pages > 0 ? (usize)avphys_pages * info.page_size : 0;
    #endif // !_WIN32
        return info;
    }

    inline static void report_error(const char* const format, ...) SL_PRINTF_FORMATER(1, 2);
    inline static void report_error(const char* const format, ...)
    {
        va_list args;
        va_start(args, format);
        logger_handler(LOG_ERROR, format, args);
        log(": %s", get_error_message());
        if (get_system() != FlagsSystem::WINDOWS)
            log("\n");
        va_end(args);
    }

    inline static const char* error_string(StrView utf16_str, bool force = false)
    {
    #if defined(_WIN32)
        if (!force && !utf16_str.contains_non_ascii_char()) return utf16_str.data;

        s32 utf8Len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)utf16_str.data, -1, NULL, 0, NULL, NULL);
        if (utf8Len == 0) {
            log_error("Conversion failed for UTF-16 string\n");
            return nullptr;
        }

        auto* utf8_str_out = (char*)get_global_allocator()->allocate(utf8Len);
        if (WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)utf16_str.data, -1, utf8_str_out, utf8Len, NULL, NULL) == 0) {
            log_error("Conversion failed for UTF-16 string\n");
            return nullptr;
        }
        return (const char*)utf8_str_out;
    #else
        UNUSED(force);
        return utf16_str.data;
    #endif
    }

    inline static StrView utf8_to_utf16_windows(const char* ansi_str)
    {
        if (ansi_str == nullptr) return {nullptr, 0};
    #if defined(_WIN32)
        s32 wideLen = MultiByteToWideChar(CP_UTF8, 0, ansi_str, -1, NULL, 0);
        if (wideLen == 0) {
            log_error("Conversion failed for string: \"%s\"\n", ansi_str);
            return {nullptr, 0};
        }

        const auto new_size = (sizeof(wchar_t)) * wideLen;
        auto* wide_str_out = (wchar_t*) get_global_allocator()->allocate(new_size);
        if (MultiByteToWideChar(CP_UTF8, 0, ansi_str, -1, wide_str_out, wideLen) == 0) {
            log_error("Conversion failed for string: \"%s\"\n", ansi_str);
            return {nullptr, 0};
        }
        return StrView((const char*)wide_str_out, new_size, true, true);
    #else
        return StrView(ansi_str);
    #endif
    }

    static const char* normalize_path(StrBuilder& builder, StrView path, bool& is_wide, bool force_wide = false)
    {
        const char* terminated_path;
        if (path.is_null_terminated) {
            terminated_path = path.data;
        } else {
            builder.append(path.data, path.size);
            builder.append_null();
            terminated_path = builder.data();
        }

        #ifdef _WIN32
            if (force_wide || (!path.is_wide && path.contains_non_ascii_char())) {
                is_wide = true;
                return utf8_to_utf16_windows(terminated_path).data;
            }
            else
                return terminated_path;
        #else
            UNUSED(force_wide);
            is_wide = path.is_wide;
            return terminated_path;
        #endif
    }

    s32 operator|(FlagsFile a, FlagsFile b)
    {
        return (static_cast<s32>(a) | static_cast<s32>(b));
    }
    s32 operator&(FlagsFile a, FlagsFile b)
    {
        return static_cast<s32>(a) & static_cast<s32>(b);
    }
    FlagsFile& operator|=(FlagsFile& a, FlagsFile b)
    {
        a = static_cast<FlagsFile>(a | b);
        return a;
    }

    bool is_file_exists(StrView file)
    {
        ScopedLogger mute(logger_muted);
        FileHandle file_handle = INVALID_FILE_HANDLE;

        if (!open_file(file, file_handle)) return false;

        return close_file(file_handle);
    }
    bool rename_file(StrView from, StrView to)
    {
        if (from.size == 0 || to.size == 0) return false;
        bool result = false;

        StrBuilder buffer_from(get_global_allocator());
        bool from_is_wide = from.is_wide;
        StrBuilder buffer_to(get_global_allocator());
        bool to_is_wide = to.is_wide;

        bool force_wide = false;
        if (from.contains_non_ascii_char() || to.contains_non_ascii_char()) {
            force_wide = true;
        }
        const char* file_from_path = normalize_path(buffer_from, from, from_is_wide, force_wide);
        const char* file_to_path = normalize_path(buffer_to, to, to_is_wide, force_wide);
    #if defined(_WIN32)
        if (from_is_wide || to_is_wide) {
            WCHAR* from_wide = (WCHAR*)file_from_path;
            WCHAR* to_wide = (WCHAR*)file_to_path;
            result = MoveFileExW(from_wide, to_wide, MOVEFILE_REPLACE_EXISTING);
        } else
            result = MoveFileExA(file_from_path, file_to_path, MOVEFILE_REPLACE_EXISTING);
    #else
        result = rename(file_from_path, file_to_path) == 0;
    #endif // !_WIN32
        if (!result) report_error("Could not rename file \"%s\" to \"%s\"", error_string(file_from_path, from_is_wide), error_string(file_to_path, to_is_wide));
        return result;
    }
    s32 compare_file_time(FileTimeUnit file_time1, FileTimeUnit file_time2)
    {
        // -1 = First is latter
        //  0 = They are equal
        //  1 = Second is latter
        #if defined(_WIN32)
            return (s32) CompareFileTime(&file_time1, &file_time2);
        #else
            // Unix times are seconds + nanoseconds (time_t + suseconds_t pattern)
            // u64 time1_ns = (u64)file_time1.seconds * 1000000000ULL + file_time1.nanoseconds;
            // u64 time2_ns = (u64)file_time2.seconds * 1000000000ULL + file_time2.nanoseconds;

            if (file_time1 < file_time2) return -1;
            if (file_time1 > file_time2) return 1;
            return 0;
        #endif // !_WIN32
    }
    bool get_file_time(FileHandle file_handle, FileTime& file_time_out)
    {
        bool result = false;

        #if defined(_WIN32)
            result = GetFileTime(file_handle, &file_time_out.creation_time,
                                              &file_time_out.last_access_time,
                                              &file_time_out.last_write_time);
        #else
            struct stat st;
            if (fstat(file_handle, &st) != 0) {
                result = false;
            } else {
                file_time_out.creation_time = (FileTimeUnit)st.st_ctime;
                file_time_out.last_access_time = (FileTimeUnit)st.st_atime;
                file_time_out.last_write_time = (FileTimeUnit)st.st_mtime;
                result = true;
            }
        #endif // !_WIN32

        if (!result) report_error("Could not get time");
        return result;
    }
    bool get_file_size(FileHandle file_handle, usize& file_size_out)
    {
        bool result = false;

        #if defined(_WIN32)
            LARGE_INTEGER size = {};
            result = GetFileSizeEx(file_handle, &size);
            file_size_out = size.QuadPart;
        #else
            struct stat st;
            if (fstat(file_handle, &st) != 0) {
                result = false;
            } else {
                file_size_out = (u64)st.st_size;
                result = true;
            }
        #endif // !_WIN32
        if (!result) {
            report_error("Could not get size of the file");
            return false;
        }
        return result;
    }
    bool write_to_file(StrView file, const char* data, usize size)
    {
        FileHandle file_handle = INVALID_FILE_HANDLE;
        if (!create_file(file, file_handle, false, FlagsFile::FILE_OPEN_READ_WRITE)) return false;
        return write_to_file(file_handle, data, size);
    }
    bool write_to_file(FileHandle file_handle, const char* data, usize size)
    {
        usize total_written = 0;
        usize write_size = 0;
    #ifdef _WIN32
        DWORD written_bytes = 0;
        do {
            write_size = size - total_written;
            if (write_size > 8192) write_size = 8192;
            auto* current_data_ptr = data + total_written;
            if (WriteFile(file_handle, current_data_ptr, write_size, &written_bytes, NULL)) {
                bool result = true;
                while (written_bytes == 0 && write_size > 0) {
                    // This means we need to retry, because write_size is too big
                    write_size /= 2;
                    result = WriteFile(file_handle, current_data_ptr, write_size, &written_bytes, NULL);
                    if (!result) break;
                }
                if (!result || written_bytes == 0) {
                    report_error("Could not write to file");
                    return false;
                }
                total_written += written_bytes;
            } else {
                report_error("Could not write to file");
                return false;
            }
        } while(total_written < size);
    #else
        do {
            write_size = size - total_written;
            if (write_size > 8192) write_size = 8192;
            const char* current_data_ptr = data + total_written;

            ssize_t written_bytes = write(file_handle, current_data_ptr, write_size);
            if (written_bytes < 0) {
                report_error("Could not write to file");
                return false;
            }
            if (written_bytes == 0 && write_size > 0) {
                // Retry with smaller size
                bool result = true;
                while (write_size > 1) {
                    write_size /= 2;
                    written_bytes = write(file_handle, current_data_ptr, write_size);
                    if (written_bytes > 0) break;
                    if (written_bytes < 0) {
                        result = false;
                        break;
                    }
                }
                if (!result || written_bytes == 0) {
                    report_error("Could not write to file");
                    return false;
                }
            }
            total_written += (usize)written_bytes;
        } while (total_written < size);
    #endif // !_WIN32
        return true;
    }
    bool create_file(StrView file, FileHandle& handle_out, bool return_error_if_file_exist, FlagsFile flags)
    {
        if (file.size == 0) return false;

        StrBuilder buffer(get_global_allocator());
        bool is_wide = file.is_wide;
        const char* file_path = normalize_path(buffer, file, is_wide);
    #ifdef _WIN32
        if (return_error_if_file_exist) {
            DWORD attributes;
            if (is_wide)
                attributes = GetFileAttributesW((LPCWSTR)file_path);
            else
                attributes = GetFileAttributesA(file_path);
            if (attributes != INVALID_FILE_ATTRIBUTES) {
                log_error("Failed to create file: \"%s\" already exists\n", error_string(file_path, is_wide));
                return false;
            }
        }
        SECURITY_ATTRIBUTES sa; memory_zero(&sa, sizeof(SECURITY_ATTRIBUTES));
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        DWORD windows_flags = 0;
        if (flags & FlagsFile::FILE_OPEN_READ)    windows_flags |= GENERIC_READ;
        if (flags & FlagsFile::FILE_OPEN_WRITE)   windows_flags |= GENERIC_WRITE;
        if (flags & FlagsFile::FILE_OPEN_EXECUTE) windows_flags |= GENERIC_EXECUTE;

        if (is_wide) {
            handle_out = CreateFileW((LPCWSTR)file_path,
                windows_flags,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                &sa,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
        } else {
            handle_out = CreateFileA(file_path,
                windows_flags,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                &sa,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
        }
    #else
        if (return_error_if_file_exist) {
            struct stat st;
            if (stat(file_path, &st) == 0) {
                log_error("Failed to create file: \"%s\" already exists\n", error_string(file_path, is_wide));
                return false;
            }
        }
        int unix_flags = 0;
        const bool want_read  = (flags & FlagsFile::FILE_OPEN_READ)  != 0;
        const bool want_write = (flags & FlagsFile::FILE_OPEN_WRITE) != 0;

        if (want_read && want_write) {
            unix_flags |= O_RDWR;
        } else if (want_write) {
            unix_flags |= O_WRONLY;
        } else if (want_read) {
            unix_flags |= O_RDONLY;
        } else {
            // No access flags requested; default to write-only
            unix_flags |= O_WRONLY;
        }
        unix_flags |= O_CREAT | O_TRUNC;

        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // 0644
        handle_out = open(file_path, unix_flags, mode);
    #endif // _WIN32
        if (handle_out == INVALID_FILE_HANDLE) {
            report_error("Could not create file \"%s\"", error_string(file_path, is_wide));
            return false;
        }
        return true;
    }
    bool delete_file(StrView file)
    {
        StrBuilder buffer(get_global_allocator());
        bool is_wide = file.is_wide;
        const char* file_path = normalize_path(buffer, file, is_wide);

    #if defined(_WIN32)
        // Clear read-only attribute
        if (is_wide) {
            if (!SetFileAttributesW((LPCWSTR)file_path, FILE_ATTRIBUTE_NORMAL)) {
                report_error("Could not change permissions of file \"%s\"", error_string(file_path, is_wide));
                return false;
            }
            if (!DeleteFileW((LPCWSTR)file_path)) {
                report_error("Could not delete file \"%s\"", error_string(file_path, is_wide));
                return false;
            }
        } else {
            if (!SetFileAttributesA(file_path, FILE_ATTRIBUTE_NORMAL)) {
                report_error("Could not change permissions of file \"%s\"", error_string(file_path, is_wide));
                return false;
            }
            if (!DeleteFileA(file_path)) {
                report_error("Could not delete file \"%s\"", error_string(file_path, is_wide));
                return false;
            }
        }
    #else
        UNUSED(is_wide);
        struct stat st;
        if (lstat(file_path, &st) == 0) {
            // Clear write permission for owner, group, and others
            if (chmod(file_path, st.st_mode | S_IWUSR | S_IWGRP | S_IWOTH) != 0) {
                report_error("Could not change permissions of file \"%s\"", error_string(file_path));
                return false;
            }
        }
        if (unlink(file_path) != 0) {
            report_error("Could not delete file \"%s\"", error_string(file_path));
            return false;
        }
    #endif // _WIN32
        return true;
    }
    bool open_file(StrView file, FileHandle& handle_out, FlagsFile flags)
    {
        if (file.size == 0) return false;
        StrBuilder null_terminated_file(get_global_allocator());

        StrBuilder builder(get_global_allocator());
        bool is_wide = file.is_wide;
        const char* file_path = normalize_path(builder, file, is_wide);

    #ifdef _WIN32
        SECURITY_ATTRIBUTES sa; memory_zero(&sa, sizeof(SECURITY_ATTRIBUTES));
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;
        DWORD windows_flags = 0;
        if (flags & FlagsFile::FILE_OPEN_READ)    windows_flags |= GENERIC_READ;
        if (flags & FlagsFile::FILE_OPEN_WRITE)   windows_flags |= GENERIC_WRITE;
        if (flags & FlagsFile::FILE_OPEN_EXECUTE) windows_flags |= GENERIC_EXECUTE;

        if (is_wide) {
            handle_out = CreateFileW((LPCWSTR)file_path,
                windows_flags,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                &sa,
                OPEN_EXISTING,
                NULL,
                NULL);
        } else {
            handle_out = CreateFileA(file_path,
                windows_flags,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                &sa,
                OPEN_EXISTING,
                NULL,
                NULL);
        }
    #else
        int unix_flags = O_RDONLY;
        if (flags & FlagsFile::FILE_OPEN_WRITE) {
            if (flags & FlagsFile::FILE_OPEN_READ)
                unix_flags = O_RDWR;
            else
                unix_flags = O_WRONLY;
        }
        handle_out = open(file_path, unix_flags);
    #endif // !_WIN32
        if (handle_out == INVALID_FILE_HANDLE) {
            report_error("Could not open file \"%s\"", error_string(file_path, is_wide));
            return false;
        }
        return true;
    }
    bool close_file(FileHandle file_handle)
    {
        if (file_handle == INVALID_FILE_HANDLE) return false;

        #ifdef _WIN32
            if (!CloseHandle(file_handle)) {
                report_error("Could not close file 0x%p", file_handle);
                return false;
            }
        #else
            if (file_handle <= 2) {  // Don't close stdin(0), stdout(1), stderr(2)
                log_error("Cannot close standard file descriptor %d\n", file_handle);
                return false;
            }
            if (close(file_handle) != 0) {
                report_error("Could not close file descriptor %d", file_handle);
                return false;
            }
        #endif // _WIN32
        return true;
    }
    bool create_folder(StrView folder, bool return_error_if_folder_exist)
    {
        if (folder.size == 0) return false;

        bool result = false;
        StrBuilder builder(get_global_allocator());
        bool is_wide = folder.is_wide;
        const char* folder_path = normalize_path(builder, folder, is_wide);
    #ifdef _WIN32
        if (!return_error_if_folder_exist) {
            DWORD attributes;
            if (is_wide)
                attributes = GetFileAttributesW((LPCWSTR)folder_path);
            else
                attributes = GetFileAttributesA(folder_path);
            if (attributes != INVALID_FILE_ATTRIBUTES && attributes & FILE_ATTRIBUTE_DIRECTORY)
                return false;
        }
        SECURITY_ATTRIBUTES sa; memory_zero(&sa, sizeof(SECURITY_ATTRIBUTES));
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = TRUE;

        BOOL result_windows;
        if (is_wide)
            result_windows = CreateDirectoryW((LPCWSTR)folder_path, &sa);
        else
            result_windows = CreateDirectoryA(folder_path, &sa);
        result = (bool)result_windows;
    #else
        if (!return_error_if_folder_exist) {
            struct stat st;
            if (stat(folder_path, &st) == 0 && S_ISDIR(st.st_mode))
                return false;
        }
        mode_t mode = 0755;
        result = mkdir(folder_path, mode) == 0;
    #endif // _WIN32

        if (!result) report_error("Could not create folder \"%s\"", error_string(folder_path, is_wide));
        return result;
    }
    bool delete_folder(StrView folder)
    {
        if (folder.size == 0) return false;

        bool result = false;
        StrBuilder builder(get_global_allocator());
        bool is_wide = folder.is_wide;
        const char* folder_path = normalize_path(builder, folder, is_wide);

    #ifdef _WIN32
        BOOL result_windows;
        if (is_wide) {
            result_windows = RemoveDirectoryW((LPCWSTR)folder_path);
        } else {
            result_windows = RemoveDirectoryA(folder_path);
        }
        result = (bool)result_windows;
    #else
        result = rmdir(folder_path) == 0;
    #endif // _WIN32

        if (!result) report_error("Could not delete folder \"%s\"", error_string(folder_path, is_wide));
        return result;
    }
    bool read_folder(StrView folder_path, Array<FileEntry>& files_out)
    {
        StrBuilder folder_path_terminated(get_global_allocator());
        bool is_wide = folder_path.is_wide;
        const char* file_path = normalize_path(folder_path_terminated, folder_path, is_wide);

    #if defined(_WIN32)
        if (is_wide) folder_path_terminated.append(file_path, memory_wstrlen((const wchar_t*)file_path));
        else folder_path_terminated.append(file_path);

        if (!folder_path.ends_with("/*") || !folder_path.ends_with(L"/*")) {
            if (folder_path.ends_with("/") || folder_path.ends_with(L"/")) {
                if (is_wide) folder_path_terminated.append(L"*");
                else folder_path_terminated.append("*");
            }
            else {
                if (is_wide) folder_path_terminated.append(L"/*");
                else folder_path_terminated.append("/*");
            }
        }
        folder_path_terminated.append_null(false);

        WIN32_FIND_DATAA data = {};
        WIN32_FIND_DATAW dataw = {};
        DWORD* attributes = nullptr;
        const char* cFileName = nullptr;
        HANDLE hFind = INVALID_HANDLE_VALUE;
        if (is_wide) {
            attributes = &dataw.dwFileAttributes;
            hFind = FindFirstFileW((LPCWSTR)folder_path_terminated.data(), &dataw);
            cFileName = (const char*)dataw.cFileName;
        } else {
            attributes = &data.dwFileAttributes;
            hFind = FindFirstFileA(folder_path_terminated.data(), &data);
            cFileName = data.cFileName;
        }

        if (hFind != INVALID_HANDLE_VALUE) {
            BOOL result = true;
            do {
                usize file_size = 0;
                if (is_wide) file_size = memory_wstrlen((const wchar_t*)cFileName);
                else file_size = memory_strlen(cFileName);

                bool ignore = false;
                if (memory_equals(cFileName, file_size, ".",   STR_LIT_SIZE(".")))   ignore = true;
                if (memory_equals(cFileName, file_size, "..",  STR_LIT_SIZE("..")))  ignore = true;
                if (memory_equals(cFileName, file_size, L".",  STR_LIT_SIZE(L".")))  ignore = true;
                if (memory_equals(cFileName, file_size, L"..", STR_LIT_SIZE(L".."))) ignore = true;

                FileType type = FileType::NORMAL;
                if (*attributes & FILE_ATTRIBUTE_DIRECTORY) type = FileType::DIRECTORY;
                else if (*attributes & FILE_ATTRIBUTE_REPARSE_POINT) type = FileType::SYMLINK;
                else if (*attributes & FILE_ATTRIBUTE_DEVICE) type = FileType::OTHER;
                if (!ignore) {
                    if (is_wide) {
                        files_out.push(error_string(cFileName), type);
                    } else {
                        files_out.push(StrView{(const char*)memory_duplicate(*get_global_allocator(), cFileName, file_size), file_size, true, is_wide}, type);
                    }
                }
                if (is_wide)
                    result = FindNextFileW(hFind, &dataw);
                else
                    result = FindNextFileA(hFind, &data);
            } while (result);
        }

        if (hFind != INVALID_HANDLE_VALUE)
            FindClose(hFind);
        else {
            report_error("Could not read folder \"%s\"", error_string(folder_path_terminated.to_string_view(true), is_wide));
            return false;
        }
        return true;
    #else
        DIR* dir = opendir(file_path);
        if (!dir) {
            report_error("Could not read folder \"%s\"", error_string(file_path));
            return false;
        }

        bool success = true;
        while (true) {
            errno = 0;
            struct dirent* entry = readdir(dir);
            if (!entry) {
                if (errno != 0) {
                    report_error("Error reading folder \"%s\"", error_string(file_path));
                    success = false;
                }
                break;
            }

            StrView entry_name = entry->d_name;
            if (memory_equals(entry->d_name, entry_name.size, ".", 1))  continue;
            if (memory_equals(entry->d_name, entry_name.size, "..", 2)) continue;

            // Construct full path for stat
            StrBuilder full_path = {};
            full_path.append(folder_path);
            if (!folder_path.ends_with("/") && !entry_name.starts_with("/"))
                full_path.append("/");
            full_path.append(entry_name);
            full_path.append_null(false);

            struct stat st;
            if (lstat(full_path.data(), &st) == 0) {
                FileType type = FileType::NORMAL;
                if (S_ISDIR(st.st_mode)) {
                    type = FileType::DIRECTORY;
                } else if (S_ISLNK(st.st_mode)) {
                    type = FileType::SYMLINK;
                } else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)) {
                    type = FileType::OTHER;
                }

                auto file_name_ptr = full_path.data() + folder_path.size + (folder_path.ends_with("/") ? 0 : 1);
                auto* file_name = memory_duplicate(*get_global_allocator(), file_name_ptr, entry_name.size);
                files_out.push(FileEntry(StrView{(const char*)file_name, entry_name.size}, type));
            } else {
                // Failed to stat, treat as normal file
                StrView file_name = {
                    (const char*)memory_duplicate(*get_global_allocator(), entry->d_name, entry_name.size),
                    entry_name.size,
                    true,
                    false
                };
                files_out.push(file_name, FileType::NORMAL);
            }

            full_path.cleanup();
        }
        closedir(dir);
        return success;
    #endif // !_WIN32
    }
    bool get_supported_flags(Array<StrView>& flags)
    {
        Cmd cmd = {};
        cmd.set_allocator(get_global_allocator());

        StrView flag_path = "flag.temp";
        FileHandle output;
        if (!create_file(flag_path, output, false, FlagsFile::FILE_OPEN_READ_WRITE)) return false;
        const auto compiler = get_compiler();
        if (compiler == FlagsCompiler::CLANG)
            cmd.push("clang++", "--help");
        else if (compiler == FlagsCompiler::GCC)
            cmd.push("g++", "--help=warnings", "--help=common", "--help=optimizers", "--help=target");
        else if (compiler == FlagsCompiler::MSVC)
            cmd.push("cl", "/help", "/nologo");
        else
            cmd.push("cc", "--help");
        CmdOptions opt;
        opt.stdout_desc = &output;
        opt.print_command = false;
        if (!cmd.execute(opt).wait()) {
            close_file(output);
            return false;
        }
        close_file(output);

        StrBuilder buffer(get_global_allocator());
        if (!read_entire_file(flag_path, buffer)) {
            delete_file(flag_path);
            return false;
        }
        auto file_view = buffer.to_string_view();
        StrView slash = "";
        if (compiler == FlagsCompiler::MSVC)
            slash = "/";
        else
            slash = "  -";
        do {
            file_view.trim();
            const auto slash_index = file_view.find_first(slash);
            if (slash_index == StrView::INVALID_INDEX) break;

            file_view.chop_left(slash_index + slash.size - 1);
            auto end_of_flag = file_view.find_first_until(' ', '\n');
            if (end_of_flag == StrView::INVALID_INDEX) {
                file_view.chop_left(file_view.find_first('\n') + 1);
                continue;
            }
            auto flag = file_view.chop_left(end_of_flag);
            const auto equal_index = flag.find_first('=');
            if (equal_index != StrView::INVALID_INDEX)      flag.chop_right(flag.size - equal_index);
            const auto coma_index = flag.find_first(',');
            if (coma_index != StrView::INVALID_INDEX)       flag.chop_right(flag.size - coma_index);
            const auto left_index = flag.find_first('<');
            if (left_index != StrView::INVALID_INDEX)       flag.chop_right(flag.size - left_index);
            const auto colon_index = flag.find_first(':');
            if (colon_index != StrView::INVALID_INDEX)      flag.chop_right(flag.size - colon_index);
            const auto bracket_index = flag.find_first('[');
            if (bracket_index != StrView::INVALID_INDEX)    flag.chop_right(flag.size - bracket_index);
            const auto sqbracket_index = flag.find_first('{');
            if (sqbracket_index != StrView::INVALID_INDEX)  flag.chop_right(flag.size - sqbracket_index);
            flags.push(flag);
        } while(file_view.size > 0);
        delete_file(flag_path);
        return true;
    }
    bool is_flag_supported_cpp(StrView expected_flag)
    {
        Array<StrView> flags(get_global_allocator());
        if (!get_supported_flags(flags))
            return false;

        for (auto& flag : flags) {
            if (flag == expected_flag) {
                return true;
            }
        }
        return false;
    }
    bool read_entire_file(FileHandle file_handle, StrBuilder& buffer)
    {
        u64 file_size = 0;
        if (!get_file_size(file_handle, file_size)) return false;

        buffer.clear();
        buffer.resize(file_size);
        if (file_size == 0) return true;

    #ifdef _WIN32
        DWORD bytes_read = 0;
        usize read_size = 0;
        do {
            read_size = file_size - buffer.count();
            auto* current_data_ptr = buffer.data() + buffer.count();
            if (ReadFile(file_handle, current_data_ptr, (DWORD)read_size, &bytes_read, NULL)) {
                bool result = true;
                while (bytes_read == 0 && read_size > 0) {
                    // This means we need to retry, because read_size is too big
                    read_size /= 2;
                    result = ReadFile(file_handle, current_data_ptr, (DWORD)read_size, &bytes_read, NULL);
                    if (!result)
                        break;
                }
                if (!result || bytes_read == 0) {
                    report_error("Could not read file");
                    return false;
                }
                buffer.set_count(buffer.count() + bytes_read);
            } else {
                report_error("Could not read file");
                return false;
            }
        } while(file_size > 0 && buffer.count() < file_size);
    #else
        usize read_size = file_size;
        usize total_read = 0;

        while (read_size > 0) {
            ssize_t bytes_read = read(file_handle, buffer.data() + total_read, read_size);
            if (bytes_read < 0) {
                while (read_size > 1) {
                    read_size /= 2;
                    bytes_read = read(file_handle, buffer.data() + total_read, read_size);
                    if (bytes_read >= 0) {
                        break;
                    }
                }
                if (bytes_read < 0) {
                    report_error("Could not read file");
                    return false;
                }
            }
            if (bytes_read == 0) break; // EOF

            total_read += (usize)bytes_read;
            read_size = file_size - total_read;
        }
        buffer.set_count(total_read);
    #endif // !_WIN32

        return true;
    }
    bool read_entire_file(StrView file_path, StrBuilder& buffer)
    {
        FileHandle file_handle = INVALID_FILE_HANDLE;
        if (!open_file(file_path, file_handle)) return false;

        return read_entire_file(file_handle, buffer);
    }
    Result file_needs_rebuilt(StrView file, LocalArray<StrView>& dependency_files)
    {
        Result result = Result::SL_FALSE;
        FileHandle h;
        if (!open_file(file, h)) return Result::SL_ERROR;

        FileTime dependency_time;
        FileTime file_time;
        if (!get_file_time(h, file_time)) {
            DEFER_RETURN(Result::SL_ERROR);
        }

        for (auto& dependency : dependency_files) {
            FileHandle dependency_h;
            if (!open_file(dependency, dependency_h)) {
                DEFER_RETURN(Result::SL_ERROR);
            }
            auto res = get_file_time(dependency_h, dependency_time);
            close_file(dependency_h);
            if (!res) {
                DEFER_RETURN(Result::SL_ERROR);
            }
            if (compare_file_time(file_time.last_write_time, dependency_time.last_write_time) < 0) {
                DEFER_RETURN(Result::SL_TRUE);
            }
        }
    end:
        close_file(h);
        return result;
    }

    bool Processes::wait_all(bool clear_array)
    {
        bool success = true;
        for (auto& proc : *this) {
            success &= proc.wait();
        }
        if (clear_array) clear();
        return success;
    }

    bool Process::wait()
    {
        bool result = true;
        if (id == INVALID_PROCESS)
            return false;
        if (done)
            return !error_happened;

    #if defined(_WIN32)
        DWORD exit_status = EXIT_FAILURE;

        if (WaitForSingleObject(id, INFINITE) == WAIT_FAILED) {
            report_error("Could not wait on process 0x%p", id);
            DEFER_RETURN(false);
        }

        if (!GetExitCodeProcess(id, &exit_status)) {
            report_error("Could not get exit code of process 0x%zx", (usize)id);
            DEFER_RETURN(false);
        }

        if (exit_status != 0) {
            log_error("Process 0x%zx exited with exit code %lu\n", (usize)id, exit_status);
            DEFER_RETURN(false);
        }
    end:
        CloseHandle(id);
        if (threadId != INVALID_PROCESS) {
            CloseHandle(threadId);
            threadId = INVALID_HANDLE_VALUE;
        }
    #else
        int status;
        int exit_code;

        if (waitpid(id, &status, 0) < 0) {
            report_error("Could not wait on process %d", id);
            DEFER_RETURN(false);
        }
        if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                log_error("Process %d exited with code %d\n", id, exit_code);
                DEFER_RETURN(false);
            }
        }
        if (WIFSIGNALED(status)) {
            log_error("Command process was terminated by signal %d\n", WTERMSIG(status));
            DEFER_RETURN(false);
        }
    end:
        close(id);
    #endif // !_WIN32
        done = true;
        if (!result) error_happened = true;
        return !error_happened;
    }

    Process Cmd::execute(CmdOptions opt)
    {
        trim();
        if (opt.print_command)
            print();
        Process proc;
#ifdef _WIN32
        BOOL success = false;
        PROCESS_INFORMATION procInfo;
        ZeroMemory(&procInfo, sizeof(PROCESS_INFORMATION));

        append_null(false);
        if (StrView(_data, _count).contains_non_ascii_char())
        {
            STARTUPINFOW startInfo;
            ZeroMemory(&startInfo, sizeof(startInfo));
            startInfo.cb = sizeof(startInfo);
            if (opt.stderr_desc) startInfo.hStdInput = *opt.stderr_desc;
            else {
                startInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
                if (startInfo.hStdInput == INVALID_HANDLE_VALUE) log_warning("Could not get STD_INPUT_HANDLE\n");
            }

            if (opt.stdout_desc) startInfo.hStdOutput = *opt.stdout_desc;
            else {
                startInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
                if (startInfo.hStdOutput == INVALID_HANDLE_VALUE) log_warning("Could not get STD_OUTPUT_HANDLE\n");
            }

            if (opt.stdin_desc) startInfo.hStdError = *opt.stdin_desc;
            else {
                startInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
                if (startInfo.hStdError == INVALID_HANDLE_VALUE) log_warning("Could not get STD_ERROR_HANDLE\n");
            }
            startInfo.dwFlags = STARTF_USESTDHANDLES;
            success = CreateProcessW(NULL, (LPWSTR)utf8_to_utf16_windows(_data).data, NULL, NULL, TRUE, 0, NULL, NULL, &startInfo, &procInfo);
        } else {
            STARTUPINFOA startInfo;
            ZeroMemory(&startInfo, sizeof(startInfo));
            startInfo.cb = sizeof(startInfo);
            if (opt.stderr_desc) startInfo.hStdInput = *opt.stderr_desc;
            else {
                startInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
                if (startInfo.hStdInput == INVALID_HANDLE_VALUE) log_warning("Could not get STD_INPUT_HANDLE\n");
            }

            if (opt.stdout_desc) startInfo.hStdOutput = *opt.stdout_desc;
            else {
                startInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
                if (startInfo.hStdOutput == INVALID_HANDLE_VALUE) log_warning("Could not get STD_OUTPUT_HANDLE\n");
            }

            if (opt.stdin_desc) startInfo.hStdError = *opt.stdin_desc;
            else {
                startInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
                if (startInfo.hStdError == INVALID_HANDLE_VALUE) log_warning("Could not get STD_ERROR_HANDLE\n");
            }
            startInfo.dwFlags = STARTF_USESTDHANDLES;
            success = CreateProcessA(NULL, _data, NULL, NULL, TRUE, 0, NULL, NULL, &startInfo, &procInfo);
        }
        if (!success) {
            report_error("Could not create process \"" SV_FORMAT "\"", (int)_count, _data);
            if (opt.reset_command) reset();
            return Process(INVALID_PROCESS);
        }
        proc = Process(procInfo.hProcess, procInfo.hThread);
    #else
        pid_t cpid = fork();
        if (cpid < 0) {
            report_error("Could not fork child process");
            return Process();
        }

        if (cpid == 0) {
            if (opt.stdin_desc) {
                if (dup2(*opt.stdin_desc, STDIN_FILENO) < 0) {
                    report_error("Could not setup stdin for child process");
                    exit(EXIT_FAILURE);
                }
            }

            if (opt.stdout_desc) {
                if (dup2(*opt.stdout_desc, STDOUT_FILENO) < 0) {
                    report_error("Could not setup stdout for child process");
                    exit(EXIT_FAILURE);
                }
            }

            if (opt.stderr_desc) {
                if (dup2(*opt.stderr_desc, STDERR_FILENO) < 0) {
                    report_error("Could not setup stderr for child process");
                    exit(EXIT_FAILURE);
                }
            }
            usize size_out;
            StrView data_view(_data, _count);
            Allocator* alloc = get_global_allocator();
            Array<const char*> arr = {alloc};
            do {
                // @TODO i know this is shit, but it works
                auto index = data_view.find_first_until(' ', '"');
                if (index == StrView::INVALID_INDEX) {
                    auto quote_index = data_view.find_first('"');
                    if (quote_index != StrView::INVALID_INDEX) {
                        usize cursor = quote_index + 1;
                        data_view.chop_left(cursor);
                        index = data_view.find_first('"');
                        auto arg = data_view.chop_left(index);
                        data_view.chop_left(1); // skip "
                        arg.trim();
                        if (arg.size > 0)
                            arr.push((const char*)memory_format(*alloc, size_out, SV_FORMAT, SV_ARG(arg)));
                        continue;
                    } else {
                        arr.push((const char*)memory_format(*alloc, size_out, SV_FORMAT, SV_ARG(data_view)));
                        break;
                    }
                }
                auto arg = data_view.chop_left(index);
                data_view.chop_left(1);
                if (arg.size > 0)
                    arr.push((const char*)memory_format(*alloc, size_out, SV_FORMAT, SV_ARG(arg)));
            } while(data_view.size > 0);
            arr.push((const char*)nullptr);

            if (execvp(arr[0], (char * const*) arr.data()) < 0) {
                report_error("Could not exec child process for %s", arr[0]);
                exit(EXIT_FAILURE);
            }
            UNREACHABLE("Cmd::execute");
        }
        proc = cpid;
    #endif // _WIN32

        if (opt.reset_command) reset();
        if (opt.async) opt.async->push(proc);
        else if (opt.wait_command) proc.wait();
        return proc;
    }

    void Cmd::trim()
    {
        auto view = to_string_view();
        _count -= view.trim_right();
        auto left_spaces = view.trim_left();
        memory_copy(_data, _count - left_spaces, _data + left_spaces, _count - left_spaces);
        _count -= left_spaces;
    }

    void Cmd::push_flag_output(FlagsCompiler compiler, bool output_to_obj)
    {
        if (compiler == FlagsCompiler::MSVC)
        {
            if (output_to_obj)
                append("/Fo:");
            else
                append("/Fe:");
        }
        else
            append("-o ");
    }

    void Cmd::push_flag_debug(FlagsCompiler compiler)
    {
        if (compiler == FlagsCompiler::MSVC)
            push("/Zi");
        else
            push("-g");
    }

    void Cmd::push_flag_optimization(FlagsOptimization optimization, FlagsCompiler compiler)
    {
        if (optimization == FlagsOptimization::NONE)
            return;

        if (compiler == FlagsCompiler::MSVC)
            append("/");
        else
            append("-");

        switch(optimization) {
            case FlagsOptimization::BASIC:
                push("O1");
                break;
            case FlagsOptimization::NORMAL:
                push("O2");
                break;
            case FlagsOptimization::SIZE:
                push("Os");
                break;
            case FlagsOptimization::SPEED:
                if (compiler == FlagsCompiler::MSVC)
                    push("O2");
                else
                    push("Ofast");
                break;
            case FlagsOptimization::ALL:
                if (compiler == FlagsCompiler::MSVC)
                    push("O2");
                else
                    push("O3");
                break;
            case FlagsOptimization::NONE:
            default: UNREACHABLE("FlagsOptimization");
        }
    }

    void Cmd::push_flag_warning(FlagsWarning warning, FlagsCompiler compiler)
    {
        switch (warning) {
            case FlagsWarning::NORMAL:
            {
                if (compiler == FlagsCompiler::MSVC)
                    push("/W3");
                else
                    push("-Wall");
                break;
            }
            case FlagsWarning::FATAL:
            {
                if (compiler != FlagsCompiler::MSVC)
                    push("-Wfatal-errors");
                break;
            }
            case FlagsWarning::ALL:
            {
                if (compiler == FlagsCompiler::MSVC)
                    push("/W4");
                else
                    push("-Wall");
                break;
            }
            case FlagsWarning::ALL_FATAL:
            {
                if (compiler == FlagsCompiler::MSVC)
                    push("/W4");
                else
                    push("-Wall", "-Wfatal-errors");
                break;
            }
            case FlagsWarning::EXTRA:
            {
                push("-Wall");
                if (compiler != FlagsCompiler::MSVC)
                    push("-Wextra");
                break;
            }
            case FlagsWarning::EXTRA_FATAL:
            {
                push("-Wall");
                if (compiler != FlagsCompiler::MSVC)
                    push("-Wextra", "-Wfatal-errors");
                break;
            }
            case FlagsWarning::VERBOSE:
            {
                push("-Wall");
                if (compiler != FlagsCompiler::MSVC)
                    push("-Wextra", "-Wpedantic");
                break;
            }
            case FlagsWarning::NONE: break;
            default: UNREACHABLE("FlagsWarning");
        }
    }

    void Cmd::push_flag_std(FlagsSTD std, bool is_cpp, FlagsCompiler compiler)
    {
        if (std == FlagsSTD::NONE)
            return;

        if (compiler == FlagsCompiler::MSVC)
            append("/std:");
        else
            append("-std=");

        if (is_cpp)
        {
            switch(std) {
                case FlagsSTD::C99:
                    push("c++98");
                    break;
                case FlagsSTD::C11:
                    push("c++11");
                    break;
                case Sl::FlagsSTD::CPP14:
                    push("c++14");
                    break;
                case FlagsSTD::C17:
                    push("c++17");
                    break;
                case FlagsSTD::CPP20:
                    push("c++20");
                    break;
                case FlagsSTD::CPPLatest:
                    if (compiler == FlagsCompiler::MSVC)
                        push("c++latest");
                    else
                        push("c++20");
                    break;
                case FlagsSTD::C2X:
                    push("c++2c");
                    break;
                case FlagsSTD::NONE:
                default: UNREACHABLE("FlagsSTD");
            }
        }
        else
        {
            switch(std) {
                case FlagsSTD::C99:
                    push("c99");
                    break;
                case FlagsSTD::CPP14:
                case FlagsSTD::C11:
                    push("c11");
                    break;
                case FlagsSTD::C17:
                    push("c17");
                    break;
                case FlagsSTD::CPPLatest:
                case FlagsSTD::CPP20:
                case FlagsSTD::C2X:
                    push("c2x");
                    break;
                case FlagsSTD::NONE:
                default: UNREACHABLE("FlagsSTD");
            }
        }
    }

    void Cmd::link_common_win_libraries()
    {
        const char* libs[] = {"user32.lib", "kernel32.lib", "gdi32.lib", "advapi32.lib", "shell32.lib"};
        for (auto& lib : libs)
            link_library(lib);
    }

    void Cmd::start_build(ExecutableOptions opt)
    {
        this->incremental_build = opt.incremental_build;
        const auto compiler = get_compiler();
        const auto compiler_name = get_compiler_name(compiler, opt.is_cpp);
        push(compiler_name);
        if (compiler == FlagsCompiler::MSVC)
            push("/nologo", "/EHsc");

        if (opt.debug) push_flag_debug(compiler);
        push_flag_warning(opt.warnings, compiler);
        push_flag_optimization(opt.optimize, compiler);
        push_flag_std(opt.std, opt.is_cpp, compiler);
    }

    void Cmd::add_include_path(StrView path)
    {
        append("-I", 2);
        append(path.data, path.size);
        append(' ');
        source_paths.push(path);
    }

    bool Cmd::include_sources_from_folder(StrView folder_path)
    {
        Array<FileEntry> files(get_global_allocator());
        if (!read_folder(folder_path, files))
            return false;

        for (auto& file : files) {
            auto name = file.name;
            if (name.ends_with(".cpp") || name.ends_with(".c")) {
                StrBuilder full_path(get_global_allocator());
                full_path.append(folder_path);
                if (!folder_path.ends_with("/") && !name.starts_with("/"))
                    full_path.append('/');
                full_path.append(name);
                full_path.append_null(false);
                add_source_file(full_path.to_string_view(true, name.is_wide || folder_path.is_wide));
            }
        }
        return true;
    }

    void Cmd::add_cpp_flag(StrView flag)
    {
        custom_flags.push(flag);
    }

    void Cmd::add_linker_flag(StrView flag)
    {
        linker_flags.push(flag);
    }

    void Cmd::add_run_argument(StrView arg)
    {
        custom_arguments.push(arg);
    }

    void Cmd::add_library_path(StrView path)
    {
        link_libraries_paths.push(path);
    }

    void Cmd::add_define(StrView define_str)
    {
        defines.push(define_str);
    }

    void Cmd::add_source_file(StrView file)
    {
        source_files.push(file);
        while (file.size > 0) {
            if (file.data[file.size - 1] == '.') {
                file.chop_right(1);
                break;
            }
            file.chop_right(1);
        }
        ASSERT_TRUE(file.size > 0);
        StrBuilder source_output(get_global_allocator());
        source_output.append(file);
        source_output.append(".obj");
        source_files_output.push(source_output.to_string_view());
    }

    void Cmd::link_library(StrView lib)
    {
        link_libraries.push(lib);
    }

    void Cmd::append_libraries()
    {
        const auto compiler = get_compiler();
        for (auto& lib : link_libraries) {
            if (compiler != FlagsCompiler::MSVC)
                append("-l");
            append(lib.data, lib.size);
            append(' ');
        }
    }

    void Cmd::append_libraries_paths()
    {
        const auto compiler = get_compiler();
        if (compiler == FlagsCompiler::MSVC) {
            if (link_libraries_paths.count() > 0 && linker_flags.count() < 1)
                append("/link ");
        }
        for (auto& path : link_libraries_paths) {
            if (compiler == FlagsCompiler::MSVC)
                append("/LIBPATH:");
            else
                append("-L");
            append(path.data, path.size);
            append(' ');
        }
    }

    void Cmd::append_defines()
    {
        for (auto& define : defines) {
            append("-D");
            append(define.data, define.size);
            append(' ');
        }
    }

    void Cmd::append_custom_flags()
    {
        for (auto& flag : custom_flags) {
            append(flag);
            append(' ');
        }
    }

    void Cmd::append_linker_flags(FlagsCompiler compiler)
    {
        if (compiler == FlagsCompiler::MSVC) {
            if (linker_flags.count() > 0) append("/link ");
        } else {
            if (linker_flags.count() > 0) append("-Wl,");
        }
        for (usize i = 0; i < linker_flags.count(); ++i) {
            auto& flag = linker_flags[i];
            append(flag.data, flag.size);
            if (i + 1 < linker_flags.count()) {
                if (compiler == FlagsCompiler::MSVC)
                    append(' ');
                else
                    append(',');

            }
        }
        if (linker_flags.count() > 0) append(' ');
    }

    void Cmd::output_folder(StrView folder)
    {
        _output_folder = folder;
    }

    void Cmd::output_file(StrView file, bool contains_ext)
    {
        output_name = file;
        output_contains_ext = contains_ext;
    }

    void Cmd::append_output_name(FlagsCompiler compiler, bool append_flag)
    {
        if (output_name.size == 0 || !output_name.data) return;

        if (append_flag)
            push_flag_output(compiler);
        if (output_name.find_first_of_chars("/\\") == StrView::INVALID_INDEX) {
            append("./");
        }
        append(output_name.data, output_name.size);
    #if defined(_WIN32)
        if (!output_contains_ext) {
            append(".exe");
        }
    #endif // !_WIN32
        append(' ');
    }

    bool read_dependencies(StrView depency_path, Array<StrView>& depencies_out, StrView output_folder)
    {
        auto cpp_index = depency_path.find_last(".cpp");
        if (cpp_index == StrView::INVALID_INDEX) {
            cpp_index = depency_path.find_last(".c");
            if (cpp_index == StrView::INVALID_INDEX) return false;
        }
        depency_path.chop_right(depency_path.size - cpp_index);

        FileHandle depency;
        StrBuilder new_depency_path(get_global_allocator());
        if (output_folder.size > 0) {
            new_depency_path.append(output_folder);
            new_depency_path.append('/');
        }
        new_depency_path.append(depency_path);
        auto compiler = get_compiler();
        if (compiler != FlagsCompiler::MSVC) {
            new_depency_path.append(".d");
            new_depency_path.append_null(false);
            if (!open_file(new_depency_path.to_string_view(true), depency)) return false;
        } else {
            new_depency_path.append("_cl.d");
            new_depency_path.append_null(false);
            if (!open_file(new_depency_path.to_string_view(true), depency)) return false;
        }
        // return true;
        StrBuilder buffer(get_global_allocator());
        if (!read_entire_file(depency, buffer)) {
            close_file(depency);
            return false;
        }
        close_file(depency);
        auto view = buffer.to_string_view();
        StrView start_dep_str = "";
        char end_dep_str;
        if (compiler == FlagsCompiler::MSVC) {
            start_dep_str = ":  ";
            end_dep_str = '\n';
            do {
                auto start_dep = view.find_first(start_dep_str);
                if (start_dep == StrView::INVALID_INDEX)
                    break;
                view.chop_left(start_dep + start_dep_str.size);
                auto end_dep = view.find_first(end_dep_str);
                if (end_dep == StrView::INVALID_INDEX)
                    break;
                auto depency_view = view.chop_left(end_dep);
                depency_view.trim();
                StrBuilder fixed_path_depency = {get_global_allocator()};
                do {
                    auto index = depency_view.find_first("\\./");
                    if (index == StrView::INVALID_INDEX) {
                        if (fixed_path_depency.count() > 0)
                            fixed_path_depency.append(depency_view);
                        break;
                    }
                    fixed_path_depency.append(depency_view.chop_left(index));
                    fixed_path_depency.append('/');
                    depency_view.chop_left(3);
                } while(depency_view.size > 0);
                if (fixed_path_depency.count() > 0) {
                    fixed_path_depency.append_null(false);
                    depency_view = fixed_path_depency.to_string_view(false);
                }
                depency_view.trim_right_char('\n');
                depency_view.trim_right_char('\r');
                depencies_out.push(depency_view);
            } while(view.size > 0);
        } else {
            start_dep_str = ": ";
            end_dep_str = ' ';
            auto start_dep = view.find_first(start_dep_str);
            if (start_dep == StrView::INVALID_INDEX)
                return false;
            view.chop_left(start_dep + start_dep_str.size);
            auto temp_view = view;
            auto temp_size = 0;
            bool skip_first = true;
            do {
                auto end_dep = temp_view.find_first(end_dep_str);
                if (end_dep == StrView::INVALID_INDEX) {
                    view.trim();
                    view.trim_left_char('\\');
                    view.trim_left_char('\n');
                    view.trim_left_char('\r');
                    view.trim_right_char('\n');
                    view.trim_right_char('\r');
                    view.trim_right_char('\\');
                    if (view.size > 0) depencies_out.push(view);
                    break;
                }
                if (temp_view.data[end_dep - 1] == '\\') {
                    temp_size += end_dep + 1;
                    temp_view.chop_left(end_dep + 1);
                    continue;
                }
                auto dep_view = view.chop_left(temp_size + end_dep);
                view.chop_left(1); // skip ' '
                dep_view.trim_left_char('\\');
                dep_view.trim_left_char('\n');
                dep_view.trim_left_char('\r');
                dep_view.trim_right_char('\n');
                dep_view.trim_right_char('\r');
                dep_view.trim_right_char('\\');
                dep_view.trim();
                temp_view = view;
                temp_size = 0;
                if (skip_first) {
                    skip_first = false;
                } else {
                    if (dep_view.size > 0) depencies_out.push(dep_view);
                }
            } while (temp_view.size > 0);
        }
        return true;
    }

    static bool compare_file_time_with_provided(StrView file, FileTimeUnit provided, s32& result_out, HashMap<StrView, FileTimeUnit, StrView::hash>* cache = nullptr)
    {
        if (cache) {
            if (auto* time = cache->get(file)) {
                result_out = compare_file_time(provided, *time);
                return true;
            }
        }
        FileHandle file_handle;
        if (!open_file(file, file_handle)) return false;
        FileTime file_time;
        if (!get_file_time(file_handle, file_time)) {
            close_file(file_handle);
            return false;
        }
        FileTimeUnit src_time = file_time.last_write_time;
        close_file(file_handle);
        result_out = compare_file_time(provided, src_time);
        if (cache) cache->insert(file, src_time);
        return true;
    }

    Result file_needs_rebuilt_cpp(StrView obj, StrView src_file, StrView output_folder, HashMap<StrView, FileTimeUnit, StrView::hash>* memoization)
    {
        ASSERT(obj.data != nullptr && obj.size > 0, "Provide correct object file path");
        ASSERT(src_file.data != nullptr && src_file.size > 0, "Provide correct source file path");

        ScopedLogger _(logger_muted);
        FileHandle obj_handle = INVALID_FILE_HANDLE;
        if (!open_file(obj, obj_handle)) return Result::SL_ERROR;

        FileTime file_time = {};
        if (!get_file_time(obj_handle, file_time)) {
            close_file(obj_handle);
            return Result::SL_ERROR;
        }
        FileTimeUnit obj_time = file_time.last_write_time;
        close_file(obj_handle);

        s32 compare;
        if (!compare_file_time_with_provided(src_file, obj_time, compare))
            return Result::SL_TRUE;
        if (compare < 0)
            return Result::SL_TRUE;

        Array<StrView> deps(get_global_allocator());
        if (!read_dependencies(src_file, deps, output_folder)) return Result::SL_ERROR;

        StrBuilder escaped_dependency(get_global_allocator());
        for (auto& dependency : deps) {
            escaped_dependency.clear();
            do {
                auto index = dependency.find_first("\\ ");
                if (index == StrView::INVALID_INDEX) {
                    escaped_dependency.append(dependency.chop_left(dependency.size));
                    escaped_dependency.append_null(false);
                    break;
                }
                escaped_dependency.append(dependency.chop_left(index));
                escaped_dependency.append(' ');
                dependency.chop_left(2);
            } while(true);
            auto dependency_view = escaped_dependency.to_string_view(true);
            s32 compare;
            if (!compare_file_time_with_provided(dependency_view, obj_time, compare, memoization))
                return Result::SL_TRUE;
            if (compare < 0)
                return Result::SL_TRUE;
        }
        return Result::SL_FALSE;
    }

    static StrView strip_cpp_postfix(StrView file)
    {
        auto cpp_index = file.find_last(".cpp");
        if (cpp_index == StrView::INVALID_INDEX) {
            cpp_index = file.find_last(".c");
        }
        if (cpp_index != StrView::INVALID_INDEX) {
            file.chop_right(file.size - cpp_index);
        }
        return file;
    }

    void Cmd::build_tree_of_folders(StrView file)
    {
        LocalArray<StrView> folders(get_global_allocator());
        file.split_by_char(folders, '/');
        StrBuilder tree(get_global_allocator());
        tree.append(this->_output_folder);
        auto folders_count = folders.count();
        if (folders_count < 1) folders_count = 1;
        for (usize i = 0; i < folders_count - 1; ++i) {
            auto folder = folders[i];
            tree.append('/');
            tree.append(folder);
            tree.append_null(false);
            create_folder(tree.to_string_view(true));
        }
    }

    bool Cmd::end_build(bool run, bool force_rebuilt)
    {
        const auto compiler = get_compiler();
        bool result = false;
        bool needs_to_rebuilt = false;
        if (source_files.count() < 1) {
            log_error("No source files were provided. Use add_source_file() to add some.\n");
            ASSERT_DEBUG(source_files.count() > 1);
            return false;
        }

        if (incremental_build) {
            ASSERT_TRUE(source_files.count() == source_files_output.count());
            create_folder(this->_output_folder);
            append_custom_flags();
            append_defines();
            { // Check if executable file exist
                StrBuilder output_full_name(get_global_allocator());
                output_full_name.append(output_name.data, output_name.size);
                #if defined(_WIN32)
                    if (!output_contains_ext) {
                        output_full_name.append(".exe");
                    }
                #endif // !_WIN32
                output_full_name.append_null(false);

                if (!is_file_exists(output_full_name.to_string_view(true)))
                    force_rebuilt = true;
            }

            Processes procs = {};
            const auto max_procs = max_concurent_procceses == 0 ? get_system_info().number_of_processors * 2 + 1 : max_concurent_procceses;
            StrBuilder output_file_object(get_global_allocator());

            HashMapOptions opt{};
            opt.allocator = get_global_allocator();
            HashMap<StrView, FileTimeUnit, StrView::hash> memoization(opt);

            const auto mark = this->_count;
            for (auto& file : source_files) {
                // Create dependency
                build_tree_of_folders(file);
                output_file_object.clear();
                output_file_object.append(_output_folder);
                output_file_object.append('/');
                output_file_object.append(strip_cpp_postfix(file));
                if (compiler == FlagsCompiler::MSVC)
                    output_file_object.append("_cl.d");
                else
                    output_file_object.append(".d");
                output_file_object.append_null(false);
                const auto dependency_path = output_file_object.to_string_view(true);
                bool need_to_recreate_dependency_file = true;
                {
                    ScopedLogger _(logger_muted);
                    if (is_file_exists(dependency_path)) {
                        FileHandle dependency_file;
                        if (open_file(dependency_path, dependency_file)) {
                            FileTime dependency_time;
                            if (get_file_time(dependency_file, dependency_time)) {
                                s32 compare;
                                if (compare_file_time_with_provided(file, dependency_time.last_write_time, compare)) {
                                    if (compare > 0) need_to_recreate_dependency_file = false;
                                }
                            }
                            close_file(dependency_file);
                        }
                    }
                }
                if (force_rebuilt || need_to_recreate_dependency_file) {
                    FileHandle dependency_file;
                    if (!create_file(dependency_path, dependency_file)) return false;

                    CmdOptions options = {};
                    options.reset_command = false;
                    options.stdout_desc = &dependency_file;
                    if (compiler == FlagsCompiler::MSVC) {
                        // MSVC will generate obj file, even we asking it not to do that
                        // so we will redirect it to trash (where it belongs).
                        append("/c /showIncludes /Fo:");
                        append(_output_folder);
                        append("/.trash.obj ");
                    } else
                        append("-MM ");
                    append(file.data, file.size);
                    {
                        auto saved_logger = log_get_current();
                        log_set_current(logger_muted);
                        if (!execute(options).wait()) {
                            log_set_current(saved_logger);
                            log_error("Failed to get dependencies of \"" SV_FORMAT "\"", SV_ARG(file));
                            return false;
                        }
                        close_file(dependency_file);
                        log_set_current(saved_logger);
                    }
                }
                this->_count = mark;
                // Check and rebuild C/C++ file if needed
                output_file_object.clear();
                output_file_object.append(_output_folder);
                output_file_object.append('/');
                output_file_object.append(file.data, file.size);
                output_file_object.append(".obj");
                output_file_object.append_null(false);
                const auto output_file_object_path = output_file_object.to_string_view(true);
                if (force_rebuilt || file_needs_rebuilt_cpp(output_file_object_path, file, _output_folder, &memoization) != Result::SL_FALSE)
                {
                    if (compiler == FlagsCompiler::MSVC)
                        append("/c ");
                    else
                        append("-c ");
                    append(file.data, file.size);
                    needs_to_rebuilt = true;
                    log_info("Rebuilding: " SV_FORMAT "\n", SV_ARG(file));
                    append(' ');
                    push_flag_output(compiler, true);
                    append(output_file_object_path);
                    append_null(false);
                    CmdOptions options = {};
                    options.reset_command = false;
                    options.async = &procs;
                    if (procs.count() >= max_procs) {
                        if (!procs.wait_all())
                            return false;
                    }
                    execute(options);
                }
                this->_count = mark;
            }
            if (!procs.wait_all())
                return false;
            if (needs_to_rebuilt) {
                append_custom_flags();
                append_output_name(compiler);
                for (auto& file : source_files) {
                    append(this->_output_folder);
                    append('/');
                    append(file.data, file.size);
                    append(".obj");
                    append(' ');
                }
                append_linker_flags(compiler);
                append_libraries_paths();
                append_libraries();
                log_info("Linking executable...\n");
                result = execute().wait();
            } else {
                _count = 0;
                result = true;
                log_info("Everything is up to date\n");
            }
        }
        else {
            append_custom_flags();
            append_defines();
            append_output_name(compiler);
            for (auto& file : source_files) {
                append(file.data, file.size);
                append(' ');
            }
            append_linker_flags(compiler);
            append_libraries_paths();
            append_libraries();
            log_info("Linking executable...\n");
            result = execute().wait();
        }
        if (result && run) {
            _count = 0;
            append_output_name(compiler, false);
            log_info("Running: " SB_FORMAT "\n", (int)_count, _data);
            for (auto& arg : custom_arguments) {
                append(arg);
                append(' ');
            }
            trim();
            CmdOptions opt;
            opt.print_command = false;
            execute(opt).wait();
        }
        clear();
        return result;
    }

    void Cmd::clear()
    {
        _count = 0;
        source_paths.set_count(0);
        source_files.set_count(0);
        source_files_output.set_count(0);
        link_libraries.set_count(0);
        link_libraries_paths.set_count(0);
        linker_flags.set_count(0);
        custom_flags.set_count(0);
        custom_arguments.set_count(0);
        defines.set_count(0);
        output_contains_ext = false;
        incremental_build = true;
        output_name = {"a", 1, true, false};
        _output_folder = {".build", 6, true, false};
    }

    void Cmd::print()
    {
        log_info("CMD: " SV_FORMAT "\n", static_cast<int>(_count), _data);
    }

    FlagsCompiler get_compiler()
    {
        #if defined(__clang__)
            return FlagsCompiler::CLANG;
        #elif defined(__GNUC__)
            return FlagsCompiler::GCC;
        #elif defined(_MSC_VER)
            return FlagsCompiler::MSVC;
        #else
            return FlagsCompiler::UNKNOWN;
        #endif
    }

    const char* get_compiler_name(FlagsCompiler compiler, bool is_cpp)
    {
        static_assert((int)FlagsCompiler::EnumSize == 4, "Unhandled FlagsCompiler type");
        switch(compiler)
        {
            case FlagsCompiler::CLANG:
            {
                if (is_cpp)
                    return "clang++";
                return "clang";
            }
            case FlagsCompiler::GCC:
            {
                if (is_cpp)
                    return "g++";
                return "gcc";
            }
            case FlagsCompiler::MSVC:
            {
                return "cl";
            }
            case FlagsCompiler::UNKNOWN:
            {
                return "cc";
            }
            default: UNREACHABLE("get_compiler_name");
        }
    }

    FlagsSystem get_system()
    {
    #if defined(_WIN32) || defined(_WIN64)
        return FlagsSystem::WINDOWS;
    #elif defined(__ANDROID__)
        return FlagsSystem::ANDROID;
    #elif defined(__APPLE__)
        return FlagsSystem::MACOS;
    #elif defined(__linux__)
        return FlagsSystem::LINUX;
    #elif defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__) || defined(__OpenBSD__)
        return FlagsSystem::BSD;
    #else
        return FlagsSystem::UNKNOWN;
    #endif
    }

    const char* get_system_name(FlagsSystem system)
    {
        static_assert((int)FlagsSystem::EnumSize == 6, "Unhandled FlagsSystem type");
        switch(system)
        {
            case FlagsSystem::WINDOWS: return "Windows";
            case FlagsSystem::MACOS:   return "MacOS";
            case FlagsSystem::LINUX:   return "Linux";
            case FlagsSystem::BSD:     return "BSD";
            case FlagsSystem::ANDROID: return "Android";
            case FlagsSystem::UNKNOWN: return "Unknown";
            default: UNREACHABLE("get_system_name");
        }
    }

    usize get_last_error_code()
    {
        #if defined(_WIN32)
            return GetLastError();
        #else
            return errno;
        #endif // _WIN32
    }

    const char* get_error_message()
    {
        const auto error_code = get_last_error_code();
    #if defined(_WIN32)
        static const char* error_msg[EZBUILD_ERROR_MESSAGE_SIZE] = {0};
        if (error_code == 0) {
            log_error("Could not get error message\n");
            return nullptr;
        } else {
            auto msgSize = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                           NULL,
                                           error_code,
                                           MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                                           (LPSTR) &error_msg,
                                           EZBUILD_ERROR_MESSAGE_SIZE - 1,
                                           NULL);
            if (msgSize == 0) {
                log_error("Could not format error message\n");
                return nullptr;
            }
            return (const char*)error_msg;
        }
    #else
        return (const char*) strerror(error_code);
    #endif // !_WIN32
    }

    const char* FileEntry::get_type_name() const
    {
        static_assert((int)FileType::EnumSize == 4, "Unhandled FileEntry type");
        switch(type)
        {
            case FileType::NORMAL: return "File";
            case FileType::DIRECTORY: return "Directory";
            case FileType::SYMLINK: return "Symlink";
            case FileType::OTHER: return "Other";
            default: return "Unknown";
        }
    }

    bool was_script_rebuilt(int argc, char** argv)
    {
        if (argv == nullptr) return false;

        StrView signature = "EZBUILD_REBUILT";
        for (int i = 0; i < argc; ++i)
        {
            if (argv[i] == nullptr) continue;
            if (memory_equals(argv[i], memory_strlen(argv[i]), signature.data, signature.size))
                return true;
        }
        return false;
    }

    void rebuild_itself_args(bool force, ExecutableOptions options, int argc, char **argv, const char *source_path, ...)
    {
        ASSERT_TRUE(argc > 0);
        const auto system = get_system();
        LocalArray<StrView> saved_args(get_global_allocator());
        for (int i = 1; i < argc; ++i)
        {
            saved_args.push(argv[i]);
            if (saved_args.last().equals("force")) {
                saved_args.set_count(saved_args.count() - 1);
                force = true;
            }
        }

        LocalArray<StrView> source_paths(get_global_allocator());
        source_paths.push(source_path);

        StrView executable_name(argv[0]);

        StrView exe_ext = ".exe";
        if (system == FlagsSystem::WINDOWS && !executable_name.ends_with(exe_ext)) {
            StrBuilder new_str(get_global_allocator());
            new_str.reserve(executable_name.size + exe_ext.size);
            new_str.append(executable_name);
            new_str.append(exe_ext);
            auto* exec = new_str.to_cstring_alloc();
            executable_name = StrView(exec, new_str.count(), true, false);
        }

        va_list args;
        va_start(args, source_path);
        for (;;) {
            const char *path = va_arg(args, const char*);
            if (path == NULL) break;
            source_paths.push(path);
        }
        va_end(args);

    #if !defined(EZBUILD_DONT_SET_CONSOLE) && defined(_WIN32)
        SetConsoleOutputCP(CP_UTF8);
    #endif // !EZBUILD_DONT_SET_CONSOLE && _WIN32

        auto needs_rebuilt_executable = file_needs_rebuilt(executable_name, source_paths);
        if (!force && needs_rebuilt_executable == Result::SL_FALSE) {
            source_paths.cleanup();
            temp_reset();
            return;
        }
        if (needs_rebuilt_executable == Result::SL_ERROR) {
            report_error("Error happened when checking dependencies, make sure you running build script at his created path");
        }

        StrBuilder old_binary_path_builder(get_global_allocator());
        old_binary_path_builder.append(executable_name.data, executable_name.size);
        old_binary_path_builder.append(".old");
        old_binary_path_builder.append_null(false);
        auto old_executable_name = old_binary_path_builder.to_string_view(true);

        if (!rename_file(executable_name, old_executable_name)) {
            exit(EXIT_FAILURE);
        }

        options.is_cpp = true;
        options.incremental_build = false;
        Cmd cmd = {};
    #ifdef EZBUILD_DEBUG
        options.debug = true;
        #ifdef _WIN32
            const auto compiler = get_compiler();
            if (compiler == FlagsCompiler::MSVC) {
                cmd.add_cpp_flag("/MTd");
            } else {
                cmd.add_define("_DEBUG");
                cmd.link_library("msvcrtd.lib");
                cmd.add_linker_flag("/NODEFAULTLIB:libcmt");
                cmd.add_linker_flag("/INCREMENTAL:NO");
            }
        #else
            cmd.add_define("_DEBUG");
        #endif // _WIN32
    #else
        // If not debug, we mute old ezbuild trace
        ScopedLogger _(logger_muted);
    #endif
        cmd.start_build(options);
        cmd.add_source_file(source_path);
        cmd.output_file(executable_name, true);
        for (auto& arg : saved_args) {
            cmd.add_run_argument(arg);
        }
        cmd.add_run_argument("EZBUILD_REBUILT");
        bool run = true;
        if (!cmd.end_build(run)) {
            rename_file(old_executable_name, executable_name);
            exit(EXIT_FAILURE);
        }
        if (system != FlagsSystem::WINDOWS)
            delete_file(old_executable_name);
        exit(EXIT_SUCCESS);
    }
} // namespace Sl
#endif // EZBUILD_IMPLEMENTATION
