// Nul compiler test suite.
// Compiles each program through the real backend (lexer -> parser -> IR ->
// asm via fasm -> link via ld (ELF) or a self-contained PE64 .exe) and runs
// the produced binary, comparing its
// output to the expected result.
//
// Build & run with: ./build --tests --run   (just ./build --tests to compile only)
//
// The suite is split in two parts:
//   - Positive tests: valid programs that must compile, link and run.
//   - Error tests:   invalid programs that must fail with a compiler error.
//                     These use fork() and are only run on non-Windows hosts.

#define SL_IMPLEMENTATION
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "Sl.hpp"
using namespace Sl;
#define EZBUILD_IMPLEMENTATION
#include "ezbuild.hpp"
#include "compiler.hpp"

static int g_pass = 0;
static int g_fail = 0;
static int g_index = 0;

static void reset_compiler_state() {
    for (auto& f : g_functions) { f.ops.cleanup(); f.regs.cleanup(); }
    g_functions.set_count(0);
    g_strings.set_count(0);
    g_vars.set_count(0);
    g_labels.set_count(0);
    g_structs.set_count(0);
    for (auto& m : g_modules) m.imports.cleanup();
    g_modules.set_count(0);
    g_current_module_name = "";
    g_globals_size = 0;
    g_functions.push(DeclaredFunction{"__entry"});
}

// Lex, parse, translate and emit a full executable for `source`.
// Returns false on any lexer/parser/compiler error. Does NOT run the binary.
static bool run_compile(const std::string& source, const std::string& name) {
    reset_compiler_state();
    out_path = StrView(name.c_str(), name.size());
    std::string src_file = name + ".nul";
    src_path = StrView(src_file.c_str(), src_file.size(), true, false);
    g_run_compiled = false;

    // Write the source to disk so the preprocessor (#include / #define) can
    // expand it exactly like the CLI does, then read the expanded result.
    FILE* f = fopen(src_file.c_str(), "w");
    if (!f) return false;
    fwrite(source.c_str(), 1, source.size(), f);
    fclose(f);

    StrBuilder expanded{};
    Array<StrView> include_chain{};
    Array<DefineMacro> macros{};
    if (!preprocess_includes(expanded, src_file.c_str(), include_chain, macros)) {
        expanded.cleanup();
        include_chain.cleanup();
        macros.cleanup();
        return false;
    }

    Lexer lexer(SV_LIT(""));
    lexer._source.append(expanded.data(), expanded.count());
    lexer._source.append_null(false);
    src_content = lexer._source.data();
    expanded.cleanup();
    include_chain.cleanup();
    macros.cleanup();

    if (!lexer.tokenize()) return false;

    Array<Expression*> exprs;
    if (!parse(lexer, exprs) || exprs.count() == 0) return false;

    Array<Instruction> global_ops{};
    Array<Variable> vars{};
    Array<VirtualReg> regs{};
    for (auto& expr : exprs) {
        ValueType rt = TYPE_NOP;
        translate_to_instruction(global_ops, regs, vars, expr, rt);
    }

    // Silence ezbuild's "[INFO] CMD: ..." lines so test output stays readable.
    bool ok;
#ifndef _WIN32
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    FILE* devnull = fopen("/dev/null", "w");
    dup2(fileno(devnull), STDOUT_FILENO);
    ok = compile_program(global_ops, regs);
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    fclose(devnull);
#else
    ok = compile_program(global_ops, regs);
#endif
    return ok;
}

// Run a previously compiled binary and capture its stdout.
static std::string run_binary(const std::string& name) {
    const bool is_windows = get_system() == FlagsSystem::WINDOWS;
    std::string cmd = is_windows ? name + ".exe 2>&1" : "./" + name + " 2>&1";
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
    pclose(p);
    return out;
}

static void cleanup_artifacts(const std::string& name) {
    remove((name + ".asm").c_str());
    remove((name + ".obj").c_str());
    remove((name + ".exe").c_str());
    remove((name + ".nul").c_str());
    remove(name.c_str());
}

// Positive test: `source` must compile, run, and produce exactly `expected`.
// Runs the compile in a forked child so a compiler_error()/exit(1) during a
// failing positive test does not kill the whole harness.
static void check(const char* test_name, const std::string& source, const std::string& expected) {
    std::string bin = "t" + std::to_string(g_index++);
    bool ok = false;
#ifndef _WIN32
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        _exit(run_compile(source, bin) ? 0 : 1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
    ok = run_compile(source, bin);
#endif
    std::string actual = ok ? run_binary(bin) : "(compile failed)";
    cleanup_artifacts(bin);

    if (ok && actual == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("FAIL: %s\n", test_name);
        if (!ok) {
            printf("  source: %s\n", source.c_str());
            printf("  compile failed\n");
        } else {
            printf("  source:   %s\n", source.c_str());
            printf("  expected: \"%s\"\n", expected.c_str());
            printf("  actual:   \"%s\"\n", actual.c_str());
        }
    }
}

// Returns true if the file at `path` contains the byte substring `needle`.
static bool file_contains(const std::string& path, const std::string& needle) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string content(n, '\0');
    fread(&content[0], 1, n, f);
    fclose(f);
    return content.find(needle) != std::string::npos;
}

// Error test: `source` must fail with a compiler error (exit code 1 or 2;
// preprocessor failures log and return false, exiting with 2, while
// parser/compiler errors exit 1).
// If `error_contains` is non-empty, stderr from the failed compile must also
// contain that substring. Implemented with fork() so the in-process
// compiler_error()/exit(1) only kills the child. Skipped on Windows (no fork).
static void check_error(const char* test_name, const std::string& source, const std::string& error_contains = "") {
#ifndef _WIN32
    std::string bin = "t" + std::to_string(g_index++);
    std::string err_path = bin + ".err";
    pid_t pid = fork();
    if (pid == 0) {
        // compiler_error() prints to stdout, so capture that (and stderr) in
        // the message file; run_compile() only silences stdout around
        // compile_program(), which restores our capture fd afterwards.
        FILE* errfile = fopen(err_path.c_str(), "w");
        if (errfile) {
            dup2(fileno(errfile), STDOUT_FILENO);
            dup2(fileno(errfile), STDERR_FILENO);
            fclose(errfile);
        }
        bool ok = run_compile(source, bin);
        fflush(stdout);
        fflush(stderr);
        _exit(ok ? 0 : 2);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool failed = WIFEXITED(status) && (WEXITSTATUS(status) == 1 || WEXITSTATUS(status) == 2);
    bool msg_matches = error_contains.empty() || file_contains(err_path, error_contains);
    std::string err_content;
    {
        FILE* ef = fopen(err_path.c_str(), "rb");
        if (ef) {
            char buf[4096];
            size_t n = fread(buf, 1, sizeof(buf) - 1, ef);
            buf[n] = 0;
            err_content.assign(buf, n);
            fclose(ef);
        }
    }
    remove(err_path.c_str());
    cleanup_artifacts(bin);

    if (failed && msg_matches) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("FAIL: %s (expected compiler error%s)\n", test_name, error_contains.empty() ? "" : " containing a message");
        if (!failed) printf("  no compiler error produced\n");
        if (!msg_matches) printf("  error message did not contain: \"%s\"\n", error_contains.c_str());
        printf("  err file: \"%s\"\n", err_content.c_str());
        printf("  source: %s\n", source.c_str());
    }
#else
    printf("SKIP: %s (error tests need fork, not available on Windows)\n", test_name);
    (void)test_name;
    (void)source;
    (void)error_contains;
#endif
}

