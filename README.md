
# Nul programming language

A small systems language that compiles to native x86-64 assembly (via
[fasm](https://flatassembler.net/)) and links a runnable executable. No
interpreter, no GC, no runtime dependency.

## Building

```sh
clang++ -o build ./build.cpp
./build compiler     # CLI compiler -> ./nulc
./build --gui        # build and launch the GUI compiler -> ./nulc_gui
./build --tests      # build and run the test suite -> ./tests/run_tests
```

`./nulc main.nul` compiles `main.nul` to `main` and runs it. A folder
argument compiles every `.nul` file inside it into one program
(`./nulc examples/09_module_basics`).

## Language tour

- **Values & types**: `i8 i16 i32 i64`, `u8 u16 u32 u64`, `bool`,
  `str`/`string`, pointers `T^` (multi-level `T^^` supported), arrays
  `T[N]`, `void`, and user `struct`s. No floats, no booleans in
  expressions. Unary minus (`-x` desugars to `0 - x`), logical
  `&&`/`||` (short-circuiting), and `!` are supported. Integer literals
  can be decimal, hex (`0x1F`) or binary (`0b1010`).
- **Preprocessor**: `#define NAME value` (object-like) at the top of a
  file; chained definitions are expanded at compile time (recursion
  capped at 16).
- **Variables**: `x := 5` (inferred) or `x : i64 = 5`; reassign with
  `=`. Globals live at file scope (`gx := 7`).
- **Control flow**: `if`/`elif`/`else` (braces required),
  `for i := 0 .. 3` (exclusive) and `for i in 0..=3` (inclusive).
  Condition loops: `for <cond> { }` or `for (<cond>) { }`
  (e.g. `for true { }` is an infinite loop). Loop bodies support
  `break` (exit the loop) and `continue` (skip to the next iteration).
  Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`; logical NOT is `!`
  (e.g. `!done`, `!false`).
- **Functions**: `fn add(a : i64, b : i64) -> i64`. A missing return
  type is inferred from the body's `return`s. Recursion, mutual
  recursion and forward calls work. Struct parameters are passed by
  reference (callee mutations are visible to the caller).
- **Defer**: `defer <call>` registers a function call (or `print`) that
  runs when the function exits, in LIFO order — regardless of which
  `return` path is taken. Arguments are evaluated (captured) when the
  `defer` statement executes. A `defer` inside a branch only fires if
  that branch ran. Typical use: `p := malloc(n); defer free(p);`.
  Limitation: a `defer` inside a loop registers once (the call runs once
  at exit with the last captured arguments).
- **Structs**: `struct Foo { x : i64, y : str, }`; init `Foo { x: 1 }`
  (omitted fields zero) or `Foo { 0 }` (all zero). C layout — use
  `offset_of`/`align_of` to inspect it.
- **Pointers**: `&x`, `^p` (read) and `^p = v` (write), `null`,
  `type_of`/`type_id`/`type_size`. Pointers print as `0x…` hex.
- **Strings**: immutable literals with `\n`/`\"`/`\\` escapes. Strings
  are not arrays and cannot be compared with `==`; use `strlen`/`memcmp`
  via the FFI. String variables expose `.len` (compile-time length, when
  the value is a literal) and `.data` (a `u8^` pointer to the bytes).
- **Arrays**: fixed-size `T[N]` values. Literals infer their size:
  `a := [10, 20, 30]`; `len(a)` is a compile-time constant. Elements are
  zero-indexed (`a[0]`) and writable (`a[1] = 99`). Assigning one array
  to another (`b := a`) is a deep copy — later writes through one alias
  never affect the other. Element types are numeric/bool only; mixing
  widens to the largest type.
- **Builtins**: `print`, `type_id`, `type_size`, `type_of`,
  `offset_of(S, "field")`, `align_of(S)`, casts with `as`.
- **Modules**: `module name` at the top of a file, `import name` in
  another, symbols referenced `name::symbol`. Files compile in name
  order, so a module file should sort before its importers.
- **FFI**: `extern fn malloc(size : u64) -> void^` calls into libc
  (malloc/free/strlen/memcpy/puts/…). `void^` is the generic pointer.

## Examples

`examples/` contains 13 sample programs, from hello world up:

| File | Shows |
|------|-------|
| `01_hello.nul` | print, literals, comments, string escapes |
| `02_variables.nul` | variables, arithmetic, casts, types |
| `03_control_flow.nul` | if/elif/else, all for-loop forms |
| `04_functions.nul` | functions, params, returns, recursion |
| `05_pointers.nul` | address-of, deref, null, multi-level pointers |
| `06_structs.nul` | structs, init, nesting, reference params |
| `07_globals.nul` | file-scope globals shared across functions |
| `08_ffi.nul` | extern C calls (malloc/free/strlen/memcpy) |
| `09_module_basics/` | multi-file modules (`module`/`import`/`::`) |
| `10_fizzbuzz.nul` | classic challenge with functions |
| `11_sorting.nul` | bubble sort over a struct "list" |
| `12_introspection.nul` | type_id/type_size/type_of/offset_of/align_of |
| `13_primes.nul` | sieve of Eratosthenes, complex control flow |

Run any single-file example with `./nulc examples/10_fizzbuzz.nul`, and
the multi-file one with `./nulc examples/09_module_basics`.
