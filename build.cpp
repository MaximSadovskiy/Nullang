#define EZBUILD_IMPLEMENTATION
#include "deps/ezbuild.hpp"

using namespace Sl;

int main(int argc, char** argv) {
    rebuild_itself(ExecutableOptions{}, argc, argv, "deps/ezbuild.hpp");

    const bool debug = is_argument_set("debug", argc, argv);
    const bool build_tests = is_argument_set("--tests", argc, argv) || is_argument_set("tests", argc, argv);
    const bool build_gui = is_argument_set("--gui", argc, argv) || is_argument_set("gui", argc, argv);

    auto opt = ExecutableOptions{.debug=true,
                                 .optimize = debug ? FlagsOptimization::NONE : FlagsOptimization::ALL,
                                 .warnings=FlagsWarning::ALL_FATAL
                                };
    bool result = false;                            
    Cmd cmd = {};
    cmd.start_build(opt);
    defer(result = cmd.end_build(true));
    for (int i = 1; i < argc; ++i) {
        StrView arg = argv[i];
        if (arg != "EZBUILD_REBUILT" && arg != "--run" && arg != "--tests" && arg != "--compiler" && arg != "compiler")
            cmd.add_run_argument(arg);
    }

    const auto system = get_system();
    if (build_tests) {
        cmd.add_source_file("tests/tests.cpp");
        cmd.add_include_path("deps");
        cmd.add_include_path(".");
        cmd.output_file("tests/run_tests");
        goto end;
    }

    if (system == FlagsSystem::WINDOWS) {
        cmd.link_common_win_libraries();
        cmd.link_library("Winmm.lib");
        cmd.link_library("raylib.lib");
        cmd.add_define("_CRT_SECURE_NO_WARNINGS");
    } else {
        cmd.add_linker_flag("deps/raylib/lib/libraylib.a");
        cmd.link_library("GL");
        cmd.link_library("m");
        cmd.link_library("pthread");
        cmd.link_library("dl");
        cmd.link_library("rt");
        cmd.link_library("X11");
    }
    cmd.add_source_file("compiler.cpp");
    cmd.add_include_path("deps");
    cmd.add_include_path("deps/raylib/include");
    cmd.add_library_path("deps/raylib/lib");
    
    if (build_gui) {
        cmd.output_file("nulc_gui");
    } else {
        cmd.add_define("COMPILER_CLI");
        cmd.output_file("nulc");
    }
end:
    return result;
}