// Codegen test: `source` must compile, emit asm containing `needle` (and NOT
// containing `excluded`, if non-empty), run, and print exactly `expected`.
// Lets us lock in backend optimizations (e.g. mul-by-power-of-2 -> shl)
// without having to disable constant folding to make them observable.
static void check_asm(const char* test_name, const std::string& source,
                      const std::string& needle, const std::string& excluded,
                      const std::string& expected) {
    std::string bin = "t" + std::to_string(g_index++);
    bool ok = false;
#ifndef _WIN32
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        _exit(run_compile(source, bin) ? 0 : 1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
    ok = run_compile(source, bin);
#endif
    std::string asm_path = bin + ".asm";
    bool found = ok && file_contains(asm_path, needle);
    bool clean = excluded.empty() || !file_contains(asm_path, excluded);
    std::string actual = ok ? run_binary(bin) : "(compile failed)";
    cleanup_artifacts(bin);

    if (ok && found && clean && actual == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("FAIL: %s\n", test_name);
        if (!ok) printf("  compile failed\n");
        if (!found) printf("  asm missing:    %s\n", needle.c_str());
        if (!clean) printf("  asm must not contain: %s\n", excluded.c_str());
        if (actual != expected) {
            printf("  expected: \"%s\"\n", expected.c_str());
            printf("  actual:   \"%s\"\n", actual.c_str());
        }
    }
}

// Windows (MS x64) backend regression test. Compiles `source` with the
// Windows target (--platform=Windows equivalent) and verifies the emitted
// asm follows the
// MS calling convention (rcx/rdx/r8/r9 register args, 32-byte shadow space,
// stack args above it, rsi/rdi callee-saved). The result is a PE64 .exe that
// cannot run on a Linux host, so only the asm is checked, plus that it
// assembles cleanly with the repo-local `fasm` (`format PE64 console`).
static void check_win_asm(const char* test_name, const std::string& source,
                          const std::vector<std::string>& needles,
                          const std::vector<std::string>& excluded = {}) {
    std::string bin = "t" + std::to_string(g_index++);
    bool ok = false;
#ifndef _WIN32
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        g_target_platform = FlagsSystem::WINDOWS;
        freopen("/dev/null", "w", stdout);
        _exit(run_compile(source, bin) ? 0 : 1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
    ok = run_compile(source, bin);
#endif
    std::string asm_path = bin + ".asm";
    bool found = ok;
    for (auto& n : needles) found = found && file_contains(asm_path, n);
    bool clean = true;
    for (auto& e : excluded) clean = clean && !file_contains(asm_path, e);
    std::string fasm_err;
    int fasm_status = -1;
    if (ok) {
        std::string cmd = "./deps/fasm/fasm " + asm_path + " " + bin + ".exe 2>&1";
        FILE* p = popen(cmd.c_str(), "r");
        char buf[256];
        while (p && fgets(buf, sizeof(buf), p)) fasm_err += buf;
        if (p) fasm_status = pclose(p);
    }
    cleanup_artifacts(bin);

    // fasm prints a "N passes, NNNN bytes." banner on success, so only a
    // nonzero exit status is a real error.
    if (ok && found && clean && fasm_status == 0) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("FAIL: %s\n", test_name);
        if (!ok) printf("  compile failed\n");
        if (!found) printf("  asm missing needle\n");
        if (!clean) printf("  asm contains excluded needle\n");
        if (fasm_status != 0) printf("  fasm(status=%d): %s\n", fasm_status, fasm_err.c_str());
    }
}

// Lex/parse/translate a set of files (alternating basename, source) as one
// program, mirroring the CLI's compile_nul_files: pass 1 parses every file
// (module declarations register modules and tag symbols), then imports are
// validated, then pass 2 translates everything with each file's module
// context. Returns false on any lexer/parser/compiler error.
static bool run_compile_files(const std::vector<std::string>& files, const std::string& name) {
    reset_compiler_state();
    out_path = StrView(name.c_str(), name.size());
    g_run_compiled = false;

    Array<Lexer*> lexers{};
    Array<Expression*> all_exprs{};
    Array<StrView> file_modules{};
    Array<usize> file_expr_start{};
    for (size_t fi = 0; fi + 1 < files.size(); fi += 2) {
        const std::string& src_file = files[fi];
        const std::string& source = files[fi + 1];
        src_path = StrView(src_file.c_str(), src_file.size(), true, false);

        auto* lexer = new Lexer(SV_LIT(""));
        lexer->_source.append(source.c_str());
        lexer->_source.append_null(false);
        src_content = lexer->_source.data();

        if (!lexer->tokenize()) { delete lexer; return false; }
        if (lexer->_tokens.is_empty()) { delete lexer; continue; }
        file_expr_start.push(all_exprs.count());
        if (!parse(*lexer, all_exprs)) { delete lexer; return false; }
        lexers.push(lexer);
        file_modules.push(g_current_module_name);
    }
    if (all_exprs.count() == 0) return false;

    // Every `import` must name a module declared by one of the files.
    for (auto& m : g_modules) {
        for (auto& imp : m.imports) {
            if (!find_module(imp)) {
                compiler_error(Token{Tok_Ident, imp}, "Module '" SV_FORMAT "' is not declared in any compiled file\n", SV_ARG(imp));
            }
        }
    }

    Array<Instruction> global_ops{};
    Array<Variable> vars{};
    Array<VirtualReg> regs{};
    usize file_idx = 0;
    for (usize i = 0; i < all_exprs.count(); ++i) {
        while (file_idx + 1 < file_expr_start.count() && i >= file_expr_start[file_idx + 1])
            ++file_idx;
        g_current_module_name = file_modules[file_idx];
        ValueType rt = TYPE_NOP;
        translate_to_instruction(global_ops, regs, vars, all_exprs[i], rt);
    }
    g_current_module_name = "";

    // The lexers' buffers back every token, so they must stay alive until the
    // binary is emitted.
    // Silence ezbuild's "[INFO] CMD: ..." lines so test output stays readable.
    bool ok;
#ifndef _WIN32
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    FILE* devnull = fopen("/dev/null", "w");
    dup2(fileno(devnull), STDOUT_FILENO);
    ok = compile_program(global_ops, regs);
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    fclose(devnull);
#else
    ok = compile_program(global_ops, regs);
#endif
    for (auto* lexer : lexers) delete lexer;
    return ok;
}

// Multi-file positive test: the files (alternating basename, source) must
// compile, run, and produce exactly `expected`.
static void check_files(const char* test_name, const std::vector<std::string>& files,
                        const std::string& expected) {
    std::string bin = "t" + std::to_string(g_index++);
    bool ok = false;
#ifndef _WIN32
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        freopen("/dev/null", "w", stdout);
        _exit(run_compile_files(files, bin) ? 0 : 1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
    ok = run_compile_files(files, bin);
#endif
    std::string actual = ok ? run_binary(bin) : "(compile failed)";
    cleanup_artifacts(bin);

    if (ok && actual == expected) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("FAIL: %s\n", test_name);
        if (!ok) {
            printf("  compile failed\n");
        } else {
            printf("  expected: \"%s\"\n", expected.c_str());
            printf("  actual:   \"%s\"\n", actual.c_str());
        }
    }
}

// Multi-file error test: the files must fail to compile, and stderr from the
// failed compile must contain `error_contains`.
static void check_files_error(const char* test_name, const std::vector<std::string>& files,
                              const std::string& error_contains) {
#ifndef _WIN32
    std::string bin = "t" + std::to_string(g_index++);
    std::string err_path = bin + ".err";
    pid_t pid = fork();
    if (pid == 0) {
        FILE* errfile = fopen(err_path.c_str(), "w");
        if (errfile) {
            dup2(fileno(errfile), STDOUT_FILENO);
            dup2(fileno(errfile), STDERR_FILENO);
            fclose(errfile);
        }
        bool ok = run_compile_files(files, bin);
        _exit(ok ? 0 : 2);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool failed = WIFEXITED(status) && WEXITSTATUS(status) == 1;
    bool msg_matches = file_contains(err_path, error_contains);
    remove(err_path.c_str());
    cleanup_artifacts(bin);

    if (failed && msg_matches) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("FAIL: %s (expected compiler error containing \"%s\")\n", test_name, error_contains.c_str());
        if (!failed) printf("  no compiler error produced\n");
    }
#else
    printf("SKIP: %s (error tests need fork, not available on Windows)\n", test_name);
    (void)test_name;
    (void)files;
    (void)error_contains;
#endif
}

static void positive_tests() {
    // --- literals & print ----------------------------------------------
    check("print int", "fn main()\n{\n    print(42);\n}\n", "42\n");
    check("print negative", "fn main()\n{\n    print(0 - 5);\n}\n", "-5\n");
    check("print string", "fn main()\n{\n    print(\"hello\");\n}\n", "hello");
    check("print two strings", "fn main()\n{\n    print(\"a\");\n    print(\"b\");\n}\n", "ab");
    check("print true", "fn main()\n{\n    print(true);\n}\n", "true\n");
    check("print false", "fn main()\n{\n    print(false);\n}\n", "false\n");
    // `\n` in a string literal is turned into a real newline byte at string
    // assembly time (append_hex), so it must print as a line break, not "\\n".
    check("string escape newline", "fn main()\n{\n    print(\"A\\nB\");\n}\n", "A\nB");
    check("string escape only newline", "fn main()\n{\n    print(\"\\n\");\n}\n", "\n");
    check("string escape quote", "fn main()\n{\n    print(\"say \\\"hi\\\"\");\n}\n", "say \"hi\"");
    check("string escape backslash", "fn main()\n{\n    print(\"a\\\\b\");\n}\n", "a\\b");
    check("string escape quote and newline", "fn main()\n{\n    print(\"q\\\"\\n\");\n}\n", "q\"\n");
    check("empty string", "fn main()\n{\n    print(\"\");\n    print(5);\n}\n", "5\n");

    // --- string .len / .data (compile-time) ----------------------------------
    check("str len", "fn main()\n{\n    s := \"hello\";\n    print(s.len);\n}\n", "5\n");
    check("str len escape newline", "fn main()\n{\n    s := \"hi\\n\";\n    print(s.len);\n}\n", "3\n");
    check("str len escape quote", "fn main()\n{\n    s := \"say \\\"hi\\\"\";\n    print(s.len);\n}\n", "8\n");
    check("str len empty", "fn main()\n{\n    s := \"\";\n    print(s.len);\n}\n", "0\n");
    check("str len copy", "fn main()\n{\n    s := \"abcd\";\n    t := s;\n    print(t.len);\n}\n", "4\n");
    check("str len reassign", "fn main()\n{\n    s := \"hello\";\n    s = \"hi\";\n    print(s.len);\n}\n", "2\n");
    check("str len typed decl", "fn main()\n{\n    s : str = \"abcd\\n\";\n    print(s.len);\n}\n", "5\n");
    check("str len comparison", "fn main()\n{\n    s := \"hello\";\n    print(s.len == 5);\n    print(s.len > 3);\n}\n", "true\ntrue\n");
    check("str len in if", "fn main()\n{\n    s := \"hello\";\n    if s.len > 3 {\n        print(1);\n    }\n}\n", "1\n");
    check("str len into var", "fn main()\n{\n    s := \"hello\";\n    x := s.len;\n    print(x);\n}\n", "5\n");
    check("str len in loop", "fn main()\n{\n    s := \"ab\";\n    for i := 0 .. s.len {\n        print(i);\n    }\n}\n", "0\n1\n");
    check("str data not null", "fn main()\n{\n    s := \"hello\";\n    print(s.data != null);\n}\n", "true\n");
    check("str data assign", "fn main()\n{\n    s := \"hello\";\n    p := s.data;\n    print(p != null);\n}\n", "true\n");
    check("str data deref read", "fn main()\n{\n    s := \"hello\";\n    d := s.data;\n    print(^d);\n}\n", "104\n");
    check("str data deref write", "fn main()\n{\n    s := \"abc\";\n    d := s.data;\n    ^d = 90;\n    print(^d);\n}\n", "90\n");
    check("str data type", "fn main()\n{\n    s := \"hello\";\n    d := s.data;\n    print(#type_of(d));\n}\n", "u8^");
    check_error("str len of non-literal", "fn getstr() -> str\n{\n    return \"abc\";\n}\nfn main()\n{\n    s := getstr();\n    print(s.len);\n}\n", "Cannot take 'len' of a string whose value is not a literal");
    check_error("str unknown member", "fn main()\n{\n    s := \"hello\";\n    print(s.size);\n}\n", "String type has no member");

    // --- arrays ----------------------------------------------------------
    check("array literal read", "fn main()\n{\n    a := [10, 20, 30];\n    print(a[0]);\n    print(a[1]);\n    print(a[2]);\n}\n", "10\n20\n30\n");
    check("array len", "fn main()\n{\n    a := [10, 20, 30];\n    print(len(a));\n}\n", "3\n");
    check("array len empty elem", "fn main()\n{\n    a := [1, 2];\n    b := [9];\n    print(len(b));\n}\n", "1\n");
    check("array index write", "fn main()\n{\n    a := [1, 2, 3];\n    a[1] = 99;\n    print(a[1]);\n}\n", "99\n");
    check("array index write then read", "fn main()\n{\n    a := [1, 2, 3];\n    a[0] = a[2];\n    print(a[0]);\n}\n", "3\n");
    check("array index expr", "fn main()\n{\n    a := [10, 20, 30];\n    print(a[0] + 1);\n}\n", "11\n");
    check("array index in arithmetic", "fn main()\n{\n    a := [10, 20, 30];\n    print(a[1] * a[2] - a[0]);\n}\n", "590\n");
    check("array len in loop", "fn main()\n{\n    a := [5, 6, 7, 8];\n    for i := 0 .. len(a) {\n        print(a[i]);\n    }\n}\n", "5\n6\n7\n8\n");
    check("array deep copy", "fn main()\n{\n    a := [1, 2, 3];\n    b := a;\n    b[0] = 9;\n    print(a[0]);\n    print(b[0]);\n}\n", "1\n9\n");
    check("array deep copy all elements", "fn main()\n{\n    a := [1, 2, 3];\n    b := a;\n    print(b[0]);\n    print(b[1]);\n    print(b[2]);\n}\n", "1\n2\n3\n");
    check("array reassign deep copy", "fn main()\n{\n    a := [1, 2, 3];\n    b := [9, 8];\n    a = b;\n    a[0] = 7;\n    print(a[0]);\n    print(b[0]);\n}\n", "7\n9\n");
    check("array typed decl", "fn main()\n{\n    a : i64[3] = [1, 2, 3];\n    print(len(a));\n    print(a[2]);\n}\n", "3\n3\n");
    check("array typed decl write", "fn main()\n{\n    a : i64[2] = [1, 2];\n    a[1] = 5;\n    print(a[1]);\n}\n", "5\n");
    check("array element promotion", "fn main()\n{\n    a := [1, 2 as u8];\n    print(a[0] + 1);\n}\n", "2\n");
    check("array comparison", "fn main()\n{\n    a := [3, 4];\n    print(a[0] < a[1]);\n}\n", "true\n");
    check("array index as bool cond", "fn main()\n{\n    a := [0, 1];\n    if a[1] {\n        print(1);\n    }\n}\n", "1\n");
    check("array element to var", "fn main()\n{\n    a := [10, 20];\n    x := a[1];\n    print(x);\n}\n", "20\n");
    check_error("array print", "fn main()\n{\n    a := [1, 2];\n    print(a);\n}\n", "Cannot print an array value");
    check_error("array index non-array", "fn main()\n{\n    x := 5;\n    print(x[0]);\n}\n", "Cannot index a non-array value");
    check_error("array len non-array", "fn main()\n{\n    print(len(5));\n}\n", "Builtin 'len' expects an array argument");
    check_error("array len arg count", "fn main()\n{\n    a := [1];\n    print(len());\n}\n", "Builtin 'len' expects exactly 1 argument");
    check_error("array typed decl mismatch", "fn main()\n{\n    a : i64[2] = [1, 2, 3];\n}\n", "cannot be assigned to a 2-element array");
    check_error("array element type", "fn main()\n{\n    a := [1, \"hi\"];\n}\n", "Invalid array element type");
    check_error("array empty literal", "fn main()\n{\n    a := [];\n}\n", "Empty array literals are not supported");

    // --- defer -----------------------------------------------------------
    check("defer runs at exit", "fn main()\n{\n    defer print(1);\n    print(0);\n}\n", "0\n1\n");
    check("defer lifo order", "fn main()\n{\n    defer print(1);\n    defer print(2);\n    defer print(3);\n}\n", "3\n2\n1\n");
    check("defer multiple returns", "fn f(v : i64) -> i64\n{\n    defer print(10);\n    if v > 5 {\n        return 1;\n    }\n    return 0;\n}\nfn main()\n{\n    print(f(9));\n    print(f(1));\n}\n", "10\n1\n10\n0\n");
    check("defer conditional true", "fn main()\n{\n    if true {\n        defer print(1);\n    }\n    print(0);\n}\n", "0\n1\n");
    check("defer conditional false", "fn main()\n{\n    if false {\n        defer print(1);\n    }\n    print(0);\n}\n", "0\n");
    check("defer conditional runtime", "fn f(on : bool)\n{\n    if on {\n        defer print(1);\n    }\n    print(0);\n}\nfn main()\n{\n    f(true);\n    f(false);\n}\n", "0\n1\n0\n");
    check("defer captures args", "fn main()\n{\n    x := 7;\n    defer print(x);\n    x = 99;\n}\n", "7\n");
    check("defer in helper fn", "fn note(v : i64)\n{\n    print(v);\n}\nfn f()\n{\n    defer note(5);\n    print(0);\n}\nfn main()\n{\n    f();\n    f();\n}\n", "0\n5\n0\n5\n");
    check("defer early return skips rest", "fn main()\n{\n    defer print(2);\n    return;\n    print(1);\n}\n", "2\n");
    check("defer before bare return", "fn f()\n{\n    defer print(1);\n    return;\n}\nfn main()\n{\n    f();\n    print(0);\n}\n", "1\n0\n");
    check("defer void fallthrough", "fn f()\n{\n    defer print(1);\n}\nfn main()\n{\n    f();\n    print(0);\n}\n", "1\n0\n");
    check("defer value from call", "fn get() -> i64\n{\n    return 42;\n}\nfn main()\n{\n    defer print(get());\n    print(0);\n}\n", "0\n42\n");
    check("defer print string", "fn main()\n{\n    defer print(\"x\");\n    print(\"y\");\n}\n", "yx");
    check("defer ffi cleanup", "extern fn malloc(size : u64) -> void^\nextern fn free(ptr : void^)\ngf := 0\nfn cleanup(p : void^)\n{\n    free(p);\n    gf = gf + 1;\n}\nfn work()\n{\n    p := malloc(4);\n    defer cleanup(p);\n    print(gf);\n}\nfn main()\n{\n    work();\n    work();\n    print(gf);\n}\n", "0\n1\n2\n");
    check_error("defer non-call", "fn main()\n{\n    defer 5;\n}\n", "Only function calls can be deferred");
    check_error("defer of reflection builtin", "fn main()\n{\n    a := [1, 2];\n    defer len(a);\n}\n", "Nothing to defer");

    // --- compile-time division / modulo by zero (was a SIGFPE crash) -------
    check_error("const div by zero", "fn main()\n{\n    print(5 / 0);\n}\n", "Division by zero in constant expression");
    check_error("const mod by zero", "fn main()\n{\n    print(5 % 0);\n}\n", "Division by zero in constant expression");
    check_error("const div by zero from expr", "fn main()\n{\n    print((2 + 3) % 0);\n}\n", "Division by zero in constant expression");
    check_error("const INT64_MIN / -1 overflow guard", "fn main()\n{\n    print((0 - 9223372036854775807 - 1) / (0 - 1));\n}\n", "Integer overflow in constant expression");

    // --- bool == / != (was rejected as "Invalid operands to EQUALS") -------
    check("bool equals bool", "fn main()\n{\n    a := true;\n    b := true;\n    print(a == b);\n    print(a != b);\n}\n", "true\nfalse\n");
    check("bool literal equality", "fn main()\n{\n    print(true == true);\n    print(true != false);\n}\n", "true\ntrue\n");
    check("bool from comparison", "fn main()\n{\n    done := 5 > 3;\n    print(done == true);\n    print(done == false);\n}\n", "true\nfalse\n");
    check_error("bool vs int equals", "fn main()\n{\n    x := 5;\n    print(x == true);\n}\n", "Invalid operands to EQUALS operation");

    // --- non-ASCII string literal (was garbled / misassembled) -------------
    check("utf-8 string literal", "fn main()\n{\n    s := \"café\";\n    print(s.len);\n}\n", "5\n");
    check("utf-8 multibyte len", "fn main()\n{\n    s := \"東京\";\n    print(s.len);\n    print(s.data != null);\n}\n", "6\ntrue\n");

    // --- arithmetic -----------------------------------------------------
    check("add", "fn main()\n{\n    print(2 + 3);\n}\n", "5\n");
    check("sub", "fn main()\n{\n    print(10 - 3);\n}\n", "7\n");
    check("mul", "fn main()\n{\n    print(6 * 7);\n}\n", "42\n");
    check("div (integer)", "fn main()\n{\n    print(21 / 2);\n}\n", "10\n");
    check("mod", "fn main()\n{\n    print(10 % 3);\n}\n", "1\n");
    check("operator precedence", "fn main()\n{\n    print(2 + 3 * 4);\n}\n", "14\n");
    check("parentheses", "fn main()\n{\n    print((2 + 3) * 4);\n}\n", "20\n");
    check("parentheses 2", "fn main()\n{\n    print((10 - 3) * 4);\n}\n", "28\n");

    // --- mul-by-power-of-2 -> shl codegen ---------------------------------
    // Runtime `x * pow2` must lower to `shl rax, k` (shift_math_optimization).
    // The excluded needle proves the codegen path is NOT relying on constant
    // folding; `check_asm` leaves folding enabled so folding-only regressions
    // would be caught by the mul-by-1 / non-pow2 / const-const tests below.
    check_asm("mul 8 -> lea", "fn main()\n{\n    x := 7;\n    print(x * 8);\n}\n",
              "lea rdi, [rdi*8]", "imul", "56\n");
    check_asm("mul 2 -> lea", "fn main()\n{\n    x := 9;\n    print(x * 2);\n}\n",
              "lea rdi, [rdi*2]", "imul", "18\n");
    check_asm("mul pow2 large -> shl", "fn main()\n{\n    x := 3;\n    print(x * 1024);\n}\n",
              "shl rdi, 10", "imul", "3072\n");
    check_asm("mul pow2 64 -> shl", "fn main()\n{\n    x := 5;\n    print(x * 64);\n}\n",
              "shl rdi, 6", "imul", "320\n");
    check_asm("mul pow2 with neg lhs", "fn main()\n{\n    x := 0 - 6;\n    print(x * 4);\n}\n",
              "lea rdi, [rdi*4]", "imul", "-24\n");
    check_asm("mul pow2 chain", "fn main()\n{\n    x := 11;\n    y := x * 8;\n    z := y * 16;\n    print(z);\n}\n",
              "shl r12, 4", "imul", "1408\n");
    check_asm("mul non-pow2 keeps imul", "fn main()\n{\n    x := 6;\n    print(x * 7);\n}\n",
              "imul rdi, 7", "shl rax", "42\n");
    check_asm("mul by 1 folds (no mult)", "fn main()\n{\n    x := 5;\n    print(x * 1);\n}\n",
              "", "imul", "5\n");
    check_asm("mul pow2 with addend folds", "fn main()\n{\n    x := 5;\n    print(x * 8 + 3);\n}\n",
              "lea rdi, [rdi*8]", "imul", "43\n");
    check_asm("mul neg pow2 -> neg+shl", "fn main()\n{\n    x := 6;\n    print(x * (0 - 8));\n}\n",
              "neg rdi", "imul", "-48\n");
    check_asm("mul const lhs -> lea", "fn main()\n{\n    x := 6;\n    print(8 * x);\n}\n",
              "lea rdi, [rsi*8]", "imul", "48\n");

    // --- math operates on the operands' actual locations --------------------
    // add/sub/imul read the operand registers directly instead of copying
    // through rax/rbx; comparisons compare in place. rbx is never touched.
    check_asm("math ops operate in place", "fn main()\n{\n    a := 5;\n    b := 3;\n    print(a + b);\n    print(a - b);\n    print(a < b);\n}\n",
              "lea rdi, [r12 + r13]", "mov rbx", "8\n2\nfalse\n");
    check_asm("compare operands in place", "fn main()\n{\n    a := 5;\n    b := 3;\n    print(a > b);\n}\n",
              "cmp rsi, rdi", "mov rbx", "true\n");
    check_asm("idiv uses divisor register in place", "fn main()\n{\n    x := 6;\n    y := 7;\n    print(x / y);\n}\n",
              "idiv rdi", "mov rcx", "0\n");
    // A single-use argument coalesces into its consumer's register: the arg
    // copy goes straight from the incoming register, with no rax/rsi hops.
    check_asm("arg coalesces into result register", "fn test(val : i64) -> i64\n{\n    print(val - 40);\n    return 0;\n}\nfn main()\n{\n    test(120);\n}\n",
              "lea rdi, [rdi - 40]", "mov rax, rdi", "80\n");

    // --- div/mod-by-power-of-2 -> shift codegen -----------------------------
    // Signed division needs the rounding correction (idiv truncates toward
    // zero); modulo keeps the dividend's sign. The excluded needle proves the
    // results do not come from idiv.
    check_asm("div pow2 -> sar", "fn main()\n{\n    x := 100;\n    print(x / 4);\n}\n",
              "sar rdi, 2", "idiv", "25\n");
    check_asm("div pow2 large -> sar", "fn main()\n{\n    x := 100000;\n    print(x / 1024);\n}\n",
              "sar rdi, 10", "idiv", "97\n");
    check_asm("div pow2 neg dividend", "fn main()\n{\n    x := 0 - 100;\n    print(x / 4);\n}\n",
              "sar rdi, 2", "idiv", "-25\n");
    check_asm("div pow2 neg dividend 2", "fn main()\n{\n    x := 0 - 7;\n    print(x / 2);\n}\n",
              "sar rdi, 1", "idiv", "-3\n");
    check_asm("div by neg pow2", "fn main()\n{\n    x := 100;\n    print(x / (0 - 8));\n}\n",
              "sar rdi, 3", "idiv", "-12\n");
    check_asm("div non-pow2 keeps idiv", "fn main()\n{\n    x := 100;\n    print(x / 6);\n}\n",
              "idiv rcx", "sar rax", "16\n");
    check_asm("mod pow2 -> and", "fn main()\n{\n    x := 100;\n    print(x % 8);\n}\n",
              "and rdi, rdx", "idiv", "4\n");
    check_asm("mod pow2 keeps dividend sign", "fn main()\n{\n    x := 0 - 7;\n    print(x % 4);\n}\n",
              "and rdi, rdx", "idiv", "-3\n");
    check_asm("mod by neg pow2", "fn main()\n{\n    x := 0 - 7;\n    print(x % (0 - 4));\n}\n",
              "and rdi, rdx", "idiv", "-3\n");
    check_asm("mod non-pow2 keeps idiv", "fn main()\n{\n    x := 100;\n    print(x % 6);\n}\n",
              "idiv rcx", "sar rax", "4\n");
    check_asm("mod pow2 divides evenly", "fn main()\n{\n    x := 64;\n    print(x % 8);\n}\n",
              "and rdi, rdx", "idiv", "0\n");

    // --- shift codegen on small integer types (i8/i16/i32) ------------------
    // Values of small types are stored sign-extended in 64-bit slots, so the
    // same sar/shl/and sequences are valid; the runtime cast (`as i8`) makes
    // the dividend non-constant so the shift path (not folding) is exercised.
    // Expected values cross-checked against C truncating semantics.
    check_asm("div pow2 i8 var", "fn main()\n{\n    x := 300;\n    y := x as i8;\n    print(y / 4);\n}\n",
              "sar rdi, 2", "idiv", "11\n");
    check_asm("div pow2 i8 neg", "fn main()\n{\n    x := 200;\n    y := x as i8;\n    print(y / 4);\n}\n",
              "sar rdi, 2", "idiv", "-14\n");
    check_asm("mod pow2 i8 neg", "fn main()\n{\n    x := 200;\n    y := x as i8;\n    print(y % 8);\n}\n",
              "and rdi, rdx", "idiv", "0\n");
    check_asm("mod pow2 i8 neg nonzero", "fn main()\n{\n    x := 207;\n    y := x as i8;\n    print(y % 8);\n}\n",
              "and rdi, rdx", "idiv", "-1\n");
    check_asm("div pow2 i16", "fn main()\n{\n    x := 40000;\n    y := x as i16;\n    print(y / 16);\n}\n",
              "sar rdi, 4", "idiv", "-1596\n");
    check_asm("mul neg pow2 i16", "fn main()\n{\n    x := 40000;\n    y := x as i16;\n    print(y * (0 - 8));\n}\n",
              "shl rdi, 3\n\tneg rdi", "imul", "204288\n");
    check_asm("div pow2 i32", "fn main()\n{\n    x := 1000;\n    y := x as i32;\n    print(y / 64);\n}\n",
              "sar rdi, 6", "idiv", "15\n");
    check_asm("mod pow2 i32", "fn main()\n{\n    x := 1000;\n    y := x as i32;\n    print(y % 32);\n}\n",
              "and rdi, rdx", "idiv", "8\n");

    // --- negative arithmetic --------------------------------------------
    // Division/mod must use signed idiv (regression: div/mul were unsigned).
    // The language has no unary minus, so negatives are written as "0 - n".
    check("neg div folded", "fn main()\n{\n    print((0 - 7) / 2);\n}\n", "-3\n");
    check("neg mod folded", "fn main()\n{\n    print((0 - 7) % 2);\n}\n", "-1\n");
    check("neg div runtime", "fn main()\n{\n    x := 0 - 7;\n    print(x / 2);\n}\n", "-3\n");
    check("neg mod runtime", "fn main()\n{\n    x := 0 - 7;\n    print(x % 2);\n}\n", "-1\n");
    check("div by negative", "fn main()\n{\n    x := 7;\n    y := 0 - 2;\n    print(x / y);\n}\n", "-3\n");
    check("mod by negative", "fn main()\n{\n    x := 7;\n    y := 0 - 2;\n    print(x % y);\n}\n", "1\n");
    check("neg div neg", "fn main()\n{\n    x := 0 - 7;\n    y := 0 - 2;\n    print(x / y);\n}\n", "3\n");
    check("neg mod neg", "fn main()\n{\n    x := 0 - 7;\n    y := 0 - 2;\n    print(x % y);\n}\n", "-1\n");
    check("neg mul runtime", "fn main()\n{\n    x := 0 - 2;\n    print(x * 3);\n}\n", "-6\n");
    check("neg mul neg", "fn main()\n{\n    x := 0 - 2;\n    y := 0 - 3;\n    print(x * y);\n}\n", "6\n");
    check("neg arithmetic chain", "fn main()\n{\n    x := 0 - 10;\n    y := x / 4 + x % 4;\n    print(y);\n}\n", "-4\n");
    check("neg in loop", "fn main()\n{\n    for i := 0 .. 3 { print(0 - i); }\n}\n", "0\n-1\n-2\n");
    check("neg call arg", "fn echo(a : i64) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    echo(0 - 42);\n}\n", "-42\n");
    check("neg var in expr", "fn main()\n{\n    x := 5;\n    print((0 - x) / 2);\n}\n", "-2\n");
    check("neg identity cross", "fn main()\n{\n    x := 0 - 6;\n    print(x * 0);\n    print(x - x);\n    print(x * 1);\n}\n", "0\n0\n-6\n");
    check("nested parens", "fn main()\n{\n    print(((2 + 3)));\n}\n", "5\n");

    // --- comparisons ----------------------------------------------------
    check("less true", "fn main()\n{\n    print(3 < 5);\n}\n", "true\n");
    check("greater true", "fn main()\n{\n    print(5 > 3);\n}\n", "true\n");
    check("greater false", "fn main()\n{\n    print(3 > 5);\n}\n", "false\n");
    check("equals true", "fn main()\n{\n    print(5 == 5);\n}\n", "true\n");
    check("equals false", "fn main()\n{\n    print(5 == 4);\n}\n", "false\n");
    check("not equals false", "fn main()\n{\n    print(5 != 5);\n}\n", "false\n");
    check("not equals true", "fn main()\n{\n    print(5 != 4);\n}\n", "true\n");
    check("not equals var lhs", "fn main()\n{\n    x := 7;\n    print(x != 5);\n}\n", "true\n");
    check("not equals var lhs equal", "fn main()\n{\n    x := 5;\n    print(x != 5);\n}\n", "false\n");
    check("not equals chain", "fn main()\n{\n    print(3 != 4);\n    print(4 != 4);\n}\n", "true\nfalse\n");
    check("not equals pointer null", "fn main()\n{\n    p := null;\n    print(p != null);\n    print(p == null);\n}\n", "false\ntrue\n");
    check("not equals addr null", "fn main()\n{\n    x := 10;\n    print(&x != null);\n}\n", "true\n");
    check("less equals true", "fn main()\n{\n    print(3 <= 3);\n}\n", "true\n");
    check("less equals false", "fn main()\n{\n    print(4 <= 3);\n}\n", "false\n");
    check("greater equals true", "fn main()\n{\n    print(3 >= 3);\n}\n", "true\n");
    check("greater equals false", "fn main()\n{\n    print(3 >= 4);\n}\n", "false\n");
    check("not true", "fn main()\n{\n    print(!true);\n}\n", "false\n");
    check("not false", "fn main()\n{\n    print(!false);\n}\n", "true\n");
    check("not zero", "fn main()\n{\n    print(!0);\n}\n", "true\n");
    check("not nonzero", "fn main()\n{\n    print(!1);\n}\n", "false\n");
    check("not comparison", "fn main()\n{\n    print(!(3 > 4));\n    print(!(5 > 4));\n}\n", "true\nfalse\n");
    check("not equality", "fn main()\n{\n    print(!(5 == 5));\n    print(!(5 == 4));\n}\n", "false\ntrue\n");
    check("not variable", "fn main()\n{\n    x := true;\n    print(!x);\n}\n", "false\n");
    check("double not", "fn main()\n{\n    print(!!true);\n}\n", "true\n");
    check("not in condition", "fn main()\n{\n    x := 3;\n    if !(x == 3) { print(1); } else { print(2); }\n}\n", "2\n");
    check("not negated loop bound", "fn main()\n{\n    for i := 0 .. 3 { if !(i == 1) { print(i); } }\n}\n", "0\n2\n");
    // lhs must be compared against rhs, never swapped (regression: the old
    // operator mapping inverted < / > and the old codegen computed `rhs OP lhs`).
    check("var lhs less", "fn main()\n{\n    x := 2;\n    print(x < 5);\n}\n", "true\n");
    check("var lhs less false", "fn main()\n{\n    x := 7;\n    print(x < 5);\n}\n", "false\n");
    check("var lhs greater", "fn main()\n{\n    x := 7;\n    print(x > 5);\n}\n", "true\n");
    check("var lhs greater false", "fn main()\n{\n    x := 2;\n    print(x > 5);\n}\n", "false\n");
    check("var lhs less equals", "fn main()\n{\n    x := 5;\n    print(x <= 5);\n}\n", "true\n");
    check("var lhs greater equals", "fn main()\n{\n    x := 5;\n    print(x >= 5);\n}\n", "true\n");
    check("comp lhs greater equals", "fn main()\n{\n    x := 3;\n    print(x >= 5);\n}\n", "false\n");
    check("comp rhs less equals", "fn main()\n{\n    x := 7;\n    print(5 <= x);\n}\n", "true\n");
    check("less equals chain", "fn main()\n{\n    x := 4;\n    print(x < 5);\n    print(5 <= 5);\n    print(x >= 4);\n}\n", "true\ntrue\ntrue\n");

    // --- casts (truncation) ----------------------------------------------
    // `300 as i8` must truncate the low bits (300 % 256 == 44), not be a no-op.
    check("cast i8 truncate ct", "fn main()\n{\n    print(300 as i8);\n}\n", "44\n");
    check("cast i8 no truncate ct", "fn main()\n{\n    print(42 as i8);\n}\n", "42\n");
    check("cast i8 sign ct", "fn main()\n{\n    print(200 as i8);\n}\n", "-56\n");
    check("cast i8 neg ct", "fn main()\n{\n    print(0 - 300 as i8);\n}\n", "-44\n");
    check("cast i16 truncate ct", "fn main()\n{\n    print(70000 as i16);\n}\n", "4464\n");
    check("cast i32 truncate ct", "fn main()\n{\n    print(4294967296 as i32);\n}\n", "0\n");
    check("cast i32 sign ct", "fn main()\n{\n    print(2147483648 as i32);\n}\n", "-2147483648\n");
    check("cast i64 no truncate ct", "fn main()\n{\n    print(300 as i64);\n}\n", "300\n");
    check("cast to bool ct", "fn main()\n{\n    print(5 as bool);\n    print(0 as bool);\n}\n", "true\nfalse\n");
    check("cast bool to int ct", "fn main()\n{\n    print(true as i8);\n    print(false as i8);\n}\n", "1\n0\n");
    check("cast then arithmetic", "fn main()\n{\n    print(1 + (300 as i8) * 2);\n}\n", "89\n");
    check("cast i8 truncate rt", "fn main()\n{\n    x := 300;\n    print(x as i8);\n}\n", "44\n");
    check("cast i8 neg rt", "fn main()\n{\n    x := 0 - 200;\n    print(x as i8);\n}\n", "56\n");
    check("cast i16 truncate rt", "fn main()\n{\n    x := 70000;\n    print(x as i16);\n}\n", "4464\n");
    check("cast i32 truncate rt", "fn main()\n{\n    x := 4294967296;\n    print(x as i32);\n}\n", "0\n");
    check("cast i64 rt", "fn main()\n{\n    x := 300;\n    print(x as i64);\n}\n", "300\n");
    check("cast to bool rt", "fn main()\n{\n    x := 5;\n    y := 0;\n    print(x as bool);\n    print(y as bool);\n}\n", "true\nfalse\n");
    check("cast expr rt", "fn main()\n{\n    x := 7;\n    print((x * 100) as i8);\n}\n", "-68\n");
    check("cast string to string ct", "fn main()\n{\n    print(\"abc\" as string);\n}\n", "abc");
    check("cast string to string rt", "fn main()\n{\n    x := \"hi\";\n    print(x as string);\n}\n", "hi");
    check("cast int to i64 identity", "fn main()\n{\n    x := 123;\n    print(x as i64);\n}\n", "123\n");
    check("cast bool to u8", "fn main()\n{\n    print(true as u8);\n    print(false as u8);\n}\n", "1\n0\n");
    check("cast bool to u8 rt", "fn main()\n{\n    x := 3 < 5;\n    print(x as u8);\n}\n", "1\n");
    check("cast u32 to i32 rt", "fn main()\n{\n    x := 3000000000 as u32;\n    print(x as i32);\n}\n", "-1294967296\n");

    // --- type builtins (#type_id / #type_size / #type_of) ------------------
    // #type_id returns the ValueType enum value: i8=3 i16=4 i32=5 i64=6
    // bool=2 string=8; #type_size returns logical bytes (bool=1, str/ptr=8);
    // #type_of returns the type name string.
    check("type_id int literal", "fn main()\n{\n    print(#type_id(5));\n}\n", "6\n");
    check("type_id int var", "fn main()\n{\n    x := 5;\n    print(#type_id(x));\n}\n", "6\n");
    check("type_id bool", "fn main()\n{\n    print(#type_id(true));\n}\n", "2\n");
    check("type_id string", "fn main()\n{\n    print(#type_id(\"abc\"));\n}\n", "12\n");
    check("type_id cast", "fn main()\n{\n    print(#type_id(5 as i8));\n    print(#type_id(5 as i16));\n    print(#type_id(5 as i32));\n}\n", "3\n4\n5\n");
    check("type_id unsigned", "fn main()\n{\n    print(#type_id(5 as u8));\n    print(#type_id(5 as u16));\n    print(#type_id(5 as u32));\n    print(#type_id(5 as u64));\n}\n", "7\n8\n9\n10\n");
    check("type_id string var", "fn main()\n{\n    x := \"hi\";\n    print(#type_id(x));\n}\n", "12\n");
    check("type_size i8", "fn main()\n{\n    print(#type_size(1 as i8));\n}\n", "1\n");
    check("type_size i16", "fn main()\n{\n    print(#type_size(1 as i16));\n}\n", "2\n");
    check("type_size i32", "fn main()\n{\n    print(#type_size(1 as i32));\n}\n", "4\n");
    check("type_size i64", "fn main()\n{\n    print(#type_size(5));\n}\n", "8\n");
    check("type_size bool", "fn main()\n{\n    print(#type_size(true));\n}\n", "1\n");
    check("type_size bool var", "fn main()\n{\n    b := 3 < 5;\n    print(#type_size(b));\n}\n", "1\n");
    check("type_size bool cast", "fn main()\n{\n    print(#type_size(5 as bool));\n}\n", "1\n");
    check("type_size string", "fn main()\n{\n    print(#type_size(\"abc\"));\n}\n", "8\n");
    check("type_of int", "fn main()\n{\n    print(#type_of(5));\n}\n", "i64");
    check("type_of i8", "fn main()\n{\n    print(#type_of(5 as i8));\n}\n", "i8");
    check("type_of i16", "fn main()\n{\n    print(#type_of(5 as i16));\n}\n", "i16");
    check("type_of bool", "fn main()\n{\n    print(#type_of(true));\n}\n", "bool");
    check("type_of string", "fn main()\n{\n    print(#type_of(\"abc\"));\n}\n", "string");
    check("type_of var", "fn main()\n{\n    x := 42;\n    print(#type_of(x));\n}\n", "i64");
    check("type_of string var", "fn main()\n{\n    x := \"hi\";\n    print(#type_of(x));\n}\n", "string");
    check("type builtins chained", "fn main()\n{\n    print(#type_id(5 as i32));\n    print(#type_size(5 as i8));\n    print(#type_of(5 as i16));\n}\n", "5\n1\ni16");
    check("type_size unsigned", "fn main()\n{\n    print(#type_size(5 as u8));\n    print(#type_size(5 as u16));\n    print(#type_size(5 as u32));\n    print(#type_size(5 as u64));\n}\n", "1\n2\n4\n8\n");
    check("type_of unsigned", "fn main()\n{\n    print(#type_of(5 as u8));\n    print(#type_of(5 as u16));\n    print(#type_of(5 as u32));\n    print(#type_of(5 as u64));\n}\n", "u8u16u32u64");
    check("type_of pointer", "fn main()\n{\n    x := 5;\n    p := &x;\n    q := &p;\n    print(#type_of(&x));\n    print(#type_of(p));\n    print(#type_of(q));\n    print(#type_of(&q));\n}\n", "i64^i64^i64^^i64^^^");
    check("type_of pointer param", "fn paramType(p : i64^^) -> i64\n{\n    print(#type_of(p));\n    return 0;\n}\nfn main()\n{\n    x := 5;\n    p := &x;\n    paramType(&p);\n}\n", "i64^^");
    // offset_of reports each field's C byte offset; align_of reports the
    // struct's C alignment (largest member alignment, empty struct -> 1).
    check("offset_of field offsets", "struct H { a : u8, b : u16, c : u32 }\nfn main()\n{\n    print(offset_of(H, \"a\"))\n    print(offset_of(H, \"b\"))\n    print(offset_of(H, \"c\"))\n}\n", "0\n2\n4\n");
    check("align_of struct alignment", "struct W { w : u16, q : i64 }\nstruct E { }\nstruct H { a : u8, b : u16, c : u32 }\nfn main()\n{\n    print(align_of(W))\n    print(align_of(H))\n    print(align_of(E))\n}\n", "8\n4\n1\n");
    check("offset_of align_of compose", "struct H { a : u8, b : u16, c : u32 }\nfn main()\n{\n    print(offset_of(H, \"b\") + align_of(H))\n    print(align_of(H) * 2 + offset_of(H, \"c\"))\n}\n", "6\n12\n");
    check("type_of pointer result", "fn getPtr(p : i64^) -> i64^\n{\n    return p;\n}\nfn main()\n{\n    x := 5;\n    print(#type_of(getPtr(&x)));\n}\n", "i64^");

    // --- type-dependent behavior ------------------------------------------
    // The same value must behave differently depending on its type: narrowing
    // casts truncate, and the reflection builtins report the static type.
    check("type dependent truncation", "fn main()\n{\n    x := 300;\n    print(x);\n    print(x as i8);\n    print(x as i16);\n    print(x as i32);\n    print(x as i64);\n}\n", "300\n44\n300\n300\n300\n");
    check("type dependent bool", "fn main()\n{\n    x := 5;\n    print(x);\n    print(x as bool);\n    print(0 as bool);\n}\n", "5\ntrue\nfalse\n");
    check("type dependent builtins", "fn main()\n{\n    x := 5;\n    print(#type_id(x));\n    print(#type_id(x as i8));\n    print(#type_size(x));\n    print(#type_size(x as i8));\n    print(#type_of(x));\n    print(#type_of(x as i8));\n}\n", "6\n3\n8\n1\ni64i8");
    check("type dependent literal", "fn main()\n{\n    print(300 as i8);\n    print(300 as i16);\n    print(300 as i32);\n    print(300 as i64);\n}\n", "44\n300\n300\n300\n");

    // --- type names as values ---------------------------------------------
    // A type name in expression context is a constant equal to its type id,
    // so it can be passed to the reflection builtins and compared directly.
    check("type name as id", "fn main()\n{\n    print(#type_id(i32));\n    print(#type_id(i8));\n    print(#type_id(bool));\n    print(#type_id(string));\n}\n", "5\n3\n2\n12\n");
    check("type name as size", "fn main()\n{\n    print(#type_size(i8));\n    print(#type_size(i16));\n    print(#type_size(i32));\n    print(#type_size(i64));\n}\n", "1\n2\n4\n8\n");
    check("type name as of", "fn main()\n{\n    print(#type_of(i8));\n    print(#type_of(string));\n}\n", "i8string");
    check("unsigned type name as id", "fn main()\n{\n    print(#type_id(u8));\n    print(#type_id(u16));\n    print(#type_id(u32));\n    print(#type_id(u64));\n}\n", "7\n8\n9\n10\n");
    check("unsigned type name as size", "fn main()\n{\n    print(#type_size(u8));\n    print(#type_size(u16));\n    print(#type_size(u32));\n    print(#type_size(u64));\n}\n", "1\n2\n4\n8\n");
    check("unsigned type name compare", "fn main()\n{\n    print(#type_id(5 as u8) == u8);\n    print(#type_id(5 as u16) == u16);\n    print(#type_id(5 as u32) == u32);\n    print(#type_id(5 as u64) == u64);\n}\n", "true\ntrue\ntrue\ntrue\n");
    check("type name compare", "fn main()\n{\n    print(#type_id(5) == i64);\n    print(#type_id(5 as i8) == i8);\n    print(#type_id(true) == #type_id(bool));\n    print(#type_id(\"hi\") == string);\n    print(#type_id(5) == i8);\n}\n", "true\ntrue\ntrue\ntrue\nfalse\n");
    check("type name conditional", "fn main()\n{\n    x := 5;\n    if (#type_id(x) == i32) { print(1); }\n    else { print(2); }\n    if (#type_id(x) == i64) { print(3); }\n}\n", "2\n3\n");
    check("type name in variable", "fn main()\n{\n    t := i16;\n    print(#type_id(t) == i16);\n    print(#type_size(t));\n}\n", "true\n2\n");

    // --- variables ------------------------------------------------------
    check("variable create", "fn main()\n{\n    x := 5;\n    print(x);\n}\n", "5\n");
    check("variable reassign", "fn main()\n{\n    x := 1;\n    x = x + 2;\n    print(x);\n}\n", "3\n");
    check("two variables", "fn main()\n{\n    a := 10;\n    b := 20;\n    print(a + b);\n}\n", "30\n");
    check("variable in expression", "fn main()\n{\n    x := 7;\n    print(x * 2);\n}\n", "14\n");
    check_error("reassign with := errors", "fn main()\n{\n    x := 1;\n    x := 2;\n}\n", "already created");
    check_error("assign to undeclared errors", "fn main()\n{\n    x = 1;\n}\n", "undeclared");

    // --- if / else ------------------------------------------------------
    check("if true", "fn main()\n{\n    if true { print(1); } else { print(2); }\n}\n", "1\n");
    check("if false", "fn main()\n{\n    if false { print(3); } else { print(4); }\n}\n", "4\n");
    check("if no else", "fn main()\n{\n    x := 5;\n    if x == 5 { print(9); }\n}\n", "9\n");
    check("if with comparison", "fn main()\n{\n    x := 10;\n    if x > 5 { print(1); } else { print(2); }\n}\n", "1\n");
    check("nested if", "fn main()\n{\n    if true { if false { print(1); } else { print(2); } }\n}\n", "2\n");

    // --- logical operators (&& / ||) -------------------------------------
    check("and true true", "fn main()\n{\n    print(true && true);\n}\n", "true\n");
    check("and true false", "fn main()\n{\n    print(true && false);\n}\n", "false\n");
    check("and false true", "fn main()\n{\n    print(false && true);\n}\n", "false\n");
    check("or false false", "fn main()\n{\n    print(false || false);\n}\n", "false\n");
    check("or false true", "fn main()\n{\n    print(false || true);\n}\n", "true\n");
    check("and or assignment", "fn main()\n{\n    a := true && false;\n    b := true || false;\n    c := a || b;\n    d := (2 < 3) && (4 > 3);\n    print(a);\n    print(b);\n    print(c);\n    print(d);\n}\n", "false\ntrue\ntrue\ntrue\n");
    check("and with numeric operands", "fn main()\n{\n    print(5 && 3);\n    print(0 && 7);\n    print(5 || 0);\n    print(0 || 7);\n}\n", "1\n0\n1\n1\n");
    check("and binds tighter than or", "fn main()\n{\n    print(true || false && false);\n    print((true || false) && false);\n}\n", "true\nfalse\n");
    check("and or with comparisons", "fn main()\n{\n    x := 5;\n    if x > 1 && x < 10 { print(1); }\n    if x < 1 || x > 2 { print(2); }\n}\n", "1\n2\n");
    check("and short-circuit side effect", "fn main()\n{\n    x := 0;\n    if false && x == 1 { }\n    if true || x == 1 { }\n    if true && x == 0 { print(3); }\n}\n", "3\n");
    check("and nested", "fn main()\n{\n    a := true;\n    b := true;\n    c := false;\n    print(a && b && c);\n    print(a || b || c);\n    print(a && b || c);\n}\n", "false\ntrue\ntrue\n");
    check("and in loop condition", "fn main()\n{\n    i := 0;\n    for i < 5 && i != 3 {\n        print(i);\n        i = i + 1;\n    }\n}\n", "0\n1\n2\n");
    check("or in loop condition", "fn main()\n{\n    i := 0;\n    for i < 2 || i < 4 {\n        print(i);\n        i = i + 1;\n    }\n}\n", "0\n1\n2\n3\n");
    check_error("logical op on strings", "fn main()\n{\n    print(\"a\" && \"b\");\n}\n", "Invalid operand");
    check_error("logical op on void", "fn main() -> i64\n{\n    if print(1) && true { return 0; }\n    return 1;\n}\n", "void value");

    // --- preprocessor (#define) ------------------------------------------
    check("define number", "#define SIZE 100\nfn main()\n{\n    print(SIZE);\n}\n", "100\n");
    check("define in expression", "#define WIDTH 20\n#define HEIGHT 30\nfn main()\n{\n    print(WIDTH * 2 + HEIGHT);\n}\n", "70\n");
    check("define chained", "#define SIZE 10\n#define DOUBLE_SIZE SIZE * 2\nfn main()\n{\n    print(DOUBLE_SIZE);\n}\n", "20\n");
    check("define string", "#define GREETING \"hi\"\nfn main()\n{\n    print(GREETING);\n}\n", "hi");
    check("define not in string", "#define FOO bar\nfn main()\n{\n    print(\"FOO\");\n}\n", "FOO");
    check_error("define not in identifier", "#define FOO 1\nfn main()\n{\n    print(FOOBAR);\n}\n", "undeclared");
    check("define unused", "#define UNUSED 42\nfn main()\n{\n    print(7);\n}\n", "7\n");
    check_error("define function-like macro", "#define MAX(a, b) a\nfn main()\n{\n    print(1);\n}\n", "function-like macros are not supported");

    // --- for loops ------------------------------------------------------
    check("for exclusive", "fn main()\n{\n    for i := 0 .. 3 { print(i); }\n}\n", "0\n1\n2\n");
    check("for condition true infinite", "fn main()\n{\n    i := 0;\n    for true {\n        i = i + 1;\n        if i == 3 { return; }\n    }\n    print(9);\n}\n", "");
    check("for condition false never runs", "fn main()\n{\n    for false { print(1); }\n    print(2);\n}\n", "2\n");
    check("for condition while", "fn main()\n{\n    i := 0;\n    for i < 3 {\n        print(i);\n        i = i + 1;\n    }\n}\n", "0\n1\n2\n");
    check("for condition not equals", "fn main()\n{\n    i := 0;\n    for i != 3 {\n        print(i);\n        i = i + 1;\n    }\n}\n", "0\n1\n2\n");
    check("for condition nested", "fn main()\n{\n    i := 0;\n    for i < 2 {\n        j := 0;\n        for j < 3 {\n            print(i * 10 + j);\n            j = j + 1;\n        }\n        i = i + 1;\n    }\n}\n", "0\n1\n2\n10\n11\n12\n");
    check("for condition bool var", "fn main()\n{\n    done := false;\n    i := 0;\n    for !done {\n        i = i + 1;\n        if i == 2 { done = true; }\n    }\n    print(i);\n}\n", "2\n");
    check("for condition return from loop", "fn main() -> i64\n{\n    for true { print(7); return 0; }\n    print(8);\n}\n", "7\n");
    check("for condition empty body", "fn main()\n{\n    i := 0;\n    for i < 0 { }\n    print(9);\n}\n", "9\n");
    check("for paren condition", "fn main()\n{\n    val := 0;\n    for (val < 10) {\n        val = val + 1;\n    }\n    print(val);\n}\n", "10\n");
    check("for break condition loop", "fn main()\n{\n    i := 0;\n    for (i < 10) {\n        i = i + 1;\n        if i == 5 { break; }\n    }\n    print(i);\n}\n", "5\n");
    check("for break true infinite", "fn main()\n{\n    i := 0;\n    for true {\n        i = i + 1;\n        if i >= 3 { break; }\n    }\n    print(i);\n}\n", "3\n");
    check("for continue condition loop", "fn main()\n{\n    i := 0;\n    for i < 5 {\n        i = i + 1;\n        if i == 3 { continue; }\n        print(i);\n    }\n    print(9);\n}\n", "1\n2\n4\n5\n9\n");
    check("for break range loop", "fn main()\n{\n    for i := 0 .. 10 {\n        if i == 3 { break; }\n        print(i);\n    }\n}\n", "0\n1\n2\n");
    check("for continue range loop", "fn main()\n{\n    for i := 0 .. 6 {\n        if i % 2 == 0 { continue; }\n        print(i);\n    }\n}\n", "1\n3\n5\n");
    check("for break nested", "fn main()\n{\n    for i := 0 .. 5 {\n        for j := 0 .. 5 {\n            if j == 2 { break; }\n            print(i * 10 + j);\n        }\n    }\n}\n", "0\n1\n10\n11\n20\n21\n30\n31\n40\n41\n");
    check("for continue nested", "fn main()\n{\n    for i := 0 .. 3 {\n        for j := 0 .. 3 {\n            if j == 1 { continue; }\n            print(i * 10 + j);\n        }\n    }\n}\n", "0\n2\n10\n12\n20\n22\n");
    check("for break after nested loop", "fn main()\n{\n    for i := 0 .. 5 {\n        for j := 0 .. 2 { print(j); }\n        if i == 2 { break; }\n    }\n    print(9);\n}\n", "0\n1\n0\n1\n0\n1\n9\n");
    check("for continue in if else", "fn main()\n{\n    for i := 0 .. 4 {\n        if i == 1 {\n            print(\"a\");\n            continue;\n        } else {\n            print(\"b\");\n        }\n    }\n}\n", "babb");
    check("for break in range with counter after", "fn main()\n{\n    for i := 0 .. 10 {\n        if i == 2 { break; }\n        print(i);\n    }\n    print(9);\n}\n", "0\n1\n9\n");
    check("for inclusive", "fn main()\n{\n    for j := 0 ..= 3 { print(j); }\n}\n", "0\n1\n2\n3\n");
    check("for accumulating sum", "fn main()\n{\n    sum := 0;\n    for n := 0 .. 5 { sum = sum + n; }\n    print(sum);\n}\n", "10\n");
    check("for with if", "fn main()\n{\n    for i := 0 .. 5 { if i == 3 { print(i); } }\n}\n", "3\n");
    check("for in exclusive", "fn main()\n{\n    for i in 0..3 { print(i); }\n}\n", "0\n1\n2\n");
    check("for in inclusive", "fn main()\n{\n    for j in 0..=3 { print(j); }\n}\n", "0\n1\n2\n3\n");
    check("for in accumulating sum", "fn main()\n{\n    sum := 0;\n    for n in 0..5 { sum = sum + n; }\n    print(sum);\n}\n", "10\n");
    check("for in nested", "fn main()\n{\n    for a in 0..3 { for b in 0..3 { print(a * 10 + b); } }\n}\n", "0\n1\n2\n10\n11\n12\n20\n21\n22\n");
    check("for in nonzero start", "fn main()\n{\n    for i in 2..5 { print(i); }\n}\n", "2\n3\n4\n");
    check("for in descending empty", "fn main()\n{\n    for i in 5..2 { print(i); }\n}\n", "");
    check("for in zero range", "fn main()\n{\n    for i in 3..3 { print(i); }\n}\n", "");
    check("for in single inclusive", "fn main()\n{\n    for i in 3..=3 { print(i); }\n}\n", "3\n");
    check("for in inclusive nonzero sum", "fn main()\n{\n    sum := 0;\n    for i in 2..=4 { sum = sum + i; }\n    print(sum);\n}\n", "9\n");
    check("for in function call bound", "fn get_n() -> i64\n{\n    return 3;\n}\nfn main()\n{\n    for i in 0..get_n() { print(i); }\n}\n", "0\n1\n2\n");
    check("for in variable bound", "fn main()\n{\n    n := 3;\n    for i in 0..n { print(i); }\n}\n", "0\n1\n2\n");
    check("for in folded bound", "fn main()\n{\n    for i in 0..(1 + 2) { print(i); }\n}\n", "0\n1\n2\n");
    check("for in param inclusive", "fn sumTo(n : i64) -> i64\n{\n    s := 0;\n    for i in 0..=n { s = s + i; }\n    return s;\n}\nfn main()\n{\n    print(sumTo(10));\n}\n", "55\n");
    check("for in reassign counter", "fn main()\n{\n    for i in 0..5 { print(i); if i == 1 { i = 3; } }\n}\n", "0\n1\n4\n");
    check("for in counter arithmetic", "fn main()\n{\n    for i in 0..3 { print(i * 2); }\n}\n", "0\n2\n4\n");
    check("for in counter unused", "fn main()\n{\n    sum := 0;\n    for i in 0..5 { sum = sum + 1; }\n    print(sum);\n}\n", "5\n");
    check("for in neg counter", "fn main()\n{\n    for i in 0..3 { print(0 - i); }\n}\n", "0\n-1\n-2\n");
    check("for in return from loop", "fn main() -> i64\n{\n    for i in 0..4 { if i == 2 { print(7); return 0; } }\n    print(8);\n}\n", "7\n");
    check("for in empty body", "fn main()\n{\n    for i in 0..3 { }\n    print(9);\n}\n", "9\n");
    check("for in loop helper", "fn sum_to(n : i64) -> i64\n{\n    s := 0;\n    for i in 0..n { s = s + i; }\n    return s;\n}\nfn main()\n{\n    print(sum_to(4));\n}\n", "6\n");
    check("for in called twice", "fn grid() -> i64\n{\n    for i in 0..2 { print(i); }\n    return 0;\n}\nfn main()\n{\n    grid();\n    grid();\n}\n", "0\n1\n0\n1\n");
    check("for in reuse var name", "fn main()\n{\n    for i in 0..2 { print(i); }\n    for i in 0..2 { print(10 + i); }\n    for i in 0..2 { print(20 + i); }\n}\n", "0\n1\n10\n11\n20\n21\n");
    check("for in inner bound outer var", "fn main()\n{\n    for i in 0..3 { for j in 0..i { print(j); } print(9); }\n}\n", "9\n0\n9\n0\n1\n9\n");
    check("for in inner start outer var", "fn main()\n{\n    sum := 0;\n    for i in 0..3 { for j in i..3 { sum = sum + 1; } }\n    print(sum);\n}\n", "6\n");
    check("for in triple nested", "fn main()\n{\n    for i in 0..2 { for j in 0..2 { for k in 0..2 { print(i * 100 + j * 10 + k); } } }\n}\n", "0\n1\n10\n11\n100\n101\n110\n111\n");
    check("for in inclusive nested", "fn main()\n{\n    sum := 0;\n    for i in 0..=2 { for j in 0..=i { sum = sum + 1; } }\n    print(sum);\n}\n", "6\n");
    check("for in with if", "fn main()\n{\n    for i in 0..5 { if i == 3 { print(i); } }\n}\n", "3\n");
    check("for in body only if", "fn main()\n{\n    for i in 0..4 { if i % 2 == 0 { print(i); } }\n}\n", "0\n2\n");
    check("for in return from nested", "fn f() -> i64\n{\n    for i in 0..3 { for j in 0..3 { if i == 1 { if j == 1 { return 42; } } } }\n    return 0;\n}\nfn main()\n{\n    print(f());\n}\n", "42\n");
    check("for in inside if", "fn main()\n{\n    x := 2;\n    if x == 2 { for i in 0..3 { print(i); } } else { print(9); }\n}\n", "0\n1\n2\n");
    check("for in loop var compare", "fn main()\n{\n    for i in 0..3 { for j in 0..3 { if i < j { print(i * 10 + j); } } }\n}\n", "1\n2\n12\n");
    check("for in sequential", "fn main()\n{\n    for i in 0..2 { print(i); }\n    for j in 0..2 { print(10 + j); }\n}\n", "0\n1\n10\n11\n");
    check("for in fresh inner var per iteration", "fn main()\n{\n    for i in 0..3 { t := i * 10; print(t); }\n}\n", "0\n10\n20\n");
    check("for in reuse var name after loop", "fn main()\n{\n    for i in 0..2 { print(i); }\n    i := 5;\n    print(i);\n}\n", "0\n1\n5\n");
    check("for reuse var name after loop", "fn main()\n{\n    for i := 0 .. 2 { print(i); }\n    i := 5;\n    print(i);\n}\n", "0\n1\n5\n");
    check("for in deep nesting wraps registers", "fn main()\n{\n    c := 0;\n    for a in 0..2 { for b in 0..2 { for d in 0..2 { for e in 0..2 { for f in 0..2 { c = c + 1; } } } } }\n    print(c);\n}\n", "32\n");
    check("for in counter in memory", "fn main()\n{\n    for i in 0..3 { p := &i; print(^p); }\n}\n", "0\n1\n2\n");
    check("for counter in memory", "fn main()\n{\n    for i := 0 .. 3 { p := &i; print(^p); }\n}\n", "0\n1\n2\n");
    check("for in nested counters in memory", "fn main()\n{\n    for i in 0..3 { for j in 0..3 { pi := &i; pj := &j; print(^pi * 10 + ^pj); } }\n}\n", "0\n1\n2\n10\n11\n12\n20\n21\n22\n");
    check("for in triple counters in memory", "fn main()\n{\n    for a in 0..2 { for b in 0..2 { for c in 0..2 { pa := &a; pb := &b; pc := &c; print(^pa * 100 + ^pb * 10 + ^pc); } } }\n}\n", "0\n1\n10\n11\n100\n101\n110\n111\n");
    check("for in nested register pressure", "fn main()\n{\n    sum := 0;\n    for i in 0..3 {\n        a1 := i * 1; a2 := i * 2; a3 := i * 3; a4 := i * 4; a5 := i * 5;\n        a6 := i * 6; a7 := i * 7; a8 := i * 8; a9 := i * 9; a10 := i * 10;\n        a11 := i * 11; a12 := i * 12;\n        for j in 0..3 {\n            sum = sum + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + i + j;\n        }\n    }\n    print(sum);\n}\n", "720\n");
    // A side-effecting loop bound (function call) must be evaluated exactly
    // once, before the loop. Regression: the bound used to be translated
    // inside the loop header, so `count()` incremented a global every
    // iteration and the counter could never catch up (infinite loop).
    check("for in bound call evaluated once", "c := 0\nfn count() -> i64\n{\n    c = c + 1;\n    return c;\n}\nfn main()\n{\n    for i in 0..count() { print(i); }\n    print(9);\n}\n", "0\n9\n");
    check("for bound call evaluated once", "c := 0\nfn count() -> i64\n{\n    c = c + 1;\n    return c;\n}\nfn main()\n{\n    for i := 0 .. count() { print(i); }\n    print(9);\n}\n", "0\n9\n");
    check("for in call in start", "c := 0\nfn start() -> i64\n{\n    c = c + 1;\n    return c;\n}\nfn main()\n{\n    for i in start()..3 { print(i); }\n    print(c);\n}\n", "1\n2\n1\n");
    check("for in counter as arg", "fn add(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    for i in 0..3 { print(add(i, 10)); }\n}\n", "10\n11\n12\n");
    check("for in inclusive counter as arg", "fn add(a : i64) -> i64\n{\n    return a * 10;\n}\nfn main()\n{\n    sum := 0;\n    for i in 0..=3 { sum = sum + add(i); }\n    print(sum);\n}\n", "60\n");
    check("for in struct member bound", "struct Foo { x : i64 }\nfn main()\n{\n    f := Foo { x: 3 };\n    for i in 0..f.x { print(i); }\n}\n", "0\n1\n2\n");
    check("for in param bound", "fn run(n : i64) -> i64\n{\n    for i in 0..n { print(i); }\n    return n;\n}\nfn main()\n{\n    print(run(3));\n}\n", "0\n1\n2\n3\n");
    check("for in complex bound expr", "fn main()\n{\n    n := 2;\n    for i in 0..(2 * n + 1) { print(i); }\n}\n", "0\n1\n2\n3\n4\n");
    check("for in descending inclusive empty", "fn main()\n{\n    for i in 3..=0 { print(i); }\n    print(9);\n}\n", "9\n");
    // Mutating the counter through a pointer inside the body: the memory
    // counter is bumped by the function and again by the loop increment.
    check("for in bump counter via ptr", "fn bump(p : i64^) -> i64\n{\n    ^p = ^p + 1;\n    return 0;\n}\nfn main()\n{\n    for i in 0..5 { print(i); bump(&i); }\n}\n", "0\n2\n4\n");
    // An if condition with side effects must be evaluated exactly once.
    check("if condition call evaluated once", "c := 0\nfn cond() -> bool\n{\n    c = c + 1;\n    return c == 1;\n}\nfn main()\n{\n    if cond() { print(1); } else { print(2); }\n    print(c);\n}\n", "1\n1\n");

    // --- functions ------------------------------------------------------
    check("function basic", "fn twice(a : i64) -> i64\n{\n    return a * 2;\n}\nfn main()\n{\n    print(twice(21));\n}\n", "42\n");
    check("function two args", "fn add(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    print(add(40, 2));\n}\n", "42\n");
    check("nested function calls", "fn add(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    print(add(add(1, 2), 3));\n}\n", "6\n");
    check("function defined after main", "fn main()\n{\n    print(twice(10));\n}\nfn twice(a : i64) -> i64\n{\n    return a * 2;\n}\n", "20\n");
    check("void function called twice", "fn say() -> i64\n{\n    print(7);\n    return 0;\n}\nfn main()\n{\n    say();\n    say();\n}\n", "7\n7\n");
    check("function prints its arg", "fn echo(x : i64) -> i64\n{\n    print(x);\n    return 0;\n}\nfn main()\n{\n    echo(99);\n}\n", "99\n");
    check("function return in expression", "fn add(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    x := add(2, 3);\n    print(x * 2);\n}\n", "10\n");
    check("multiple functions compose", "fn inc(a : i64) -> i64\n{\n    return a + 1;\n}\nfn dec(a : i64) -> i64\n{\n    return a - 1;\n}\nfn main()\n{\n    print(inc(inc(dec(5))));\n}\n", "6\n");
    check("loop helper", "fn sum_to(n : i64) -> i64\n{\n    s := 0;\n    for i := 0 .. n { s = s + i; }\n    return s;\n}\nfn main()\n{\n    print(sum_to(4));\n}\n", "6\n");

    // --- operator precedence ---------------------------------------------
    // Comparisons must bind looser than +, -, *, / (regression: `10 > 2 + 3`
    // used to parse as `(10 > 2) + 3` because all shared precedence level 1).
    check("comparison looser than plus", "fn main()\n{\n    print(10 > 2 + 3);\n}\n", "true\n");
    check("plus tighter than comparison", "fn main()\n{\n    print(2 + 3 > 4);\n}\n", "true\n");
    check("comparison looser than minus", "fn main()\n{\n    print(10 - 3 < 4);\n}\n", "false\n");
    check("comparison looser than mult", "fn main()\n{\n    print(10 > 2 * 3);\n}\n", "true\n");
    check("mult tighter than plus", "fn main()\n{\n    print(10 - 3 * 2);\n}\n", "4\n");
    check("same precedence left assoc", "fn main()\n{\n    print(20 / 5 * 2);\n}\n", "8\n");
    check("parens override", "fn main()\n{\n    print(10 > (2 + 3));\n}\n", "true\n");
    check("comparison equality mix", "fn main()\n{\n    print(2 * 3 == 6);\n}\n", "true\n");
    // `as` must bind tighter than comparisons (regression: it shared level 1,
    // so `a as u64 == b as u64` parsed as `((a as u64) == b) as u64`).
    check("cast binds tighter than comparison", "fn main()\n{\n    print(300 as u8 > 44);\n    print(300 as u8 == 44);\n}\n", "false\ntrue\n");
    check("cast compare wraps", "fn main()\n{\n    print((0 - 1) as u64 == (0 - 1) as u64);\n}\n", "true\n");
    // `as` must also bind tighter than * and + (precedence level 4 vs 3): the
    // cast applies to the single operand, then the arithmetic happens outside.
    check("cast binds tighter than mult", "fn main()\n{\n    print(3 as i8 * 2);\n}\n", "6\n");
    check("cast truncates before plus", "fn main()\n{\n    print(300 as i8 + 1);\n}\n", "45\n");

    // --- function return type propagation ---------------------------------
    // A call must report the callee's real return type (regression: calls
    // returned TYPE_NOP, crashing `x := g()` and rejecting `g() + g()`).
    check("call result in variable", "fn g() -> i64\n{\n    return 5;\n}\nfn main()\n{\n    x := g();\n    print(x);\n}\n", "5\n");
    check("call result in arithmetic", "fn g() -> i64\n{\n    return 5;\n}\nfn main()\n{\n    print(g() + g());\n}\n", "10\n");
    check("call result squared", "fn sq(x : i64) -> i64\n{\n    return x * x;\n}\nfn main()\n{\n    a := 3;\n    b := sq(a);\n    print(b);\n}\n", "9\n");
    check("noarg call before def", "fn main()\n{\n    x := val();\n    print(x);\n}\nfn val() -> i64\n{\n    return 7;\n}\n", "7\n");
    check("void-like call result used", "fn say() -> i64\n{\n    print(7);\n    return 0;\n}\nfn main()\n{\n    x := say();\n    print(x);\n}\n", "7\n0\n");

    // --- void functions ------------------------------------------------
    // A function without value returns is void and may only be called as a
    // statement. `-> type` is optional: omitting it means the function is void
    // and any value `return` is an error.
    check("void function called twice", "fn say()\n{\n    print(7);\n}\nfn main()\n{\n    say();\n    say();\n}\n", "7\n7\n");
    check("void function explicit type", "fn ping() -> void\n{\n    print(3);\n}\nfn main()\n{\n    ping();\n}\n", "3\n");
    check("void function bare return", "fn ping()\n{\n    print(3);\n    return;\n}\nfn main()\n{\n    ping();\n    ping();\n}\n", "3\n3\n");
    check("empty body void function", "fn foo()\n{\n}\nfn main()\n{\n    foo();\n    print(1);\n}\n", "1\n");
    check("void function after value return", "fn f(x : i64)\n{\n    if x > 0 { return; }\n    print(x);\n}\nfn main()\n{\n    f(1);\n    f(0 - 2);\n}\n", "-2\n");

    // A function without `-> type` is void: it may only use bare `return;`
    // statements. Value returns require an explicit `-> type`.
    check("implicit void function returns value to caller", "fn g() -> i64\n{\n    return 5;\n}\nfn main()\n{\n    x := g();\n    print(x + 1);\n}\n", "6\n");
    check("implicit void function", "fn g()\n{\n    print(9);\n}\nfn main()\n{\n    g();\n}\n", "9\n");
    check("void recursion callable", "fn g(n : i64)\n{\n    if n > 0 { g(n - 1); }\n    print(n);\n}\nfn main()\n{\n    g(3);\n}\n", "0\n1\n2\n3\n");

    // Explicit `fn f() -> type` return types, including truncation of the
    // returned value to the declared width.
    check("explicit u8 return truncates", "fn get() -> u8\n{\n    return 300;\n}\nfn main()\n{\n    print(get());\n}\n", "44\n");
    check("explicit i8 return truncates", "fn get() -> i8\n{\n    return 200;\n}\nfn main()\n{\n    print(get());\n}\n", "-56\n");
    check("explicit i64 return widening", "fn get() -> i64\n{\n    return 300 as u8;\n}\nfn main()\n{\n    print(get());\n}\n", "44\n");
    check("explicit string return", "fn greet() -> string\n{\n    return \"hi\";\n}\nfn main()\n{\n    print(greet());\n}\n", "hi");
    check("explicit bool return", "fn is_pos(n : i64) -> bool\n{\n    return n > 0;\n}\nfn main()\n{\n    print(is_pos(5));\n    print(is_pos(0 - 3));\n}\n", "true\nfalse\n");
    check("explicit typed arithmetic return", "fn add(a : u8, b : u8) -> u8\n{\n    return a + b;\n}\nfn main()\n{\n    print(add(200, 200));\n}\n", "144\n");
    check("declared recursive return type", "fn fib(n : i64) -> i64\n{\n    if n < 2 { return n; }\n    return fib(n - 1) + fib(n - 2);\n}\nfn main()\n{\n    print(fib(10));\n}\n", "55\n");

    // Mixed numeric return types in one body are fine (auto promotion/cast to
    // the type fixed by the first value return).
    check("mixed numeric returns promote", "fn maybe(n : i64) -> i64\n{\n    if n > 0 { return 5; }\n    return 300 as u8;\n}\nfn main()\n{\n    print(maybe(1));\n    print(maybe(0));\n}\n", "5\n44\n");
    check("mixed numeric returns declared", "fn maybe(n : i64) -> u8\n{\n    if n > 0 { return 300; }\n    return 7;\n}\nfn main()\n{\n    print(maybe(1));\n    print(maybe(0));\n}\n", "44\n7\n");
    check("main without return exits zero", "fn main()\n{\n    print(42);\n}\n", "42\n");

    // --- function calls & arguments corner cases --------------------------
    // Multi-arg calls, argument forwarding, nesting, and call results used in
    // arbitrary expression positions.
    check("multi arg sum", "fn add3(a : i64, b : i64, c : i64) -> i64\n{\n    return a + b + c;\n}\nfn main()\n{\n    print(add3(1, 2, 3));\n}\n", "6\n");
    check("many args", "fn sum5(a : i64, b : i64, c : i64, d : i64, e : i64) -> i64\n{\n    return a + b + c + d + e;\n}\nfn main()\n{\n    print(sum5(1, 2, 3, 4, 5));\n}\n", "15\n");
    check("stack args 7", "fn sum7(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, g : i64) -> i64\n{\n    return a + b + c + d + e + f + g;\n}\nfn main()\n{\n    print(sum7(1, 2, 3, 4, 5, 6, 7));\n}\n", "28\n");
    check("stack args 9", "fn sum9(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, g : i64, h : i64, i : i64) -> i64\n{\n    return a + b + c + d + e + f + g + h + i;\n}\nfn main()\n{\n    print(sum9(1, 2, 3, 4, 5, 6, 7, 8, 9));\n}\n", "45\n");
    check("nested calls", "fn add3(a : i64, b : i64, c : i64) -> i64\n{\n    return a + b + c;\n}\nfn double(x : i64) -> i64\n{\n    return x * 2;\n}\nfn main()\n{\n    print(double(add3(1, 2, 3)));\n}\n", "12\n");
    check("call result in expression", "fn add3(a : i64, b : i64, c : i64) -> i64\n{\n    return a + b + c;\n}\nfn double(x : i64) -> i64\n{\n    return x * 2;\n}\nfn main()\n{\n    print(add3(1, 2, 3) + double(4));\n}\n", "14\n");
    check("call in arithmetic chain", "fn chain(a : i64, b : i64, c : i64) -> i64\n{\n    return a * b + c;\n}\nfn main()\n{\n    print(chain(chain(1, 2, 3), 2, 1));\n}\n", "11\n");
    check("call in if condition", "fn isEven(n : i64) -> bool\n{\n    return (n % 2) == 0;\n}\nfn main()\n{\n    if isEven(8) { print(\"even\\n\"); } else { print(\"odd\\n\"); }\n    if isEven(7) { print(\"even\\n\"); } else { print(\"odd\\n\"); }\n}\n", "even\nodd\n");
    check("call in loop bound", "fn addLoop(n : i64) -> i64\n{\n    s := 0;\n    for i := 0 .. n { s = s + i; }\n    return s;\n}\nfn main()\n{\n    for i := 0 .. addLoop(2) { print(i); }\n}\n", "0\n");
    check("call result stored then reused", "fn addTo(a : i64) -> i64\n{\n    return a + 10;\n}\nfn main()\n{\n    x := addTo(1);\n    print(x);\n    print(x + 1);\n}\n", "11\n12\n");
    check("same var passed to two params", "fn add3(a : i64, b : i64, c : i64) -> i64\n{\n    return a + b + c;\n}\nfn main()\n{\n    x := 5;\n    print(add3(x, x, x));\n}\n", "15\n");
    check("argument is expression", "fn add3(a : i64, b : i64, c : i64) -> i64\n{\n    return a + b + c;\n}\nfn main()\n{\n    print(add3(1 + 1, 2 * 3, 10 - 4));\n}\n", "14\n");
    check("argument is call result", "fn add3(a : i64, b : i64, c : i64) -> i64\n{\n    return a + b + c;\n}\nfn main()\n{\n    print(add3(1, 2, add3(3, 4, 5)));\n}\n", "15\n");
    check("negative argument", "fn echo(a : i64) -> i64\n{\n    return a;\n}\nfn main()\n{\n    print(echo(0 - 42));\n}\n", "-42\n");
    check("bool argument", "fn isEven(n : i64) -> bool\n{\n    return (n % 2) == 0;\n}\nfn main()\n{\n    print(isEven(10));\n    print(isEven(9));\n}\n", "true\nfalse\n");
    // A typed bool parameter prints as true/false, like any bool value.
    check("bool argument prints as bool", "fn echo(a : bool) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    echo(2 < 3);\n}\n", "true\n");
    check("param forwarding", "fn ident(x : i64) -> i64\n{\n    return x;\n}\nfn addTo(a : i64) -> i64\n{\n    return a + 10;\n}\nfn main()\n{\n    print(addTo(ident(1) + ident(2)));\n}\n", "13\n");
    check("deep ident nesting", "fn ident(x : i64) -> i64\n{\n    return x;\n}\nfn main()\n{\n    print(ident(ident(ident(42))));\n}\n", "42\n");

    // Argument passing semantics: parameters are copies; mutating one must not
    // affect the caller, and unused parameters are fine.
    check("param mutation is by value", "fn mutate(p : i64) -> i64\n{\n    p = p + 1;\n    return p;\n}\nfn main()\n{\n    v := 5;\n    print(mutate(v));\n    print(v);\n}\n", "6\n5\n");
    check("unused parameter", "fn unused(a : i64, b : i64) -> i64\n{\n    return b;\n}\nfn main()\n{\n    print(unused(1, 2));\n}\n", "2\n");
    check("param shadowed by local", "fn shadow(x : i64) -> i64\n{\n    y := x + 1;\n    return y;\n}\nfn main()\n{\n    print(shadow(10));\n}\n", "11\n");
    check("param as loop counter", "fn sumTo(n : i64) -> i64\n{\n    s := 0;\n    for i := 0 ..= n { s = s + i; }\n    return s;\n}\nfn main()\n{\n    print(sumTo(10));\n}\n", "55\n");

    // --- typed parameters (name : type) ------------------------------------
    // A declared parameter type changes how the parameter behaves in the body:
    // bool prints as "true"/"false", u32 compares unsigned, and type builtins
    // report the declared type.
    check("typed bool param", "fn t(b : bool) -> i64\n{\n    print(b);\n    return 0;\n}\nfn main()\n{\n    t(true);\n    t(false);\n    t(2 < 3);\n}\n", "true\nfalse\ntrue\n");
    check("typed bool param casts", "fn t(b : bool) -> i64\n{\n    print(b as u32);\n    return 0;\n}\nfn main()\n{\n    t(true);\n}\n", "1\n");
    check("typed u32 param unsigned compare", "fn t(a : u32) -> i64\n{\n    print(a > (1000 as u32));\n    return 0;\n}\nfn main()\n{\n    t(5);\n    t(0 - 1);\n}\n", "false\ntrue\n");
    check("typed param type builtins", "fn t(a : u32) -> i64\n{\n    print(#type_id(a));\n    print(#type_size(a));\n    print(#type_of(a));\n    return 0;\n}\nfn main()\n{\n    t(5);\n}\n", "9\n4\nu32");
    check("mixed typed params", "fn t(a : u32, b : i64) -> i64\n{\n    print(#type_id(a));\n    print(#type_id(b));\n    return 0;\n}\nfn main()\n{\n    t(1, 2);\n}\n", "9\n6\n");
    check("typed string param", "fn t(s : string) -> i64\n{\n    print(s);\n    return 0;\n}\nfn main()\n{\n    t(\"hello\");\n}\n", "hello");
    check("typed params in user example", "fn test(arg1 : u32, arg2 : bool) -> i64\n{\n    print(arg2);\n    print(arg1 > (1000 as u32));\n    return 0;\n}\nfn main()\n{\n    test(5, true);\n    test(0 - 1, false);\n}\n", "true\nfalse\nfalse\ntrue\n");

    // --- call argument type checking ---------------------------------------
    // A typed parameter accepts only arguments of a compatible type: integer
    // parameters take any integer width (width/signedness are implicit
    // conversions, like the rest of the language), bool parameters take only
    // bools, and string parameters take only strings. There are no implicit
    // bool<->int conversions.
    check("typed param accepts any int width", "fn t(a : u8, b : u64) -> i64\n{\n    print(#type_id(a));\n    print(#type_id(b));\n    return 0;\n}\nfn main()\n{\n    t(1, 2);\n}\n", "7\n10\n");
    check("bool arg to bool param", "fn t(b : bool) -> i64\n{\n    print(b);\n    return 0;\n}\nfn main()\n{\n    t(true);\n    t(2 < 3);\n}\n", "true\ntrue\n");
    check("typed string param accepts string", "fn t(a : u32, b : string) -> i64\n{\n    print(#type_id(b));\n    return 0;\n}\nfn main()\n{\n    t(5, \"hi\");\n}\n", "12\n");

    // A typed parameter is passed as a full 64-bit slot, so printing it must
    // truncate the value to the declared width: a u8 parameter holding 300
    // prints 44 (low 8 bits), a signed width re-sign-extends, and an unsigned
    // width never prints negative.
    check("u8 param print truncates", "fn t(a : u8) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    t(300);\n    t(0 - 1);\n    t(5);\n}\n", "44\n255\n5\n");
    check("i8 param print truncates", "fn t(a : i8) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    t(200);\n    t(0 - 1);\n}\n", "-56\n-1\n");
    check("u16 param print truncates", "fn t(a : u16) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    t(70000);\n    t(0 - 1);\n}\n", "4464\n65535\n");
    check("u32 param print truncates", "fn t(a : u32) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    t(0 - 1);\n}\n", "4294967295\n");
    check("i8 param compare truncates", "fn t(a : i8) -> i64\n{\n    print(a == 44);\n    return 0;\n}\nfn main()\n{\n    t(300);\n    t(0 - 1);\n}\n", "true\nfalse\n");
    check("i8 param arithmetic truncates", "fn t(a : i8) -> i64\n{\n    print(a + 1);\n    print(a * 2);\n    return 0;\n}\nfn main()\n{\n    t(300);\n}\n", "45\n88\n");
    check("u8 param compare truncates", "fn t(a : u8) -> i64\n{\n    print(a == 44);\n    print(a < 100);\n    return 0;\n}\nfn main()\n{\n    t(300);\n}\n", "true\ntrue\n");

    // One test that exercises the full argument/parameter compatibility matrix.
    // 10 value types can be passed as an argument. The language allows integers
    // only into integer parameters, bools only into bool parameters, strings
    // only into string parameters. The source and expected output are built
    // below so every legal (arg, param) pair is compiled and run (66
    // combinations); the 34 illegal ones are covered by the generated matrix
    // of check_error tests in error_tests().
    {
        const char* arg_types[10] = {"i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "bool", "string"};
        const char* arg_exprs[10] = {"1 as i8", "1 as i16", "1 as i32", "1", "1 as u8", "1 as u16", "1 as u32", "1 as u64", "true", "\"x\""};
        auto compatible = [&](int arg, int param) -> bool {
            if (arg == 9) return param == 9;  // string arg: string param only
            if (param == 9) return false;     // int/bool arg: no string param
            if (arg == 8) return param == 8;  // bool arg: bool param only
            if (param == 8) return false;     // int arg: no bool param
            return true;                      // int arg: int param
        };
        std::string src, expected;
        for (int p = 0; p < 10; ++p)
            src += "fn takes" + std::string(arg_types[p]) + "(a : " + arg_types[p] + ") -> i64 { print(\"ok\\n\"); return 0; }\n";
        src += "fn main()\n{\n";
        for (int p = 0; p < 10; ++p)
            for (int a = 0; a < 10; ++a) {
                if (!compatible(a, p)) continue;
                src += "takes" + std::string(arg_types[p]) + "(" + arg_exprs[a] + ");\n";
                expected += "ok\n";
            }
        src += "}\n";
        check("argument type matrix", src, expected);
    }

    // Return-value corner cases.
    check("early return", "fn condRet(x : i64) -> i64\n{\n    if x > 10 { return 1; }\n    return 0;\n}\nfn main()\n{\n    print(condRet(3));\n    print(condRet(30));\n}\n", "0\n1\n");
    check("return in both if paths", "fn isOdd(n : i64) -> bool\n{\n    if n == 0 { return false; }\n    return true;\n}\nfn main()\n{\n    print(isOdd(3));\n    print(isOdd(0));\n}\n", "true\nfalse\n");
    check("return bool", "fn getBool() -> bool\n{\n    return 3 < 5;\n}\nfn main()\n{\n    print(getBool());\n}\n", "true\n");
    check("return local computed in loop", "fn sumLoop(n : i64) -> i64\n{\n    s := 0;\n    for i := 0 .. n { s = s + i; }\n    return s;\n}\nfn main()\n{\n    print(sumLoop(5));\n}\n", "10\n");
    check("call result reused twice", "fn get() -> i64\n{\n    return 7;\n}\nfn main()\n{\n    x := get();\n    print(x + get());\n}\n", "14\n");
    check("discard call result", "fn side() -> i64\n{\n    print(3);\n    return 0;\n}\nfn main()\n{\n    side();\n    side();\n}\n", "3\n3\n");

    // Recursion (regression: a recursive call saw its callee's return type as
    // NOP, so `fib(n - 1) + fib(n - 2)` failed with "Invalid operands").
    check("factorial recursion", "fn fact(n : i64) -> i64\n{\n    if n <= 1 { return 1; }\n    return n * fact(n - 1);\n}\nfn main()\n{\n    print(fact(5));\n}\n", "120\n");
    check("fib recursion", "fn fib(n : i64) -> i64\n{\n    if n < 2 { return n; }\n    return fib(n - 1) + fib(n - 2);\n}\nfn main()\n{\n    print(fib(10));\n}\n", "55\n");
    check("mutual recursion", "fn isOdd(n : i64) -> bool\n{\n    if n == 0 { return false; }\n    return isEven(n - 1);\n}\nfn isEven(n : i64) -> bool\n{\n    if n == 0 { return true; }\n    return isOdd(n - 1);\n}\nfn main()\n{\n    print(isEven(10));\n    print(isOdd(7));\n}\n", "true\ntrue\n");
    check("nested recursion", "fn fib(n : i64) -> i64\n{\n    if n < 2 { return n; }\n    return fib(n - 1) + fib(n - 2);\n}\nfn main()\n{\n    print(fib(fib(5)));\n}\n", "5\n");
    check("recursive descending sum", "fn sumDown(n : i64) -> i64\n{\n    if n == 0 { return 0; }\n    return n + sumDown(n - 1);\n}\nfn main()\n{\n    print(sumDown(4));\n}\n", "10\n");

    // Frame codegen: leaf functions get no prologue/epilogue, callers keep one.
    check_asm("leaf function frameless", "fn leaf() -> i64\n{\n    return 7;\n}\nfn main()\n{\n    print(leaf());\n}\n",
              "__leaf:\n\tmov rax, 7\n\tret", "", "7\n");
    check_asm("caller keeps frame", "fn leaf() -> i64\n{\n    return 7;\n}\nfn main()\n{\n    x := leaf();\n    print(x);\n}\n",
              "push rbp", "", "7\n");

    // --- comparisons with compile-time operands ----------------------------
    // Compile-time operands must be inlined, not read from uninitialized
    // stack slots (regression in OP_GREATER / OP_GREATER_EQUALS codegen).
    check("comp lhs greater", "fn main()\n{\n    x := 3;\n    print(5 < x);\n}\n", "false\n");
    check("comp lhs greater true", "fn main()\n{\n    x := 7;\n    print(5 < x);\n}\n", "true\n");
    check("comp both compare", "fn main()\n{\n    print(5 < 3);\n}\n", "false\n");
    check("comp both compare true", "fn main()\n{\n    print(3 < 5);\n}\n", "true\n");

    // --- for loop counter (OP_INC) register liveness -----------------------
    // The counter register must survive dead_code elimination.
    check("for counter live", "fn main()\n{\n    for i := 0 .. 4 { print(i); }\n}\n", "0\n1\n2\n3\n");
    check("for counter in arithmetic", "fn main()\n{\n    for i := 0 .. 3 { print(i * 2); }\n}\n", "0\n2\n4\n");
    check("nested for counters", "fn main()\n{\n    for i := 0 .. 3 { for j := 0 .. 3 { print(i * 10 + j); } }\n}\n", "0\n1\n2\n10\n11\n12\n20\n21\n22\n");
    check("for inclusive counter", "fn main()\n{\n    sum := 0;\n    for i := 0 ..= 4 { sum = sum + i; }\n    print(sum);\n}\n", "10\n");

    // --- constant folding ------------------------------------------------
    // Expressions whose operands are all compile-time must fold to constants
    // at translate time instead of emitting a runtime operation.
    check("folded arithmetic", "fn main()\n{\n    print(2 + 3 * 4);\n}\n", "14\n");
    check("folded comparison less", "fn main()\n{\n    print(1 + 1 < 4);\n}\n", "true\n");
    check("folded comparison equals", "fn main()\n{\n    print(2 * 3 == 6);\n}\n", "true\n");
    check("folded comparison greater eq", "fn main()\n{\n    print(2 * 3 >= 7);\n}\n", "false\n");
    check("folded comparison in if true", "fn main()\n{\n    if 2 + 2 == 4 { print(1); } else { print(2); }\n}\n", "1\n");
    check("folded comparison in if false", "fn main()\n{\n    if 2 + 2 == 5 { print(1); } else { print(2); }\n}\n", "2\n");
    check("folded comparison as arg", "fn echo(a : bool) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    echo(2 < 3);\n}\n", "true\n");
    check("folded bound for loop", "fn main()\n{\n    for i := 0 .. (1 + 2) { print(i); }\n}\n", "0\n1\n2\n");

    // --- control flow codegen cleanup -------------------------------------
    // A compile-time condition must emit only the taken branch: no condition
    // spill, no branch, no jump, no label (regression: it emitted a useless
    // `mov QWORD [rbp - 16], 1`, a jump-to-next and a dead else branch).
    check("const if emits taken branch", "fn main()\n{\n    if 2 + 2 == 4 { print(\"A\\n\"); } else { print(\"B\\n\"); }\n}\n", "A\n");
    check_asm("const if no branches", "fn main()\n{\n    if 2 + 2 == 4 { print(\"A\\n\"); } else { print(\"B\\n\"); }\n}\n",
              "call __print_str", ".lab_", "A\n");
    // A no-else `if` must not emit a jump-to-next right before its merge label
    // (regression: it emitted `jmp .lab_N` directly before `.lab_N:`).
    check_asm("no-else if no jump to next", "fn main()\n{\n    x := 9;\n    if x > 4 { print(\"A\\n\"); }\n    print(\"B\\n\");\n}\n",
              "call __print_str", "call __print_str\n\tjmp", "A\nB\n");
    check("no-else if true falls through", "fn main()\n{\n    x := 9;\n    if x > 4 { print(1); }\n    print(2);\n}\n", "1\n2\n");
    check("no-else if false falls through", "fn main()\n{\n    x := 2;\n    if x > 4 { print(1); }\n    print(2);\n}\n", "2\n");

    // --- algebraic identities --------------------------------------------
    // x + 0, x * 1, x - x, etc. must not emit a runtime operation; the
    // result reuses the operand register or folds to a constant.
    check("identity plus zero", "fn main()\n{\n    x := 7;\n    print(x + 0);\n}\n", "7\n");
    check("identity zero plus", "fn main()\n{\n    x := 7;\n    print(0 + x);\n}\n", "7\n");
    check("identity minus zero", "fn main()\n{\n    x := 7;\n    print(x - 0);\n}\n", "7\n");
    check("identity times one", "fn main()\n{\n    x := 7;\n    print(x * 1);\n}\n", "7\n");
    check("identity one times", "fn main()\n{\n    x := 7;\n    print(1 * x);\n}\n", "7\n");
    check("identity times zero", "fn main()\n{\n    x := 7;\n    print(x * 0);\n}\n", "0\n");
    check("identity zero times", "fn main()\n{\n    x := 7;\n    print(0 * x);\n}\n", "0\n");
    check("identity divide one", "fn main()\n{\n    x := 7;\n    print(x / 1);\n}\n", "7\n");
    check("identity mod one", "fn main()\n{\n    x := 7;\n    print(x % 1);\n}\n", "0\n");
    check("identity minus self", "fn main()\n{\n    x := 7;\n    print(x - x);\n}\n", "0\n");
    check("identity equals self", "fn main()\n{\n    x := 7;\n    print(x == x);\n}\n", "true\n");
    check("identity less self", "fn main()\n{\n    x := 7;\n    print(x < x);\n}\n", "false\n");
    check("identity less eq self", "fn main()\n{\n    x := 7;\n    print(x <= x);\n}\n", "true\n");
    check("identity in assignment", "fn main()\n{\n    x := 7;\n    y := x * 0;\n    print(y);\n}\n", "0\n");
    check("identity as arg", "fn echo(a : i64) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    x := 7;\n    echo(x * 0);\n}\n", "0\n");
    check("identity chained in if", "fn main()\n{\n    x := 0;\n    if x * 1 == x { print(5); }\n}\n", "5\n");

    // --- bool literals ---------------------------------------------------
    check("print true", "fn main()\n{\n    print(true);\n}\n", "true\n");
    check("print false", "fn main()\n{\n    print(false);\n}\n", "false\n");
    check("if literal true", "fn main()\n{\n    if true { print(1); } else { print(2); }\n}\n", "1\n");
    check("if literal false", "fn main()\n{\n    if false { print(1); } else { print(2); }\n}\n", "2\n");
    check("bool literal arg", "fn echo(a : bool) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    echo(true);\n}\n", "true\n");

    // --- nested for / if ------------------------------------------------
    check("if else inside for", "fn main()\n{\n    for i := 0 .. 3 { if i == 1 { print(10 + i); } else { print(20 + i); } }\n}\n", "20\n11\n22\n");
    check("for inside if", "fn main()\n{\n    x := 2;\n    if x == 2 { for i := 0 .. 3 { print(i); } } else { print(9); }\n}\n", "0\n1\n2\n");
    check("for inside else", "fn main()\n{\n    x := 1;\n    if x == 2 { print(9); } else { for i := 0 .. 3 { print(i); } }\n}\n", "0\n1\n2\n");
    check("triple nested for", "fn main()\n{\n    for i := 0 .. 2 { for j := 0 .. 2 { for k := 0 .. 2 { print(i * 100 + j * 10 + k); } } }\n}\n", "0\n1\n10\n11\n100\n101\n110\n111\n");
    check("nested if in for", "fn main()\n{\n    for i := 0 .. 3 { if i > 0 { if i < 3 { print(i); } } }\n}\n", "1\n2\n");
    check("else if chain", "fn main()\n{\n    x := 2;\n    if x == 1 { print(1); } else { if x == 2 { print(2); } else { print(3); } }\n}\n", "2\n");
    check("else if single", "fn main()\n{\n    x := 2;\n    if x == 1 { print(1); } else if x == 2 { print(2); }\n}\n", "2\n");
    check("else if first branch", "fn main()\n{\n    x := 1;\n    if x == 1 { print(1); } else if x == 2 { print(2); } else { print(3); }\n}\n", "1\n");
    check("else if final else", "fn main()\n{\n    x := 9;\n    if x == 1 { print(1); } else if x == 2 { print(2); } else { print(3); }\n}\n", "3\n");
    check("else if three way", "fn main()\n{\n    x := 3;\n    if x == 1 { print(1); } else if x == 2 { print(2); } else if x == 3 { print(3); } else { print(4); }\n}\n", "3\n");
    check("else if returns", "fn f(x : i64) -> i64\n{\n    if x == 1 { return 10; } else if x == 2 { return 20; } else if x == 3 { return 30; }\n    return 0;\n}\nfn main()\n{\n    print(f(2));\n    print(f(3));\n    print(f(4));\n}\n", "20\n30\n0\n");
    check("else if in loop", "fn main()\n{\n    sum := 0;\n    for i in 0..5 { if i == 0 { sum = sum + 1; } else if i == 1 { sum = sum + 2; } else if i == 2 { sum = sum + 4; } else { sum = sum + 8; } }\n    print(sum);\n}\n", "23\n");
    check_asm("else if inverted branches", "fn main()\n{\n    x := 2;\n    if x == 1 { print(1); } else if x == 2 { print(2); } else { print(3); }\n}\n",
              "cmp r12, 2\n\tjne .lab_4", "jmp .lab_4", "2\n");
    check("elif single", "fn main()\n{\n    x := 2;\n    if x == 1 { print(1); } elif x == 2 { print(2); }\n}\n", "2\n");
    check("elif final else", "fn main()\n{\n    x := 9;\n    if x == 1 { print(1); } elif x == 2 { print(2); } else { print(3); }\n}\n", "3\n");
    check("elif chained", "fn main()\n{\n    x := 3;\n    if x == 1 { print(1); } elif x == 2 { print(2); } elif x == 3 { print(3); }\n}\n", "3\n");
    check("elif mixed with else if", "fn main()\n{\n    x := 3;\n    if x == 1 { print(1); } elif x == 2 { print(2); } else if x == 3 { print(3); } else { print(4); }\n}\n", "3\n");
    check("elif returns", "fn f(x : i64) -> i64\n{\n    if x == 1 { return 10; } elif x == 2 { return 20; } return 0;\n}\nfn main()\n{\n    print(f(2));\n}\n", "20\n");
    check_asm("elif same as else if codegen", "fn main()\n{\n    x := 2;\n    if x == 1 { print(1); } elif x == 2 { print(2); } else { print(3); }\n}\n",
              "cmp r12, 2\n\tjne .lab_4", "jmp .lab_4", "2\n");
    check_error("elif without braces", "fn main()\n{\n    x := 2;\n    if x == 1 { print(1); } elif x == 2 print(2);\n}\n", "please use braces");
    check_error("elif reserved", "fn main()\n{\n    elif := 5;\n}\n", "Expected keyword");
    check("if with for in else", "fn main()\n{\n    x := 0;\n    if x == 0 { print(7); } else { for i := 0 .. 2 { print(i); } }\n    print(8);\n}\n", "7\n8\n");
    check("for with return", "fn find(n : i64) -> i64\n{\n    for i := 0 .. 5 { if i == n { return i; } }\n    return 99;\n}\nfn main()\n{\n    print(find(3));\n}\n", "3\n");
    check("triangular loop", "fn main()\n{\n    sum := 0;\n    for i := 0 .. 4 { for j := 0 .. i { sum = sum + 1; } }\n    print(sum);\n}\n", "6\n");
    check("inner loop bound outer var", "fn main()\n{\n    for i := 0 .. 3 { for j := 0 .. i { print(j); } print(9); }\n}\n", "9\n0\n9\n0\n1\n9\n");
    check("inner loop starts at outer var", "fn main()\n{\n    sum := 0;\n    for i := 0 .. 3 { for j := i .. 3 { sum = sum + 1; } }\n    print(sum);\n}\n", "6\n");
    check("nested for accumulate", "fn main()\n{\n    sum := 0;\n    for i := 0 .. 3 { for j := 0 .. 3 { sum = sum + i * j; } }\n    print(sum);\n}\n", "9\n");
    check("deep if nesting", "fn main()\n{\n    if true { if true { if true { if true { print(1); } } } } else { print(2); }\n}\n", "1\n");
    check("if inside for inside if", "fn main()\n{\n    if 1 < 2 { for i := 0 .. 3 { if i * 2 < 5 { print(i); } } }\n}\n", "0\n1\n2\n");
    check("nested loops counter var", "fn main()\n{\n    for i := 0 .. 2 { for j := 0 .. 2 { if i == j { print(i * 10 + j); } } }\n}\n", "0\n11\n");
    check("nested for with mod", "fn main()\n{\n    for i := 0 .. 4 { for j := 0 .. 4 { if j % 2 == 0 { print(i * 10 + j); } } }\n}\n", "0\n2\n10\n12\n20\n22\n30\n32\n");
    check("else in nested for", "fn main()\n{\n    for i := 0 .. 2 { for j := 0 .. 2 { if i > j { print(i); } else { print(j); } } }\n}\n", "0\n1\n1\n1\n");
    check("sequential for loops", "fn main()\n{\n    for i := 0 .. 2 { print(i); }\n    for j := 0 .. 2 { print(10 + j); }\n}\n", "0\n1\n10\n11\n");
    check("for then if then for", "fn main()\n{\n    for i := 0 .. 2 { print(i); }\n    if 1 < 3 { print(8); }\n    for j := 2 .. 4 { print(j); }\n}\n", "0\n1\n8\n2\n3\n");
    check("nested for in if in for", "fn main()\n{\n    for i := 0 .. 3 { if i == 1 { for j := 0 .. 2 { print(i * 10 + j); } } }\n}\n", "10\n11\n");
    check("return from nested for", "fn f() -> i64\n{\n    for i := 0 .. 3 { for j := 0 .. 3 { if i == 1 { if j == 1 { return 42; } } } }\n    return 0;\n}\nfn main()\n{\n    print(f());\n}\n", "42\n");
    check("nested loops in called function", "fn grid() -> i64\n{\n    for i := 0 .. 2 { for j := 0 .. 2 { print(i * 10 + j); } }\n    return 0;\n}\nfn main()\n{\n    grid();\n    grid();\n}\n", "0\n1\n10\n11\n0\n1\n10\n11\n");

    // --- for / if edge cases ----------------------------------------------
    check("if empty body else", "fn main()\n{\n    if false { } else { print(1); }\n}\n", "1\n");
    check("if empty then no else", "fn main()\n{\n    if false { print(1); }\n    print(2);\n}\n", "2\n");
    check("if empty body comp true", "fn main()\n{\n    if 1 < 2 { } else { print(1); }\n    print(2);\n}\n", "2\n");
    check("for empty if body", "fn main()\n{\n    for i := 0 .. 4 { if i == 1 { } else { print(i); } }\n}\n", "0\n2\n3\n");
    check("for with unreachable if", "fn main()\n{\n    for i := 0 .. 3 { if false { print(9); } print(i); }\n}\n", "0\n1\n2\n");
    check("for counter unused", "fn main()\n{\n    sum := 0;\n    for i := 0 .. 5 { sum = sum + 1; }\n    print(sum);\n}\n", "5\n");
    check("for bound function call", "fn get_n() -> i64\n{\n    return 3;\n}\nfn main()\n{\n    for i := 0 .. get_n() { print(i); }\n}\n", "0\n1\n2\n");
    check("if condition function call", "fn check(x : i64) -> i64\n{\n    if x == 3 { return 1; }\n    return 0;\n}\nfn main()\n{\n    if check(3) == 1 { print(7); } else { print(8); }\n}\n", "7\n");
    check("for with folded condition", "fn main()\n{\n    for i := 0 .. 4 { if i * 0 == 0 { print(i); } }\n}\n", "0\n1\n2\n3\n");
    check("return from loop main", "fn main() -> i64\n{\n    for i := 0 .. 4 { if i == 2 { print(7); return 0; } }\n    print(8);\n}\n", "7\n");
    check("four level nesting", "fn main()\n{\n    for i := 0 .. 2 { if i == 0 { for j := 0 .. 2 { if j == 1 { print(i * 10 + j); } } } }\n}\n", "1\n");
    check("loop var compare inner", "fn main()\n{\n    for i := 0 .. 3 { for j := 0 .. 3 { if i < j { print(i * 10 + j); } } }\n}\n", "1\n2\n12\n");
    check("nested for inside if inside for", "fn main()\n{\n    for i := 0 .. 3 { if i == 2 { for j := 0 .. 2 { for k := 0 .. 2 { print(i * 100 + j * 10 + k); } } } }\n}\n", "200\n201\n210\n211\n");
    check("for with body only if", "fn main()\n{\n    for i := 0 .. 4 { if i % 2 == 0 { print(i); } }\n}\n", "0\n2\n");
    check("if reassign in loop", "fn main()\n{\n    x := 0;\n    for i := 0 .. 3 { if i == 1 { x = x + 1; } }\n    print(x);\n}\n", "1\n");
    check("inclusive nested", "fn main()\n{\n    sum := 0;\n    for i := 0 ..= 2 { for j := 0 ..= i { sum = sum + 1; } }\n    print(sum);\n}\n", "6\n");
    check("reuse loop var name", "fn main()\n{\n    for i := 0 .. 2 { print(i); }\n    for i := 0 .. 2 { print(10 + i); }\n    for i := 0 .. 2 { print(20 + i); }\n}\n", "0\n1\n10\n11\n20\n21\n");
    check("reuse loop var same name nested if", "fn main()\n{\n    x := 5;\n    if x > 0 { for i := 0 .. 2 { print(i); } }\n    for i := 0 .. 2 { print(10 + i); }\n    if x > 0 { for i := 0 .. 2 { print(20 + i); } }\n}\n", "0\n1\n10\n11\n20\n21\n");
    check("loop var scoped after loop", "fn main()\n{\n    for i := 0 .. 2 { print(i); }\n    x := 9;\n    print(x);\n}\n", "0\n1\n9\n");

    // --- SSA / register liveness corner cases -------------------------------
    // Discarded call results must not spill into a slot that a live variable
    // owns (regression: OP_CALL stored rax unconditionally into the result
    // register's stale allocation-time offset, aliasing a live slot).
    check("discard call result", "fn f() -> i64\n{\n    print(1);\n    return 99;\n}\nfn main()\n{\n    f();\n    print(2);\n}\n", "1\n2\n");
    check("discard call result then var", "fn f() -> i64\n{\n    return 99;\n}\nfn main()\n{\n    f();\n    x := 5;\n    print(x);\n}\n", "5\n");
    check("discard call result then loop", "fn f() -> i64\n{\n    return 99;\n}\nfn main()\n{\n    f();\n    for i := 0 .. 3 { print(i); }\n}\n", "0\n1\n2\n");
    check("discard two call results", "fn f() -> i64\n{\n    print(1);\n    return 2;\n}\nfn main()\n{\n    f();\n    f();\n    print(3);\n}\n", "1\n1\n3\n");
    check("discard call in loop", "fn f() -> i64\n{\n    return 99;\n}\nfn main()\n{\n    for i := 0 .. 2\n    {\n        x := 5;\n        f();\n        print(x);\n    }\n}\n", "5\n5\n");
    check("used call result still spills", "fn f() -> i64\n{\n    return 42;\n}\nfn main()\n{\n    y := f();\n    print(y);\n}\n", "42\n");
    // A value computed but never consumed must not affect later code.
    check("unused expression statement", "fn main()\n{\n    2 + 3;\n    print(1);\n}\n", "1\n");
    check("unused variable", "fn main()\n{\n    x := 42;\n    print(5);\n}\n", "5\n");
    check("dead branch eliminated", "fn main()\n{\n    if false { print(9 + 9); }\n    print(3);\n}\n", "3\n");
    // Long live chains: every register must get its own stack slot.
    check("many live registers", "fn main()\n{\n    a := 1;\n    b := a + 1;\n    c := b + 1;\n    d := c + 1;\n    e := d + 1;\n    print(a + b + c + d + e);\n}\n", "15\n");
    check("deep arithmetic chain", "fn main()\n{\n    x := 2;\n    y := x * 3 + 1;\n    z := (y - 4) * 2;\n    w := z + y / x;\n    print(w);\n}\n", "9\n");
    check("value used twice", "fn main()\n{\n    x := 7;\n    print(x + 1);\n    print(x + 2);\n}\n", "8\n9\n");
    check_asm("loop counter stays in register",
              "fn main()\n{\n    for i := 0 .. 5 { print(i); }\n}\n",
              "inc r12", "inc qword", "0\n1\n2\n3\n4\n");
    check_asm("loop counter hint cycles callee-saved",
              "fn main()\n{\n    for i := 0 .. 3 {\n        for j := 0 .. 3 {\n            print(i + j);\n        }\n    }\n}\n",
              "inc r13", "", "0\n1\n2\n1\n2\n3\n2\n3\n4\n");
    check_asm("call-crossing value in callee-saved",
              "fn main()\n{\n    n := 3;\n    for i := 0 .. n { print(i); }\n}\n",
              "push r12", "push rsi", "0\n1\n2\n");
    // Arguments travel in registers (SysV style) instead of the stack.
    check_asm("register argument passing",
              "fn add(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    print(add(2, 3));\n}\n",
              "mov rdi, 2\n\tmov rsi, 3", "mov [rsp", "5\n");
    check_asm("arg function frameless",
              "fn add(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    print(add(2, 3));\n}\n",
              "__add:\n\tmov r8, rdi", "__add:\n\tpush", "5\n");
    check_asm("recursive call arg in register",
              "fn fact(n : i64) -> i64\n{\n    if n < 2 { return 1; }\n    return n * fact(n - 1);\n}\nfn main()\n{\n    print(fact(5));\n}\n",
              "lea rdi, [r12 - 1]\n\tcall __fact", "[rbp -", "120\n");
    // Peephole: redundant rax round trips are collapsed.
    check_asm("peephole forwards print arg directly",
              "fn main()\n{\n    print(5);\n}\n",
              "mov rdi, 5\n\tcall __print_num", "mov rdi, rax", "5\n");
    check_asm("peephole drops store-reload round trip",
              "fn fact(n : i64) -> i64\n{\n    if n < 2 { return 1; }\n    return n * fact(n - 1);\n}\nfn main()\n{\n    print(fact(5));\n}\n",
              "mov r12, rax\n\tmov rdi, rax\n\tcall __print_num", "mov r12, rax\n\tmov rax, r12", "120\n");
    // add/sub of literal 1 must lower to inc/dec (globals spill through a
    // register, so `add rsi, 1` -> `inc rsi`; inc/dec leave CF untouched and
    // the compiler always emits an explicit test/cmp before reading flags).
    check_asm("add 1 -> inc", "gx : i64 = 1\nfn main()\n{\n    gx = gx + 1\n    print(gx)\n}\n",
              "inc rsi", "add rsi, 1", "2\n");
    check_asm("sub 1 -> dec", "gx : i64 = 5\nfn main()\n{\n    gx = gx - 1\n    print(gx)\n}\n",
              "dec rsi", "sub rsi, 1", "4\n");
    // `jcc .lab_A` followed by `jmp .lab_B` directly before `.lab_A:` inverts
    // to a single `j<!cc> .lab_B` and falls through into the label.
    check_asm("loop falls through into exit",
              "fn main()\n{\n    for i := 0 .. 5 { print(i); }\n}\n",
              "jge .lab_2\n.lab_1:", "jmp .lab_2", "0\n1\n2\n3\n4\n");
    check_asm("if/else inverts branch",
              "fn main()\n{\n    x := 5;\n    if x > 3 { print(1); } else { print(0); }\n}\n",
              "jle .lab_1\n.lab_0:", "jmp .lab_1", "1\n");
    check_asm("for in falls through into exit",
              "fn main()\n{\n    for i in 0..5 { print(i); }\n}\n",
              "jge .lab_2\n.lab_1:", "jmp .lab_2", "0\n1\n2\n3\n4\n");
    check_asm("for in inclusive uses jg",
              "fn main()\n{\n    for i in 0..=3 { print(i); }\n}\n",
              "jg .lab_2\n.lab_1:", "jmp .lab_2", "0\n1\n2\n3\n");
    check_asm("for in counter uses inc",
              "fn main()\n{\n    for i in 0..3 { print(i); }\n}\n",
              "inc r12", "add r12, 1", "0\n1\n2\n");
    check_asm("for in nested counter r13",
              "fn main()\n{\n    for a in 0..2 { for b in 0..2 { print(a * 10 + b); } }\n}\n",
              "inc r13", "add r13, 1", "0\n1\n10\n11\n");
    check_asm("for in counter lives in memory",
              "fn main()\n{\n    for i in 0..3 { p := &i; print(^p); }\n}\n",
              "inc qword [rbp - 8]", "inc r12", "0\n1\n2\n");
    check_asm("for in nested counters in memory",
              "fn main()\n{\n    for i in 0..3 { for j in 0..3 { pi := &i; pj := &j; print(^pi * 10 + ^pj); } }\n}\n",
              "inc qword [rbp - 16]", "inc r12", "0\n1\n2\n10\n11\n12\n20\n21\n22\n");
    // The hoisted loop bound stays in a callee-saved register across the
    // body's call and the bound call runs exactly once, before the header.
    check_asm("for in bound call hoisted callee-saved",
              "c := 0\nfn count() -> i64\n{\n    c = c + 1;\n    return c;\n}\nfn main()\n{\n    for i in 0..count() { print(i); }\n    print(9);\n}\n",
              "call __count\n\tmov r13, rax\n.lab_0:\n\tcmp r12, r13", "call __count\n.lab_0:", "0\n9\n");
    check("old value kept after reassign", "fn main()\n{\n    x := 1;\n    y := x;\n    x = 2;\n    print(y);\n    print(x);\n}\n", "1\n2\n");
    check("bool stored and printed", "fn main()\n{\n    x := 3 < 5;\n    print(x);\n}\n", "true\n");
    check("bool var in if", "fn main()\n{\n    x := 5;\n    b := x == 5;\n    if b { print(1); } else { print(2); }\n}\n", "1\n");
    check("comparison of two vars", "fn main()\n{\n    x := 5;\n    y := 3;\n    print(x < y);\n}\n", "false\n");
    // comp_time operands folded into runtime operations must inline.
    check("comp rhs in runtime add", "fn main()\n{\n    x := 7;\n    print(x + (1 + 2));\n}\n", "10\n");
    check("comp lhs in runtime add", "fn main()\n{\n    x := 7;\n    print((1 + 2) + x);\n}\n", "10\n");
    check("comp folded chain", "fn main()\n{\n    print(1 + 2 * 3 + 4 - 5);\n}\n", "6\n");
    check("comp folded call args", "fn add(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    print(add(1 + 2, 3 + 4));\n}\n", "10\n");
    check("call result in loop bound", "fn get() -> i64\n{\n    return 3;\n}\nfn main()\n{\n    n := get();\n    for i := 0 .. n { print(i); }\n}\n", "0\n1\n2\n");
    check("string var reused", "fn main()\n{\n    s := \"hi\";\n    print(s);\n    print(s);\n}\n", "hihi");
    // Strings are 8-byte pointers, so `=` reassignment and function returns
    // must copy the pointer, not do anything type-specific.
    check("string reassign", "fn main()\n{\n    s := \"a\";\n    s = \"b\";\n    print(s);\n}\n", "b");
    check("string reassign twice", "fn main()\n{\n    s := \"a\";\n    s = \"b\";\n    s = \"c\";\n    print(s);\n}\n", "c");
    check("function returns string literal", "fn getS() -> string\n{\n    return \"hi\\n\";\n}\nfn main()\n{\n    print(getS());\n}\n", "hi\n");
    check("function returns string var", "fn getS() -> string\n{\n    s := \"hello\";\n    return s;\n}\nfn main()\n{\n    print(getS());\n}\n", "hello");
    check("loop heavy body liveness", "fn main()\n{\n    s := 0;\n    for i := 0 .. 5\n    {\n        t := i * 2;\n        u := t + 1;\n        s = s + u;\n    }\n    print(s);\n}\n", "25\n");
    check("reassign loop counter", "fn main()\n{\n    for i := 0 .. 5\n    {\n        print(i);\n        if i == 1 { i = 3; }\n    }\n}\n", "0\n1\n4\n");

    // --- more for / if ------------------------------------------------------
    check("for empty body", "fn main()\n{\n    for i := 0 .. 3 { }\n    print(5);\n}\n", "5\n");
    check("for empty body runtime bound", "fn main()\n{\n    n := 2;\n    for i := 0 .. n { }\n    print(9);\n}\n", "9\n");
    check("for empty body in function", "fn hold() -> i64\n{\n    for i := 0 .. 2 { }\n    return 7;\n}\nfn main()\n{\n    print(hold());\n}\n", "7\n");
    check("for reversed range empty", "fn main()\n{\n    for i := 3 .. 0 { print(i); }\n    print(1);\n}\n", "1\n");
    check("for equal bounds empty", "fn main()\n{\n    for i := 0 .. 0 { print(i); }\n    print(1);\n}\n", "1\n");
    check("for inclusive single", "fn main()\n{\n    for i := 0 ..= 0 { print(i); }\n    print(1);\n}\n", "0\n1\n");
    check("for if else parity", "fn main()\n{\n    for i := 0 .. 4\n    {\n        if i % 2 == 0 { print(10 + i); } else { print(20 + i); }\n    }\n}\n", "10\n21\n12\n23\n");
    check("if chain inside for", "fn main()\n{\n    for i := 0 .. 4\n    {\n        if i == 0 { print(1); }\n        else { if i == 2 { print(2); } else { print(3); } }\n    }\n}\n", "1\n3\n2\n3\n");
    check("for else contains for", "fn main()\n{\n    for i := 0 .. 3\n    {\n        if i == 2 { print(7); }\n        else { for j := 0 .. 2 { print(i * 10 + j); } }\n    }\n}\n", "0\n1\n10\n11\n7\n");
    check("loop var arithmetic condition", "fn main()\n{\n    for i := 0 .. 5 { if i * 2 >= 5 { print(i); } }\n}\n", "3\n4\n");
    check("for bound compound expr", "fn main()\n{\n    for i := 0 .. (1 + 2) * 1 { print(i); }\n}\n", "0\n1\n2\n");
    check("if reassign both branches", "fn main()\n{\n    x := 1;\n    if 2 < 3 { x = 5; } else { x = 6; }\n    print(x);\n}\n", "5\n");
    check("nested if reassign accumulate", "fn main()\n{\n    x := 0;\n    for i := 0 .. 3\n    {\n        if i == 1 { x = x + 10; }\n        else { if i == 2 { x = x + 1; } }\n    }\n    print(x);\n}\n", "11\n");
    check("for inside fn inside loop", "fn run() -> i64\n{\n    for i := 0 .. 2 { print(i); }\n    return 0;\n}\nfn main()\n{\n    for j := 0 .. 2 { run(); }\n}\n", "0\n1\n0\n1\n");
    check("if condition comp mixed", "fn main()\n{\n    x := 4;\n    if x > 2 + 1 { print(1); }\n}\n", "1\n");
    check("for body calls value fn", "fn double(a : i64) -> i64\n{\n    return a * 2;\n}\nfn main()\n{\n    for i := 0 .. 3 { print(double(i)); }\n}\n", "0\n2\n4\n");
    check("empty if both branches", "fn main()\n{\n    if 1 < 2 { } else { }\n    print(1);\n}\n", "1\n");
    check("sequential loops shared acc", "fn main()\n{\n    s := 0;\n    for i := 0 .. 3 { s = s + 1; }\n    for i := 0 .. 2 { s = s + 10; }\n    print(s);\n}\n", "23\n");
    check("return from nested if in loop", "fn f() -> i64\n{\n    for i := 0 .. 3\n    {\n        if i == 0 { }\n        else { if i == 2 { return 5; } }\n    }\n    return 9;\n}\nfn main()\n{\n    print(f());\n}\n", "5\n");
    check("deep for if for if nesting", "fn main()\n{\n    for i := 0 .. 2\n    {\n        if i == 1\n        {\n            for j := 0 .. 2\n            {\n                if j == 0 { print(i * 10 + j); }\n            }\n        }\n    }\n}\n", "10\n");
    check("two loops same body var", "fn main()\n{\n    x := 0;\n    for i := 0 .. 3 { x = x + 1; }\n    for i := 0 .. 2 { x = x + x; }\n    print(x);\n}\n", "12\n");

    // --- misc -----------------------------------------------------------
    check("line comment", "// hello\nfn main()\n{\n    print(5);\n}\n", "5\n");
    check("multiple statements", "fn main()\n{\n    print(1);\n    print(2);\n    print(3);\n}\n", "1\n2\n3\n");
    check("numbers on one line", "fn main()\n{\n    print(1); print(2); print(3);\n}\n", "1\n2\n3\n");
    check("combined", "fn main()\n{\n    x := 2;\n    y := 3;\n    print(x + y * x);\n}\n", "8\n");

    // --- corner cases ---------------------------------------------------
    // These lock in edge-case semantics verified against the reference
    // compiler: literal parsing, optional semicolons, type mixing, scoping
    // and the C-style behavior of the runtime.

    // Literal parsing: leading zeros are accepted; the largest i64 literal
    // plus one wraps to i64 min (both folded and runtime); a literal below
    // 2^63 stays signed, so `0 - 4294967295` is -4294967295 (no u64 wrap);
    // a literal in [2^63, 2^64-1] is u64, so `0 - 2^63` wraps to 2^63.
    check("leading zeros", "fn main()\n{\n    print(007);\n    print(0);\n}\n", "7\n0\n");
    check("i64 max plus one folded", "fn main()\n{\n    print(9223372036854775807 + 1);\n}\n", "-9223372036854775808\n");
    check("i64 max plus one runtime", "fn main()\n{\n    x := 9223372036854775807;\n    print(x + 1);\n}\n", "-9223372036854775808\n");
    check("literal below 2^63 signed", "fn main()\n{\n    print(0 - 4294967295);\n}\n", "-4294967295\n");
    check("zero minus 2^63 wraps", "fn main()\n{\n    print(0 - 9223372036854775808);\n}\n", "9223372036854775808\n");
    check("u32 max literal", "fn main()\n{\n    print(4294967295);\n}\n", "4294967295\n");

    // Semicolons are optional: a statement simply ends at the next statement.
    check("statements without semicolons", "fn main()\n{\n    print(1)\n    print(2)\n}\n", "1\n2\n");
    check("statements same line no semicolons", "fn main()\n{\n    print(1) print(2)\n}\n", "1\n2\n");

    // Comparisons produce booleans; chains are left-associative. A bool cannot
    // be mixed with an int in arithmetic or a comparison (use an explicit
    // `as i64` cast), so a chained comparison needs a cast on the inner one.
    check("comparison chain", "fn main()\n{\n    print((1 < 2) as i64 < 3);\n}\n", "true\n");

    // Strings: spaces inside literals are preserved, a non-empty string
    // variable is truthy as a condition (it is just its pointer).
    check("string with space", "fn main()\n{\n    print(\"hello world\");\n}\n", "hello world");
    check("string var as condition", "fn main()\n{\n    s := \"a\";\n    if s { print(1); } else { print(2); }\n}\n", "1\n");

    // u8/u16 equality and comparison promote both sides to i32 first, so
    // `255 as u8` is 255 (not -1) when compared against signed values.
    check("u8 equals truncated", "fn main()\n{\n    print(300 as u8 == 44);\n}\n", "true\n");
    check("mixed u8 i8 equality", "fn main()\n{\n    print(5 as u8 == 5 as i8);\n}\n", "true\n");
    check("u8 neg vs i8 neg equality", "fn main()\n{\n    print(255 as u8 == (0 - 1) as i8);\n}\n", "false\n");
    check("u8 neg equals 255", "fn main()\n{\n    print((0 - 1) as u8 == 255);\n}\n", "true\n");

    // Casts are single-expression, so `a as t as t` chains cleanly.
    check("chained casts", "fn main()\n{\n    print(5 as u8 as u8);\n}\n", "5\n");

    // An inner block may declare a fresh variable with the same name as an
    // outer one; they are distinct stack slots.
    check("shadow var in nested for", "fn main()\n{\n    for i := 0 .. 3\n    {\n        x := i * 2;\n        for j := 0 .. 2 { print(x + j); }\n    }\n}\n", "0\n1\n2\n3\n4\n5\n");

    // for bounds are evaluated once up front: mutating the bound variable in
    // the body does not extend the loop, and a negative bound gives an empty
    // loop (the counter comparison is signed).
    check("loop bound read once", "fn main()\n{\n    n := 3;\n    for i := 0 .. n\n    {\n        print(i);\n        if i == 1 { n = 1; }\n    }\n}\n", "0\n1\n");
    check("for negative bound empty", "fn main()\n{\n    for i := 0 .. (0 - 1) { print(i); }\n    print(9);\n}\n", "9\n");
    check("inclusive loop mixed statements", "fn main()\n{\n    for i := 0 ..= 3\n    {\n        if i == 1 { print(10); }\n        print(i);\n    }\n}\n", "0\n10\n1\n2\n3\n");

    // div/mod truncate toward zero like C with negative operands.
    check("C style div mod negatives", "fn main()\n{\n    print((0 - 5) % 3);\n    print((0 - 5) / 3);\n    print(10 % (0 - 3));\n}\n", "-2\n-1\n1\n");

    // Return-type propagation also applies to u8 (the truncation survives a
    // function return), and recursion with several early returns works.
    check("function returns u8", "fn f() -> u8\n{\n    return 300 as u8;\n}\nfn main()\n{\n    print(f());\n}\n", "44\n");
    check("fib 15", "fn f(n : i64) -> i64\n{\n    if n == 0 { return 0; }\n    if n == 1 { return 1; }\n    return f(n - 1) + f(n - 2);\n}\nfn main()\n{\n    print(f(15));\n}\n", "610\n");

    // bools report size 1, but internally they still occupy 8-byte stack
    // slots: a bool variable wedged between two ints must not clobber them.
    check("bool slot between ints", "fn main()\n{\n    a := 100;\n    b := 2 < 3;\n    c := 200;\n    print(a);\n    print(b);\n    print(c);\n    print(a + c);\n}\n", "100\ntrue\n200\n300\n");

    // --- bool arithmetic (requires an explicit `as` cast) ------------------
    check("bool runtime arithmetic", "fn main()\n{\n    x := 5;\n    b := x > 3;\n    print(b as i64 + 10);\n    print(b as i64 - 1);\n    print(b as i64 * 5);\n    print(b as i64 / 2);\n    print(b as i64 % 2);\n    print(1 - (b as i64));\n}\n", "11\n0\n5\n0\n1\n0\n");
    check("bool literal arithmetic", "fn main()\n{\n    print(true as i64 + 1);\n    print(false as i64 * 10);\n    print(false as i64 - 1);\n}\n", "2\n0\n-1\n");
    check("bool negated via arithmetic", "fn main()\n{\n    print(1 - ((2 < 3) as i64));\n    print(1 - ((2 > 3) as i64));\n}\n", "0\n1\n");
    check("bool mixed with unsigned", "fn main()\n{\n    print((2 < 3) as i64 + (5 as u8));\n    print((2 < 3) as i64 == (1 as u8));\n}\n", "6\ntrue\n");

    // --- bool comparisons (a bool must be cast to compare with an int) -----
    check("runtime bool vs int", "fn main()\n{\n    x := 5;\n    b := x == 5;\n    print(b as i64 == 0);\n    print(b as i64 < 2);\n    print(b as i64 >= 1);\n    print(b as i64 > 1);\n}\n", "false\ntrue\ntrue\nfalse\n");
    check("bool literal vs int", "fn main()\n{\n    print(true as i64 == 1);\n    print(false as i64 == 0);\n    print(false as i64 < 2);\n    print(1 > (true as i64));\n}\n", "true\ntrue\ntrue\nfalse\n");
    check("bool result compared to int", "fn main()\n{\n    print((2 < 3) as i64 == 1);\n    print((2 < 3) as i64 == 0);\n    print((2 > 3) as i64 == 0);\n}\n", "true\nfalse\ntrue\n");

    // --- bool variable management ------------------------------------------
    check("bool reassigned", "fn main()\n{\n    b := 2 < 3;\n    print(b);\n    b = 5 > 3;\n    print(b);\n    b = 5 < 3;\n    print(b);\n}\n", "true\ntrue\nfalse\n");
    check("bool copied to var", "fn main()\n{\n    b := 2 < 3;\n    c := b;\n    print(c);\n}\n", "true\n");
    check("bool set inside loop", "fn main()\n{\n    b := false;\n    for i := 0 .. 3 { if i == 2 { b = true; } }\n    if b { print(1); } else { print(2); }\n}\n", "1\n");
    check("many bools between ints", "fn main()\n{\n    a := 1;\n    b := 2 < 3;\n    c := 2;\n    d := 4 > 5;\n    e := 3;\n    f := 5 < 10;\n    g := 4;\n    h := 1 == 1;\n    i := 5;\n    j := 2 > 1;\n    print(a + c + e + g + i);\n    print(b);\n    print(d);\n    print(f);\n    print(h);\n    print(j);\n}\n", "15\ntrue\nfalse\ntrue\ntrue\ntrue\n");

    // --- bool with functions ----------------------------------------------
    check("function returns runtime bool", "fn gt(a : i64, b : i64) -> bool\n{\n    return a > b;\n}\nfn main()\n{\n    if gt(3, 2) { print(1); } else { print(2); }\n    if gt(3, 5) { print(1); } else { print(2); }\n}\n", "1\n2\n");
    check("returned bool in arithmetic", "fn lt(a : i64, b : i64) -> bool\n{\n    return a < b;\n}\nfn main()\n{\n    print(lt(3, 5) as i64 + 10);\n    print(1 - (lt(3, 5) as i64));\n}\n", "11\n0\n");
    check("runtime bool cast roundtrip", "fn main()\n{\n    x := 5;\n    b := x > 3;\n    print(b as i64);\n    print(b as u8);\n    print(b as u64);\n    print((2 < 3) as bool);\n}\n", "1\n1\n1\ntrue\n");

    // --- bool in control flow ---------------------------------------------
    // Bools cannot be used as loop bounds; cast them to an int first.
    check("bool loop bound cast", "fn main()\n{\n    n := 3 < 5;\n    for i := 0 .. n as i64 { print(i); }\n}\n", "0\n");

    // Assignment to self is a no-op; deep paren nesting is fine.
    check("self assignment", "fn main()\n{\n    x := 7;\n    x = x;\n    print(x);\n}\n", "7\n");
    check("deep parens", "fn main()\n{\n    print((((((((((5))))))))));\n}\n", "5\n");

    // --- pointers ---------------------------------------------------------
    // `&x` takes an address, `^p` reads through it, `^p = v` writes through
    // it (both `=` and `:=` forms). Pointer params take `u8^`/`bool^^`-style
    // type syntax; nested `^^p`/`^^^p` chains keep one indirection level per
    // star.
    check("pointer deref read", "fn main()\n{\n    x := 42;\n    p := &x;\n    print(^p);\n}\n", "42\n");
    check("pointer deref store", "fn main()\n{\n    x := 10;\n    p := &x;\n    ^p = 20;\n    print(x);\n}\n", "20\n");
    check("pointer deref store colon-equals", "fn main()\n{\n    x := 10;\n    p := &x;\n    ^p := 30;\n    print(x);\n}\n", "30\n");
    check("pointer read-modify-write", "fn main()\n{\n    x := 41;\n    p := &x;\n    ^p = ^p + 1;\n    print(x);\n}\n", "42\n");
    check("deref of addressof store", "fn main()\n{\n    x := 10;\n    ^(&x) = 20;\n    print(x);\n}\n", "20\n");
    check("nested deref read", "fn main()\n{\n    x := 43;\n    p := &x;\n    q := &p;\n    print(^^q);\n}\n", "43\n");

    // A dereference is an lvalue: `&^p` is the address of the pointee
    // location, i.e. the pointer itself with one level peeled (`&*p` ≡ `p`).
    check("address of deref is the pointer", "fn main()\n{\n    x := 10;\n    p := &x;\n    q := &^p;\n    print(^q);\n}\n", "10\n");
    check("address of deref peels one level", "fn main()\n{\n    x := 10;\n    p := &x;\n    q := &p;\n    r := &^^q;\n    print(^r);\n}\n", "10\n");
    check("type_of address of deref", "fn main()\n{\n    x := 5;\n    p := &x;\n    print(#type_of(&^p));\n}\n", "i64^");
    check("type_of address of deeper deref", "fn main()\n{\n    x := 5;\n    p := &x;\n    q := &p;\n    print(#type_of(&^^q));\n}\n", "i64^");
    check("address of deref passed to typed param", "fn getId(q : i64^) -> i64\n{\n    return ^q;\n}\nfn main()\n{\n    x := 10;\n    p := &x;\n    print(getId(&^p));\n}\n", "10\n");
    check("store through address of deref", "fn main()\n{\n    x := 10;\n    p := &x;\n    ^(&^p) = 20;\n    print(x);\n}\n", "20\n");
    check("store through peeled double deref", "fn main()\n{\n    x := 10;\n    p := &x;\n    q := &p;\n    ^(&^^q) = 99;\n    print(x);\n}\n", "99\n");
    check("address of deref after pointer reassign", "fn main()\n{\n    x := 1;\n    y := 2;\n    p := &x;\n    p = &y;\n    q := &^p;\n    print(^q);\n}\n", "2\n");
    check("read through address of deref", "fn main()\n{\n    x := 7;\n    p := &x;\n    print(^(&^p));\n}\n", "7\n");
    check("deref of address of double deref", "fn main()\n{\n    x := 9;\n    p := &x;\n    q := &p;\n    print(^(&^^q));\n}\n", "9\n");
    check("address of parenthesized variable", "fn main()\n{\n    x := 5;\n    p := &(x);\n    print(^p);\n}\n", "5\n");
    check("type_of address of deref of param", "fn meta(p : i64^) -> i64\n{\n    print(#type_of(&^p));\n    return 0;\n}\nfn main()\n{\n    x := 5;\n    meta(&x);\n}\n", "i64^");

    // Pointer equality compares raw addresses; relational pointer order and
    // pointer-vs-number comparisons are rejected.
    check("pointer equality true", "fn main()\n{\n    x := 42;\n    p := &x;\n    print(&x == p);\n}\n", "true\n");
    check("pointer equality false", "fn main()\n{\n    x := 1;\n    y := 2;\n    print(&x == &y);\n}\n", "false\n");
    check("pointer self equality", "fn main()\n{\n    x := 5;\n    p := &x;\n    q := &x;\n    print(p == q);\n}\n", "true\n");
    check("pointer equality in if", "fn main()\n{\n    x := 120;\n    y := &x;\n    if (&x == y) {\n        print(\"True\");\n    } else {\n        print(\"False\");\n    }\n}\n", "True");
    check("pointer equality after reassign", "fn main()\n{\n    x := 1;\n    y := 2;\n    p := &x;\n    q := &y;\n    p = q;\n    print(p == &y);\n}\n", "true\n");
    check_error("pointer vs numeric equality", "fn main()\n{\n    x := 1;\n    print(&x == 5);\n}\n", "Invalid operands to EQUALS operation");
    check_error("numeric vs pointer equality", "fn main()\n{\n    x := 1;\n    print(5 == &x);\n}\n", "Invalid operands to EQUALS operation");
    check("pointer relational less", "fn main()\n{\n    x := 1;\n    y := 2;\n    print(&x < &y);\n}\n", "false\n");
    check("pointer relational less equal self", "fn main()\n{\n    x := 1;\n    p := &x;\n    print(p <= &x);\n}\n", "true\n");
    check("pointer relational greater", "fn main()\n{\n    x := 1;\n    y := 2;\n    print(&x > &y);\n}\n", "true\n");
    check("pointer relational greater equal self", "fn main()\n{\n    x := 1;\n    p := &x;\n    print(p >= &x);\n}\n", "true\n");
    check("pointer relational less self", "fn main()\n{\n    x := 1;\n    p := &x;\n    print(p < &x);\n}\n", "false\n");
    check("pointer relational greater self", "fn main()\n{\n    x := 1;\n    p := &x;\n    print(p > &x);\n}\n", "false\n");

    // Comparisons work across all integer types. Signed comparisons must stay
    // signed (setl/setg) for negative i8/i16/i32/i64 values, unsigned ones must
    // stay unsigned (setb/seta) for high-bit u8/u16/u32/u64 values, and mixed
    // signed/unsigned operands promote to unsigned (u32 vs i32, u64 vs i64).
    check("comparisons across all integer types", "fn main()\n{\n"
        "    a := (0 - 1) as i8;\n"
        "    b := 127 as i8;\n"
        "    c := (0 - 1) as i16;\n"
        "    d := 32767 as i16;\n"
        "    e := (0 - 1) as i32;\n"
        "    f := 2147483647 as i32;\n"
        "    g := (0 - 1) as i64;\n"
        "    h := 9223372036854775807 as i64;\n"
        "    i := 255 as u8;\n"
        "    j := 1 as u8;\n"
        "    k := 65535 as u16;\n"
        "    l := 1 as u16;\n"
        "    m := 4294967295 as u32;\n"
        "    n := 1 as u32;\n"
        "    o := 18446744073709551615 as u64;\n"
        "    p := 1 as u64;\n"
        "    print(a < b);\n"
        "    print(c < d);\n"
        "    print(e < f);\n"
        "    print(g < h);\n"
        "    print(a > b);\n"
        "    print(i > j);\n"
        "    print(k > l);\n"
        "    print(m > n);\n"
        "    print(o > p);\n"
        "    print(o < p);\n"
        "    print(i < j);\n"
        "    print(m < n);\n"
        "    print(o <= o);\n"
        "    print(m >= m);\n"
        "    print(a == a);\n"
        "    print((0 - 5 as i32) <= (0 - 5 as i32));\n"
        "    print((0 - 5 as i64) >= (0 - 5 as i64));\n"
        "    print((255 as u8) > (254 as u8));\n"
        "    print((65535 as u16) <= (65535 as u16));\n"
        "    print((127 as i8) == (127 as i8));\n"
        "    print((0 - 128 as i8) > 0);\n"
        "    print((0 - 32768 as i16) < 0);\n"
        "    print((0 - 2147483648 as i32) < 0);\n"
        "    print((0 - 9223372036854775807 as i64) < 0);\n"
        "    print((0 - 1 as i8) < (0 as i8));\n"
        "    print((0 - 1 as i16) < (0 as i16));\n"
        "    print((0 - 1 as i32) < (0 as i32));\n"
        "    print((0 - 1 as i64) < (0 as i64));\n"
        "    print((255 as u8) <= (255 as u8));\n"
        "    print((18446744073709551615 as u64) >= (1 as u64));\n"
        "    print((4294967295 as u32) > (2147483647 as i32));\n"
        "    print((18446744073709551615 as u64) > (0 - 1 as i64));\n"
        "    print((0 - 1) > (0 as u64));\n"
        "    print((255 as u8) > (256 as u16));\n"
        "}\n",
        "true\ntrue\ntrue\ntrue\nfalse\ntrue\ntrue\ntrue\ntrue\nfalse\nfalse\nfalse\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\nfalse\nfalse\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\nfalse\ntrue\nfalse\n");

    check("comp-time comparison all widths", "fn main()\n{\n"
        "    print((0 - 1 as i8) < (0 as i8));\n"
        "    print((0 - 1 as i16) < (0 as i16));\n"
        "    print((0 - 1 as i32) < (0 as i32));\n"
        "    print((0 - 1 as i64) < (0 as i64));\n"
        "    print((255 as u8) > (1 as u8));\n"
        "    print((65535 as u16) > (1 as u16));\n"
        "    print((4294967295 as u32) > (1 as u32));\n"
        "    print((18446744073709551615 as u64) > (1 as u64));\n"
        "    print((18446744073709551615 as u64) < (1 as u64));\n"
        "    print((4294967295 as u32) > (0 as i32));\n"
        "    print((0 - 1) < (0 as u64));\n"
        "}\n",
        "true\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\nfalse\ntrue\nfalse\n");
    check("pointer param store", "fn inc(p : i64^) -> i64\n{\n    ^p = ^p + 1;\n    return 0;\n}\nfn main()\n{\n    x := 41;\n    inc(&x);\n    print(x);\n}\n", "42\n");
    check("u8^ param type", "fn set(p : u8^, v : u8) -> i64\n{\n    ^p = v;\n    return 0;\n}\nfn get(p : u8^) -> i64\n{\n    return ^p;\n}\nfn main()\n{\n    x := 300 as u8;\n    set(&x, 200);\n    print(x);\n    print(get(&x));\n}\n", "200\n200\n");
    check("bool^^ param nested store", "fn flip(p : bool^^) -> i64\n{\n    ^^p = true;\n    return 0;\n}\nfn main()\n{\n    b := false;\n    bb := &b;\n    flip(&bb);\n    print(b);\n}\n", "true\n");

    // Pointer re-interpretation casts (`&x as u8^`) are PTR->PTR only and are
    // free (same 64-bit address); the cast's pointee/depth flow into arg
    // checks, derefs and `#type_of`.
    check("cast pointer passed to typed param", "fn poke(p : u8^) -> i64\n{\n    ^p = 200;\n    return 0;\n}\nfn main()\n{\n    x := 300 as u8;\n    poke(&x as u8^);\n    print(x);\n}\n", "200\n");
    check("cast pointer deref-store", "fn main()\n{\n    x := 300 as u8;\n    ^(&x as u8^) = 200;\n    print(x);\n}\n", "200\n");
    check("cast pointer deref read", "fn main()\n{\n    x := 7;\n    p := &x as i64^;\n    print(^p);\n}\n", "7\n");
    check("cast to deeper pointer chain", "fn main()\n{\n    x := 5;\n    p := &x;\n    q := &p as i64^^;\n    print(^^q);\n}\n", "5\n");
    check("cast to triple pointer chain", "fn main()\n{\n    x := 9;\n    p := &x;\n    q := &p;\n    r := &q as i64^^^;\n    print(^^^r);\n}\n", "9\n");
    check("type_of cast", "fn main()\n{\n    x := 5;\n    print(#type_of(&x as u8^));\n}\n", "u8^");

    // --- typed declarations (`x : TYPE = value`) ------------------------
    // The type annotation is optional (`x := v` infers it). When present it
    // forces the variable's type: numeric values promote/shrink (implicit
    // cast) and every other kind must match exactly.
    check("typed decl exact type", "fn main()\n{\n    a : i64 = 123;\n    print(a);\n}\n", "123\n");
    check("typed decl shrink", "fn main()\n{\n    b : u8 = 300;\n    print(b);\n}\n", "44\n");
    check("typed decl shrink all widths", "fn main()\n{\n    a : i8 = 128;\n    b : i16 = 33000;\n    c : u8 = 256;\n    d : u16 = 70000;\n    e : u32 = 4294967296;\n    print(a);\n    print(b);\n    print(c);\n    print(d);\n    print(e);\n}\n", "-128\n-32536\n0\n4464\n0\n");
    check("typed decl promote", "fn main()\n{\n    d : i64 = 300 as u8;\n    print(d);\n}\n", "44\n");
    check("typed decl small value", "fn main()\n{\n    c : u8 = 7;\n    print(c);\n}\n", "7\n");
    check("typed decl bool", "fn main()\n{\n    e : bool = true;\n    print(e);\n}\n", "true\n");
    check("typed decl string", "fn main()\n{\n    f : string = \"hi\";\n    print(f);\n}\n", "hi");
    check("typed decl type_of", "fn main()\n{\n    a : i64 = 123;\n    b : u8 = 300;\n    d : i64 = 300 as u8;\n    e : bool = true;\n    print(#type_of(a));\n    print(#type_of(b));\n    print(#type_of(d));\n    print(#type_of(e));\n}\n", "i64u8i64bool");
    check("typed decl shrunk value compared", "fn main()\n{\n    b : u8 = 300;\n    print(b > 255);\n}\n", "false\n");
    check("typed decl copy from variable", "fn main()\n{\n    s := 5;\n    t : i64 = s;\n    print(t);\n}\n", "5\n");
    check("typed decl then reassign", "fn main()\n{\n    x : i64 = 5;\n    x = 6;\n    print(x);\n}\n", "6\n");
    check("typed decl recompute as u8", "fn main()\n{\n    y : i64 = 1;\n    y = 300 as u8;\n    print(y);\n}\n", "44\n");
    check("typed decl redeclare", "fn main()\n{\n    z := 4;\n    z : i64 = 9;\n    print(z);\n}\n", "9\n");
    check("typed decl pointer", "fn main()\n{\n    x := 7;\n    p : i64^ = &x;\n    print(^p);\n    print(#type_of(p));\n}\n", "7\ni64^");
    check("typed decl pointer chain", "fn main()\n{\n    x := 7;\n    p := &x;\n    q : i64^^ = &p;\n    print(^^q);\n    print(#type_of(q));\n}\n", "7\ni64^^");
    check("typed decl pointer metadata override", "fn main()\n{\n    x := 7;\n    r : u8^ = &x;\n    print(^r);\n    print(#type_of(r));\n}\n", "7\nu8^");
    check("typed decl pointer store", "fn main()\n{\n    x := 7;\n    p : i64^ = &x;\n    ^p = 9;\n    print(x);\n}\n", "9\n");
    check_error("typed decl bool from int", "fn main()\n{\n    b : bool = 5;\n}\n", "Cannot assign i64 to bool variable");
    check_error("typed decl int from string", "fn main()\n{\n    i : i64 = \"str\";\n}\n", "Cannot assign string to i64 variable");
    check_error("typed decl string from int", "fn main()\n{\n    s : string = 5;\n}\n", "Cannot assign i64 to string variable");
    check_error("typed decl int from bool", "fn main()\n{\n    x : i64 = true;\n}\n", "Cannot assign bool to i64 variable");
    check_error("typed decl pointer from int", "fn main()\n{\n    p : i64^ = 5;\n}\n", "Cannot assign i64 to Ptr variable");
    check_error("typed decl pointer from string", "fn main()\n{\n    p : i64^ = \"hi\";\n}\n", "Cannot assign string to Ptr variable");
    check_error("typed decl void", "fn main()\n{\n    v : void = 5;\n}\n", "Variable cannot have type 'void'");
    check_error("typed decl unknown type", "fn main()\n{\n    v : print = 5;\n}\n", "Unknown type name: 'print'");
    check_error("typed decl missing equals", "fn main()\n{\n    v : i64 5;\n}\n", "Expected '=' after the type in a typed declaration");


    // --- functions + pointers ------------------------------------------
    // Pointer values flow through functions: as the result of a call (the
    // returned address), as typed params (`i64^`/`i64^^`/`i64^^^`), across
    // recursion, and through reassignment of the pointer variable itself.
    check("pointer param returned by function", "fn getPtr(p : i64^) -> i64^\n{\n    return p;\n}\nfn main()\n{\n    x := 30;\n    q := getPtr(&x);\n    print(^q);\n}\n", "30\n");
    check("function returns deref of pointer param", "fn derefVal(p : i64^) -> i64\n{\n    return ^p;\n}\nfn main()\n{\n    x := 7;\n    print(derefVal(&x));\n}\n", "7\n");
    check("swap two variables through pointers", "fn swap(a : i64^, b : i64^) -> i64\n{\n    t := ^a;\n    ^a = ^b;\n    ^b = t;\n    return 0;\n}\nfn main()\n{\n    x := 1;\n    y := 2;\n    swap(&x, &y);\n    print(x);\n    print(y);\n}\n", "2\n1\n");
    check("triple pointer through function", "fn getX(q : i64^^^) -> i64\n{\n    return ^^^q;\n}\nfn main()\n{\n    x := 50;\n    p := &x;\n    q := &p;\n    print(getX(&q));\n}\n", "50\n");
    check("pointer variable reassigned to new address", "fn main()\n{\n    x := 1;\n    y := 2;\n    p := &x;\n    p = &y;\n    print(^p);\n}\n", "2\n");
    check("fill through pointer param twice", "fn fill(p : i64^, n : i64) -> i64\n{\n    ^p = n;\n    return 0;\n}\nfn main()\n{\n    x := 1;\n    fill(&x, 10);\n    fill(&x, 20);\n    print(x);\n}\n", "20\n");
    check("mutual recursion void functions", "fn even(n : i64)\n{\n    print(n);\n    if n == 0 { return; }\n    odd(n - 1);\n}\nfn odd(n : i64)\n{\n    print(n);\n    if n == 0 { return; }\n    even(n - 1);\n}\nfn main()\n{\n    even(3);\n}\n", "3\n2\n1\n0\n");
    check("recursion accumulates through pointer", "fn accumulate(p : i64^, n : i64) -> i64\n{\n    if n < 0 { return 0; }\n    ^p = ^p + n;\n    accumulate(p, n - 1);\n    return 0;\n}\nfn main()\n{\n    x := 0;\n    accumulate(&x, 3);\n    print(x);\n}\n", "6\n");
    check("pointer to loop counter", "fn main()\n{\n    for i := 0 .. 3 {\n        p := &i;\n        print(^p);\n    }\n}\n", "0\n1\n2\n");
    // `&x` yields a runtime stack address (nondeterministic under ASLR), so
    // only the codegen is locked in: address-of becomes `lea` from the frame
    // pointer and a dereference-store writes through a register.
    check_asm("pointer lea + store-through codegen",
              "fn main()\n{\n    x := 10;\n    p := &x;\n    ^p = 20;\n    print(x);\n}\n",
              "lea rdi, [rbp", "mov [rbp - 8], rbx", "20\n");

    // --- structs --------------------------------------------------------
    // The user's test.nul example: typed struct declaration, struct literal
    // init (x at offset 0, str y at offset 8), struct param, member reads.
    check("struct user example",
        "struct Foo\n{\n    x : i64,\n    y : str,\n}\n\n"
        "fn test_arg(foo : Foo)\n{\n    print(foo.x)\n    print(foo.y)\n}\n\n"
        "fn main()\n{\n    foo := Foo { x: 5, y: \"test\\n\" }\n"
        "    test_arg(foo)\n    print(foo.x)\n    print(foo.y)\n}\n",
        "5\ntest\n5\ntest\n");
    // Member assignment writes through the struct pointer at the field offset.
    check("struct member assignment", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    foo := Foo { x: 5 }\n    print(foo.x)\n    foo.x = 3\n    print(foo.x)\n}\n", "5\n3\n");
    check("struct member assignment str", "struct Foo\n{\n    x : i64,\n    y : str,\n}\nfn main()\n{\n    foo := Foo { x: 1, y: \"a\" }\n    foo.y = \"b\"\n    print(foo.x)\n    print(foo.y)\n}\n", "1\nb");
    check("struct field arithmetic", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    foo := Foo { x: 3 }\n    print(foo.x * foo.x + 1)\n}\n", "10\n");
    // Struct values flow through functions (return type and reassignment).
    check("struct return from function", "struct Foo\n{\n    x : i64,\n    y : i64,\n}\nfn make() -> Foo\n{\n    return Foo { x: 1, y: 2 }\n}\nfn main()\n{\n    f := make()\n    print(f.x)\n    print(f.y)\n}\n", "1\n2\n");
    check("struct reassign same type", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    a := Foo { x: 1 }\n    b := Foo { x: 2 }\n    a = b\n    print(a.x)\n}\n", "2\n");
    // Struct params have reference semantics: callee mutation is visible.
    check("struct param reference semantics", "struct Foo\n{\n    x : i64,\n}\nfn bump(f : Foo)\n{\n    f.x = f.x + 1\n}\nfn main()\n{\n    foo := Foo { x: 1 }\n    bump(foo)\n    print(foo.x)\n}\n", "2\n");
    // Struct-typed fields (nested structs): the member load yields a struct
    // value that can be stored in a variable and its members read.
    check("nested struct field", "struct Inner\n{\n    a : i64,\n}\nstruct Outer\n{\n    inner : Inner,\n    b : i64,\n}\nfn main()\n{\n    o := Outer { inner: Inner { a: 10 }, b: 20 }\n    i := o.inner\n    print(i.a)\n    print(o.b)\n}\n", "10\n20\n");
    // Trailing comma in struct literals and struct declarations.
    check("struct literal trailing comma", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    foo := Foo { x: 4, }\n    print(foo.x)\n}\n", "4\n");
    // A struct pointer is not allowed, but a pointer *field* is fine.
    check("struct pointer field deref", "struct Foo\n{\n    p : i64^,\n}\nfn main()\n{\n    n := 5\n    foo := Foo { p: &n }\n    print(^foo.p)\n}\n", "5\n");
    // Codegen: member loads/stores become `mov reg, [base + N]` with the
    // field's byte offset, and struct allocation calls __alloc.
    check_asm("struct member load offset", "struct Foo\n{\n    x : i64,\n    y : i64,\n}\nfn main()\n{\n    foo := Foo { x: 1, y: 2 }\n    print(foo.y)\n}\n",
              "mov rdi, [rsi + 8]", "", "2\n");
    // Struct values are reserved on the stack: the base address comes from a
    // `lea` relative to rbp, and no heap allocator is called.
    check_asm("struct reserved on stack", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    foo := Foo { x: 1 }\n    print(foo.x)\n}\n",
              "lea rsi, [rbp - 8]\n\tmov qword [rsi], 1", "call __alloc", "1\n");

    // --- struct corner cases ---------------------------------------------
    // Empty struct: zero fields allocate nothing but are still usable.
    check("empty struct", "struct Empty\n{\n}\nfn main()\n{\n    e := Empty {}\n    print(42)\n}\n", "42\n");
    // Mixed field types (bool, negative, unsigned) round-trip through fields.
    check("struct mixed field types", "struct Flags\n{\n    on : bool,\n    neg : i64,\n    tiny : u8,\n}\nfn main()\n{\n    fl := Flags { on: true, neg: 0 - 7, tiny: 200 }\n    print(fl.on)\n    print(fl.neg)\n    print(fl.tiny)\n}\n", "true\n-7\n200\n");
    // Struct members work as ordinary values in conditions.
    check("struct member in condition", "struct Foo\n{\n    x : i64,\n    y : i64,\n}\nfn main()\n{\n    f := Foo { x: 3, y: 5 }\n    if f.x < f.y { print(\"less\\n\") }\n}\n", "less\n");
    // A struct variable can be re-initialized from a new literal (reassign),
    // including inside a loop.
    check("struct literal reassign", "struct P\n{\n    n : i64,\n}\nfn main()\n{\n    a := P { n: 1 }\n    a = P { n: 9 }\n    print(a.n)\n    for i := 0 .. 2 {\n        a = P { n: i }\n        print(a.n)\n    }\n}\n", "9\n0\n1\n");
    // Several struct members used as arguments in a single call.
    check("struct members as args", "struct Foo\n{\n    x : i64,\n}\nfn echo(a : i64, b : i64, c : i64) -> i64\n{\n    return a * 100 + b * 10 + c\n}\nfn main()\n{\n    f := Foo { x: 5 }\n    print(echo(f.x, 2, f.x + 1))\n}\n", "526\n");
    // A str member is passed to a str parameter like any string value.
    check("struct str member to str param", "struct Foo\n{\n    s : str,\n}\nfn shout(s : str) -> i64\n{\n    print(s)\n    return 0\n}\nfn main()\n{\n    f := Foo { s: \"hello\" }\n    shout(f.s)\n}\n", "hello");
    // A struct-typed member (nested struct) is passed to a param of the same
    // struct type.
    check("struct-typed member as struct arg", "struct Inner\n{\n    a : i64,\n}\nstruct Outer\n{\n    inner : Inner,\n    b : i64,\n}\nfn pass(f : Inner) -> i64\n{\n    return f.a\n}\nfn main()\n{\n    o := Outer { inner: Inner { a: 5 }, b: 3 }\n    i := o.inner\n    print(pass(i))\n    print(pass(o.inner))\n    print(o.b)\n}\n", "5\n5\n3\n");
    // A struct returned by a function is passed straight into another.
    check("struct return passed to function", "struct Foo\n{\n    x : i64,\n    y : str,\n}\nfn make() -> Foo\n{\n    return Foo { x: 7, y: \"hi\" }\n}\nfn consume(f : Foo) -> i64\n{\n    return f.x\n}\nfn main()\n{\n    print(consume(make()))\n    f := make()\n    print(f.x)\n    print(f.y)\n}\n", "7\n7\nhi");
    // Struct values chain through functions: pass a struct, return a struct
    // built from its members.
    check("struct in/out through functions", "struct Pair\n{\n    a : i64,\n    b : i64,\n}\nfn add(p : Pair) -> i64\n{\n    return p.a + p.b\n}\nfn double(p : Pair) -> Pair\n{\n    return Pair { a: p.a * 2, b: p.b * 2 }\n}\nfn main()\n{\n    p := Pair { a: 3, b: 4 }\n    q := double(p)\n    print(add(q))\n    print(q.a)\n    print(q.b)\n}\n", "14\n6\n8\n");
    // --- custom zero initialization --------------------------------------
    // `Foo {0}` fills every member with its zero value: 0 for numbers, NULL
    // for strings (printing a NULL str prints "(NULL)").
    check("struct zero init", "struct Foo\n{\n    x : u64,\n    y : str,\n}\nfn main()\n{\n    a := Foo {0}\n    print(a.x)\n    print(\"|\")\n    print(a.y)\n    print(\"|\")\n}\n", "0\n|(NULL)|");
    check("struct zero init with trailing comma", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    a := Foo {0,}\n    print(a.x)\n}\n", "0\n");
    // Zero-init works when the struct is returned from a function.
    check("struct zero init returned", "struct Foo\n{\n    x : i64,\n    y : i64,\n}\nfn make_zero() -> Foo\n{\n    return Foo {0}\n}\nfn main()\n{\n    a := make_zero()\n    print(a.x)\n    print(a.y)\n}\n", "0\n0\n");
    // Zero-init recurses into struct-typed members and zeroes bools.
    check("struct zero init nested", "struct Inner\n{\n    a : i64,\n}\nstruct Outer\n{\n    inner : Inner,\n    b : bool,\n    c : i64,\n}\nfn main()\n{\n    o := Outer {0}\n    i := o.inner\n    print(i.a)\n    print(o.b)\n    print(o.c)\n}\n", "0\nfalse\n0\n");
    check("struct zero init small ints", "struct Foo\n{\n    a : u8,\n    b : i32,\n}\nfn main()\n{\n    f := Foo {0}\n    print(f.a)\n    print(f.b)\n}\n", "0\n0\n");
    check("struct zero init str is null", "struct Foo\n{\n    s : str,\n}\nfn main()\n{\n    f := Foo {0}\n    print(f.s)\n    print(\"x\")\n}\n", "(NULL)x");
    // A partial member list zeroes the unspecified members instead of leaving
    // them as garbage: y becomes NULL and z becomes 0.
    check("partial init zeroes rest", "struct Foo\n{\n    x : u64,\n    y : str,\n    z : i64,\n}\nfn main()\n{\n    a := Foo { x: 5 }\n    print(a.x)\n    print(\"|\")\n    print(a.y)\n    print(\"|\")\n    print(a.z)\n}\n", "5\n|(NULL)|0\n");
    // Zero-init stores a literal 0 into every 8-byte member slot (including
    // string fields, which become NULL) with no heap allocation.
    check_asm("zero init zeroes every slot", "struct Foo\n{\n    x : u64,\n    y : str,\n}\nfn main()\n{\n    a := Foo {0}\n    print(a.x)\n}\n",
              "mov qword [rsi], 0\n\tmov qword [rsi + 8], 0", "call __alloc", "0\n");
    // --- null keyword ------------------------------------------------------
    // `null` is a void* constant whose value is 0; it assigns to any pointer
    // type and compares equal to other null pointers.
    check("null pointer declare", "fn main()\n{\n    p : i64^ = null\n    print(p)\n}\n", "0x0\n");
    check_asm("ptr print uses __print_ptr", "fn main()\n{\n    p : i64^ = null\n    print(p);\n}\n",
        "call __print_ptr", "call __print_unum", "0x0\n");
    check("null pointer compare", "fn main()\n{\n    p : i64^ = null\n    if p == null { print(\"is null\\n\") }\n    q := &p\n    if ^q == null { print(\"still null\\n\") }\n}\n", "is null\nstill null\n");
    check("null print", "fn main()\n{\n    print(null)\n}\n", "0x0\n");
    check("print null str", "struct Foo\n{\n    s : str,\n}\nfn main()\n{\n    f := Foo {0}\n    print(f.s)\n}\n", "(NULL)");
    // --- pointers to struct members ---------------------------------------
    // `&f.x` is a valid lvalue address: a single-level pointer into the
    // struct. Read it, write through it, and pass it to an i64^ param.
    check("address of member read", "struct Foo\n{\n    x : i64,\n    y : str,\n}\nfn main()\n{\n    f := Foo { x: 5, y: \"hi\" }\n    p := &f.x\n    print(^p)\n    ^p = 9\n    print(f.x)\n    q := &f.y\n    print(^q)\n}\n", "5\n9\nhi");
    check("address of member to pointer param", "struct Foo\n{\n    x : i64,\n    y : i64,\n}\nfn inc(p : i64^) -> i64\n{\n    ^p = ^p + 1\n    return ^p\n}\nfn main()\n{\n    f := Foo { x: 1, y: 2 }\n    print(inc(&f.x))\n    print(f.x)\n    print(f.y)\n}\n", "2\n2\n2\n");
    // `&f.p` where `p` is a pointer field is one level up (i64^^).
    check("address of pointer member", "struct Foo\n{\n    p : i64^,\n}\nfn main()\n{\n    n := 7\n    f := Foo { p: &n }\n    q := &f.p\n    print(^^q)\n}\n", "7\n");
    // A str member's address is a string^ param's value.
    check("address of str member to string^ param", "struct Foo\n{\n    s : str,\n}\nfn up(p : string^) -> i64\n{\n    print(^p)\n    return 0\n}\nfn main()\n{\n    f := Foo { s: \"world\" }\n    up(&f.s)\n}\n", "world");
    check_asm("address of member is base+offset lea", "struct Foo\n{\n    x : i64,\n    y : i64,\n}\nfn main()\n{\n    f := Foo { x: 1, y: 2 }\n    p := &f.y\n    print(^p)\n}\n",
              "lea rsi, [rsi + 8]", "", "2\n");

    // --- Windows MS x64 calling convention ----------------------------
    // Compile with the Windows backend and assert the MS convention in the
    // generated asm: args in rcx/rdx/r8/r9, 32-byte shadow space, stack args
    // above it at [rsp+32..], and rsi/rdi treated as callee-saved.
    check_win_asm("win 6 args shadow + stack args",
        "fn sum6(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64) -> i64\n{\n    return a + b + c + d + e + f;\n}\n"
        "fn main()\n{\n    print(sum6(1, 2, 3, 4, 5, 6));\n}\n",
        {"mov rcx, 1", "mov rdx, 2", "mov r8, 3", "mov r9, 4",
         "mov [rsp+32], rax", "mov [rsp+40], rax"},
        {"mov rdi, 1", "mov rsi, 2"});
    check_win_asm("win 9 args 5 stack slots",
        "fn sum9(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, g : i64, h : i64, i : i64) -> i64\n{\n    return a + b + c + d + e + f + g + h + i;\n}\n"
        "fn main()\n{\n    print(sum9(1, 2, 3, 4, 5, 6, 7, 8, 9));\n}\n",
        {"mov [rsp+32], rax", "mov [rsp+40], rax", "mov [rsp+48], rax",
         "mov [rsp+56], rax", "mov [rsp+64], rax"});
    check_win_asm("win callee-saved rsi/rdi/r12",
        "fn fib(n : i64) -> i64\n{\n    if n < 2 { return n; }\n    return fib(n - 1) + fib(n - 2);\n}\n"
        "fn main()\n{\n    print(fib(10));\n}\n",
        {"push rsi", "push rdi", "push r12", "pop r12", "pop rdi", "pop rsi",
         "mov rcx, rdi"});

    // --- globals ----------------------------------------------------------
    // A global variable is declared at the top block, lives in the shared
    // __globals region and is visible from every function. Typed and untyped
    // declarations, reassignment, and reads in multiple functions.
    check("global typed declaration", "gx : i64 = 5\nfn main()\n{\n    print(gx)\n}\n", "5\n");
    check("global untyped declaration", "gx := 7\nfn main()\n{\n    print(gx)\n}\n", "7\n");
    check("global typed str", "gs : str = \"hello\"\nfn main()\n{\n    print(gs)\n}\n", "hello");
    check("global typed bool", "gb : bool = true\nfn main()\n{\n    print(gb)\n}\n", "true\n");
    check("global reassign", "gx : i64 = 1\nfn main()\n{\n    gx = gx + 1\n    print(gx)\n}\n", "2\n");
    check("global read in expression", "gx : i64 = 3\nfn main()\n{\n    print(gx * gx + 1)\n}\n", "10\n");
    check("global multiple distinct", "ga : i64 = 1\ngb : i64 = 2\ngc : str = \"s\"\nfn main()\n{\n    print(ga)\n    print(gb)\n    print(gc)\n}\n", "1\n2\ns");
    // The same global is shared across functions: writes in one are seen by
    // the others.
    check("global shared across functions", "g : i64 = 1\nfn inc()\n{\n    g = g + 1\n}\nfn get() -> i64\n{\n    return g\n}\nfn main()\n{\n    print(get())\n    inc()\n    print(get())\n    inc()\n    print(get())\n}\n", "1\n2\n3\n");
    check("global as call arg", "g : i64 = 5\nfn double(v : i64) -> i64\n{\n    return v * 2\n}\nfn main()\n{\n    print(double(g))\n    g = double(g)\n    print(g)\n}\n", "10\n10\n");
    check("global in condition", "g : bool = true\nfn main()\n{\n    if g { print(\"yes\\n\") }\n    g = false\n    if g { print(\"no\\n\") }\n}\n", "yes\n");
    check("global in loop", "g : i64 = 0\nfn main()\n{\n    for i := 0 .. 4\n    {\n        g = g + i\n    }\n    print(g)\n}\n", "6\n");
    // Address-of and deref-store work on globals like on locals.
    check("global address of", "gx : i64 = 9\nfn main()\n{\n    p := &gx\n    print(^p)\n    ^p = 12\n    print(gx)\n}\n", "9\n12\n");
    check("global pointer to global", "gx : i64 = 5\ngp : i64^ = &gx\nfn main()\n{\n    print(^gp)\n    ^gp = 8\n    print(gx)\n}\n", "5\n8\n");
    // Globals of struct type: member reads/writes, reassignment, zero-init and
    // passing to struct-parameter functions.
    check("global struct member read", "struct P\n{\n    a : i64,\n    b : i64,\n}\ngp : P = P { a: 3, b: 4 }\nfn main()\n{\n    print(gp.a)\n    print(gp.b)\n}\n", "3\n4\n");
    check("global struct member assign", "struct P\n{\n    a : i64,\n}\ngp : P = P { a: 1 }\nfn main()\n{\n    gp.a = 20\n    print(gp.a)\n}\n", "20\n");
    check("global struct zero init", "struct P\n{\n    a : i64,\n    b : str,\n}\ngp : P = P {0}\nfn main()\n{\n    print(gp.a)\n    print(\"|\")\n    print(gp.b)\n    print(\"|\")\n}\n", "0\n|(NULL)|");
    check("global struct reassign literal", "struct P\n{\n    a : i64,\n}\ngp : P = P { a: 1 }\nfn main()\n{\n    gp = P { a: 9 }\n    print(gp.a)\n}\n", "9\n");
    check("global struct to struct param", "struct P\n{\n    a : i64,\n    b : i64,\n}\ngp : P = P { a: 5, b: 6 }\nfn sum(p : P) -> i64\n{\n    return p.a + p.b\n}\nfn main()\n{\n    print(sum(gp))\n}\n", "11\n");
    check("global struct from function", "struct P\n{\n    a : i64,\n}\ngp : P = P { a: 1 }\nfn set()\n{\n    gp = P { a: 7 }\n}\nfn main()\n{\n    set()\n    print(gp.a)\n}\n", "7\n");
    check("global struct member address", "struct P\n{\n    a : i64,\n    b : i64,\n}\ngp : P = P { a: 2, b: 3 }\nfn main()\n{\n    q := &gp.b\n    print(^q)\n    ^q = 30\n    print(gp.b)\n}\n", "3\n30\n");
    // Globals feeding struct literals and global statements producing values.
    check("global into struct literal", "struct P\n{\n    v : i64,\n}\ngx : i64 = 4\nfn main()\n{\n    s := P { v: gx }\n    print(s.v)\n}\n", "4\n");
    check("global value in return", "g : i64 = 42\nfn val() -> i64\n{\n    return g\n}\nfn main()\n{\n    print(val())\n}\n", "42\n");
    // Codegen: globals read and write the shared __globals region (both in the
    // entry statement and from function bodies), never a stack slot.
    check_asm("global accessed via __globals", "gx : i64 = 5\nfn main()\n{\n    print(gx)\n    gx = 7\n}\n",
              "[__globals + 0]", "", "5\n");
    check_asm("global struct zero init zeroes memory", "struct Foo\n{\n    x : i64,\n    y : i64,\n}\ngf : Foo = Foo {0}\nfn main()\n{\n    print(gf.x)\n    print(gf.y)\n}\n",
              "mov qword [r12], 0\n\tmov qword [r12 + 8], 0", "call __alloc", "0\n0\n");
    check_asm("global stored before call to main", "gx : i64 = 5\nfn main()\n{\n    print(gx)\n}\n",
              "mov [__globals + 0], rax\n\tcall __main", "", "5\n");

    // --- C FFI (`extern fn`) ------------------------------------------------
    // `extern fn` declares a bodyless C function resolved by the C linker.
    // (Semicolons after `as u8^` are required: a cast's `^` suffix greedily
    // swallows a following statement that starts with `^`.)
    check("extern malloc deref store", "extern fn malloc(size : u64) -> void^\nextern fn free(ptr : void^)\nfn main()\n{\n    p := malloc(4) as u8^;\n    ^p = 200;\n    print(^p);\n    free(p);\n}\n", "200\n");
    // malloc/free round-trip across two allocations.
    check("extern two allocations", "extern fn malloc(size : u64) -> void^\nextern fn free(ptr : void^)\nfn main()\n{\n    a := malloc(4) as u8^;\n    b := malloc(4) as u8^;\n    ^a = 3;\n    ^b = 4;\n    print(^a + ^b);\n    free(a);\n    free(b);\n}\n", "7\n");
    // A str argument converts to char* for extern calls (C's implicit const char*).
    check("extern strlen of str", "extern fn strlen(s : void^) -> u64\nfn main()\n{\n    print(strlen(\"hello\"))\n}\n", "5\n");
    // Multi-argument extern call; void^ accepts any pointer (u8^ -> void^).
    check("extern strcmp two args", "extern fn strcmp(a : void^, b : void^) -> i32\nfn main()\n{\n    print(strcmp(\"abc\", \"abd\"))\n}\n", "-1\n");
    // extern declarations can come after their call sites (forward decl).
    check("extern forward declaration", "fn main()\n{\n    print(strlen(\"hi\"))\n}\nextern fn strlen(s : void^) -> u64\n", "2\n");
    // A real C memory-copy through malloc'd buffers (void^ in, void^ out).
    check("extern memcpy buffers", "extern fn malloc(size : u64) -> void^\nextern fn free(ptr : void^)\nextern fn memcpy(dst : void^, src : void^, n : u64) -> void^\nfn main()\n{\n    a := malloc(4) as u8^;\n    b := malloc(4) as u8^;\n    ^a = 77;\n    memcpy(b, a, 1);\n    print(^b);\n    free(a);\n    free(b);\n}\n", "77\n");
    // extern fn returning a pointer whose result is cast and null-checked.
    check("extern getenv result", "extern fn getenv(name : void^) -> void^\nfn main()\n{\n    p := getenv(\"NOT_A_REAL_ENV_VAR_12345\") as u8^;\n    if p == null { print(\"null\\n\") }\n}\n", "null\n");
    // Casting a raw pointer to a struct pointer (`malloc as Pair^`): the
    // result behaves like `&struct_var`, so field access goes through it.
    // Struct names lex as identifiers, which used to leave the cast's `^`
    // suffix unconsumed and garble the following statements.
    check("cast raw pointer to struct pointer", "extern fn malloc(size : u64) -> void^\nstruct Pair\n{\n    flag : bool,\n    pad : u8,\n    big : i64,\n}\nfn main()\n{\n    p := malloc(16) as Pair^;\n    p.flag = true;\n    p.pad = 7;\n    p.big = 1234;\n    print(p.flag);\n    print(p.pad);\n    print(p.big);\n}\n", "true\n7\n1234\n");
    // Deref of a struct pointer yields a value aliasing the same block, and a
    // typed param receives the same reference (writes are visible to caller).
    check("cast struct pointer deref aliases", "extern fn malloc(size : u64) -> void^\nstruct Pair\n{\n    big : i64,\n}\nfn bump(p : Pair^)\n{\n    g := ^p\n    g.big = g.big + 1\n    p.big = p.big + 1;\n}\nfn main()\n{\n    p := malloc(8) as Pair^;\n    p.big = 40;\n    bump(p);\n    print(p.big);\n}\n", "42\n");
    // A non-pointer source cannot be cast to a struct pointer.
    check_error("cast int to struct pointer", "struct Pair\n{\n    big : i64,\n}\nfn main()\n{\n    p := 5 as Pair^;\n}\n", "Invalid operands to CAST operation");
    // A negative argument computed without a unary-minus literal.
    check("extern i64 roundtrip", "extern fn labs(x : i64) -> i64\nfn main()\n{\n    print(labs(0 - 42))\n}\n", "42\n");
    // The emitted asm must declare the C symbol so the linker resolves it.
    check_asm("extern symbol declared in asm", "extern fn labs(x : i64) -> i64\nfn main()\n{\n    print(labs(1))\n}\n",
              "extrn labs", "", "1\n");
    // Struct fields use C alignment: a u8 then a u8 pack at byte offsets 0/1
    // (memcmp reads the actual struct bytes through a raw byte view).
    check("struct C alignment packed u8s", "extern fn memcmp(l : void^, r : void^, n : u64) -> i64\nstruct Pair\n{\n    a : u8,\n    b : u8,\n}\nfn main()\n{\n    p1 := Pair { a: 0, b: 0 }\n    p2 := Pair { a: 0, b: 1 }\n    print(memcmp(^(&p1 as u8^^), ^(&p2 as u8^^), 1) == 0)\n    print(memcmp(^(&p1 as u8^^), ^(&p2 as u8^^), 2) == 0)\n}\n", "true\nfalse\n");
    // Exact C offsets: {a: u8, b: u16, c: u32} lands a@0, b@2, c@4. A known
    // byte pattern is memcpy'd in, then the fields read back at those offsets.
    check("struct C alignment byte offsets", "extern fn memcpy(dst : void^, src : void^, n : u64) -> void^\nstruct Bytes8\n{\n    b0 : u8,\n    b1 : u8,\n    b2 : u8,\n    b3 : u8,\n    b4 : u8,\n    b5 : u8,\n    b6 : u8,\n    b7 : u8,\n}\nstruct H\n{\n    a : u8,\n    b : u16,\n    c : u32,\n}\nfn main()\n{\n    src := Bytes8 { b0: 1, b1: 170, b2: 52, b3: 18, b4: 120, b5: 86, b6: 52, b7: 18 }\n    h := H { a: 0, b: 0, c: 0 }\n    memcpy(^(&h as u8^^), ^(&src as u8^^), 8)\n    print(h.a)\n    print(h.b)\n    print(h.c)\n}\n", "1\n4660\n305419896\n");
    // Every field width reads/writes exactly its own bytes: sub-32-bit fields
    // zero/sign-extend on load and stores never clobber byte neighbours.
    check("struct C alignment mixed widths", "struct Mix\n{\n    b : u8,\n    w : u16,\n    d : u32,\n    s : i8,\n    i : i32,\n    q : i64,\n}\nfn main()\n{\n    m := Mix { b: 200, w: 60000, d: 4000000000, s: 0 - 5, i: 0 - 70000, q: 1234567890123 }\n    print(m.b)\n    print(m.w)\n    print(m.d)\n    print(m.s)\n    print(m.i)\n    print(m.q)\n    m.b = 1\n    m.w = 2\n    m.d = 3\n    m.s = 0 - 6\n    m.i = 0 - 8\n    m.q = 9\n    print(m.b)\n    print(m.w)\n    print(m.d)\n    print(m.s)\n    print(m.i)\n    print(m.q)\n}\n", "200\n60000\n4000000000\n-5\n-70000\n1234567890123\n1\n2\n3\n-6\n-8\n9\n");
    // Struct storage bases are aligned to the struct's C alignment, not just
    // packed: B (align 8, size 16) placed after a single-byte struct lands at
    // an 8-aligned frame offset (40 below rbp here, not the packed 33).
    check_asm("struct alignment frame bases", "struct A { x : u8 }\nstruct B { a : u8, b : i64 }\nfn main()\n{\n    a := A { x : 1 }\n    b := B { a : 2, b : 3 }\n    print(b.b)\n}\n",
              "[rbp - 40]", "", "3\n");
    // C FFI: a struct value converts to a pointer arg for extern calls (C's
    // implicit &x), so raw bytes of the struct can be handed to C. write() then
    // proves byte-exact C layout: u32@0 + u16@4, i64@0 + u8@8, bool@0 + u8@1.
    check("struct C alignment raw write", "extern fn write(fd : i64, buf : void^, count : u64) -> i64\nstruct A { x : u32, y : u16 }\nstruct B { a : i64, b : u8 }\nstruct C { f : bool, x : u8 }\nfn main()\n{\n    a := A { x : 16909060, y : 1286 }\n    b := B { a : 72623859790382856, b : 9 }\n    c := C { f : true, x : 2 }\n    write(1, a, 6)\n    write(1, b, 9)\n    write(1, c, 2)\n}\n", "\x04\x03\x02\x01\x06\x05\x08\x07\x06\x05\x04\x03\x02\x01\x09\x01\x02");

    // --- modules -------------------------------------------------------
    // A module spans files: two files declaring `module m` share every symbol
    // (functions, globals, structs) without any import or qualification.
    check_files("module spans files call", {
        "a.nul", "module m\nfn helper(x : i64) -> i64\n{\n    return x * 2;\n}\n",
        "b.nul", "module m\nfn main()\n{\n    print(helper(21));\n}\n",
    }, "42\n");
    check_files("module spans files global", {
        "a.nul", "module m\ngx := 7\nfn set_gx(v : i64)\n{\n    gx = v;\n}\n",
        "b.nul", "module m\nfn main()\n{\n    print(gx);\n    set_gx(9);\n    print(gx);\n}\n",
    }, "7\n9\n");
    check_files("module spans files struct", {
        "a.nul", "module m\nstruct Point\n{\n    x : i64,\n    y : i64,\n}\n",
        "b.nul", "module m\nfn main()\n{\n    p := Point { x: 3, y: 4 }\n    print(p.x + p.y);\n}\n",
    }, "7\n");
    // Same-module circular cross-file calls (forward reference both ways).
    check_files("module spans files circular call", {
        "a.nul", "module m\nfn is_even(n : i64) -> bool\n{\n    if n == 0 { return true; }\n    return is_odd(n - 1);\n}\n",
        "b.nul", "module m\nfn is_odd(n : i64) -> bool\n{\n    if n == 0 { return false; }\n    return is_even(n - 1);\n}\nfn main()\n{\n    print(is_even(10));\n    print(is_odd(10));\n}\n",
    }, "true\nfalse\n");

    // Global-module symbols are visible from any module without an import.
    check_files("global module visible to module", {
        "a.nul", "fn helper(x : i64) -> i64\n{\n    return x + 1;\n}\ngx := 100\n",
        "b.nul", "module m\nfn main()\n{\n    print(helper(41));\n    print(gx);\n}\n",
    }, "42\n100\n");

    // `import` grants visibility; qualified refs reach imported symbols.
    check_files("import qualified call", {
        "a.nul", "module lib\nfn greet() -> i64\n{\n    return 42;\n}\n",
        "b.nul", "import lib\nfn main()\n{\n    print(lib::greet());\n}\n",
    }, "42\n");
    check_files("import qualified global read write", {
        "a.nul", "module data\ngx := 7\n",
        "b.nul", "import data\nfn main()\n{\n    print(data::gx);\n    data::gx = 9;\n    print(data::gx);\n}\n",
    }, "7\n9\n");
    check_files("import qualified struct", {
        "a.nul", "module geo\nstruct Point\n{\n    x : i64,\n    y : i64,\n}\nfn dist(p : Point) -> i64\n{\n    return p.x + p.y;\n}\n",
        "b.nul", "import geo\nfn main()\n{\n    p := geo::Point { x: 5, y: 6 }\n    print(geo::dist(p));\n}\n",
    }, "11\n");
    // Module functions can accept/return qualified struct types.
    check_files("qualified struct param and return", {
        "a.nul", "module geo\nstruct Pair\n{\n    a : i64,\n    b : i64,\n}\n",
        "b.nul", "import geo\nfn swap(p : geo::Pair) -> geo::Pair\n{\n    return geo::Pair { a: p.b, b: p.a };\n}\nfn main()\n{\n    r := swap(geo::Pair { a: 1, b: 2 });\n    print(r.a);\n    print(r.b);\n}\n",
    }, "2\n1\n");
    // A module's function may call a global-module function (visible everywhere).
    check_files("module calls global function", {
        "a.nul", "fn twice(x : i64) -> i64\n{\n    return x * 2;\n}\n",
        "b.nul", "module m\nfn main()\n{\n    print(twice(21));\n}\n",
    }, "42\n");

    // Same function name in two imported modules: unqualified is ambiguous, so
    // both must be qualified.
    check_files("qualified disambiguation", {
        "a.nul", "module a\nfn val() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "module b\nfn val() -> i64\n{\n    return 2;\n}\n",
        "c.nul", "import a\nimport b\nfn main()\n{\n    print(a::val());\n    print(b::val());\n}\n",
    }, "1\n2\n");
    // Same struct name in two imported modules, disambiguated by qualification.
    check_files("qualified struct disambiguation", {
        "a.nul", "module a\nstruct S\n{\n    x : i64,\n}\n",
        "b.nul", "module b\nstruct S\n{\n    x : i64,\n}\n",
        "c.nul", "import a\nimport b\nfn main()\n{\n    p := a::S { x: 1 }\n    q := b::S { x: 2 }\n    print(p.x + q.x);\n}\n",
    }, "3\n");
    // main may live in a module; __entry finds it across modules.
    check_files("main in module", {
        "a.nul", "module app\nfn main()\n{\n    print(7);\n}\n",
    }, "7\n");

    // __entry forwards the OS command line to main: argc is the argument
    // count (1 when run without extra arguments) and argv points at the
    // argument array, whose first element is the program path.
    check("main receives argc", "fn main(argc : i64)\n{\n    print(argc);\n}\n", "1\n");
    check("main receives argc argv", "fn main(argc : i64, argv : str^)\n{\n    print(argc);\n    print(argv != null);\n}\n", "1\ntrue\n");
    // `strlen(^argv)` reads argv[0] directly through the char**; `^(argv + 0)`
    // reaches the same slot via pointer arithmetic — both must agree, which
    // also regression-tests pointer-arithmetic metadata propagation.
    check("main argv[0] is the program path", "extern fn strlen(s : void^) -> u64\nfn main(argc : i64, argv : str^)\n{\n    print(strlen(^argv) == strlen(^(argv + 0)));\n}\n", "true\n");

    // Pointer arithmetic keeps the pointer operand's metadata through `:=`,
    // reversed operands, and chained adds, so the result stays dereferenceable.
    check("ptr arith metadata via decl", "extern fn strlen(s : void^) -> u64\nfn main(argc : i64, argv : str^)\n{\n    i := 1\n    x := argv + i\n    print(argc > 0 && strlen(^(argv + 0)) >= 0 && x != null);\n}\n", "true\n");
    // Writes through an advanced pointer land in the right byte slots.
    check("deref assign via ptr arith", "extern fn malloc(size : u64) -> void^\nfn main()\n{\n    buf := malloc(4) as u8^;\n    ^(buf + 1) = 66;\n    ^(buf + 2) = 67;\n    print(^(buf + 1));\n    print(^(buf + 2));\n}\n", "66\n67\n");

    // Typed struct declaration in a module: the declared type resolves to the
    // struct's real module, and so does an unqualified init (`Foo { ... }`),
    // so the two identities must match even though neither is qualified.
    check_files("typed struct declaration in module", {
        "a.nul", "module app\nstruct Foo\n{\n    x : u64,\n    y : u8,\n}\nfn add(f : Foo^) -> u64\n{\n    return f.x + f.y;\n}\nfn main()\n{\n    f :Foo = Foo { x: 123, y: 45 };\n    print(f.x + f.y);\n    print(add(&f));\n}\n",
    }, "168\n168\n");

    // A module may qualify its own symbols (self-reference via the module
    // name), and `::` never changes which symbol is being called.
    check_files("own module self qualification", {
        "a.nul", "module m\nfn val() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "module m\nfn main()\n{\n    print(val());\n    print(m::val());\n}\n",
    }, "1\n1\n");

    // Same global name in two modules: unqualified is ambiguous, so qualify.
    check_files("qualified global disambiguation", {
        "a.nul", "module a\ngx := 1\n",
        "b.nul", "module b\ngx := 2\n",
        "c.nul", "import a\nimport b\nfn main()\n{\n    print(a::gx);\n    print(b::gx);\n    a::gx = 10;\n    b::gx = 20;\n    print(a::gx + b::gx);\n}\n",
    }, "1\n2\n30\n");

    // Importing the same module twice is harmless (visibility is idempotent).
    check_files("double import is harmless", {
        "a.nul", "module lib\nfn v() -> i64\n{\n    return 42;\n}\n",
        "b.nul", "import lib\nimport lib\nfn main()\n{\n    print(lib::v());\n}\n",
    }, "42\n");

    // A module file may declare nothing but its name; importing it still
    // succeeds (it counts as a declared module).
    check_files("import empty module file", {
        "a.nul", "module lib\n",
        "b.nul", "import lib\nfn main()\n{\n    print(1);\n}\n",
    }, "1\n");

    // Import grants visibility of that one module only: `lib` can use
    // `core::`, but main can only reach `core::` through `lib::`.
    check_files("transitive import stays hidden from main", {
        "a.nul", "module core\nfn base() -> i64\n{\n    return 40;\n}\n",
        "b.nul", "module lib\nimport core\nfn total() -> i64\n{\n    return core::base() + 2;\n}\n",
        "c.nul", "import lib\nfn main()\n{\n    print(lib::total());\n}\n",
    }, "42\n");

    // Three-module chain: core <- mid <- top, each reachable by its importer.
    check_files("three module chain", {
        "a.nul", "module core\nfn base() -> i64\n{\n    return 40;\n}\n",
        "b.nul", "module mid\nimport core\nfn add(x : i64) -> i64\n{\n    return core::base() + x;\n}\n",
        "c.nul", "module top\nimport mid\nfn main()\n{\n    print(mid::add(2));\n}\n",
    }, "42\n");

    // One module may import several modules and pick among them by name.
    check_files("module imports several modules", {
        "a.nul", "module x\nfn f() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "module y\nfn f() -> i64\n{\n    return 2;\n}\n",
        "c.nul", "module app\nimport x\nimport y\nfn main()\n{\n    print(x::f());\n    print(y::f());\n}\n",
    }, "1\n2\n");

    // Circular imports/calls between two distinct modules.
    check_files("cross-module circular call", {
        "a.nul", "module a\nimport b\nfn is_even(n : i64) -> bool\n{\n    if n == 0 { return true; }\n    return b::is_odd(n - 1);\n}\n",
        "b.nul", "module b\nimport a\nfn is_odd(n : i64) -> bool\n{\n    if n == 0 { return false; }\n    return a::is_even(n - 1);\n}\nfn main()\n{\n    print(a::is_even(10));\n    print(b::is_odd(10));\n}\n",
    }, "true\nfalse\n");

    // Forward reference across modules: the callee is declared in a later file.
    check_files("cross-module forward call", {
        "a.nul", "module app\nimport lib\nfn main()\n{\n    print(lib::later(21));\n}\n",
        "b.nul", "module lib\nfn later(x : i64) -> i64\n{\n    return x * 2;\n}\n",
    }, "42\n");

    // Qualified struct zero-initialization (`geo::Point {0}`).
    check_files("qualified struct zero init", {
        "a.nul", "module geo\nstruct Point\n{\n    x : i64,\n    y : i64,\n}\n",
        "b.nul", "import geo\nfn main()\n{\n    p := geo::Point {0}\n    print(p.x);\n    print(p.y);\n}\n",
    }, "0\n0\n");

    // A pointer to a qualified struct crossing module boundaries: built in one
    // module, passed to a function in another, dereferenced for member access.
    check_files("qualified struct pointer across modules", {
        "a.nul", "module geo\nstruct Point\n{\n    x : i64,\n}\nfn read(p : Point^) -> i64\n{\n    return p.x;\n}\n",
        "b.nul", "import geo\nfn main()\n{\n    p := geo::Point { x: 42 }\n    print(geo::read(&p));\n}\n",
    }, "42\n");

    // A module function returning its struct is consumed unqualified elsewhere;
    // the returned value keeps its declaring module for member access.
    check_files("module struct return consumed elsewhere", {
        "a.nul", "module geo\nstruct Point\n{\n    x : i64,\n    y : i64,\n}\nfn make(x : i64, y : i64) -> Point\n{\n    return Point { x: x, y: y };\n}\n",
        "b.nul", "import geo\nfn main()\n{\n    p := geo::make(3, 4);\n    print(p.x + p.y);\n}\n",
    }, "7\n");

    // A struct field can be typed with a qualified struct from another module;
    // the nested value and its owner resolve in different modules.
    check_files("qualified struct field type", {
        "a.nul", "module geo\nstruct Point\n{\n    x : i64,\n}\n",
        "b.nul", "module shapes\nimport geo\nstruct Circle\n{\n    center : geo::Point,\n    r : i64,\n}\n",
        "c.nul", "import geo\nimport shapes\nfn main()\n{\n    p := geo::Point { x: 3 }\n    c := shapes::Circle { center: p, r: 4 }\n    print(p.x + c.r);\n}\n",
    }, "7\n");

    // Module globals and functions drive control flow and loop bounds.
    check_files("module globals in loops and branches", {
        "a.nul", "module cfg\nlimit := 3\nflag := true\n",
        "b.nul", "import cfg\nfn main()\n{\n    total := 0\n    for i := 1 .. cfg::limit + 1\n    {\n        if cfg::flag { total = total + i; }\n    }\n    print(total);\n}\n",
    }, "6\n");

    // Module function results in branch conditions.
    check_files("module function in branch conditions", {
        "a.nul", "module m\nfn even(n : i64) -> bool\n{\n    return n % 2 == 0;\n}\n",
        "b.nul", "import m\nfn main()\n{\n    if m::even(4) { print(1); } else { print(2); }\n    if m::even(3) { print(3); } else { print(4); }\n}\n",
    }, "1\n4\n");
}

static void error_tests() {
    check_error("call to undeclared function", "fn main()\n{\n    foo();\n}\n");
    check_error("missing arguments", "fn foo(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    print(foo(1));\n}\n");
    check_error("too many arguments", "fn foo(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    print(foo(1, 2, 3));\n}\n");
    check_error("zero arguments for expected two", "fn foo(a : i64, b : i64) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    foo();\n}\n");
    check_error("main called with args", "fn main()\n{\n    main(1);\n}\n");
    check_error("builtin wrong arg count", "fn main()\n{\n    print(#type_id(1, 2));\n}\n");
    check_error("offset_of wrong arg count", "struct H { a : u8 }\nfn main()\n{\n    print(offset_of(H));\n}\n", "expects exactly 2 arguments");
    check_error("align_of wrong arg count", "struct H { a : u8 }\nfn main()\n{\n    print(align_of(H, \"a\"));\n}\n", "expects exactly 1 argument");
    check_error("offset_of unknown struct", "fn main()\n{\n    print(offset_of(Nope, \"a\"));\n}\n", "Cannot find struct with name: 'Nope'");
    check_error("offset_of unknown field", "struct H { a : u8 }\nfn main()\n{\n    print(offset_of(H, \"zz\"));\n}\n", "has no field named 'zz'");
    check_error("offset_of non-string field", "struct H { a : u8 }\nfn main()\n{\n    print(offset_of(H, 5));\n}\n", "expects a field name string literal");
    check_error("redeclare variable", "fn main()\n{\n    x := 1;\n    x := 2;\n}\n");
    check_error("loop variable shadowing", "fn main()\n{\n    for i := 0 .. 2 { for i := 0 .. 2 { print(i); } }\n}\n");
    check_error("loop variable shadowing in form", "fn main()\n{\n    for i in 0..2 { for i in 0..2 { print(i); } }\n}\n", "already created");
    check_error("shadow loop counter in body", "fn main()\n{\n    for i in 0..3 { i := 9; print(i); }\n}\n", "already created");
    check_error("loop var undeclared after loop", "fn main()\n{\n    for i in 0..2 { print(i); }\n    print(i);\n}\n", "undeclared");
    check_error("for in missing range", "fn main()\n{\n    for i in 0 { print(i); }\n}\n", "Expected '..' after for expression");
    check_error("for in without braces", "fn main()\n{\n    for i in 0..3\n        print(i);\n}\n", "not C (shit), please use braces");
    check_error("loop var assign after loop", "fn main()\n{\n    for i in 0..3 { print(i); }\n    i = 5;\n}\n", "undeclared");
    check_error("for in bool bound", "fn main()\n{\n    for i in 0..true { print(i); }\n}\n", "Invalid operands to loop bound operation");
    check_error("duplicate function", "fn foo() -> i64\n{\n    return 0;\n}\nfn foo() -> i64\n{\n    return 0;\n}\nfn main() -> i64\n{\n    return 0;\n}\n");
    check_error("unknown token", "fn main()\n{\n    print(2 @ 3);\n}\n");
    check_error("unclosed string", "fn main()\n{\n    print(\"abc);\n}\n");
    check_error("reserved __entry", "fn __entry() -> i64\n{\n    return 0;\n}\nfn main() -> i64\n{\n    return 0;\n}\n");
    check_error("main too many arguments", "fn main(a : i64, b : i64, c : i64)\n{\n}\n", "at most 2 arguments");
    check_error("main argc wrong type", "fn main(x : u8^^)\n{\n}\n", "First parameter of 'main' must be 'i64'");
    check_error("main argv wrong type", "fn main(a : i64, b : i64)\n{\n}\n", "Second parameter of 'main' must be 'str^'");
    // Arithmetic is only defined for numbers (+/- also for one pointer and one
    // number). Strings/arrays/pointer mult used to slip through a MAX() type
    // fallback and crash at runtime.
    check_error("no string plus int", "fn main()\n{\n    s := \"abc\" + 1;\n}\n", "Invalid operands to PLUS operation");
    check_error("no string times int", "fn main()\n{\n    s := \"abc\" * 2;\n}\n", "Invalid operands to MULT operation");
    check_error("no array plus int", "fn main()\n{\n    arr := [1, 2];\n    x := arr + 1;\n}\n", "Invalid operands to PLUS operation");
    check_error("no ptr mult", "extern fn malloc(size : u64) -> void^\nfn main()\n{\n    p := malloc(8) as u8^;\n    x := p * 2;\n}\n", "Invalid operands to MULT operation");
    check_error("no ptr divide", "extern fn malloc(size : u64) -> void^\nfn main()\n{\n    p := malloc(8) as u8^;\n    x := p / 2;\n}\n", "Invalid operands to DIVIDE operation");
    check_error("no two ptrs minus", "extern fn malloc(size : u64) -> void^\nfn main()\n{\n    a := malloc(8) as u8^;\n    b := malloc(8) as u8^;\n    d := a - b;\n}\n", "Invalid operands to MINUS operation");

    // `#assert <expr>` evaluates its operand entirely at compile time and
    // fails compilation (with the directive's file and line) when the result
    // is below 1. Passing asserts emit no code at all.
    check("#assert passes at top level and in functions", "#assert 1\n#assert 2 + 3 == 5\n#assert true\nfn main()\n{\n    #assert 10 > 5\n    print(\"ok\\n\");\n}\n", "ok\n");
    check_error("#assert zero fails", "fn main()\n{\n    #assert 0\n}\n", "#assert failed: expression evaluated to 0");
    check_error("#assert false fails", "fn main()\n{\n    #assert false\n}\n", "#assert failed");
    check_error("#assert failed comparison", "fn main()\n{\n    #assert 1 > 2\n}\n", "#assert failed");
    check_error("#assert negative fails", "fn main()\n{\n    #assert -5\n}\n", "#assert failed: expression evaluated to -5");
    check_error("#assert string type", "fn main()\n{\n    #assert \"str\"\n}\n", "#assert expression must be of a numeric or bool type, got string");
    check_error("#assert runtime value", "fn main()\n{\n    x := 5\n    #assert x\n}\n", "#assert expression must be a compile-time constant");
    check_error("#unknown directive", "#foo 1\n", "Unknown compile-time directive '#foo'");
    check_error("reserved __ prefix", "fn __mine() -> i64\n{\n    return 0;\n}\nfn main() -> i64\n{\n    return 0;\n}\n", "reserved '__' prefix");
    check_error("reserved __ prefix 2", "fn __x()\n{\n    print(1);\n}\nfn main()\n{\n    print(1);\n}\n", "reserved '__' prefix");
    check_error("reserved __ variable", "fn main()\n{\n    __x := 5;\n    print(__x);\n}\n", "reserved '__' prefix");
    check_error("reserved __ global variable", "__gx := 5\nfn main()\n{\n    print(__gx);\n}\n", "reserved '__' prefix");
    check_error("reserved __ parameter", "fn foo(__x : i64)\n{\n    print(__x);\n}\nfn main()\n{\n    foo(1);\n}\n", "reserved '__' prefix");
    check_error("reserved __ struct name", "struct __Point\n{\n    x : i64\n}\nfn main()\n{\n    print(1);\n}\n", "reserved '__' prefix");
    check_error("reserved __ struct field", "struct Point\n{\n    __x : i64\n}\nfn main()\n{\n    print(1);\n}\n", "reserved '__' prefix");
    // The reflection builtins lex as `#`-directives, so their old spellings
    // are no longer reserved: user functions may reuse those names.
    check("type_ names not reserved", "fn type_of(x : i64) -> i64\n{\n    return x + 1;\n}\nfn main()\n{\n    print(type_of(41));\n}\n", "42\n");
    check_error("#type_id no args", "fn main()\n{\n    print(#type_id());\n}\n", "expects exactly 1 argument");
    check_error("reserved print keyword", "fn print() -> i64\n{\n    return 0;\n}\nfn main() -> i64\n{\n    return 0;\n}\n");
    check_error("reserved fn keyword", "fn fn() -> i64\n{\n    return 0;\n}\nfn main() -> i64\n{\n    return 0;\n}\n");
    check_error("inconsistent return types", "fn foo() -> i64\n{\n    if 1 { return 5; }\n    return \"str\";\n}\nfn main()\n{\n    print(1);\n}\n", "Inconsistent return type");
    check_error("return value from void function", "fn foo() -> void\n{\n    return 5;\n}\nfn main()\n{\n    print(1);\n}\n", "Cannot return a value from a void function");
    check("implicit return type inferred from value return", "fn foo()\n{\n    return 5;\n}\nfn main()\n{\n    print(foo());\n}\n", "5\n");
    check("implicit return type inferred across branches", "fn foo()\n{\n    if true {\n        return 5;\n    }\n    return 9;\n}\nfn main()\n{\n    print(foo());\n}\n", "5\n");
    check("implicit return type inferred with recursion", "fn fib(n: i64)\n{\n    if n <= 1 {\n        return n;\n    }\n    return fib(n - 1) + fib(n - 2);\n}\nfn main()\n{\n    print(fib(10));\n}\n", "55\n");
    check_error("implicit return type must be consistent", "fn foo()\n{\n    return 5;\n    return \"str\";\n}\nfn main()\n{\n    print(1);\n}\n", "Inconsistent return type");
    check_error("bare return from value function", "fn foo() -> i64\n{\n    return;\n}\nfn main()\n{\n    print(1);\n}\n", "Inconsistent return type");
    check_error("unclosed block", "fn main() -> string\n{\n    print(1);\n");
    check_error("use of undeclared variable", "fn main()\n{\n    print(y);\n}\n");
    check_error("use of undeclared variable in expr", "fn main()\n{\n    print(y + 1);\n}\n");
    check_error("assignment to undeclared variable", "fn main()\n{\n    x = 5;\n}\n");
    check_error("undeclared variable in return", "fn foo() -> i64\n{\n    return z;\n}\nfn main() -> i64\n{\n    return 0;\n}\n");
    check_error("variable in own initializer", "fn main()\n{\n    x := x + 1;\n}\n");
    check_error("redeclare variable in loop body", "fn main()\n{\n    x := 1;\n    for i := 0 .. 2 { x := 9; print(x); }\n}\n");
    check("unary minus literal", "fn main()\n{\n    print(-5);\n}\n", "-5\n");
    check("unary minus variable", "fn main()\n{\n    x := 5;\n    print(-x);\n}\n", "-5\n");
    check("unary minus expression", "fn main()\n{\n    x := 5;\n    print(-(x + 1));\n}\n", "-6\n");
    check("double negative", "fn main()\n{\n    print(-(-3));\n}\n", "3\n");
    check("unary minus binds tighter than mult", "fn main()\n{\n    print(-2 * 3);\n}\n", "-6\n");
    check("hex literal", "fn main()\n{\n    print(0xFF);\n    print(0xff);\n    print(0x10 + 2);\n}\n", "255\n255\n18\n");
    check("binary literal", "fn main()\n{\n    print(0b1010);\n    print(0b1111);\n}\n", "10\n15\n");
    check("hex and binary in expressions", "fn main()\n{\n    print(0xFF - 0x0F);\n    print(0b1010 + 0b0101);\n}\n", "240\n15\n");
    check_error("bool plus bool", "fn main()\n{\n    print(true + true);\n}\n");
    check("bool equals bool", "fn main()\n{\n    print(true == true);\n}\n", "true\n");
    check("bool not equals bool", "fn main()\n{\n    print(true != false);\n}\n", "true\n");
    check_error("bool equals int", "fn main()\n{\n    print(true == 1);\n}\n");
    check_error("string equals string", "fn main()\n{\n    print(\"a\" == \"a\");\n}\n");
    check_error("string not equals string", "fn main()\n{\n    print(\"a\" != \"b\");\n}\n");
    check_error("invalid digit in literal", "fn main()\n{\n    print(5x);\n}\n");
    check_error("float literal not supported", "fn main()\n{\n    print(3.14);\n}\n", "floats are not supported yet");
    check_error("float with leading zero not supported", "fn main()\n{\n    print(0.5);\n}\n", "floats are not supported yet");
    check_error("semicolon inside call", "fn main()\n{\n    print(5;);\n}\n");
    check_error("double semicolon", "fn main()\n{\n    print(1);;\n    print(2);\n}\n");
    check_error("if without braces", "fn main()\n{\n    if 2 < 3 print(1);\n}\n", "not C (shit), please use braces");
    check_error("for without braces", "fn main()\n{\n    for i := 0 .. 3\n        print(i);\n}\n", "not C (shit), please use braces");
    check_error("else without braces", "fn main()\n{\n    if 2 < 3 { print(1); } else print(2);\n}\n", "not C (shit), please use braces");
    check_error("function body without braces", "fn foo() -> i64\n    print(1);\n}\nfn main() -> i64\n{\n    return 0;\n}\n", "not C (shit), please use braces");
    check_error("if braces message mentions if", "fn main()\n{\n    if 2 < 3 print(1);\n}\n", "for the if block");
    check_error("for braces message mentions for", "fn main()\n{\n    for i := 0 .. 3\n        print(i);\n}\n", "for the for block");
    check_error("break outside loop", "fn main()\n{\n    break;\n}\n", "Cannot use 'break' outside of a loop");
    check_error("continue outside loop", "fn main()\n{\n    print(continue);\n}\n", "Cannot use 'continue' outside of a loop");
    check_error("print without arguments", "fn main()\n{\n    print();\n}\n");
    check_error("print void value", "fn foo()\n{\n    print(1);\n}\nfn main()\n{\n    print(foo());\n}\n", "Cannot print a void value");
    check_error("assign void value", "fn foo()\n{\n    print(1);\n}\nfn main()\n{\n    x := foo();\n    print(x);\n}\n", "Cannot assign a void value");
    check_error("void if condition", "fn foo()\n{\n    print(1);\n}\nfn main()\n{\n    if foo() { print(2); }\n}\n", "void value as an if condition");
    check_error("void function argument", "fn foo()\n{\n    print(1);\n}\nfn echo(a : i64) -> i64\n{\n    return a;\n}\nfn main()\n{\n    echo(foo());\n}\n", "Cannot pass a void value");
    check_error("return void value", "fn foo()\n{\n    print(1);\n}\nfn bar() -> i64\n{\n    return foo();\n}\nfn main()\n{\n    print(bar());\n}\n", "Cannot return a void value");
    check_error("void parameter type", "fn foo(a : void)\n{\n    print(a);\n}\nfn main()\n{\n    foo(1);\n}\n", "Parameter cannot have type 'void'");
    // Every parameter must carry an explicit type (`name : i64`, `p : u8^`).
    check_error("untyped parameter rejected", "fn foo(a) -> i64\n{\n    return a;\n}\nfn main()\n{\n    foo(1);\n}\n", "must have an explicit type");
    check_error("partially untyped parameter rejected", "fn foo(a : i64, b) -> i64\n{\n    return a + b;\n}\nfn main()\n{\n    foo(1, 2);\n}\n", "must have an explicit type");
    check_error("cast to void", "fn main()\n{\n    print(5 as void);\n}\n", "Invalid operands");
    check_error("unknown return type", "fn foo() -> foo\n{\n    return 0;\n}\nfn main()\n{\n    print(1);\n}\n", "Unknown return type");
    check_error("type name as variable", "fn main()\n{\n    i64 := 5;\n    print(i64);\n}\n");
    check_error("function named after type", "fn i8() -> i64\n{\n    return 0;\n}\nfn main() -> i64\n{\n    return 0;\n}\n");
    check_error("unknown parameter type", "fn foo(a : foo) -> i64\n{\n    return 0;\n}\nfn main() -> i64\n{\n    return 0;\n}\n", "Unknown parameter type");
    check_error("param type hint at call site", "fn foo(a : i64) -> i64\n{\n    return a;\n}\nfn main()\n{\n    print(foo(1 : u32));\n}\n", "Expected ')', got ':'");
    check_error("string arg to bool param", "fn t(b : bool) -> i64\n{\n    print(b);\n    return 0;\n}\nfn main()\n{\n    t(\"abc\");\n}\n", "Argument type mismatch");
    check_error("string arg to int param", "fn t(a : u32) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    t(\"abc\");\n}\n", "Argument type mismatch");
    check_error("int arg to string param", "fn t(s : string) -> i64\n{\n    print(s);\n    return 0;\n}\nfn main()\n{\n    t(5);\n}\n", "Argument type mismatch");
    check_error("string arg to bool param in mixed call", "fn factorial(val : u32, val2 : bool) -> i64\n{\n    print(val);\n    return 0;\n}\nfn main()\n{\n    factorial(5, \"dfsa\");\n}\n", "Argument type mismatch");

    // --- bool does not implicitly convert to int ----------------------------
    // Mixing a bool with an int in arithmetic or a comparison is an error;
    // an explicit `as i64` cast is required.
    check_error("int arg to bool param", "fn t(b : bool) -> i64\n{\n    print(b);\n    return 0;\n}\nfn main()\n{\n    t(1);\n}\n", "Argument type mismatch");
    check_error("bool arg to int param", "fn t(a : u32) -> i64\n{\n    print(a);\n    return 0;\n}\nfn main()\n{\n    t(true);\n}\n", "Argument type mismatch");
    check_error("bool plus int", "fn main()\n{\n    print((2 < 3) + 1);\n}\n");
    check_error("bool var plus int", "fn main()\n{\n    b := 2 < 3;\n    print(b + 1);\n}\n");
    check_error("int plus bool", "fn main()\n{\n    print(1 + (2 < 3));\n}\n");
    check_error("int equals bool", "fn main()\n{\n    print(5 == true);\n}\n");
    check_error("bool equals int zero", "fn main()\n{\n    print(false == 0);\n}\n");
    check_error("bool less than int", "fn main()\n{\n    print(true < 2);\n}\n");
    check_error("bool mod int", "fn main()\n{\n    print((2 < 3) % 2);\n}\n");
    check_error("runtime bool loop bound", "fn main()\n{\n    n := 3 < 5;\n    for i := 0 .. n { print(i); }\n}\n", "loop bound");
    check_error("inclusive bool bound", "fn main()\n{\n    b := 1 == 1;\n    for i := 0 ..= b { print(i); }\n}\n", "loop bound");
    check_error("bool comp bound", "fn main()\n{\n    for i := 0 .. (3 < 5) { print(i); }\n}\n", "loop bound");

    // --- invalid argument/parameter matrix ----------------------------------
    // The complement of the positive matrix: every (arg, param) pair the
    // language rejects must fail with "Argument type mismatch" (34 pairs: int
    // arg -> bool/string param, bool arg -> int/string param, string arg ->
    // int/bool param). Uses the same compatible() rule as the positive test.
    {
        const char* arg_types[10] = {"i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "bool", "string"};
        const char* arg_exprs[10] = {"1 as i8", "1 as i16", "1 as i32", "1", "1 as u8", "1 as u16", "1 as u32", "1 as u64", "true", "\"x\""};
        auto compatible = [&](int arg, int param) -> bool {
            if (arg == 9) return param == 9;  // string arg: string param only
            if (param == 9) return false;     // int/bool arg: no string param
            if (arg == 8) return param == 8;  // bool arg: bool param only
            if (param == 8) return false;     // int arg: no bool param
            return true;                      // int arg: int param
        };
        std::string header;
        for (int p = 0; p < 10; ++p)
            header += "fn takes" + std::string(arg_types[p]) + "(a : " + arg_types[p] + ") -> i64 { return 0; }\n";
        for (int p = 0; p < 10; ++p)
            for (int a = 0; a < 10; ++a) {
                if (compatible(a, p)) continue;
                std::string src = header;
                src += "fn main()\n{\n";
                src += "takes" + std::string(arg_types[p]) + "(" + arg_exprs[a] + ");\n";
                src += "}\n";
                std::string name = "invalid arg matrix " + std::string(arg_types[a]) + " to " + std::string(arg_types[p]);
                check_error(name.c_str(), src, "Argument type mismatch");
            }
    }

    check_error("unclosed parenthesis", "fn main()\n{\n    print(5\n}\n");

    // --- unsigned types --------------------------------------------------
    // Unsigned values are stored zero-extended; casts, arithmetic, division,
    // comparisons and printing must use unsigned semantics. Reference values
    // cross-checked against C (uint8_t/uint16_t/uint32_t/uint64_t).
    check("u8 cast print", "fn main()\n{\n    print(5 as u8);\n}\n", "5\n");
    check("u16 cast print", "fn main()\n{\n    print(5 as u16);\n}\n", "5\n");
    check("u32 cast print", "fn main()\n{\n    print(5 as u32);\n}\n", "5\n");
    check("u64 cast print", "fn main()\n{\n    print(5 as u64);\n}\n", "5\n");
    check("u32 variable init", "fn main()\n{\n    x := 5 as u32;\n    print(x);\n}\n", "5\n");
    check("u64 in expression", "fn main()\n{\n    print(1 + 2 as u64);\n}\n", "3\n");
    check("u32 high value print", "fn main()\n{\n    x := 3000000000 as u32;\n    print(x);\n}\n", "3000000000\n");
    check("u32 cast truncates", "fn main()\n{\n    x := 4294967296 as u32;\n    print(x);\n}\n", "0\n");
    check("i64 to u64 wraps", "fn main()\n{\n    print((0 - 1) as u64);\n}\n", "18446744073709551615\n");
    check("u64 to i64 reinterprets", "fn main()\n{\n    x := (0 - 1) as u64;\n    print(x as i64);\n}\n", "-1\n");

    // --- integer literal ranges -----------------------------------------
    // Literals are parsed as unsigned magnitudes so u64 literals up to
    // 2^64-1 work (atoll used to silently clamp them to INT64_MAX). Values in
    // [2^63, 2^64-1] are typed u64; values above 2^64-1 are a compile error.
    check("u64 max literal", "fn main()\n{\n    print(18446744073709551615);\n}\n", "18446744073709551615\n");
    check("u64 max literal as u64", "fn main()\n{\n    print(18446744073709551615 as u64);\n}\n", "18446744073709551615\n");
    check("i64 max literal", "fn main()\n{\n    print(9223372036854775807);\n}\n", "9223372036854775807\n");
    check("i64 min underflow boundary", "fn main()\n{\n    print(9223372036854775808 as i64);\n}\n", "-9223372036854775808\n");
    // 2^63 parses as a u64 literal (values in [2^63, 2^64-1] are typed u64),
    // so an explicit `as u64` must print it back without going negative.
    check("u64 min literal", "fn main()\n{\n    print(9223372036854775808 as u64);\n}\n", "9223372036854775808\n");
    check("u64 min literal arithmetic", "fn main()\n{\n    x := 9223372036854775808;\n    print(x + x);\n}\n", "0\n");
    check("u64 max equals neg one", "fn main()\n{\n    print(18446744073709551615 == (0 - 1) as u64);\n}\n", "true\n");
    check("u64 max bit pattern", "fn main()\n{\n    print(18446744073709551615 as i8);\n    print(18446744073709551615 as i64);\n}\n", "-1\n-1\n");
    check("u64 literal arithmetic", "fn main()\n{\n    x := 18446744073709551615;\n    print(x / (4 as u64));\n    print(x % (1000 as u64));\n    print(x > (1000 as u64));\n}\n", "4611686018427387903\n615\ntrue\n");
    check("u64 literal truncates", "fn main()\n{\n    print(18446744073709551615 as u32);\n}\n", "4294967295\n");
    check_error("u64 literal overflow", "fn main()\n{\n    print(18446744073709551616);\n}\n");
    check_error("u64 literal overflow many digits", "fn main()\n{\n    print(99999999999999999999);\n}\n");
    check("u64 print runtime", "fn f(a : i64) -> i64 {\n    print(a as u64);\n    return 0;\n}\nfn main() {\n    f(0 - 1);\n}\n", "18446744073709551615\n");
    check("u32 div non-pow2", "fn main()\n{\n    x := 3000000000 as u32;\n    print(x / (1000000000 as u32));\n}\n", "3\n");
    check("u32 mod non-pow2", "fn main()\n{\n    x := 3000000000 as u32;\n    print(x % (1000000000 as u32));\n}\n", "0\n");
    check("u64 div wraps", "fn f(a : i64) -> i64 {\n    x := a as u64;\n    print(x / (4 as u64));\n    return 0;\n}\nfn main() {\n    f(0 - 1);\n}\n", "4611686018427387903\n");
    check("u64 mod wraps", "fn f(a : i64) -> i64 {\n    x := a as u64;\n    print(x % (1000 as u64));\n    return 0;\n}\nfn main() {\n    f(0 - 1);\n}\n", "615\n");
    check("u64 compare wraps", "fn f(a : i64) -> i64 {\n    x := a as u64;\n    print(x > (1000 as u64));\n    return 0;\n}\nfn main() {\n    f(0 - 1);\n}\n", "true\n");
    check("u64 fused jump wraps", "fn f(a : i64) -> i64 {\n    x := a as u64;\n    if (x > (1000 as u64)) { print(1); } else { print(2); }\n    return 0;\n}\nfn main() {\n    f(0 - 1);\n}\n", "1\n");
    // --- u8 / u16 semantics ------------------------------------------------
    // u8/u16 values promote to i32 for arithmetic but must be truncated to
    // their width when stored; negatives must wrap (0-1 -> 255 / 65535).
    check("u8 cast truncates", "fn main()\n{\n    print(300 as u8);\n}\n", "44\n");
    check("u8 truncate runtime", "fn f(a : i64) -> i64 {\n    x := a as u8;\n    print(x);\n    return 0;\n}\nfn main() {\n    f(300);\n}\n", "44\n");
    check("u8 negative wraps", "fn main()\n{\n    print((0 - 1) as u8);\n}\n", "255\n");
    check("u8 negative wraps runtime", "fn f(a : i64) -> i64 {\n    x := a as u8;\n    print(x);\n    return 0;\n}\nfn main() {\n    f(0 - 1);\n}\n", "255\n");
    check("u16 cast truncates", "fn main()\n{\n    print(70000 as u16);\n}\n", "4464\n");
    check("u16 negative wraps", "fn main()\n{\n    print((0 - 1) as u16);\n}\n", "65535\n");
    // u8/u16 promote to i32, so comparisons happen in signed i32 space:
    // 255 < 200 is false (not unsigned-wrapped-true).
    check("u8 compare promotes to i32", "fn main()\n{\n    print((0 - 1) as u8 < 200 as u8);\n}\n", "false\n");
    check("u8 arithmetic promotes to i32", "fn main()\n{\n    print((200 as u8) + (100 as u8));\n}\n", "300\n");
    check("u8 plus i8 promotes", "fn main()\n{\n    print((200 as u8) + (100 as i8));\n}\n", "300\n");
    // --- u32 semantics ------------------------------------------------------
    // u32 does NOT promote (u32+i64 -> i64), but u32 values compared against
    // each other must use unsigned comparisons (4294967295 > 1000 is true).
    check("u32 compare wraps", "fn main()\n{\n    print((0 - 1) as u32 > 1000 as u32);\n}\n", "true\n");
    check("u32 compare wraps rt", "fn f(a : i64) -> i64 {\n    x := a as u32;\n    print(x > (1000 as u32));\n    return 0;\n}\nfn main() {\n    f(0 - 1);\n}\n", "true\n");
    check("u32 mixed with i64 promotes", "fn main()\n{\n    print((5 as u32) + 1);\n}\n", "6\n");
    // i32 arithmetic must wrap at the operation boundary, so the result is the
    // same whether it feeds print directly or further arithmetic (10^10 % 2^32
    // = 1410065408; 1410065408 + 1 = 1410065409, NOT 10000000001).
    check("i32 mult wraps", "fn main()\n{\n    x := 100000 as i32;\n    print(x * x);\n    print(x * x + 1);\n}\n", "1410065408\n1410065409\n");
    check("i32 add wraps rt", "fn f(a : i64) -> i64 {\n    x := a as i32;\n    print(x + x);\n    print(x + x + 1);\n    return 0;\n}\nfn main() {\n    f(2000000000);\n}\n", "-294967296\n-294967295\n");
    check("u32 mult wraps", "fn main()\n{\n    x := 100000 as u32;\n    print(x * x);\n    print(x * x + 1);\n}\n", "1410065408\n1410065409\n");
    check("u32 mod truncates rt", "fn f(a : i64) -> i64 {\n    x := a as u32;\n    print(x % (1000000000 as u32));\n    return 0;\n}\nfn main() {\n    f(3000000000);\n}\n", "0\n");
    check("u32 loop bound", "fn main()\n{\n    for i := 0 .. 3 as u32 { print(i); }\n}\n", "0\n1\n2\n");
    check_asm("u32 div pow2 -> shr", "fn main()\n{\n    x := 255 as u32;\n    print(x / (4 as u32));\n}\n",
              "shr rdi, 2", "idiv", "63\n");
    check_asm("u32 mod pow2 -> and", "fn main()\n{\n    x := 255 as u32;\n    print(x % (8 as u32));\n}\n",
              "and rdi, rdx", "idiv", "7\n");
    check_asm("u64 div non-pow2 keeps div", "fn f(a : i64) -> i64 {\n    x := a as u64;\n    print(x / (1000 as u64));\n    return 0;\n}\nfn main() {\n    f(0 - 1);\n}\n",
              "mov rcx, 1000\n\tdiv rcx", "idiv", "18446744073709551\n");
    // A runtime u32 cast must zero-extend via `mov eax, eax`; `movzx rax, eax`
    // has no valid encoding and would fail to assemble.
    check_asm("u32 runtime cast zero-extends", "fn f(a : i64) -> i64 {\n    x := a as u32;\n    print(x);\n    return 0;\n}\nfn main() {\n    f(0 - 1);\n}\n",
              "mov eax, eax", "movzx rax, eax", "4294967295\n");

    // --- invalid casts --------------------------------------------------
    check_error("cast string to int", "fn main()\n{\n    print(\"abc\" as i8);\n}\n");
    check_error("cast string to bool", "fn main()\n{\n    print(\"abc\" as bool);\n}\n");
    check_error("cast int to string", "fn main()\n{\n    print(5 as string);\n}\n");
    check_error("cast bool to string", "fn main()\n{\n    print(true as string);\n}\n");
    check_error("cast void to int", "fn foo()\n{\n    print(1);\n}\nfn main()\n{\n    print(foo() as i8);\n}\n");
    check_error("cast to unknown type", "fn main()\n{\n    print(5 as foo);\n}\n");

    // --- invalid pointers ------------------------------------------------
    check_error("deref non-pointer read", "fn main()\n{\n    x := 10;\n    print(^x);\n}\n", "Cannot dereference");
    check_error("deref non-pointer store", "fn main()\n{\n    x := 10;\n    ^x = 20;\n}\n", "Cannot dereference");
    check_error("address of literal", "fn main()\n{\n    print(&10);\n}\n", "Cannot take the address of a non-lvalue expression");
    check_error("address of bool literal", "fn main()\n{\n    print(&true);\n}\n", "Cannot take the address of a non-lvalue expression");
    check_error("address of string literal", "fn main()\n{\n    print(&\"hi\");\n}\n", "Cannot take the address of a non-lvalue expression");
    check_error("address of cast result", "fn main()\n{\n    x := 1;\n    print(&(x as u8^));\n}\n", "Cannot take the address of a non-lvalue expression");
    check_error("address of call result", "fn f() -> i64\n{\n    return 1;\n}\nfn main()\n{\n    print(&f());\n}\n", "Cannot take the address of a non-lvalue expression");
    check_error("address of address", "fn main()\n{\n    x := 1;\n    print(&(&x));\n}\n", "Cannot take the address of a non-lvalue expression");
    check_error("address of deref of non-pointer", "fn main()\n{\n    x := 10;\n    print(&^x);\n}\n", "Cannot dereference a non-pointer value");
    check_error("assign int to pointer param", "fn f(p : i64^) -> i64\n{\n    return 0;\n}\nfn main()\n{\n    f(5);\n}\n");
    check_error("address of non-lvalue expression", "fn main()\n{\n    x := 1;\n    print(&(x + 1));\n}\n", "Cannot take the address of a non-lvalue expression");
    check_error("assign to non-lvalue expression", "fn main()\n{\n    x := 1;\n    &x = 5;\n}\n", "Cannot assign to a non-lvalue expression");
    check_error("assign to arithmetic result", "fn main()\n{\n    x := 1;\n    x + 1 = 5;\n}\n", "Cannot assign to a non-lvalue expression");
    check_error("assign to call result", "fn f() -> i64\n{\n    return 1;\n}\nfn main()\n{\n    f() = 5;\n}\n", "Cannot assign to a non-lvalue expression");
    check_error("assign to literal", "fn main()\n{\n    5 = 3;\n}\n", "Cannot assign to a non-lvalue expression");
    check_error("assign through parens to arithmetic", "fn main()\n{\n    x := 1;\n    (x + 1) = 5;\n}\n", "Cannot assign to a non-lvalue expression");
    check_error("assign to cast result", "fn main()\n{\n    x := 1;\n    (x as i64^) = 5;\n}\n", "Cannot assign to a non-lvalue expression");
    check_error("assign to cast result no parens", "fn main()\n{\n    x := 1;\n    x as i64^ = 5;\n}\n", "Cannot assign to a non-lvalue expression");
    check_error("assign through mismatched pointee", "fn f(p : u8^) -> i64\n{\n    return 0;\n}\nfn main()\n{\n    x := 5;\n    p := &x;\n    f(&^p);\n}\n", "expected a pointer to u8, got a pointer to i64");
    check_error("deref non-pointer call result", "fn f() -> i64\n{\n    return 5;\n}\nfn main()\n{\n    print(^f());\n}\n", "Cannot dereference");
    check_error("double deref of single pointer", "fn main()\n{\n    x := 10;\n    p := &x;\n    print(^^p);\n}\n", "Cannot dereference");
    check_error("pointer to string param", "fn f(s : string) -> i64\n{\n    print(s);\n    return 0;\n}\nfn main()\n{\n    x := 10;\n    f(&x);\n}\n", "Argument type mismatch");
    check_error("assign pointer to int variable", "fn main()\n{\n    x := 1;\n    y := 2;\n    q := &y;\n    x = q;\n    print(x);\n}\n", "Cannot assign Ptr to i64 variable");
    check_error("u64 address to u8^ param", "fn test(val : u8^) -> i64\n{\n    print(val);\n    return 0;\n}\nfn main()\n{\n    x := 120 as u64;\n    test(&x);\n}\n", "expected a pointer to u8, got a pointer to u64");
    check_error("int address to string^ param", "fn f(p : string^) -> i64\n{\n    print(p);\n    return 0;\n}\nfn main()\n{\n    x := 10;\n    f(&x);\n}\n", "expected a pointer to string, got a pointer to i64");
    check_error("single-level pointer to double-level param", "fn f(p : bool^^) -> i64\n{\n    return 0;\n}\nfn main()\n{\n    b := false;\n    f(&b);\n}\n", "expected a 2-level pointer, got a 1-level pointer");
    check_error("pointer cast to non-pointer", "fn main()\n{\n    x := 10;\n    p := &x;\n    y := p as i64;\n}\n", "Invalid operands to CAST operation");
    check_error("non-pointer cast to pointer", "fn main()\n{\n    x := 5;\n    p := x as i64^;\n}\n", "Invalid operands to CAST operation");
    check_error("pointer cast to bool", "fn main()\n{\n    x := 10;\n    p := &x;\n    b := p as bool;\n}\n", "Invalid operands to CAST operation");
    check_error("unknown struct in literal", "fn main()\n{\n    foo := Foo { x: 1 };\n}\n");
    check_error("unknown struct member in literal", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    foo := Foo { y: 1 };\n}\n", "doesnt contain 'y' field");
    check_error("duplicate member in literal", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    foo := Foo { x: 1, x: 2 };\n}\n", "Duplicate initialization of member 'x'");
    check_error("struct member read on non-struct", "fn main()\n{\n    x := 10;\n    print(x.field);\n}\n", "Cannot access a member of a non-struct value");
    check_error("struct member assign on non-struct", "fn main()\n{\n    x := 10;\n    x.field = 5;\n}\n", "Cannot assign a member of a non-struct value");
    check_error("print struct value", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    foo := Foo { x: 1 };\n    print(foo);\n}\n", "Cannot print a struct value");
    check_error("struct arg mismatch", "struct Foo\n{\n    x : i64,\n}\nstruct Bar\n{\n    x : i64,\n}\nfn f(foo : Foo)\n{\n    print(foo.x);\n}\nfn main()\n{\n    b := Bar { x: 1 };\n    f(b);\n}\n", "Argument type mismatch for function 'f'");
    check_error("struct reassign mismatch", "struct Foo\n{\n    x : i64,\n}\nstruct Bar\n{\n    x : i64,\n}\nfn main()\n{\n    a := Foo { x: 1 };\n    b := Bar { x: 2 };\n    a = b;\n}\n", "Cannot assign Bar to Foo variable");
    // A typed declaration must initialize with the exact struct type
    // (regression: `f : Foo = Bar { ... }` used to compile because the
    // declared-type check sees TYPE_STRUCT on both sides).
    check_error("struct typed decl mismatch", "struct Foo\n{\n    x : i64,\n}\nstruct Bar\n{\n    x : i64,\n}\nfn main()\n{\n    a : Foo = Bar { x: 1 };\n}\n", "Cannot assign Bar to Foo variable");
    check_error("struct typed decl call mismatch", "struct Foo\n{\n    x : i64,\n}\nstruct Bar\n{\n    x : i64,\n}\nfn get_bar() -> Bar\n{\n    return Bar { x: 1 };\n}\nfn main()\n{\n    a : Foo = get_bar();\n}\n", "Cannot assign Bar to Foo variable");
    check_error("struct typed decl var mismatch", "struct Foo\n{\n    x : i64,\n}\nstruct Bar\n{\n    x : i64,\n}\nfn main()\n{\n    b := Bar { x: 1 };\n    a : Foo = b;\n}\n", "Cannot assign Bar to Foo variable");
    check_error("struct type declared twice", "struct Foo\n{\n    x : i64,\n}\nstruct Foo\n{\n    y : i64,\n}\nfn main()\n{\n    foo := Foo { x: 1 };\n}\n");
    // --- pointers to structs ---------------------------------------------
    // `Foo^` is a pointer to a Foo value: the address of a struct variable's
    // slot. Read/write members through it, alias the original struct, and
    // pass it around like any other pointer.
    check("pointer to struct variable", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    f := Foo { x: 42 };\n    p : Foo^ = &f;\n    print(p.x);\n}\n", "42\n");
    check("pointer to struct alias writes through", "struct Foo\n{\n    x : i64,\n    y : i64,\n}\nfn main()\n{\n    f := Foo { x: 1, y: 2 };\n    p : Foo^ = &f;\n    p.y = 9;\n    print(f.y);\n    p.x = p.x + 10;\n    print(f.x);\n}\n", "9\n11\n");
    check("pointer to struct param mutates caller", "struct Foo\n{\n    x : i64,\n}\nfn bump(p : Foo^)\n{\n    p.x = p.x + 1;\n}\nfn main()\n{\n    f := Foo { x: 5 };\n    bump(&f);\n    bump(&f);\n    print(f.x);\n}\n", "7\n");
    check("struct pointer to struct pointer param", "struct Foo\n{\n    x : i64,\n}\nfn read(p : Foo^) -> i64\n{\n    return p.x;\n}\nfn main()\n{\n    f := Foo { x: 4 };\n    print(read(&f));\n}\n", "4\n");
    check("address of whole struct untyped", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    f := Foo { x: 7 };\n    p := &f;\n    print(p.x);\n}\n", "7\n");
    check("pointer to struct reassign and null", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    f := Foo { x: 1 };\n    g := Foo { x: 2 };\n    p : Foo^ = &f;\n    p = &g;\n    print(p.x);\n    p = null;\n    if p == null { print(\"null\\n\"); }\n}\n", "2\nnull\n");
    check("address of struct-typed member", "struct Inner\n{\n    a : i64,\n}\nstruct Outer\n{\n    inner : Inner,\n}\nfn main()\n{\n    o := Outer { inner: Inner { a: 3 } };\n    p := &o.inner;\n    print(p.a);\n    p.a = 10;\n    q := o.inner;\n    print(q.a);\n}\n", "3\n10\n");
    check("deref struct pointer", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    f := Foo { x: 5 };\n    p : Foo^ = &f;\n    g := ^p;\n    print(g.x);\n}\n", "5\n");
    check("deref struct pointer assign", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    f := Foo { x: 1 };\n    p : Foo^ = &f;\n    ^p = Foo { x: 99 };\n    print(f.x);\n}\n", "99\n");
    check("deref struct pointer to struct param", "struct Foo\n{\n    x : i64,\n}\nfn bump(f : Foo)\n{\n    f.x = f.x + 1;\n}\nfn main()\n{\n    f := Foo { x: 5 };\n    p : Foo^ = &f;\n    bump(^p);\n    print(f.x);\n}\n", "6\n");
    check("struct pointer field", "struct Bar\n{\n    y : i64,\n}\nstruct Foo\n{\n    p : Bar^,\n    n : i64,\n}\nfn main()\n{\n    bar := Bar { y: 3 };\n    foo := Foo { p: &bar, n: 1 };\n    p := foo.p;\n    print(p.y);\n}\n", "3\n");
    check("struct pointer field null init", "struct Bar\n{\n    y : i64,\n}\nstruct Foo\n{\n    p : Bar^,\n}\nfn main()\n{\n    foo := Foo { p: null };\n    if foo.p == null { print(\"null\\n\"); }\n}\n", "null\n");
    check("pointer to struct return", "struct Foo\n{\n    x : i64,\n}\ng : Foo = Foo { x: 11 }\nfn get() -> Foo^\n{\n    return &g;\n}\nfn main()\n{\n    p := get();\n    print(p.x);\n}\n", "11\n");
    check("global struct pointer variable", "struct Foo\n{\n    x : i64,\n}\ngp : Foo^ = null\nfn main()\n{\n    f := Foo { x: 8 };\n    gp = &f;\n    print(gp.x);\n}\n", "8\n");
    check("pointer to struct of member field", "struct Inner\n{\n    a : i64,\n}\nstruct Outer\n{\n    p : Inner^,\n}\nfn main()\n{\n    inner := Inner { a: 13 };\n    o := Outer { p: &inner };\n    q := o.p;\n    print(q.a);\n}\n", "13\n");
    check_error("struct pointer arg wrong struct", "struct Foo\n{\n    x : i64,\n}\nstruct Bar\n{\n    x : i64,\n}\nfn f(p : Bar^)\n{\n    print(p.x);\n}\nfn main()\n{\n    foo := Foo { x: 1 };\n    f(&foo);\n}\n", "Argument type mismatch for function 'f'");
    check_error("struct typed decl from wrong struct pointer", "struct Foo\n{\n    x : i64,\n}\nstruct Bar\n{\n    x : i64,\n}\nfn main()\n{\n    b := Bar { x: 1 };\n    p : Bar^ = &b;\n    f : Foo = ^p;\n}\n", "Cannot assign Bar to Foo variable");
    check_error("unknown field type", "struct Foo\n{\n    x : Baz,\n}\nfn main()\n{\n    foo := Foo { x: 1 };\n}\n");
    check_error("address of member of non-struct", "fn main()\n{\n    x := 10;\n    p := &x.field;\n}\n", "non-struct value");
    check_error("address of member of undeclared struct member", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    f := Foo { x: 1 };\n    p := &f.zzz;\n}\n", "doesnt contain 'zzz' field");
    // A struct member passed where a different type is expected still errors.
    check_error("struct i64 member to str param", "struct Foo\n{\n    x : i64,\n}\nfn need_str(s : str) -> i64\n{\n    return 0\n}\nfn main()\n{\n    f := Foo { x: 5 };\n    need_str(f.x);\n}\n", "Argument type mismatch for function 'need_str'");
    // A struct-typed member passed to a param of a different struct type.
    check_error("struct-typed member to wrong struct param", "struct Inner\n{\n    a : i64,\n}\nstruct Other\n{\n    a : i64,\n}\nstruct Outer\n{\n    inner : Inner,\n}\nfn take(o : Other) -> i64\n{\n    return o.a\n}\nfn main()\n{\n    x := Outer { inner: Inner { a: 1 } };\n    take(x.inner);\n}\n", "Argument type mismatch for function 'take'");
    // `&f.x` (a pointer) cannot satisfy a struct param.
    check_error("address of member to struct param", "struct Foo\n{\n    x : i64,\n}\nfn take_struct(f : Foo) -> i64\n{\n    return f.x\n}\nfn main()\n{\n    f := Foo { x: 5 };\n    take_struct(&f.x);\n}\n", "Argument type mismatch for function 'take_struct'");
    // --- struct zero-init errors ------------------------------------------
    // `Foo {0}` is the only value allowed alone in the braces; any other value
    // or form is invalid.
    check_error("zero init with nonzero value", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    a := Foo {5};\n}\n");
    check_error("zero init with two values", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    a := Foo {0, 0};\n}\n");
    check_error("zero init mixed with member", "struct Foo\n{\n    x : i64,\n}\nfn main()\n{\n    a := Foo {0, x: 1};\n}\n");
    // --- global errors -----------------------------------------------------
    // Assigning to an undeclared global is the same error as for locals.
    check_error("assign undeclared global", "fn main()\n{\n    gx = 5;\n}\n", "use operator ':='");
    // `:=` on an already-declared global is rejected; use `=`.
    check_error("redeclare global with :=", "gx : i64 = 1\nfn main()\n{\n    gx := 5;\n}\n", "use operator '='");
    // Globals are type-checked like locals.
    check_error("global wrong type", "gx : i64 = 1\nfn main()\n{\n    gx = \"str\";\n}\n", "Cannot assign string to i64 variable");
    check_error("global unknown type", "gx : Zzz = 1\nfn main()\n{\n    print(gx);\n}\n", "Expected a type name after ':'");
    // --- C FFI errors -------------------------------------------------------
    // `extern fn` declares a C function: a body is a contradiction.
    check_error("extern fn with body", "extern fn foo(x : i64) -> i64\n{\n    return x;\n}\nfn main()\n{\n    print(foo(1));\n}\n", "extern function cannot have a body");
    // Struct values don't cross the C ABI; require an explicit pointer.
    check_error("extern struct param", "struct S\n{\n    a : i64,\n}\nextern fn foo(s : S) -> i64\nfn main()\n{\n    print(foo(1));\n}\n", "parameter of struct type is not supported");
    check_error("extern struct return", "struct S\n{\n    a : i64,\n}\nextern fn foo() -> S\nfn main()\n{\n    print(1);\n}\n", "extern function returning a struct is not supported");
    // A plain void param is still invalid even for extern functions.
    check_error("extern void param", "extern fn foo(v : void) -> i64\nfn main()\n{\n    print(1);\n}\n", "Parameter cannot have type 'void'");
    // `extern` must be followed by `fn`.
    check_error("extern without fn", "extern x : i64\nfn main()\n{\n    print(1);\n}\n", "Expected 'fn' after 'extern'");

    // --- modules -------------------------------------------------------
    check_error("module named global", "module global\nfn main()\n{\n    print(1);\n}\n", "'global' is a reserved name");
    check_error("module not first", "fn foo()\n{\n}\nmodule m\nfn main()\n{\n    print(1);\n}\n", "must be the first expression");
    check_error("module self import", "module m\nimport m\nfn main()\n{\n    print(1);\n}\n", "cannot import itself");
    check_files_error("import undeclared module", {
        "a.nul", "import lib\nfn main()\n{\n    print(1);\n}\n",
    }, "is not declared in any compiled file");
    check_files_error("qualified call without import", {
        "a.nul", "module lib\nfn foo() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "fn main()\n{\n    print(lib::foo());\n}\n",
    }, "not visible");
    check_files_error("call to hidden module function", {
        "a.nul", "module lib\nfn foo() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "fn main()\n{\n    print(foo());\n}\n",
    }, "Call to undeclared function");
    check_files_error("ambiguous function call", {
        "a.nul", "module a\nfn val() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "module b\nfn val() -> i64\n{\n    return 2;\n}\n",
        "c.nul", "import a\nimport b\nfn main()\n{\n    print(val());\n}\n",
    }, "Ambiguous reference to function");
    check_files_error("ambiguous global read", {
        "a.nul", "module a\ngx := 1\n",
        "b.nul", "module b\ngx := 2\n",
        "c.nul", "import a\nimport b\nfn main()\n{\n    print(gx);\n}\n",
    }, "Ambiguous reference to variable");
    check_files_error("ambiguous struct use", {
        "a.nul", "module a\nstruct S\n{\n    x : i64,\n}\n",
        "b.nul", "module b\nstruct S\n{\n    x : i64,\n}\n",
        "c.nul", "import a\nimport b\nfn main()\n{\n    p := S { x: 1 }\n    print(p.x);\n}\n",
    }, "Ambiguous reference to struct");
    check_files_error("duplicate function in module", {
        "a.nul", "module m\nfn foo() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "module m\nfn foo() -> i64\n{\n    return 2;\n}\nfn main()\n{\n    print(1);\n}\n",
    }, "already declared");
    check_files_error("module not visible global read", {
        "a.nul", "module lib\ngx := 5\n",
        "b.nul", "fn main()\n{\n    print(lib::gx);\n}\n",
    }, "not visible");

    // `module`/`import` require a real identifier as the module name.
    check_error("module keyword as module name", "module fn\nfn main()\n{\n    print(1);\n}\n", "Expected module name");
    check_error("import keyword as module name", "import fn\nfn main()\n{\n    print(1);\n}\n", "Expected module name");

    // A file can declare a module at most once (it must be the first
    // expression, so a second `module` in the same file is rejected).
    check_error("module declared twice in one file", "module m\nmodule m\nfn main()\n{\n    print(1);\n}\n", "must be the first expression");

    // Visibility is per-module and not transitive: importing `lib` does not
    // expose the modules `lib` itself imports.
    check_files_error("transitive import not visible", {
        "a.nul", "module core\ngx := 5\n",
        "b.nul", "module lib\nimport core\nfn use_it() -> i64\n{\n    return core::gx;\n}\n",
        "c.nul", "import lib\nfn main()\n{\n    print(core::gx);\n}\n",
    }, "not visible");

    // Same symbol name in the same module, spread across files: redeclaration.
    check_files_error("duplicate struct in module", {
        "a.nul", "module m\nstruct S\n{\n    x : i64,\n}\n",
        "b.nul", "module m\nstruct S\n{\n    y : i64,\n}\nfn main()\n{\n    print(1);\n}\n",
    }, "already declared");
    check_files_error("duplicate global in module", {
        "a.nul", "module m\ngx := 1\n",
        "b.nul", "module m\ngx := 2\nfn main()\n{\n    print(1);\n}\n",
    }, "already created");

    // Qualified references to symbols the module does not declare.
    check_files_error("call undeclared qualified function", {
        "a.nul", "module lib\nfn foo() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "import lib\nfn main()\n{\n    print(lib::nope());\n}\n",
    }, "Call to undeclared function");
    check_files_error("qualified undeclared struct use", {
        "a.nul", "module lib\nstruct S\n{\n    x : i64,\n}\n",
        "b.nul", "import lib\nfn main()\n{\n    p := lib::Nope { x: 1 }\n}\n",
    }, "Expected keyword");

    // The global module's symbols are visible everywhere, so a same-named
    // module symbol makes the unqualified name ambiguous, not shadowed.
    check_files_error("ambiguous function with global module symbol", {
        "a.nul", "fn val() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "module m\nfn val() -> i64\n{\n    return 2;\n}\n",
        "c.nul", "import m\nfn main()\n{\n    print(val());\n}\n",
    }, "Ambiguous reference to function");
    check_files_error("ambiguous global with global module symbol", {
        "a.nul", "gx := 1\n",
        "b.nul", "module m\ngx := 2\n",
        "c.nul", "import m\nfn main()\n{\n    print(gx);\n}\n",
    }, "Ambiguous reference to variable");

    // Unqualified global *write* is ambiguous too when two modules are visible.
    check_files_error("ambiguous global write", {
        "a.nul", "module a\ngx := 1\n",
        "b.nul", "module b\ngx := 2\n",
        "c.nul", "import a\nimport b\nfn main()\n{\n    gx = 5;\n}\n",
    }, "Ambiguous reference to variable");

    // `module` must be the first expression even when the file imports first.
    check_files_error("module after import", {
        "a.nul", "module lib\nfn v() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "import lib\nmodule m\nfn main()\n{\n    print(1);\n}\n",
    }, "must be the first expression");

    // A struct field typed with a module the declaring module never imported.
    check_files_error("struct field type from unimported module", {
        "a.nul", "module geo\nstruct Point\n{\n    x : i64,\n}\n",
        "b.nul", "module shapes\nstruct Circle\n{\n    center : geo::Point,\n}\nfn main()\n{\n    print(1);\n}\n",
    }, "not visible");

    // A module calling another module's function without importing it.
    check_files_error("module calls unimported module", {
        "a.nul", "module lib\nfn v() -> i64\n{\n    return 1;\n}\n",
        "b.nul", "module app\nfn main()\n{\n    print(lib::v());\n}\n",
    }, "not visible");

    // Importing a module that no compiled file declares, from a later file.
    check_files_error("import undeclared module in second file", {
        "a.nul", "module m\nfn main()\n{\n    print(1);\n}\n",
        "b.nul", "module m\nimport ghost\n",
    }, "is not declared in any compiled file");
}

int main() {
    positive_tests();
    error_tests();
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
