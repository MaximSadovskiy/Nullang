// `#run` executes compiled code inside the compiler process, so the scratch
// shared library is loaded with the platform's dynamic-loader API. Only POSIX
// hosts are supported for compile-time execution (the compiler itself).
#if defined(__unix__) || defined(__APPLE__)
    #include <dlfcn.h>
#endif

// ---- Enums ----
enum TokenType {
    Tok_Ident,
    Tok_IntLit,
    Tok_FloatLit,
    Tok_StrLit,
    Tok_BoolLit,
    Tok_NullLit,
    Tok_Cast,
    Tok_Type,
    Tok_Keyword,
    Tok_Return,
    Tok_Defer,
    Tok_Function,
    Tok_Module,
    Tok_ImportModule,
    Tok_Struct,
    Tok_If,
    Tok_Else,
    Tok_Elif,
    Tok_For,
    Tok_Break,
    Tok_Continue,
    Tok_Oper,
    Tok_LParen,
    Tok_RParen,
    Tok_LArrow,
    Tok_RArrow,
    Tok_Arrow,
    Tok_Equals,
    Tok_DEquals,
    Tok_NotEquals,
    Tok_LEquals,
    Tok_GEquals,
    Tok_Not,
    Tok_DDot,
    Tok_DDotInclusive,
    Tok_DColon,
    Tok_ColonEquals,
    Tok_SemiColon,
    Tok_Comma,
    Tok_Plus,
    Tok_Minus,
    Tok_Mult,
    Tok_Caret,
    Tok_Div,
    Tok_Mod,
    Tok_Ampersand,
    Tok_AndAnd,
    Tok_OrOr,
    Tok_Dot,
    Tok_LBracket,
    Tok_RBracket,
    Tok_LSquare,
    Tok_RSquare,
    Tok_Directive,
    Tok_Eof,
};

enum InstructionDataType
{
    TYPE_VARIABLE,
    TYPE_ADDRESS,
    TYPE_CALL,
};

enum ExpressionType {
    Expr_Unknown = 0,
    Expr_Binary,
    Expr_Print,
    Expr_Assignment,
    Expr_Module,
    Expr_ImportModule,
    Expr_Function,
    Expr_For,
    Expr_Block,
    Expr_Call,
    Expr_Struct,
    Expr_StructInit,
    Expr_MemberCall,
    Expr_Return,
    Expr_If,
    Expr_AddressOf,
    Expr_Deref,
    Expr_DerefAssign,
    Expr_Not,
    Expr_Break,
    Expr_Continue,
    Expr_Defer,
    Expr_ArrayLit,
    Expr_Index,
    Expr_IndexAssign,
    Expr_Assert,
    Expr_Run,
    Expr_ComptimeLib,
};

enum ValueType
{
    TYPE_NOP = 0,
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_PTR,
    TYPE_STR,
    TYPE_STRUCT,
    TYPE_ARRAY,
};

enum InstructionType
{
    OP_NOP = 0,
    OP_PUSH_I8,
    OP_PUSH_I16,
    OP_PUSH_I32,
    OP_PUSH_I64,
    OP_PUSH_PTR,
    OP_PUSH_STR,
    OP_PUSH_BOOL,
    OP_PLUS,
    OP_MINUS,
    OP_MULT,
    OP_DIVIDE,
    OP_MOD,
    OP_DROP,
    OP_DUP,
    OP_INC,
    OP_PRINT,
    OP_LABEL,
    OP_EQUALS,
    OP_NOT_EQUALS,
    OP_LESS,
    OP_LESS_EQUALS,
    OP_CAST,
    OP_GREATER,
    OP_GREATER_EQUALS,
    OP_NOT,
    OP_JMP,
    OP_JMP_IF,
    OP_STORE,
    OP_CALL,
    OP_RET,
    OP_LEA,
    OP_LOAD_PTR,
    OP_STORE_PTR,
    OP_ALLOC,
    // __entry reads the OS-provided command line: OP_ENTRY_ARGC loads the
    // argument count and OP_ENTRY_ARGV the pointer to the argument array from
    // the entry stack, so the synthesized `call main` can pass them on.
    OP_ENTRY_ARGC,
    OP_ENTRY_ARGV,
};
// Physical registers available to the linear-scan allocator. rax/rbx/rcx/rdx
// stay reserved as codegen scratch (arithmetic temporaries, idiv operands,
// string addresses), and rbx additionally because OP_STORE/OP_PUSH_STR use it
// as a scratch register. rbp/rsp are excluded (frame).
enum PhysReg : s8 {
    PR_RSI, PR_RDI, PR_R8, PR_R9, PR_R10, PR_R11, // caller-saved
    PR_R12, PR_R13, PR_R14, PR_R15,              // callee-saved
    PR_COUNT,
    PR_NONE = -1,
};
enum MathType {
    SHIFT_NONE,
    SHIFT_MULT,
    SHIFT_DIV,
    SHIFT_MOD
};

// ---- Forward declarations ----
struct Token;
struct Lexer;
struct Expression;
struct Variable;
struct Instruction;
struct BinaryExpr;
struct StructExpr;
struct FunctionExpr;
struct Operator;
struct DeclaredString;
struct VirtualReg;
struct BlockExpr;
struct CallExpr;
struct MemberCallExpr;
struct PrintExpr;
struct ReturnExpr;
struct BreakExpr;
struct ContinueExpr;
struct DeferExpr;
struct ForExpr;
struct IfExpr;
struct AssignmentExpr;
struct AddressOfExpr;
struct DerefExpr;
struct DerefAssignExpr;
struct NotExpr;
struct ArrayLitExpr;
struct IndexExpr;
struct IndexAssignExpr;
struct FunctionArgument;
struct DeclaredFunction;
struct LiveInterval;
struct Module;

Token eof_token();
void compiler_error(Token location, const char* const format, ...);
u32 type_size(ValueType type);
bool is_valid_operation(InstructionType operation_type, ValueType lhs, ValueType rhs, ValueType& result_type);
bool is_digit(char ch);
int is_operator(char ch);
bool is_keyword(StrView ident);
bool is_word(char ch);
extern const char* src_content;
extern Operator operators_array[];

// ---- Structs ----

struct Token {
    enum TokenType type = Tok_Eof;
    StrView val = "Eof";
};

struct Field {
    StrView name = "Eof";
    u32 size = 8;
    // C-style byte offset of this field within the struct: each field is
    // aligned to its own size (u8 -> 1, i32 -> 4, pointers -> 8) and the
    // struct's total size is padded to its largest member alignment.
    u32 offset = 0;
    // Declared type metadata of the field: the primitive ValueType, plus
    // pointee/depth for `i64^` pointer fields and the owning struct name for
    // struct-typed fields.
    ValueType type = TYPE_I64;
    ValueType pointee = TYPE_NOP;
    u8 ptr_depth = 0;
    StrView struct_name = "";
    StrView struct_module = "";
    // For pointer-to-struct fields (`p : Bar^`): the pointee struct's name,
    // so member access and type checks can resolve the layout.
    StrView pointee_struct_name = "";
    StrView pointee_struct_module = "";
};


struct Keyword {
    StrView val = "Eof";
};


struct LabelRef
{
    s64 ip;
};


struct Variable
{
    StrView name;
    ValueType type;
    usize reg_index;
    bool is_local;
    bool is_argument;
    bool is_ptr;
    bool is_accesible = true;
    StrView struct_name = "";
    // The module the struct type (struct_name) is declared in; needed to
    // resolve member access when the same struct name exists in several
    // visible modules.
    StrView struct_module = "";
    // Pointer metadata, valid when `type == TYPE_PTR`: `pointee` is the base
    // type of the pointee chain (u8 for `u8^` and `bool` for `bool^^`) and
    // `ptr_depth` is how many indirection levels remain (1 for `u8^`, 2 for
    // `bool^^`).
    ValueType pointee = TYPE_NOP;
    u8 ptr_depth = 0;
    // For pointer-to-struct variables (`p : Foo^`): the pointee struct's name
    // and the module it is declared in.
    StrView pointee_struct_name = "";
    StrView pointee_struct_module = "";
    // For global variables: the byte offset of this variable's slot inside the
    // shared __globals region (assigned once when the global is first created).
    // Local variables never use it.
    usize global_offset = (usize)-1;
    // The module this variable was declared in ("" = the global module). Local
    // variables keep the default; only globals use it for visibility checks.
    StrView module_name = "";
    // For string variables: the index of the string literal this variable
    // currently holds, when that literal is statically known (-1 otherwise).
    // Compile-time metadata only, used by the `.len` member accessor.
    s64 str_literal_index = -1;
    // For array variables: the element type and the fixed element count.
    // Compile-time metadata only, used by `len()` and indexed access.
    ValueType array_elem = TYPE_NOP;
    s64 array_len = -1;
    // Compile-time constant variable (`x := #run { ... }` or any expression
    // that folded to a constant): every read folds to comp_time_val and no
    // runtime slot store is emitted. Only numeric/bool values qualify.
    bool is_comp_time = false;
    s64 comp_time_val = 0;
};


struct BinaryOp
{
    InstructionType type;
    usize lhs_index;
    usize rhs_index;
};

struct CallArg
{
    Expression* expr;
    ValueType type = TYPE_NOP;
    usize reg_index;
    u32 size;
};

struct CallSite
{
    Token name;
    Array<CallArg> args;
    bool is_void = false;
    // The module the callee is declared in ("" = global). Set when the call is
    // translated so the asm emitter can mangle the label for named modules.
    StrView module = "";
};

struct AssignTarget
{
    Variable var;
};

struct Instruction
{
    enum InstructionType type;
    InstructionDataType data_type;
    Token location;
    union {
        struct LabelRef label;
        struct AssignTarget target;
        struct CallSite call;
        struct BinaryOp binop;
    };
    usize reg_index;
    // Payload for ops that carry a plain constant: OP_ALLOC's reserved size in
    // bytes and OP_LOAD_PTR/OP_STORE_PTR's byte displacement from the base
    // pointer (member offset within a struct).
    usize int_val = 0;
    // Struct member access width in bytes for OP_LOAD_PTR/OP_STORE_PTR
    // (u8 -> 1, i32 -> 4, ...). Zero means a full 8-byte access, which is
    // how plain `^p` dereferences behave.
    u8 byte_size = 0;
    // For OP_ALLOC: the C alignment of the reserved struct region, in bytes
    // (1, 2, 4 or 8). The frame and return-area placeholders pad their running
    // offset so every reserved region's base address is a multiple of this.
    u32 align = 1;
    bool is_visited;
};

struct VirtualReg
{
    usize index;
    u32 offset;
    bool is_comp_time;
    bool is_visited;
    // VirtualReg allocation: which physical register holds this value (PR_NONE
    // = spilled to its stack slot), and a preferred register for loop counters.
    s8 phys = PR_NONE;
    s8 hint = PR_NONE;
    ValueType type;
    union {
        s64   int_val;
        void* ptr_val;
        s64   str_val;
        bool  bool_val;
    };
    // True when this virtual register is a per-function binding of a global
    // variable. Such registers are never allocated to a physical register and
    // never get a frame offset: their value lives in the shared __globals
    // region, addressed at `offset`.
    bool is_global = false;
};

struct Expression
{
    virtual ~Expression() = default;

    Token tok = eof_token();
    ExpressionType type = Expr_Unknown;
};

struct BinaryExpr : Expression
{
    Expression* lhs = nullptr;
    Expression* rhs = nullptr;
    // Indirection depth of a type-name operand (`u8^` -> 1, `bool^^` -> 2),
    // used when a type name appears as a cast target. Zero for all other
    // operands.
    u8 ptr_depth = 0;
    // For a module-qualified identifier (`mod::name`): the module part. Empty
    // means the plain (unqualified) form.
    StrView module_name = "";
};

struct ModuleExpr : Expression
{
};

struct ModuleImportExpr : Expression
{
    StrView short_name = "";
};

struct PrintExpr : Expression
{
    Expression* rhs = nullptr;
};

// `#assert <expr>`: the operand is evaluated entirely at compile time (via
// the constant-folding translation); a result below 1 is a compile error
// reported at this token's file and line.
struct AssertExpr : Expression
{
    Expression* rhs = nullptr;
};

// `#run { ... }`: a block executed at compile time. The block is wrapped in a
// synthetic function, compiled to a scratch shared library and run in the
// compiler process; its `return` value (i64) becomes a compile-time constant.
struct RunExpr : Expression
{
    BlockExpr* block = nullptr;
};

// `#libc` / `#lib("path")`: registers a shared library to preload before
// `#run` blocks execute, so their extern calls resolve against it. A no-op at
// translation time.
struct ComptimeLibExpr : Expression
{
    StrView path = ""; // "" = platform libc soname (#libc)
};

struct AssignmentExpr : Expression
{
    Token field_name = {Tok_StrLit, ""};
    Token oper = eof_token();
    bool is_local = false;
    Expression* rhs = nullptr;
    // Explicit declaration type (`x : u8 = 5`). When set (not TYPE_NOP) the
    // variable is forced to this type: numeric values are converted
    // (promote/shrink) and any other mismatch is an error. For pointers the
    // declared pointee/depth (`u8^`, `bool^^`) override the inferred metadata.
    ValueType declared_type = TYPE_NOP;
    ValueType declared_pointee = TYPE_NOP;
    u8 declared_ptr_depth = 0;
    // Explicit struct type in a typed declaration (`foo : Foo = ...`).
    StrView declared_struct_name = "";
    StrView declared_struct_module = "";
    // Declared pointee struct for pointer-to-struct declarations
    // (`p : Foo^ = ...`).
    StrView declared_pointee_struct_name = "";
    StrView declared_pointee_struct_module = "";
    // Explicit array type in a typed declaration (`a : i64[5] = ...`): the
    // element type and the declared element count.
    ValueType declared_array_elem = TYPE_NOP;
    s64 declared_array_len = -1;
    // For a module-qualified target (`mod::gx = 5`): the module of the global
    // variable being assigned.
    StrView module_name = "";
};

struct AddressOfExpr : Expression
{
    Expression* operand = nullptr;
};

// A dereference (`^ptr`). `pointee`/`ptr_depth` describe the *result* of the
// dereference and are filled in during translation so nested dereferences
// (`^^pp`) know the inner pointer's type: the result is `pointee` when
// `ptr_depth == 0`, otherwise a pointer with `ptr_depth` more levels.
struct DerefExpr : Expression
{
    Expression* target = nullptr;
    ValueType pointee = TYPE_NOP;
    u8 ptr_depth = 0;
    // For a dereference that yields a struct value (`^p` where p : Foo^): the
    // pointee struct's name and module, filled in during translation.
    StrView pointee_struct_name = "";
    StrView pointee_struct_module = "";
};

// `^ptr = value` — a store through a pointer. `target` is the DerefExpr being
// assigned to.
struct DerefAssignExpr : Expression
{
    Expression* target = nullptr;
    Expression* rhs = nullptr;
};

// `!x` — logical negation. Produces a bool result.
struct NotExpr : Expression
{
    Expression* operand = nullptr;
};

// `[a, b, c]` — an array literal. All elements are numeric and share one
// element type; the literal's length is its element count.
struct ArrayLitExpr : Expression
{
    Array<Expression*> elements;
    // The promoted element type, resolved during translation so metadata
    // queries (indexing, len, deep-copy) know the element width.
    ValueType elem_type = TYPE_NOP;
};

// `arr[i]` — a runtime index read from an array. `base` is the array value
// expression, `index` the (numeric) subscript.
struct IndexExpr : Expression
{
    Expression* base = nullptr;
    Expression* index = nullptr;
};

// `arr[i] = v` — a store through an array element.
struct IndexAssignExpr : Expression
{
    Expression* base = nullptr;
    Expression* index = nullptr;
    Expression* rhs = nullptr;
};

struct BlockExpr : Expression
{
    Array<Expression*> exprs;
};

struct FunctionArgument
{
    Expression* expr = nullptr;
    ValueType type = TYPE_NOP;
    ValueType pointee = TYPE_NOP;
    u8 ptr_depth = 0;
    // Struct-typed parameters (`foo : Foo`) carry the declared struct name so
    // callers can be type-checked and the callee's prologue can tag the arg
    // variable with it.
    StrView struct_name = "";
    StrView struct_module = "";
    // For pointer-to-struct parameters (`p : Foo^`): the pointee struct's name.
    StrView pointee_struct_name = "";
    StrView pointee_struct_module = "";
};

struct FunctionExpr : Expression
{
    Array<Expression*> args;
    Array<FunctionArgument> arg_types;
    BlockExpr* block = nullptr;
    ValueType return_type = TYPE_NOP; // TYPE_NOP = inferred from `return`s
    // For `fn foo() -> Foo`: the declared struct return type, so a call site
    // assigning the result can tag its variable with the struct name.
    StrView return_struct_name = "";
    StrView return_struct_module = "";
    // For `fn foo() -> Foo^`: the pointee struct of the returned pointer, so a
    // call site assigning the result can tag its variable's pointee metadata.
    ValueType return_pointee = TYPE_NOP;
    u8 return_ptr_depth = 0;
    StrView return_pointee_struct_name = "";
    StrView return_pointee_struct_module = "";
    // `extern fn foo(...) -> T;`: a C FFI declaration with no body. The symbol
    // is resolved by the C linker, so the compiler never emits a body for it.
    bool is_extern = false;
};

struct IfExpr : Expression
{
    Expression* condition = nullptr;
    BlockExpr* if_block = nullptr;
    BlockExpr* else_block = nullptr;
};

struct ForExpr : Expression
{
    Expression* left_cond;
    Expression* right_cond;
    BlockExpr* block;
    bool is_inclusive;
    // `for <condition> { }`: a condition-driven loop (e.g. `for true { }` is
    // an infinite loop). When set, left_cond is the condition evaluated at the
    // top of every iteration and right_cond is unused.
    bool is_condition = false;
};

struct StructExpr : Expression
{
    Array<Field> fields;
    usize total_size = 0;
    // C alignment of the whole struct: the largest member alignment. Allocated
    // storage for the struct is placed at an address that is a multiple of this.
    u32 align = 1;
};

struct StructInitExpr : Expression
{
    Array<StrView> field_names;
    Array<Expression*> field_values;
    // `Foo {0}`: initialize every member with its zero value (0 for numbers,
    // "" for strings, recursively for struct-typed members) instead of naming
    // members.
    bool zero_init = false;
    // For `mod::Foo { ... }`: the module the struct is declared in.
    StrView module_name = "";
};

struct MemberCallExpr : Expression
{
    Token field = eof_token();
};

struct ReturnExpr : Expression
{
    Expression* rhs = nullptr;
};

struct BreakExpr : Expression
{
};

struct ContinueExpr : Expression
{
};

struct DeferExpr : Expression
{
    Expression* expr = nullptr;
};

struct CallExpr : Expression
{
    Array<CallArg> args;
    // For `mod::func(...)`: the module the function is declared in.
    StrView module_name = "";
};

struct Operator {
    StrView val;
    enum TokenType type;
    int precedence = -1;
    bool contains(char op) const {
        return val.data[0] == op;
    }
};

struct Lexer
{
public:
    LocalArray<Token> _tokens;
    StrBuilder _source;
    Lexer(StrView source) {
        // read_entire_file(source, _source);
        src_content = _source.data();
    }

    ~Lexer() {_tokens.cleanup(); _source.cleanup();}

    void skip() {
        if (_tokens.count() > 0)
            _tokens.pop();
    }

    Token next() {
        if (_tokens.count() == 0) return eof_token();
        auto tok = _tokens.last();
        _tokens.pop();
        return tok;
    }

    Token peak() {
        if (_tokens.count() == 0) return eof_token();
        return _tokens.last();
    }
    // The token after the one `peak()` returns (a peek-two lookahead), or Eof
    // when there is none. Used to tell `x : type` (typed declaration, single
    // ':') apart from `mod :: name` (module-qualified reference, two ':').
    Token peak_next() {
        if (_tokens.count() < 2) return eof_token();
        return _tokens[_tokens.count() - 2];
    }
    bool tokenize()
    {
        if (_source.count() == 0) return true;

        auto source_view = _source.to_string_view();
        while(!done())
        {
            usize mark = get_index();
            auto ch = next_char();
            if (ch == '"') {
                while (ch != 0) {
                    ch = peak_char();
                    if (ch == 0) {
                        auto tok = Token{Tok_StrLit, source_view.sub_view(mark, get_index())};
                        compiler_error(tok, "Unclosed string litteral '" SV_FORMAT "' \n", SV_ARG(tok.val));
                    }
                    if (ch == '\\') {
                        skip_char();
                        if (peak_char() != 0)
                            skip_char();
                        continue;
                    }
                    if (ch == '"' || ch == '\n') {
                        skip_char();
                        break;
                    }
                    skip_char();
                }
                _tokens.push(Token{Tok_StrLit, source_view.sub_view(mark+1, get_index()-1)});
            } else if (is_word(ch)) {
                while (ch != 0) {
                    ch = peak_char();
                    if (!is_word(ch) && !is_digit(ch)) {
                        break;
                    }
                    skip_char();
                }
                auto ident_view = source_view.sub_view(mark, get_index());
                auto type = Tok_Ident;
                if (is_keyword(ident_view))
                    type = Tok_Keyword;
                else if (ident_view == "return")
                    type = Tok_Return;
                else if (ident_view == "defer")
                    type = Tok_Defer;
                else if (ident_view == "fn")
                    type = Tok_Function;
                else if (ident_view == "module")
                    type = Tok_Module;
                else if (ident_view == "import")
                    type = Tok_ImportModule;
                else if (ident_view == "if")
                    type = Tok_If;
                else if (ident_view == "else")
                    type = Tok_Else;
                else if (ident_view == "elif")
                    type = Tok_Elif;
                else if (ident_view == "for")
                    type = Tok_For;
                else if (ident_view == "break")
                    type = Tok_Break;
                else if (ident_view == "continue")
                    type = Tok_Continue;
                else if (ident_view == "struct")
                    type = Tok_Struct;
                else if (ident_view == "as")
                    type = Tok_Cast;
                _tokens.push(Token{type, ident_view});
            } else if (is_digit(ch)) {
                // Hex (`0xFF`) and binary (`0b1010`) literals. A leading `0`
                // followed by `x`/`X`/`b`/`B` switches the accepted digit set.
                char base = 10;
                if (ch == '0') {
                    char prefix = peak_char();
                    if (prefix == 'x' || prefix == 'X')
                        base = 16;
                    else if (prefix == 'b' || prefix == 'B')
                        base = 2;
                    if (base != 10) {
                        skip_char(); // consume the 'x'/'X'/'b'/'B'
                        while (ch != 0) {
                            ch = peak_char();
                            bool is_digit_ok = (base == 16) ? ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) : (ch == '0' || ch == '1');
                            if (is_word(ch) && !is_digit_ok) {
                                auto tok = Token{Tok_IntLit, source_view.sub_view(mark, get_index())};
                                compiler_error(tok, "Invalid digit '%c' in %s constant: '" SV_FORMAT "'\n", ch, base == 16 ? "hexadecimal" : "binary", SV_ARG(tok.val));
                            }
                            if (!is_digit_ok)
                                break;
                            skip_char();
                        }
                        if (base == 2 && is_digit(peak_char())) {
                            auto tok = Token{Tok_IntLit, source_view.sub_view(mark, get_index())};
                            compiler_error(tok, "Invalid digit '%c' in binary constant: '" SV_FORMAT "'\n", peak_char(), SV_ARG(tok.val));
                        }
                    }
                }
                if (base == 10) {
                    while (ch != 0) {
                        ch = peak_char();
                        if (is_word(ch)) {
                            auto tok = Token{Tok_IntLit, source_view.sub_view(mark, get_index())};
                            compiler_error(tok, "Invalid digit '%c' in demical contant: '" SV_FORMAT "'\n", ch, SV_ARG(tok.val));
                        }
                        if (!is_digit(ch))
                            break;
                        skip_char();
                    }
                }
                // Float literal: digits '.' digits. Floats aren't supported
                // yet, so consume the whole literal and report a dedicated
                // error (a '.' followed by a non-digit, as in `1..5`, is the
                // range operator, not a float).
                if (base == 10 && peak_char() == '.' && get_index() + 1 < _source.count()
                    && is_digit(_source.data()[get_index() + 1])) {
                    skip_char(); // '.'
                    while (is_digit(peak_char()))
                        skip_char();
                    auto tok = Token{Tok_FloatLit, source_view.sub_view(mark, get_index())};
                    compiler_error(tok, "floats are not supported yet\n");
                }
                _tokens.push(Token{Tok_IntLit, source_view.sub_view(mark, get_index())});
            } else if (ch == '^') {
                // Pointer type suffix (`u8^`, `bool^^`). Only valid in type
                // positions (param/return/cast); a standalone `^` is a parse
                // error later.
                _tokens.push(Token{Tok_Caret, source_view.sub_view(mark, get_index())});
            } else if (ch == '#') {
                // Compile-time directive: the whole `#name` lexes as one
                // token. Only the built-in directives are recognized; an
                // unknown name is rejected here so typos fail loudly instead
                // of parsing as code.
                while (is_word(peak_char()))
                    skip_char();
                auto directive = source_view.sub_view(mark, get_index());
                if (directive != "#assert" && directive != "#type_id"
                    && directive != "#type_size" && directive != "#type_of"
                    && directive != "#run" && directive != "#libc"
                    && directive != "#lib")
                    compiler_error(Token{Tok_Directive, directive}, "Unknown compile-time directive '" SV_FORMAT "', expected '#assert', '#run', '#libc', '#lib', '#type_id', '#type_size' or '#type_of'\n", SV_ARG(directive));
                _tokens.push(Token{Tok_Directive, directive});
            } else if (ch == '[') {
                // Array literal (`[1, 2, 3]`) and postfix indexing (`arr[i]`).
                _tokens.push(Token{Tok_LSquare, source_view.sub_view(mark, get_index())});
            } else if (ch == ']') {
                _tokens.push(Token{Tok_RSquare, source_view.sub_view(mark, get_index())});
            } else if (auto oper_index = is_operator(ch)) {
                auto next = peak_char();
                usize end_mark = get_index();
                if (ch == '/' && next == '/') {
                    skip_char();
                    ch = peak_char();
                    while (ch != 0 && ch != '\n') {
                        ch = next_char();
                    }
                    continue;
                }
                if (ch == ':' && next == '=') {
                    skip_char();
                    end_mark = get_index();
                    _tokens.push(Token{Tok_ColonEquals, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (ch == '-' && next == '>') {
                    skip_char();
                    end_mark = get_index();
                    _tokens.push(Token{Tok_Arrow, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (ch == '=' && next == '=') {
                    skip_char();
                    end_mark = get_index();
                    _tokens.push(Token{Tok_DEquals, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (ch == '!' && next == '=') {
                    skip_char();
                    end_mark = get_index();
                    _tokens.push(Token{Tok_NotEquals, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (ch == '&' && next == '&') {
                    skip_char();
                    end_mark = get_index();
                    _tokens.push(Token{Tok_AndAnd, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (ch == '|' && next == '|') {
                    skip_char();
                    end_mark = get_index();
                    _tokens.push(Token{Tok_OrOr, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (ch == '<' && next == '=') {
                    skip_char();
                    end_mark = get_index();
                    _tokens.push(Token{Tok_LEquals, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (ch == '>' && next == '=') {
                    skip_char();
                    end_mark = get_index();
                    _tokens.push(Token{Tok_GEquals, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (ch == '.' && next == '.') {
                    auto type = Tok_DDot;
                    skip_char();
                    next = peak_char();
                    if (next == '=') {
                        type = Tok_DDotInclusive;
                        skip_char();
                    }
                    end_mark = get_index();
                    _tokens.push(Token{type, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (oper_index == -1) {
                    skip_char();
                    continue;
                }
                auto type = operators_array[oper_index-1].type;
                _tokens.push(Token{type, source_view.sub_view(mark, get_index())});
            } else if (ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ') {
                // xd
            } else {
                auto tok = Token{Tok_Eof, source_view.sub_view(mark, get_index())};
                compiler_error(tok, "Unknown token '%c' in: '" SV_FORMAT "'\n", ch, SV_ARG(tok.val));
            }
        }
        _tokens.reverse();
        return true;
   }
private:
        void skip_char() {
            ++_index;
        }
        char next_char() {
            auto ch = peak_char();
            ++_index;
            return ch;
        }
        char peak_char() {
            if (_index >= _source.count()) return '\0';
            return _source.data()[_index];
        }
        usize get_index() {
            return _index;
        }
        bool done() {
            return _index >= _source.count();
        }
        usize _index = 0;
};

struct DeclaredFunction
{
    StrView name = "wtf";
    FunctionExpr* expr = nullptr;
    Array<Instruction> ops{};
    Array<VirtualReg> regs{};
    ValueType return_type = TYPE_NOP;
    StrView return_struct_name = "";
    StrView return_struct_module = "";
    // Pointee metadata for `-> Foo^` return types (see FunctionExpr).
    ValueType return_pointee = TYPE_NOP;
    u8 return_ptr_depth = 0;
    StrView return_pointee_struct_name = "";
    StrView return_pointee_struct_module = "";
    // Bytes the callee writes into its caller-reserved return slot (struct
    // returns): the declared return struct's size plus any nested structs it
    // materializes. Set by translate_function_body.
    usize return_area_size = 0;
    // Synthetic wrapper around a `#run { ... }` block: its body's trailing
    // expression value becomes the implicit `return` (ordinary functions
    // fall back to `return 0` instead).
    bool is_comptime_wrapper = false;
    // `extern fn` FFI declaration: the symbol comes from the C linker, so the
    // compiler must not emit a body/label for it.
    bool is_extern = false;
    // The module this function is declared in ("" = the global module). Used
    // for visibility checks and to mangle the emitted label.
    StrView module_name = "";
    // Source file the function was parsed from. Translation runs after every
    // file is parsed, so compiler_error's file/line attribution must be
    // pointed back at this file while the body translates.
    StrView src_path = SV_LIT("");
    const char* src_content = "";
    // Optimization/register passes (dead_code .. update_all_offsets) have
    // already run on this function's ops. `#run` compiles a scratch shared
    // library mid-translation, running the passes early; the final binary's
    // emission must not run them twice.
    bool passes_done = false;
    bool operator==(const DeclaredFunction& other) const {
        return name == other.name;
    }
    bool operator!=(const DeclaredFunction& other) const {
        return name != other.name;
    }
};

struct DeclaredStruct
{
    StrView name = "wtf";
    StructExpr* expr = nullptr;
    // The module this struct is declared in ("" = the global module). Used for
    // visibility checks.
    StrView module_name = "";
    bool operator==(const DeclaredStruct& other) const {
        return name == other.name;
    }
    bool operator!=(const DeclaredStruct& other) const {
        return name != other.name;
    }
};

struct DeclaredString
{
    StrView name = "wtf";
    BinaryExpr* expr = nullptr;
    u64 offset = 0;
};

struct Label
{
    u64 ip;
};

struct LiveInterval {
    usize reg;
    usize start;
    usize end;
    s8 hint;
};

struct Module {
    StrView name = "";
    Array<DeclaredFunction> functions;
    Array<DeclaredStruct>   structs;
    Array<DeclaredString>   strings;
    // Modules this module `import`s: symbols declared in an imported module
    // become visible inside this module (the global module's symbols are always
    // visible everywhere without an import).
    Array<StrView> imports;
};

// ---- Function declarations ----

// Persistent storage for strings that must outlive parsing (synthetic
// `#run` function names, copied `#lib("path")` library paths).
static Array<StrBuilder>& comptime_name_pool();

Expression* parse_primary(Lexer& lexer);
Expression* parse_expression(Lexer& lexer, Expression* lhs = nullptr, int min_prec = 0, bool allow_assignment = true);
Expression* parse_ident(Lexer& lexer, bool allow_assignment = true);
Expression* parse_keyword(Lexer& lexer);
void add_function_or_report_if_exit(FunctionExpr* expr);
void add_struct_or_report_if_exit(StructExpr* expr);
void add_string(BinaryExpr* expr);
DeclaredString* get_string(StrView name, usize* index = nullptr);
ValueType str_to_value_type(StrView type);
VirtualReg translate_to_instruction(Array<Instruction>& ops, Array<VirtualReg>& regs, Array<Variable>& local_vars, Expression* expr, ValueType& return_type);
StrView phys_name(s8 p);
const char* value_type_to_str(ValueType type);
const char* inst_type_to_str(InstructionType type);
bool is_numeric_type(ValueType type);
bool is_unsigned_type(ValueType type);
bool is_signed_type(ValueType type);
bool types_compatible(ValueType arg, ValueType param);
ValueType promote_type(ValueType a, ValueType b);
InstructionType operator_to_instruction(enum TokenType operation);
const char* tok_type_to_str(enum TokenType type);
const char* expr_type_to_str(ExpressionType type);
template <typename T> T* new_expr(Token tok, ExpressionType type);
CallExpr* new_call_expr(Token tok);
StructExpr* new_struct_expr(Token name);
MemberCallExpr* new_member_call(Token struct_tok, Token field);
BinaryExpr* new_binary_expr(Token tok, Expression* lhs = nullptr, Expression* rhs = nullptr);
PrintExpr* new_print_expr(Token tok, Expression* rhs = nullptr);
AssignmentExpr* new_assignment_expr(Token tok, Expression* rhs, bool is_local, Token oper, Token field_name = eof_token());
ReturnExpr* new_return_expr(Token tok, Expression* rhs);
DeferExpr* new_defer_expr(Token tok, Expression* rhs);
BreakExpr* new_break_expr(Token tok);
ContinueExpr* new_continue_expr(Token tok);
BlockExpr* new_block_expr(Token tok);
FunctionExpr* new_function_expr(Token tok, Array<Expression*>& args, Array<FunctionArgument>& arg_types, BlockExpr* block);
ForExpr* new_for_expr(Token tok, Expression* left_cond, Expression* right_cond, BlockExpr* block, bool is_inclusive);
IfExpr* new_if_expr(Token tok, Expression* condition, BlockExpr* if_block, BlockExpr* else_block);
IfExpr* parse_if(Token tok, Lexer& lexer);
BlockExpr* parse_else_tail(Lexer& lexer);
int is_operator(Token tok);
int prec(int index);
void add_field_or_report_if_exist(StructExpr* expr, Field field);
void check_assignment(Token variable, Token oper, bool is_already_exist, bool check_assignment = true);
void skip_semicolon_if_exist(Lexer& lexer);
Instruction allocate_label();
void set_label(Instruction label, Array<Instruction>& ops);
Variable* get_variable(Token variable, bool is_only_local, Array<Variable>* local_vars = nullptr);
Variable* get_and_add_variable(Token variable, bool is_local, ValueType type, usize reg_index, Array<Variable>* local_vars = nullptr);
Expression* parse_binary_expr(Lexer& lexer, bool allow_assignment = true);
BlockExpr* parse_block(Lexer& lexer);
Expression* parse_type(Token tok);
AssignmentExpr* parse_assignment(Token variable, bool is_local, Lexer& lexer, Token field_name = eof_token());
StructExpr* parse_struct(Lexer& lexer);
Expression* parse_struct_init(Lexer& lexer, Token struct_tok, StrView module_name = "");
FunctionExpr* parse_function(Lexer& lexer);
DeclaredString* get_or_add_string(StrView name, usize* index = nullptr);
bool parse(Lexer& lexer, Array<Expression*>& exprs);
VirtualReg& allocate_reg(Array<VirtualReg>& regs);
void update_all_offsets(Array<VirtualReg>& regs);
void compute_reg_uses(Array<Instruction>& ops, usize reg_count, Array<usize>& uses);
template <typename T> bool is_power_of_two(T n);
template <typename T> T get_power_of_two_exponent(T n);
static s64 pow2_mask(s64 k);
static void peephole_asm(StrBuilder& builder, usize start);
static void load_reg(StrBuilder& builder, const char* dst, const VirtualReg& reg);
static void store_reg(StrBuilder& builder, const VirtualReg& res, const char* src);
static void load_reg_if_needed(StrBuilder& builder, const char* dst, const VirtualReg& reg);
static void emit_truncate(StrBuilder& builder, ValueType type, u32 from_width = 0);
static void append_reg32(StrBuilder& builder, const char* dest);
static void emit_truncate_dest(StrBuilder& builder, const char* dest, ValueType type);
static void emit_rhs_operand(StrBuilder& builder, const VirtualReg& rhs);
static void emit_binary_op(StrBuilder& builder, const char* op, const VirtualReg& lhs, const VirtualReg& rhs, const VirtualReg& res);
static void emit_cmp(StrBuilder& builder, const VirtualReg& lhs, const VirtualReg& rhs);
static bool emit_lea_opt(StrBuilder& builder, int op, const VirtualReg& lhs, const VirtualReg& rhs, const VirtualReg& res);
bool shift_math_optimization(MathType type, StrBuilder& builder, VirtualReg& lhs, VirtualReg& rhs, VirtualReg& res);
bool compile_ops(StrBuilder& builder, Array<Instruction>& ops, Array<VirtualReg>& regs, bool has_frame = true);
bool compile_function(StrBuilder& builder, DeclaredFunction func);
void append_hex(StrBuilder& builder, StrView str, u32& append_nulls);
void dead_code(DeclaredFunction& func);
void reg_reads_and_writes(Instruction& inst, Array<bool>& gen, Array<bool>& kill);
void compute_liveness(DeclaredFunction& func, Array<bool>& live);
void dead_store_elim(DeclaredFunction& func);
void simplify_control_flow(DeclaredFunction& func);
static bool reg_is_touched(Instruction& inst, usize r);
void build_intervals(DeclaredFunction& func, Array<LiveInterval>& out);
void allocate_registers(DeclaredFunction& func);
void add_std_library(StrBuilder& builder, bool is_windows);
bool compile_program(Array<Instruction>& global_ops, Array<VirtualReg>& global_regs);
s64 truncate_value(s64 val, ValueType type);
bool cast_is_free(ValueType src, ValueType dst);
VirtualReg& make_const(Array<VirtualReg>& regs, ValueType type, s64 val);
void eval_binary(InstructionType instruction_type, ValueType type, VirtualReg& lhs, VirtualReg& rhs, VirtualReg& res);
bool parse_u64_literal(StrView text, u64& out);
void translate_function_body(DeclaredFunction& fun);
void print_help(const char* exe);

// ---- Globals and static tables ----

StrView out_path = SV_LIT("output");
StrView src_path = SV_LIT("");
const char* src_content = "";
bool g_run_compiled = true;
// Nesting depth of `for` loops while translating, used to cycle loop-counter
// register hints through the callee-saved set so nested counters stay apart.
static int g_for_depth = 0;

// Break/continue targets of the loops currently being translated. A stack so
// that a `break`/`continue` inside a nested loop jumps to the innermost one.
struct LoopContext {
    s64 break_label;    // g_labels index to jump to for `break`
    s64 continue_label; // g_labels index to jump to for `continue`
};
static Array<LoopContext> g_loop_stack;

// Every spilled register gets a full QWORD stack slot (all codegen moves a
// 64-bit word at once), so slot size is a constant regardless of the register's
// logical type.
static const u32 STACK_REGISTER_SIZE = 8;

// Object-like `#define NAME value` macro: its `name` is replaced textually
// by `value` on every later non-directive line (and inside included files).
// Names/values are heap-copied because the per-file source buffer is freed
// when preprocess_includes returns.
struct DefineMacro {
    StrView name;
    StrView value;
};

// Append `line` to `out`, replacing each macro name with its value. Text
// inside string literals is left untouched (like C); `depth` caps recursive
// expansion of values that themselves reference macros, so self-referencing
// definitions terminate instead of looping forever.
static void expand_macro_line(StrBuilder& out, const char* line, usize line_len,
                              Array<DefineMacro>& macros, int depth) {
    usize i = 0;
    bool in_string = false;
    while (i < line_len) {
        char c = line[i];
        if (in_string) {
            out.append(c);
            if (c == '\\' && i + 1 < line_len) {
                out.append(line[i + 1]);
                i += 2;
                continue;
            }
            if (c == '"') in_string = false;
            ++i;
            continue;
        }
        if (c == '"') {
            in_string = true;
            out.append(c);
            ++i;
            continue;
        }
        if (is_word(c) || is_digit(c)) {
            usize start = i;
            while (i < line_len && (is_word(line[i]) || is_digit(line[i]))) ++i;
            StrView ident(line + start, i - start);
            bool expanded = false;
            for (auto& m : macros) {
                if (m.name.size == ident.size && memcmp(m.name.data, ident.data, ident.size) == 0) {
                    if (depth < 16)
                        expand_macro_line(out, m.value.data, m.value.size, macros, depth + 1);
                    else
                        out.append(m.value.data, m.value.size);
                    expanded = true;
                    break;
                }
            }
            if (!expanded)
                out.append(ident.data, ident.size);
            continue;
        }
        out.append(c);
        ++i;
    }
}

// Expand `#include "path"` directives into the included files' literal
// contents. This is the very first compilation step, running before any
// lexing, parsing or optimization, so a directive expands textually exactly
// where it appears. Include paths resolve relative to the including file's
// directory (absolute paths are used as-is); an include cycle is an error.
// `chain` tracks the files currently being expanded, so a file included
// directly or indirectly from itself is reported rather than recursing
// forever. `macros` collects `#define NAME value` substitutions shared across
// all included files.
static bool preprocess_includes(StrBuilder& out, const char* file_path,
                                Array<StrView>& chain, Array<DefineMacro>& macros) {
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        log_error("Failed to open source file: '%s'\n", file_path);
        return false;
    }
    fseek(file, 0, SEEK_END);
    long n = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* src = new char[n + 1];
    fread(src, 1, n, file);
    fclose(file);
    src[n] = 0;

    const usize path_len = (usize)strlen(file_path);
    for (auto& p : chain)
        if (p.size == path_len && memcmp(p.data, file_path, path_len) == 0) {
            log_error("Include cycle detected for file: '%s'\n", file_path);
            delete[] src;
            return false;
        }
    chain.push(StrView(file_path));
    defer(chain.pop());

    // Directory prefix of this file ("" when it has no directory), used to
    // resolve relative includes.
    const char* slash = strrchr(file_path, '/');
    const char* backslash = strrchr(file_path, '\\');
    const char* sep = (backslash && (!slash || backslash > slash)) ? backslash : slash;
    const usize dir_len = sep ? (usize)(sep - file_path + 1) : 0;

    usize pos = 0;
    while (pos < (usize)n) {
        usize line_start = pos;
        usize line_end = pos;
        while (line_end < (usize)n && src[line_end] != '\n') ++line_end;

        usize l = line_start;
        while (l < line_end && (src[l] == ' ' || src[l] == '\t')) ++l;
        const char* line = src + l;
        const usize tlen = line_end - l;

        if (tlen >= 8 && memcmp(line, "#include", 8) == 0) {
            usize p = 8;
            while (p < tlen && (line[p] == ' ' || line[p] == '\t')) ++p;
            if (p >= tlen || line[p] != '"') {
                log_error("Malformed #include directive in file '%s': expected #include \"path\"\n", file_path);
                delete[] src;
                return false;
            }
            ++p;
            usize q = p;
            while (q < tlen && line[q] != '"') ++q;
            if (q >= tlen || q + 1 != tlen) {
                log_error("Malformed #include directive in file '%s': expected #include \"path\"\n", file_path);
                delete[] src;
                return false;
            }
            StrView inc_path(line + p, q - p);
            if (inc_path.size == 0) {
                log_error("Malformed #include directive in file '%s': empty path\n", file_path);
                delete[] src;
                return false;
            }

            // Absolute paths are used as-is; relative ones join the including
            // file's directory (a bare drive letter counts as absolute).
            const bool absolute = inc_path.data[0] == '/' || inc_path.data[0] == '\\'
                || (inc_path.size >= 3 && inc_path.data[1] == ':'
                    && (inc_path.data[2] == '/' || inc_path.data[2] == '\\'));
            StrBuilder resolved{};
            if (!absolute && dir_len > 0)
                resolved.append(file_path, dir_len);
            resolved.append(inc_path).append_null(false);
            StrView resolved_view = resolved.to_string_view(true);

            if (!preprocess_includes(out, resolved_view.data, chain, macros)) {
                resolved.cleanup();
                delete[] src;
                return false;
            }
            resolved.cleanup();
        } else if (tlen >= 7 && memcmp(line, "#define", 7) == 0) {
            usize p = 7;
            while (p < tlen && (line[p] == ' ' || line[p] == '\t')) ++p;
            if (p >= tlen || !is_word(line[p])) {
                log_error("Malformed #define directive in file '%s': expected #define NAME value\n", file_path);
                delete[] src;
                return false;
            }
            usize q = p;
            while (q < tlen && (is_word(line[q]) || is_digit(line[q]))) ++q;
            StrView name(line + p, q - p);
            if (q < tlen && line[q] == '(') {
                log_error("Malformed #define directive in file '%s': function-like macros are not supported\n", file_path);
                delete[] src;
                return false;
            }
            usize v = q;
            while (v < tlen && (line[v] == ' ' || line[v] == '\t')) ++v;
            usize vend = tlen;
            while (vend > v && (line[vend - 1] == ' ' || line[vend - 1] == '\t')) --vend;
            StrView value(line + v, vend - v);
            // Heap-copy the name/value: `src` is freed below, but macros live
            // on and are shared with every other included file.
            char* name_copy = new char[name.size];
            memcpy(name_copy, name.data, name.size);
            char* value_copy = new char[value.size];
            memcpy(value_copy, value.data, value.size);
            macros.push(DefineMacro{StrView(name_copy, name.size), StrView(value_copy, value.size)});
        } else {
            expand_macro_line(out, src + line_start, line_end - line_start, macros, 0);
            if (line_end < (usize)n) out.append('\n');
        }
        pos = (line_end < (usize)n) ? line_end + 1 : line_end;
    }

    delete[] src;
    return true;
}

// Target platform override, set from the command line (--platform=Windows).
// UNKNOWN means "compile for the system the compiler is running on".
FlagsSystem g_target_platform = FlagsSystem::UNKNOWN;

// Effective platform the generated code targets: the --platform override when
// given, otherwise the system running the compiler.
static FlagsSystem target_platform() {
    if (g_target_platform != FlagsSystem::UNKNOWN) return g_target_platform;
    return get_system();
}

// Whether the generated assembly targets Windows (Microsoft x64 calling
// convention). A function (not a constant) so the override is honored when the
// compiler is embedded and re-targeted per call.
static bool is_windows_target() {
    return target_platform() == FlagsSystem::WINDOWS;
}

// Case-insensitive platform name match ("windows" == "WINDOWS" == "Windows").
bool platform_name_matches(StrView name, const char* canonical) {
    while (name.size > 0 && *canonical) {
        char a = name.data[0];
        char b = *canonical;
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return false;
        name.data++;
        name.size--;
        canonical++;
    }
    return name.size == 0 && *canonical == 0;
}

// Parse a --platform=<name> value into a FlagsSystem, or FlagsSystem::UNKNOWN
// when the name does not match any known platform.
FlagsSystem parse_platform(StrView value) {
    for (int s = 0; s < (int)FlagsSystem::EnumSize; ++s) {
        FlagsSystem sys = (FlagsSystem)s;
        if (platform_name_matches(value, get_system_name(sys))) return sys;
    }
    return FlagsSystem::UNKNOWN;
}

// rsi/rdi are caller-saved on SysV but callee-saved (nonvolatile) on Windows,
// so the allocator may park call-crossing values in them and functions must
// push/pop them. r8-r11 are always caller-saved, r12-r15 always callee-saved.
static bool phys_callee_saved(int p) {
    if (is_windows_target())
        return p == PR_RSI || p == PR_RDI || p == PR_R12 || p == PR_R13 || p == PR_R14 || p == PR_R15;
    return p == PR_R12 || p == PR_R13 || p == PR_R14 || p == PR_R15;
}

// Function names the user may not redefine: the reserved entry point and
// remaining built-ins are real ident tokens that would otherwise be
// shadowable; the language keywords can never reach here (parse_function
// requires Tok_Ident) but are listed as defense in depth. The reflection
// builtins need no reservation: they lex as `#`-directives now.
static StrView forbidden_function_names[] = {
    "__entry",
    "offset_of", "align_of",
    "true", "false", "print",
    "i8", "i16", "i32", "i64",
    "u8", "u16", "u32", "u64",
    "bool", "string", "str", "void",
    "return", "fn", "if", "else", "for", "struct", "as", "defer",
    "null",
};

static Keyword keyword_array[] = {
    {"true"},
    {"false"},
    {"print"},
    {"null"},
    {"i8"},
    {"i16"},
    {"i32"},
    {"i64"},
    {"u8"},
    {"u16"},
    {"u32"},
    {"u64"},
    {"bool"},
    {"string"},
    {"str"},
    {"void"},
    {"extern"},
};

Operator operators_array[] = {
    {"(", Tok_LParen},
    {")", Tok_RParen},
    {"=", Tok_Equals},
    {":", Tok_DColon},
    {";", Tok_SemiColon},
    {",", Tok_Comma},
    {".", Tok_Dot},
    {"{", Tok_LBracket},
    {"}", Tok_RBracket},
    {"as", Tok_Cast,    6},
    {"<",  Tok_LArrow,  3},
    {">",  Tok_RArrow,  3},
    {"<=", Tok_LEquals, 3},
    {">=", Tok_GEquals, 3},
    {"==", Tok_DEquals, 3},
    {"!",  Tok_Not},
    {"!=", Tok_NotEquals, 3},
    {"+",  Tok_Plus,    4},
    {"-",  Tok_Minus,   4},
    {"*",  Tok_Mult,    5},
    {"/",  Tok_Div,     5},
    {"%",  Tok_Mod,     5},
    {"&",  Tok_Ampersand},
    {"&&", Tok_AndAnd,  2},
    {"||", Tok_OrOr,    1},
};

// Integer argument registers for the platform calling convention. SysV (Linux)
// passes the first six arguments in rdi/rsi/rdx/rcx/r8/r9; Microsoft x64
// (Windows) passes the first four in rcx/rdx/r8/r9. Further arguments go on
// the stack (Windows reserves a 32-byte shadow space for the callee first).
// Shared by the call-site loader and the callee's argument-copy prologue.
static const char* const* arg_regs() {
    static const char* const sysv[6] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    static const char* const msvc[6] = {"rcx", "rdx", "r8", "r9", "", ""};
    return is_windows_target() ? msvc : sysv;
}

static int arg_reg_count() {
    return is_windows_target() ? 4 : 6;
}

// PhysReg pool slot for a register-based call argument, or PR_NONE when the
// argument register (rcx/rdx) is not part of the allocator pool.
static s8 arg_phys(int i) {
    const char* name = arg_regs()[i];
    if (strcmp(name, "rdi") == 0) return PR_RDI;
    if (strcmp(name, "rsi") == 0) return PR_RSI;
    if (strcmp(name, "r8") == 0) return PR_R8;
    if (strcmp(name, "r9") == 0) return PR_R9;
    return PR_NONE;
}

// Bytes of caller-reserved scratch the callee may use for its register
// arguments (Microsoft x64 requires 32; SysV has none).
static int shadow_space_bytes() {
    return is_windows_target() ? 32 : 0;
}

// Incoming argument register as an allocator-pool id, or -1 when it is not part
// of the pool (rdx/rcx never are on SysV, rcx/rdx never on Windows, so they can
// never be a destination and never take part in a move cycle). Only used by the
// prologue argument copy.
static const s8* arg_reg_pool_id() {
    static const s8 sysv[6] = {PR_RDI, PR_RSI, -1, -1, PR_R8, PR_R9};
    static const s8 msvc[6] = {-1, -1, PR_R8, PR_R9, -1, -1};
    return is_windows_target() ? msvc : sysv;
}

Array<DeclaredFunction> g_functions;
Array<DeclaredStruct> g_structs;
Array<DeclaredString> g_strings;
Array<Variable> g_vars;
Array<Label> g_labels;
// Running byte size of the __globals region, used to assign each global
// variable its fixed slot offset (one full-width QWORD slot per variable).
usize g_globals_size = 0;

// ---- Compile-time execution (`#run`) ----
// Shared libraries preloaded (dlopen, RTLD_GLOBAL) before a `#run` block
// executes, so the block's extern calls resolve against them: `#libc` adds
// the platform's libc soname, `#lib("path")` adds an explicit library.
Array<StrView> g_comptime_libs;
// Counter distinguishing successive `#run` blocks' synthetic functions and
// scratch files.
usize g_comptime_counter = 0;

// Resolved addresses of extern (C FFI) symbols for compile-time execution.
// The scratch shared object cannot carry PC32 relocations against undefined
// symbols (`ld -shared` rejects them), so `#run` resolves every extern via
// dlsym(RTLD_DEFAULT) *inside the compiler* and emits direct
// `mov rax, <address>; call rax` sequences instead of symbolic calls.
struct ComptimeExternAddress
{
    StrView name;
    void* address = nullptr;
};
Array<ComptimeExternAddress> g_comptime_externs;
// True while asm for a `#run` scratch library is being emitted, so the call
// emitter routes extern calls through the resolved-address table above.
bool g_comptime_emitting = false;

// ---- Module system ----
// Registry of every module declared by the compiled files (index 0/name "" is
// the implicit *global* module every file belongs to unless it declares a
// `module` line). `Module.imports` is filled as `import` statements parse.
Array<Module> g_modules;
// The module of the file whose expressions are currently being parsed or
// translated. Symbols resolve against this: same module, the global module, or
// an imported module are visible; anything else is not.
static StrView g_current_module_name = "";

static Module* find_module(StrView name)
{
    for (auto& m : g_modules)
        if (m.name == name) return &m;
    return nullptr;
}

static Module* find_or_add_module(StrView name)
{
    if (auto* m = find_module(name)) return m;
    g_modules.push(Module{name});
    return &g_modules.last();
}

// Is a symbol declared in `symbol_module` visible from the module currently
// being compiled? The global module ("") is visible everywhere.
static bool module_symbol_visible(StrView symbol_module)
{
    if (symbol_module == g_current_module_name) return true;
    if (symbol_module == "") return true;
    for (auto& m : g_modules)
        if (m.name == g_current_module_name)
            for (auto& imp : m.imports)
                if (imp == symbol_module) return true;
    return false;
}

// Resolve a struct name. `module_name` is non-empty only for the qualified
// form `mod::Foo`, which must match exactly and requires the module to be
// visible. The unqualified form collects every visible declaration with that
// name: 0 matches returns INVALID_INDEX, more than one is an ambiguous
// reference error.
static usize find_visible_struct(StrView name, StrView module_name)
{
    if (module_name.size > 0) {
        if (!module_symbol_visible(module_name))
            compiler_error(Token{Tok_Ident, name},
                "Module '" SV_FORMAT "' is not visible from module '" SV_FORMAT "': declare or import it before use\n",
                SV_ARG(module_name), SV_ARG(g_current_module_name));
        for (usize i = 0; i < g_structs.count(); ++i)
            if (g_structs[i].name == name && g_structs[i].module_name == module_name)
                return i;
        return Array<DeclaredStruct>::INVALID_INDEX;
    }
    usize found = Array<DeclaredStruct>::INVALID_INDEX;
    usize count = 0;
    for (usize i = 0; i < g_structs.count(); ++i) {
        if (g_structs[i].name != name) continue;
        if (!module_symbol_visible(g_structs[i].module_name)) continue;
        found = i;
        ++count;
    }
    if (count > 1)
        compiler_error(Token{Tok_Ident, name},
            "Ambiguous reference to struct '" SV_FORMAT "': declared in multiple visible modules, use 'module::struct_name' to qualify it\n",
            SV_ARG(name));
    return count == 1 ? found : Array<DeclaredStruct>::INVALID_INDEX;
}

// Resolve a function call target the same way find_visible_struct resolves
// structs (see above).
static usize find_visible_function(Token tok, StrView module_name)
{
    if (module_name.size > 0) {
        if (!module_symbol_visible(module_name))
            compiler_error(tok,
                "Module '" SV_FORMAT "' is not visible from module '" SV_FORMAT "': declare or import it before use\n",
                SV_ARG(module_name), SV_ARG(g_current_module_name));
        for (usize i = 0; i < g_functions.count(); ++i)
            if (g_functions[i].name == tok.val && g_functions[i].module_name == module_name)
                return i;
        return Array<DeclaredFunction>::INVALID_INDEX;
    }
    usize found = Array<DeclaredFunction>::INVALID_INDEX;
    usize count = 0;
    for (usize i = 0; i < g_functions.count(); ++i) {
        if (g_functions[i].name != tok.val) continue;
        if (!module_symbol_visible(g_functions[i].module_name)) continue;
        found = i;
        ++count;
    }
    if (count > 1)
        compiler_error(tok,
            "Ambiguous reference to function '" SV_FORMAT "': declared in multiple visible modules, use 'module::name()' to qualify it\n",
            SV_ARG(tok.val));
    return count == 1 ? found : Array<DeclaredFunction>::INVALID_INDEX;
}

// Find a global variable. Qualified (`module_name` non-empty) requires an
// exact module match and that the module be visible. The unqualified form
// collects every visible declaration with that name: 0 matches returns
// nullptr, more than one is an ambiguous reference error.
static Variable* find_global_variable(StrView name, StrView module_name)
{
    if (module_name.size > 0) {
        if (!module_symbol_visible(module_name)) return nullptr;
        for (auto& var : g_vars)
            if (var.name == name && var.module_name == module_name)
                return &var;
        return nullptr;
    }
    Variable* found = nullptr;
    usize count = 0;
    for (auto& var : g_vars) {
        if (var.name != name) continue;
        if (!module_symbol_visible(var.module_name)) continue;
        found = &var;
        ++count;
    }
    if (count > 1)
        compiler_error(Token{Tok_Ident, name},
            "Ambiguous reference to variable '" SV_FORMAT "': declared in multiple visible modules, use 'module::name' to qualify it\n",
            SV_ARG(name));
    return count == 1 ? found : nullptr;
}

// Find a function by exact name regardless of module, used for __entry's call
// to `main`. Errors when multiple modules declare a function with the same
// name, since the entry point must be unambiguous.
static usize find_function_any_module(StrView name)
{
    usize found = Array<DeclaredFunction>::INVALID_INDEX;
    usize count = 0;
    for (usize i = 0; i < g_functions.count(); ++i) {
        if (g_functions[i].name != name) continue;
        found = i;
        ++count;
    }
    if (count > 1)
        compiler_error(Token{Tok_Ident, name},
            "Ambiguous entry point: multiple functions named '" SV_FORMAT "' declared in different modules\n",
            SV_ARG(name));
    return count == 1 ? found : Array<DeclaredFunction>::INVALID_INDEX;
}

static const char hex_chars[] = "0123456789ABCDEF";

// Master switch: M1 keeps this off so codegen output is byte-identical while
// the scan and interval math prove out; load_reg/store_reg read `phys` once it
// is enabled.
static bool g_register_allocation = true;

// Function bodies currently being translated, used to break call cycles
// during lazy translation (e.g. `main` calling a function declared after it).
static Array<StrView> g_translating_functions;

// Return type inferred so far for the function body being translated.
// Recursive calls read this, so `return fib(n - 1) + fib(n - 2)` sees the
// type established by earlier `return` statements instead of NOP.
static ValueType g_live_function_return_type = TYPE_NOP;

// Struct-return translation context. A struct-returning function receives its
// caller-reserved return slot address as a hidden parameter; g_function_hidden_slot
// remembers that parameter's register while the body is translated. While the
// `return` value of such a function is being translated (g_return_slot_reg set),
// struct literals are materialized in the return slot instead of the local
// struct area: the returned data must survive the function, and only the
// caller's frame does. g_return_area_off tracks how much of the return slot a
// single `return` consumes (top-level struct first, then any nested struct
// literals); g_return_area_max remembers the largest amount any `return` used
// so the caller reserves enough.
static usize g_function_hidden_slot = (usize)-1;
static usize g_return_slot_reg = (usize)-1;
static usize g_return_area_off = 0;
static usize g_return_area_max = 0;

// A registered `defer` call. The call's arguments are translated (evaluated)
// inline when the `defer` statement executes, but the OP_CALL itself is popped
// off the stream and re-emitted right before every `return`. flag_reg holds a
// bool: 1 if this `defer` statement actually ran (so conditional defers skip
// the call when their branch wasn't taken), 0-initialized at function entry.
struct DeferredCall
{
    Instruction call;
    usize flag_reg;
};
static Array<DeferredCall> g_deferred_calls;

// ---- Functions ----

StrView phys_name(s8 p) {
    static StrView names[PR_COUNT] = {"rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"};
    return (p >= 0 && p < PR_COUNT) ? names[p] : "?";
}

// The sub-register spelling of a 64-bit register for a sized member access:
// `rax` -> `eax`/`ax`/`al`, `r8` -> `r8d`/`r8w`/`r8b`, `rsi` -> `esi`/`si`/`sil`.
static const char* reg_subname(const char* name, u32 width) {
    static char buf[8];
    if (width >= 8) return name;
    if (name[0] == 'r' && name[1] >= '0' && name[1] <= '9') {
        usize i = 1;
        buf[0] = 'r';
        while (name[i] >= '0' && name[i] <= '9') {
            buf[i] = name[i];
            ++i;
        }
        buf[i++] = width == 1 ? 'b' : (width == 2 ? 'w' : 'd');
        buf[i] = 0;
        return buf;
    }
    if (width == 1) {
        if (!strcmp(name, "rsi")) return "sil";
        if (!strcmp(name, "rdi")) return "dil";
        buf[0] = name[1];
        buf[1] = 'l';
        buf[2] = 0;
        return buf;
    }
    if (width == 2) {
        if (!strcmp(name, "rsi")) return "si";
        if (!strcmp(name, "rdi")) return "di";
        buf[0] = name[1];
        buf[1] = 0;
        return buf;
    }
    if (!strcmp(name, "rsi")) return "esi";
    if (!strcmp(name, "rdi")) return "edi";
    buf[0] = 'e';
    buf[1] = name[1];
    buf[2] = 0;
    return buf;
}

// Memory-operand size suffix: `byte`/`word`/`dword`/`qword`.
static const char* mem_width_suffix(u32 width) {
    switch (width) {
        case 1: return "byte";
        case 2: return "word";
        case 4: return "dword";
        default: return "qword";
    }
}

// Emit a sized struct-member load into the full-width destination `dst` from
// the composed memory operand `mem`. Sub-32-bit fields zero/sign-extend per
// their declared type; 32-bit fields zero-extend via the dword write or
// sign-extend with movsxd; 8-byte fields are a plain mov.
static void emit_width_load(StrBuilder& builder, const char* dst, StrView mem, u32 width, ValueType type) {
    if (width >= 8) {
        (builder.append("\tmov ") << dst).append(", ").append(mem) << '\n';
        return;
    }
    if (width == 4) {
        if (type == TYPE_I32)
            (builder.append("\tmovsxd ") << dst).append(", dword ").append(mem) << '\n';
        else
            (builder.append("\tmov ") << reg_subname(dst, 4)).append(", dword ").append(mem) << '\n';
        return;
    }
    const bool sx = type == TYPE_I8 || type == TYPE_I16;
    (builder.append(sx ? "\tmovsx " : "\tmovzx ") << dst).append(width == 2 ? ", word " : ", byte ").append(mem) << '\n';
}

const char* value_type_to_str(ValueType type) {
    switch(type) {
        case TYPE_NOP:    return "nop";
        case TYPE_VOID:   return "void";
        case TYPE_I8:     return "i8";
        case TYPE_I16:    return "i16";
        case TYPE_I32:    return "i32";
        case TYPE_I64:    return "i64";
        case TYPE_U8:     return "u8";
        case TYPE_U16:    return "u16";
        case TYPE_U32:    return "u32";
        case TYPE_U64:    return "u64";
        case TYPE_PTR:    return "Ptr";
        case TYPE_STR:    return "string";
        case TYPE_BOOL:   return "bool";
        case TYPE_STRUCT: return "struct";
        case TYPE_ARRAY:  return "array";
        default: UNREACHABLE("value_type_to_str");
    }
    return "";
}

const char* inst_type_to_str(InstructionType type) {
    switch(type) {
        case OP_NOP: return "NOP";
        case OP_PUSH_I8: return "PUSH_I8";
        case OP_PUSH_I16: return "PUSH_I16";
        case OP_PUSH_I32: return "PUSH_I32";
        case OP_PUSH_I64: return "PUSH_I64";
        case OP_PUSH_PTR: return "PUSH_PTR";
        case OP_PUSH_STR: return "PUSH_STR";
        case OP_PUSH_BOOL: return "PUSH_BOOL";
        case OP_PLUS: return "PLUS";
        case OP_MINUS: return "MINUS";
        case OP_MULT: return "MULT";
        case OP_DIVIDE: return "DIVIDE";
        case OP_MOD: return "MOD";
        case OP_DROP: return "DROP";
        case OP_DUP: return "DUP";
        case OP_INC: return "INC";
        case OP_PRINT: return "PRINT";
        case OP_EQUALS: return "EQUALS";
        case OP_NOT_EQUALS: return "NOT_EQUALS";
        case OP_LESS: return "LESS";
        case OP_LESS_EQUALS: return "LESS_EQUALS";
        case OP_CAST: return "CAST";
        case OP_GREATER: return "GREATER";
        case OP_GREATER_EQUALS: return "GREATER_EQUALS";
        case OP_NOT: return "NOT";
        case OP_JMP: return "JMP";
        case OP_JMP_IF: return "JMP_IF";
        case OP_LABEL: return "LABEL";
        case OP_STORE: return "STORE";
        case OP_CALL: return "CALL";
        case OP_RET: return "RET";
        case OP_LEA: return "LEA";
        case OP_LOAD_PTR: return "LOAD_PTR";
        case OP_STORE_PTR: return "STORE_PTR";
        case OP_ALLOC: return "ALLOC";
        case OP_ENTRY_ARGC: return "ENTRY_ARGC";
        case OP_ENTRY_ARGV: return "ENTRY_ARGV";

        default: UNREACHABLE("inst_type_to_str");
    }
    return "";
}

bool is_numeric_type(ValueType type)
{
    return type >= TYPE_I8 && type <= TYPE_U64;
}

bool is_unsigned_type(ValueType type)
{
    // Pointers count as unsigned: relational address comparisons order by raw
    // 64-bit value, so `&x < &y` emits the unsigned `jb` rather than `jl`.
    return (type >= TYPE_U8 && type <= TYPE_U64) || type == TYPE_PTR;
}

bool is_signed_type(ValueType type)
{
    return is_numeric_type(type) && !is_unsigned_type(type);
}

// Can a value of type `arg` be passed to a parameter declared `param`?
// Untyped parameters accept anything; string parameters accept only strings;
// bool parameters accept only bools; integer parameters accept only integers.
// There are no implicit conversions between bool and int.
bool types_compatible(ValueType arg, ValueType param)
{
    if (param == TYPE_NOP) return true;
    if (arg == TYPE_VOID || arg == TYPE_NOP) return false;
    if (param == TYPE_PTR) return arg == TYPE_PTR;
    if (arg == TYPE_PTR) return false;
    if (arg == TYPE_STR) return param == TYPE_STR;
    if (param == TYPE_STR) return false;
    if (arg == TYPE_BOOL) return param == TYPE_BOOL;
    if (param == TYPE_BOOL) return false;
    if (arg == TYPE_STRUCT || param == TYPE_STRUCT)
        // The exact struct identity is checked at the call site where the
        // struct name is known; here just match the category.
        return arg == param;
    return is_numeric_type(arg) && is_numeric_type(param);
}


// Integer promotion: sub-32-bit operands promote to i32 (C-like), then among
// {i32, u32, i64, u64} the wider type wins and unsigned breaks ties. So
// `u8 * x` is i32 arithmetic, `u32 + i64` is i64, and `u64 op i64` is u64.
ValueType promote_type(ValueType a, ValueType b)
{
    auto promote_to_int = [](ValueType t) -> ValueType {
        switch (t) {
            case TYPE_I8: case TYPE_U8:
            case TYPE_I16: case TYPE_U16:
                return TYPE_I32;
            default:
                return t;
        }
    };
    a = promote_to_int(a);
    b = promote_to_int(b);
    if (a == b) return a;
    if (a == TYPE_U64 || b == TYPE_U64) return TYPE_U64;
    if (a == TYPE_I64 || b == TYPE_I64) return TYPE_I64;
    if (a == TYPE_U32 || b == TYPE_U32) return TYPE_U32;
    return TYPE_I32;
}

InstructionType operator_to_instruction(enum TokenType operation)
{
    switch (operation) {
        case Tok_Plus: return OP_PLUS;
        case Tok_Minus: return OP_MINUS;
        case Tok_Mult: return OP_MULT;
        case Tok_Div: return OP_DIVIDE;
        case Tok_Mod: return OP_MOD;
        case Tok_DEquals: return OP_EQUALS;
        case Tok_NotEquals: return OP_NOT_EQUALS;
        case Tok_LArrow: return OP_LESS;
        case Tok_RArrow: return OP_GREATER;
        case Tok_LEquals: return OP_LESS_EQUALS;
        case Tok_GEquals: return OP_GREATER_EQUALS;
        case Tok_Cast: return OP_CAST;
        default: return OP_NOP;
    }
}




const char* tok_type_to_str(enum TokenType type) {
    switch(type) {
        case Tok_Ident: return "Ident";
        case Tok_IntLit: return "IntLit";
        case Tok_FloatLit: return "FloatLit";
        case Tok_StrLit: return "StrLit";
        case Tok_BoolLit: return "BoolLit";
        case Tok_NullLit: return "NullLit";
        case Tok_Keyword: return "Keyword";
        case Tok_Return: return "Return";
        case Tok_Defer: return "Defer";
        case Tok_Function: return "Function";
        case Tok_Module: return "Module";
        case Tok_ImportModule: return "Import";
        case Tok_Struct: return "Struct";
        case Tok_Cast: return "Cast";
        case Tok_Type: return "Type";
        case Tok_If: return "If";
        case Tok_Else: return "Else";
        case Tok_Elif: return "Elif";
        case Tok_For: return "For";
        case Tok_Break: return "Break";
        case Tok_Continue: return "Continue";
        case Tok_Oper: return "Oper";
        case Tok_LParen: return "LParen";
        case Tok_RParen: return "RParen";
        case Tok_LArrow: return "LArrow";
        case Tok_RArrow: return "RArrow";
        case Tok_Arrow: return "Arrow";
        case Tok_Equals: return "Equals";
        case Tok_DEquals: return "DEquals";
        case Tok_NotEquals: return "NotEquals";
        case Tok_Not: return "Not";
        case Tok_SemiColon: return "SemiColon";
        case Tok_DColon: return "DColon";
        case Tok_ColonEquals: return "ColonEquals";
        case Tok_DDot: return "DDot";
        case Tok_DDotInclusive: return "DDotIncl";
        case Tok_Comma: return "Comma";
        case Tok_Dot: return "Dot";
        case Tok_LBracket: return "LBracket";
        case Tok_RBracket: return "RBracket";
        case Tok_LSquare: return "LSquare";
        case Tok_RSquare: return "RSquare";
        case Tok_Plus: return "Plus";
        case Tok_Minus: return "Minus";
        case Tok_Mult: return "Mult";
        case Tok_Caret: return "Caret";
        case Tok_Div: return "Div";
        case Tok_Mod: return "Mod";
        case Tok_Ampersand: return "Ampersand";
        case Tok_Directive: return "Directive";

        case Tok_Eof: return "Eof";

        default: UNREACHABLE("tok_type_to_str");
    }
}

const char* expr_type_to_str(ExpressionType type) {
    switch(type) {
        case Expr_Unknown: return "Unknown";
        case Expr_Binary: return "Binary";
        case Expr_Print: return "Print";
        case Expr_Assignment: return "Assignment";
        case Expr_Function: return "Function";
        case Expr_Module: return "Module";
        case Expr_ImportModule: return "ImportModule";
        case Expr_Block: return "Block";
        case Expr_Call: return "Call";
        case Expr_For: return "For";
        case Expr_Return: return "Return";
        case Expr_Struct: return "Struct";
        case Expr_StructInit: return "StructInit";
        case Expr_MemberCall: return "MemberCall";
        case Expr_If: return "If";
        case Expr_AddressOf: return "AddressOf";
        case Expr_Deref: return "Deref";
        case Expr_DerefAssign: return "DerefAssign";
        case Expr_Not: return "Not";
        case Expr_Break: return "Break";
        case Expr_Continue: return "Continue";
        case Expr_Defer: return "Defer";
        case Expr_ArrayLit: return "ArrayLit";
        case Expr_Index: return "Index";
        case Expr_IndexAssign: return "IndexAssign";
        case Expr_Assert: return "Assert";
        case Expr_Run: return "Run";
        case Expr_ComptimeLib: return "ComptimeLib";

        default: UNREACHABLE("expr_type_to_str");
    }
}


template <typename T>
T* new_expr(Token tok, ExpressionType type)
{
    auto* expr = new T();
    expr->tok = tok;
    expr->type = type;
    return expr;
}

CallExpr* new_call_expr(Token tok) { return new_expr<CallExpr>(tok, Expr_Call); }

StructExpr* new_struct_expr(Token name) { return new_expr<StructExpr>(name, Expr_Struct); }

StructInitExpr* new_struct_init_expr(Token name) { return new_expr<StructInitExpr>(name, Expr_StructInit); }

MemberCallExpr* new_member_call(Token struct_tok, Token field)
{
    auto* expr = new_expr<MemberCallExpr>(struct_tok, Expr_MemberCall);
    expr->field = field;
    return expr;
}

BinaryExpr* new_binary_expr(Token tok, Expression* lhs, Expression* rhs)
{
    auto* expr = new_expr<BinaryExpr>(tok, Expr_Binary);
    expr->lhs = lhs;
    expr->rhs = rhs;
    return expr;
}

ModuleExpr* new_module_expr(Token tok)
{
    auto* expr = new_expr<ModuleExpr>(tok, Expr_Module);
    return expr;
}

ModuleImportExpr* new_module_import_expr(Token tok, StrView short_name)
{
    auto* expr = new_expr<ModuleImportExpr>(tok, Expr_ImportModule);
    expr->short_name = short_name;
    return expr;
}

PrintExpr* new_print_expr(Token tok, Expression* rhs)
{
    auto* expr = new_expr<PrintExpr>(tok, Expr_Print);
    expr->rhs = rhs;
    return expr;
}

AssignmentExpr* new_assignment_expr(Token tok, Expression* rhs, bool is_local, Token oper, Token field_name)
{
    auto* expr = new_expr<AssignmentExpr>(tok, Expr_Assignment);
    expr->oper = oper;
    expr->field_name = field_name;
    expr->rhs = rhs;
    expr->is_local = is_local;
    return expr;
}

ReturnExpr* new_return_expr(Token tok, Expression* rhs)
{
    auto* expr = new_expr<ReturnExpr>(tok, Expr_Return);
    expr->rhs = rhs;
    return expr;
}

DeferExpr* new_defer_expr(Token tok, Expression* rhs)
{
    auto* expr = new_expr<DeferExpr>(tok, Expr_Defer);
    expr->expr = rhs;
    return expr;
}

BreakExpr* new_break_expr(Token tok)
{
    return new_expr<BreakExpr>(tok, Expr_Break);
}

ContinueExpr* new_continue_expr(Token tok)
{
    return new_expr<ContinueExpr>(tok, Expr_Continue);
}

BlockExpr* new_block_expr(Token tok) { return new_expr<BlockExpr>(tok, Expr_Block); }

// The `__` prefix is reserved for compiler-internal symbols (entry point,
// exit stub, print helpers and the labels the FASM codegen emits for user
// functions). No user-defined name (function, variable, parameter, struct
// name or struct field) may use it.
static void check_reserved_prefix(Token tok, const char* what)
{
    if (tok.val.starts_with("__"))
        compiler_error(tok, "%s '" SV_FORMAT "' starts with the reserved '__' prefix and cannot be defined\n", what, SV_ARG(tok.val));
}

FunctionExpr* new_function_expr(Token tok, Array<Expression*>& args, Array<FunctionArgument>& arg_types, BlockExpr* block)
{
    for (auto name : forbidden_function_names) {
        if (tok.val == name) {
            compiler_error(tok, "Function name '" SV_FORMAT "' is reserved and cannot be defined\n", SV_ARG(tok.val));
        }
    }
    check_reserved_prefix(tok, "Function name");
    auto* expr = new_expr<FunctionExpr>(tok, Expr_Function);
    expr->args.set_data(args.data());
    expr->args.set_capacity(args.capacity());
    expr->args.set_count(args.count());
    expr->arg_types.set_data(arg_types.data());
    expr->arg_types.set_capacity(arg_types.capacity());
    expr->arg_types.set_count(arg_types.count());
    expr->block = block;
    return expr;
}

ForExpr* new_for_expr(Token tok, Expression* left_cond, Expression* right_cond, BlockExpr* block, bool is_inclusive)
{
    auto* expr = new_expr<ForExpr>(tok, Expr_For);
    expr->left_cond = left_cond;
    expr->right_cond = right_cond;
    expr->block = block;
    expr->is_inclusive = is_inclusive;
    return expr;
}

IfExpr* new_if_expr(Token tok, Expression* condition, BlockExpr* if_block, BlockExpr* else_block)
{
    auto* expr = new_expr<IfExpr>(tok, Expr_If);
    expr->condition = condition;
    expr->if_block = if_block;
    expr->else_block = else_block;
    return expr;
}

Token eof_token() { return {Tok_Eof, {"Eof", 3}}; }

int is_operator(Token tok) {
    int index = 1;
    for (auto& oper : operators_array) {
        if (oper.type == tok.type)
            return index;
        ++index;
    }
    return 0;
}

int prec(int index) {
    return operators_array[index-1].precedence;
}

bool is_digit(char ch) {
    return (ch >= '0' && ch <= '9');
}
int is_operator(char ch) {
    int index = 1;
    for (auto& oper : operators_array) {
        if (oper.contains(ch))
            return index;
        ++index;
    }
    return 0;
}

bool is_keyword(StrView ident) {
    for (auto& keyw : keyword_array) {
        if (keyw.val.equals(ident))
            return true;
    }
    return false;
}

bool is_word(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

void add_field_or_report_if_exist(StructExpr* expr, Field field)
{
    ASSERT_NOT_NULL(expr);
    for (auto& f : expr->fields) {
        if (f.name == field.name) {
            compiler_error(Token{Tok_Ident, field.name}, "Duplicate member '" SV_FORMAT "'\n", SV_ARG(field.name));
        }
    }
    expr->fields.push(field);
}


void check_assignment(Token variable, Token oper, bool is_already_exist, bool check_assignment)
{
    if (is_already_exist && oper.type == Tok_ColonEquals)
        compiler_error(variable, "Variable '" SV_FORMAT "' already created, if you want to reassign it: use operator '='\n", SV_ARG(variable.val));
    if (check_assignment && !is_already_exist && oper.type == Tok_Equals)
        compiler_error(variable, "Assignment to undeclared variable '" SV_FORMAT "', if you want to create variable: use operator ':='\n", SV_ARG(variable.val));
}

void compiler_error(Token location, const char* const format, ...)
{
    va_list args;
    va_start(args, format);
        ptrdiff_t loc = location.val.data - src_content;
        auto line = 0;
        auto offset = 0;
        // Tokens not pointing into the source (e.g. the synthetic EOF token)
        // would yield a bogus pointer difference; skip the line computation.
        if (src_content && loc >= 0) {
            StrView view(src_content, loc+1);
            while(view.size > 0) {
                ++line;
                auto last_line = view.chop_left_by_delimeter("\n");
                if (view.size == 0) {
                    offset = last_line.size;
                    break;
                }
            }
        }

        ASSERT_TRUE(src_path.is_null_terminated);
        fprintf(stdout, "\x1b[36m%s:%d:%d: \x1b[31merror: ", src_path.data, line, offset);
        printf("\x1b[0m");
        vfprintf(stdout, format, args);
    va_end(args);
    exit(1);
}

void skip_semicolon_if_exist(Lexer& lexer)
{
    if (lexer.peak().type == Tok_SemiColon)
        lexer.skip();
}

Instruction allocate_label()
{
    g_labels.push(Label());
    return Instruction {
        .type = OP_LABEL,
        .label = {(s64)g_labels.count()-1},
        .is_visited = true,
    };
}

void set_label(Instruction label, Array<Instruction>& ops)
{
    ASSERT_TRUE(label.type == OP_LABEL);
    g_labels[label.label.ip].ip = ops.count() - 1;
}

Variable* get_variable(Token variable, bool is_only_local, Array<Variable>* local_vars)
{
    if (local_vars) {
        for (auto& var : *local_vars) {
            if (var.name == variable.val && var.is_accesible) {
                // A cached global copy from an earlier top-level statement is a
                // valid match only if its module is still visible here; the
                // unqualified scan below re-checks visibility and would
                // otherwise lose to a same-named global from a hidden module.
                if (var.is_local || module_symbol_visible(var.module_name))
                    return &var;
            }
        }
    }
    if (!is_only_local)
        return find_global_variable(variable.val, "");
    return nullptr;
}

Variable* get_and_add_variable(Token variable, bool is_local, ValueType type, usize reg_index, Array<Variable>* local_vars)
{
    check_reserved_prefix(variable, "Variable");
    if (local_vars) {
        for (auto& var : *local_vars) {
            if (var.name == variable.val && var.is_accesible) {
                return &var;
            }
        }
    }
    if (!is_local) {
        if (auto* found = find_global_variable(variable.val, ""))
            return found;
    }
    if (is_local) {
        ASSERT_NOT_NULL(local_vars);
        local_vars->push(Variable{variable.val, type, reg_index, true});
        return &local_vars->last();
    } else {
        g_vars.push(Variable{variable.val, type, reg_index, false});
        auto& var = g_vars.last();
        var.module_name = g_current_module_name;
        var.global_offset = g_globals_size;
        g_globals_size += STACK_REGISTER_SIZE;
        return &var;
    }
}

Expression* parse_binary_expr(Lexer& lexer, bool allow_assignment) {
    Token tok = lexer.peak();
    if (tok.type == Tok_RParen) {
        compiler_error(tok, "Unmatched ')'\n");
    }
    if (tok.type == Tok_LParen) {
        lexer.skip();
        Expression* inner = parse_expression(lexer, nullptr, 0, allow_assignment);
        Token close = lexer.next();
        if (close.val != ")") {
            compiler_error(close, "Expected ')', got '" SV_FORMAT "'\n", SV_ARG(close.val));
        }
        return inner;
    }
    if (tok.type == Tok_Ident) {
        return parse_ident(lexer, allow_assignment);
    }
    if (tok.type == Tok_IntLit) {
        lexer.skip();
        return new_binary_expr(tok);
    }
    if (tok.type == Tok_StrLit) {
        lexer.skip();
        auto* expr = new_binary_expr(tok);
        add_string(expr);
        return expr;
    }
    if (tok.type == Tok_LSquare) {
        // Array literal: `[e0, e1, ...]`. The element type and length are
        // resolved during translation.
        lexer.skip();
        auto* arr = new_expr<ArrayLitExpr>(tok, Expr_ArrayLit);
        if (lexer.peak().type == Tok_RSquare) {
            compiler_error(tok, "Empty array literals are not supported\n");
        }
        while (true) {
            Expression* inner = parse_expression(lexer);
            if (!inner)
                compiler_error(tok, "Expected an element in the array literal\n");
            arr->elements.push(inner);
            auto close = lexer.next();
            if (close.type == Tok_RSquare)
                break;
            if (close.type != Tok_Comma)
                compiler_error(close, "Expected ',' or ']' in array literal, got '" SV_FORMAT "'\n", SV_ARG(close.val));
        }
        return arr;
    }
    // Unary prefix operators: `&x` (address-of) and `^x` (dereference).
    // The operand binds tighter than any binary operator (min_prec 7), so
    // `^x + 1` parses as `(^x) + 1` and `^p` as an assignment lhs becomes
    // `^p = v` (a store through the pointer) instead of a read.
    if (tok.type == Tok_Ampersand) {
        // The operand binds tighter than `as` (prec 6), so `&x as u8^` is the
        // cast of the address, not the address of a cast.
        lexer.skip();
        auto* operand = parse_expression(lexer, nullptr, 7, false);
        if (!operand)
            compiler_error(tok, "Expected expression after '&'\n");
        auto* addr = new_expr<AddressOfExpr>(tok, Expr_AddressOf);
        addr->operand = operand;
        return addr;
    }
    if (tok.type == Tok_Caret) {
        // Dereference: `^p`, `^^p`, `^(&x)`. The operand binds tighter than
        // `as` (prec 6), so `^p as u8^` is the cast of the dereferenced value,
        // not a deref of a cast. In type positions the same `^` is the pointer
        // type suffix (`u8^`); prefix (deref) vs postfix (type suffix) is
        // disambiguated by the token's position.
        lexer.skip();
        auto* operand = parse_expression(lexer, nullptr, 7, false);
        if (!operand)
            compiler_error(tok, "Expected expression after '*'\n");
        auto* deref = new_expr<DerefExpr>(tok, Expr_Deref);
        deref->target = operand;
        return deref;
    }
    if (tok.type == Tok_Not) {
        // Logical negation: `!cond`. The operand binds tighter than any binary
        // operator (min_prec 7), so `!a == b` parses as `(!a) == b`.
        lexer.skip();
        auto* operand = parse_expression(lexer, nullptr, 7, false);
        if (!operand)
            compiler_error(tok, "Expected expression after '!'\n");
        auto* not_expr = new_expr<NotExpr>(tok, Expr_Not);
        not_expr->operand = operand;
        return not_expr;
    }
    if (tok.type == Tok_Minus) {
        // Unary minus: `-5`, `-x`. Desugars to `0 - x` so it reuses the whole
        // binary-minus pipeline (type checking, constant folding, codegen)
        // unchanged. The operand binds tighter than any binary operator, so
        // `-x * 2` parses as `(-x) * 2`.
        lexer.skip();
        auto* operand = parse_expression(lexer, nullptr, 7, false);
        if (!operand)
            compiler_error(tok, "Expected expression after '-'\n");
        return new_binary_expr(tok, new_binary_expr(Token{Tok_IntLit, StrView("0", 1)}), operand);
    }
    return parse_keyword(lexer);
}

BlockExpr* parse_block(Lexer& lexer) {
    Token tok = lexer.peak();
    if (tok.type == Tok_LBracket) {
        lexer.skip();
        auto* block = new_block_expr(tok);
        if (lexer.peak().type == Tok_RBracket) {
            lexer.skip();
            return block;
        }
        auto* expr = parse_expression(lexer);
        if (expr) {
            block->exprs.push(expr);
        }
        Token next_tok;
        do {
            next_tok = lexer.peak();
            if (next_tok.type == Tok_SemiColon) {
                lexer.skip();
                next_tok = lexer.peak();
            }
            if (next_tok.type == Tok_RBracket) {
                lexer.skip();
                return block;
            }
            expr = parse_expression(lexer);
            if (!expr) break;
            block->exprs.push(expr);

        } while (next_tok.type != Tok_Eof);
        compiler_error(next_tok, "Expected } at the end of the block, but got: '" SV_FORMAT "'\n", SV_ARG(next_tok.val));
    } else {
        compiler_error(tok, "This language is not C (shit), please use braces { ... }, but got: '" SV_FORMAT "'\n", SV_ARG(tok.val));
    }
    compiler_error(tok, "Expected start of the block, but got: '" SV_FORMAT "'\n", SV_ARG(tok.val));
    exit(1);
}
// `if cond { }` / `if cond { } else { }` / `if a { } else if b { } else { }`.
// An `else if` chain desugars into nested if/else: the nested `if` becomes a
// single statement in the else block, so it reuses the whole if/else pipeline
// (translation, liveness, codegen) unchanged.
IfExpr* parse_if(Token tok, Lexer& lexer)
{
    auto* cond = parse_expression(lexer);
    auto next = lexer.peak();
    if (next.type != Tok_LBracket) {
        compiler_error(next, "This language is not C (shit), please use braces { ... } for the if block\n");
    }
    BlockExpr* if_block = parse_block(lexer);
    BlockExpr* else_block = parse_else_tail(lexer);
    ASSERT_NOT_NULL(if_block);
    return new_if_expr(tok, cond, if_block, else_block);
}
// After an if-block, consume a trailing `else`/`elif` and return the else
// block, or nullptr if none. `elif cond { }` is sugar for `else if cond { }`
// and both chain recursively, so `if a { } elif b { } else if c { } else { }`
// nests exactly like their hand-written `else { if ... }` equivalents.
BlockExpr* parse_else_tail(Lexer& lexer)
{
    auto kw = lexer.peak();
    if (kw.type != Tok_Else && kw.type != Tok_Elif)
        return nullptr;
    lexer.next();
    BlockExpr* else_block = new_block_expr(kw);
    if (kw.type == Tok_Elif) {
        auto* nested_cond = parse_expression(lexer);
        auto peak = lexer.peak();
        if (peak.type != Tok_LBracket) {
            compiler_error(peak, "This language is not C (shit), please use braces { ... } for the if block\n");
        }
        BlockExpr* nested_if_block = parse_block(lexer);
        BlockExpr* nested_else = parse_else_tail(lexer);
        else_block->exprs.push(new_if_expr(kw, nested_cond, nested_if_block, nested_else));
    } else if (lexer.peak().type == Tok_If) {
        // `else if cond { }`
        else_block->exprs.push(parse_if(lexer.next(), lexer));
    } else {
        // `else { }`
        else_block = parse_block(lexer);
    }
    ASSERT_NOT_NULL(else_block);
    return else_block;
}
Expression* parse_type(Token tok) {
    if (str_to_value_type(tok.val) == TYPE_NOP) return nullptr;
    return new_binary_expr(Token{Tok_Type, tok.val});
}
Expression* parse_keyword(Lexer& lexer) {
    Token tok = lexer.next();
    if (tok.type == Tok_Directive) {
        if (tok.val == "#assert") {
            // `#assert <expr>`: parsed like any expression; evaluation and the
            // pass/fail check happen during translation, which reports this
            // token's file and line on failure.
            auto* assert_expr = new_expr<AssertExpr>(tok, Expr_Assert);
            assert_expr->rhs = parse_expression(lexer);
            ASSERT_NOT_NULL(assert_expr->rhs);
            skip_semicolon_if_exist(lexer);
            return assert_expr;
        }
        if (tok.val == "#run") {
            // `#run { ... }`: a compile-time block. Parsed as an ordinary
            // block expression; execution happens during translation.
            auto* run = new_expr<RunExpr>(tok, Expr_Run);
            auto open = lexer.peak();
            if (open.type != Tok_LBracket)
                compiler_error(open, "Expected '{' after '#run', got '" SV_FORMAT "'\n", SV_ARG(open.val));
            run->block = parse_block(lexer);
            if (!run->block)
                compiler_error(open, "Expected block after '#run'\n");
            return run;
        }
        if (tok.val == "#libc" || tok.val == "#lib") {
            // `#libc` / `#lib("path")`: register a shared library to preload
            // before `#run` blocks execute, so their extern calls resolve
            // against it. Registered at parse time (parsing finishes before
            // any translation), so declaration order relative to `#run` does
            // not matter.
            StrView path = "";
            if (tok.val == "#libc") {
                path = SV_LIT("libc.so.6");
                #if defined(__APPLE__)
                    path = SV_LIT("libc.dylib");
                #endif
            } else {
                auto open = lexer.next();
                if (open.type != Tok_LParen)
                    compiler_error(open, "Expected '(' after '#lib', got '" SV_FORMAT "'\n", SV_ARG(open.val));
                auto str = lexer.next();
                if (str.type != Tok_StrLit)
                    compiler_error(str, "Expected a string literal inside #lib(...), got '" SV_FORMAT "'\n", SV_ARG(str.val));
                auto close = lexer.next();
                if (close.type != Tok_RParen)
                    compiler_error(close, "Expected ')' after the '#lib' path, got '" SV_FORMAT "'\n", SV_ARG(close.val));
                path = str.val;
                // Relative paths resolve against the DECLARING SOURCE FILE's
                // directory (library next to the code that uses it), so the
                // compiler's working directory never matters.
                bool absolute = path.size > 0 && path.data[0] == '/';
                bool has_slash = false;
                for (usize c = 0; c < path.size; ++c)
                    if (path.data[c] == '/') has_slash = true;
                comptime_name_pool().push(StrBuilder{});
                auto& resolved = comptime_name_pool().last();
                if (!absolute && !has_slash) {
                    // Bare name: anchor at the source file's directory.
                    usize last_slash = 0;
                    bool found_slash = false;
                    for (usize c = 0; c < src_path.size; ++c)
                        if (src_path.data[c] == '/') { last_slash = c; found_slash = true; }
                    if (found_slash)
                        resolved.append(src_path.sub_view(0, last_slash + 1));
                    else
                        resolved.append("./");
                }
                resolved.append(path);
                resolved.append_null(false);
                g_comptime_libs.push(resolved.to_string_view(true));
                skip_semicolon_if_exist(lexer);
                return new_expr<ComptimeLibExpr>(tok, Expr_ComptimeLib);
            }
            skip_semicolon_if_exist(lexer);
            return new_expr<ComptimeLibExpr>(tok, Expr_ComptimeLib);
        }
        // `#type_id(expr)` / `#type_size(expr)` / `#type_of(expr)`: the
        // reflection builtins. They lex as directives (so they can never be
        // shadowed by user functions) but translate through the ordinary
        // builtin-call path, so they reuse its argument checking.
        auto open = lexer.next();
        if (open.type != Tok_LParen)
            compiler_error(open, "Expected '(' after '" SV_FORMAT "', got '" SV_FORMAT "'\n", SV_ARG(tok.val), SV_ARG(open.val));
        auto* call = new_call_expr(tok);
        auto close = lexer.peak();
        if (close.type != Tok_RParen) {
            do {
                Expression* inner = parse_expression(lexer);
                if (!inner) break;
                call->args.push(CallArg{inner});
                close = lexer.next();
            } while (close.type == Tok_Comma);
        } else {
            lexer.next();
            return call;
        }
        if (close.type != Tok_RParen)
            compiler_error(close, "Expected ')', got '" SV_FORMAT "'\n", SV_ARG(close.val));
        return call;
    }
    if (tok.type == Tok_Return) {
        // Bare `return;` / `return }` (no value) is a void return.
        auto peek = lexer.peak();
        if (peek.type == Tok_SemiColon || peek.type == Tok_RBracket || peek.type == Tok_Eof)
            return new_return_expr(tok, nullptr);
        return new_return_expr(tok, parse_expression(lexer));
    }

    if (tok.type == Tok_Defer) {
        // `defer <call>`: run the call when the function exits. Any expression
        // is parsed (the deferred operand must be a call, checked at translate).
        return new_defer_expr(tok, parse_expression(lexer));
    }

    if (tok.type == Tok_Break) {
        return new_break_expr(tok);
    }

    if (tok.type == Tok_Continue) {
        return new_continue_expr(tok);
    }

    if (tok.type == Tok_If) {
        return parse_if(tok, lexer);
    }

    if (tok.type == Tok_For) {
        // `for x in start..end { }`: the `in` binds the loop variable to each
        // range value. It desugars to `for x := start .. end`, which reuses
        // the counter-loop translation below.
        const usize tc = lexer._tokens.count();
        const bool in_form =
            tc >= 2
            && lexer.peak().type == Tok_Ident
            && lexer._tokens[tc - 2].type == Tok_Ident
            && lexer._tokens[tc - 2].val == "in";
        if (in_form) {
            Token var_tok = lexer.next();
            lexer.skip(); // consume the `in` keyword
            auto* start = parse_expression(lexer);
            ASSERT_NOT_NULL(start);
            auto double_dot = lexer.next();
            if (double_dot.type == Tok_DDot || double_dot.type == Tok_DDotInclusive) {
                auto* right_cond = parse_expression(lexer);
                ASSERT_NOT_NULL(right_cond);
                auto bracket = lexer.peak();
                if (bracket.type == Tok_LBracket) {
                    BlockExpr* block = parse_block(lexer);
                    ASSERT_NOT_NULL(block);
                    auto* left_cond = new_assignment_expr(var_tok, start, true,
                        Token{Tok_ColonEquals, var_tok.val});
                    return new_for_expr(tok, left_cond, right_cond, block, double_dot.type == Tok_DDotInclusive);
                } else {
                    compiler_error(bracket, "This language is not C (shit), please use braces { ... } for the for block\n");
                }
            } else {
                compiler_error(double_dot, "Expected '..' after for expression\n");
            }
        }
        auto* left_cond = parse_expression(lexer);
        ASSERT_NOT_NULL(left_cond);
        auto next_tok = lexer.peak();
        if (next_tok.type == Tok_LBracket) {
            // `for <cond> { }`: a condition-driven loop. The condition is
            // evaluated before every iteration; `for true { }` is an infinite
            // loop, `for false { }` never runs the block.
            BlockExpr* block = parse_block(lexer);
            ASSERT_NOT_NULL(block);
            auto* for_expr = new_for_expr(tok, left_cond, nullptr, block, false);
            for_expr->is_condition = true;
            return for_expr;
        }
        auto double_dot = lexer.next();
        if (double_dot.type == Tok_DDot || double_dot.type == Tok_DDotInclusive) {
            auto* right_cond = parse_expression(lexer);
            ASSERT_NOT_NULL(right_cond);
            auto bracket = lexer.peak();
            if (bracket.type == Tok_LBracket) {
                BlockExpr* block = parse_block(lexer);
                ASSERT_NOT_NULL(block);
                return new_for_expr(tok, left_cond, right_cond, block, double_dot.type == Tok_DDotInclusive);
            } else {
                compiler_error(bracket, "This language is not C (shit), please use braces { ... } for the for block\n");
            }
        } else {
            compiler_error(double_dot, "Expected '..' after for expression\n");
        }
    }

    if (tok.type != Tok_Keyword) {
        compiler_error(tok, "Expected keyword, but got: '" SV_FORMAT "'\n", SV_ARG(tok.val));
    }

    if (tok.val == "print") {
        auto* cond = parse_expression(lexer);
        ASSERT_NOT_NULL(cond);
        return new_print_expr(tok, cond);
    }
    if (tok.val == "true") {
        return new_binary_expr(Token{Tok_BoolLit, tok.val});
    }
    if (tok.val == "false") {
        return new_binary_expr(Token{Tok_BoolLit, tok.val});
    }
    if (tok.val == "null") {
        return new_binary_expr(Token{Tok_NullLit, tok.val});
    }
    if (auto* type = parse_type(tok))
        return type;
    compiler_error(tok, "Unknown keyword: '" SV_FORMAT "'\n", SV_ARG(tok.val));
    exit(1);
}

AssignmentExpr* parse_assignment(Token variable, bool is_local, Lexer& lexer, Token field_name)
{
    auto tok = lexer.next();
    if (tok.type == Tok_Equals || tok.type == Tok_ColonEquals) {
        auto* inner = parse_expression(lexer);
        skip_semicolon_if_exist(lexer);

        return new_assignment_expr(variable, inner, is_local, tok, field_name);
    }
    compiler_error(tok, "Expected assignment expression, but got '" SV_FORMAT "'\n", SV_ARG(tok.val));
    return nullptr;
}

// `name : TYPE = expr` / `name : TYPE^ = expr`: a variable declaration with an
// explicit type. The type annotation is optional in the language (`name :=
// expr` infers it), but when present it forces the variable's type.
AssignmentExpr* parse_typed_declaration(Token variable, bool is_local, Lexer& lexer)
{
    lexer.next(); // consume ':'
    auto type_tok = lexer.next();
    // A module-qualified struct type: `x : mod :: Foo = ...`. The module name
    // is the first identifier, `::` is the two-colon separator.
    StrView type_module = "";
    if (type_tok.type == Tok_Ident && lexer.peak().type == Tok_DColon && lexer.peak_next().type == Tok_DColon) {
        type_module = type_tok.val;
        lexer.skip();
        lexer.skip();
        type_tok = lexer.next();
        if (type_tok.type != Tok_Ident)
            compiler_error(type_tok, "Expected a type name after '" SV_FORMAT "::', got '" SV_FORMAT "'\n", SV_ARG(type_module), SV_ARG(type_tok.val));
    }
    u8 ptr_depth = 0;
    while (lexer.peak().type == Tok_Caret) {
        ++ptr_depth;
        lexer.next();
    }
    ValueType declared = TYPE_NOP;
    StrView declared_struct_name = "";
    StrView declared_struct_module = "";
    if (type_tok.type == Tok_Keyword) {
        if (type_module.size > 0)
            compiler_error(type_tok, "Expected a struct type after '" SV_FORMAT "::'\n", SV_ARG(type_module));
        declared = str_to_value_type(type_tok.val);
        if (declared == TYPE_NOP)
            compiler_error(type_tok, "Unknown type name: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
        if (declared == TYPE_VOID && ptr_depth == 0)
            compiler_error(type_tok, "Variable cannot have type 'void'\n");
    } else if (type_tok.type == Tok_Ident) {
        usize s_idx = find_visible_struct(type_tok.val, type_module);
        if (s_idx == Array<DeclaredStruct>::INVALID_INDEX)
            compiler_error(type_tok, "Expected a type name after ':', got '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
        declared = TYPE_STRUCT;
        declared_struct_name = type_tok.val;
        declared_struct_module = g_structs[s_idx].module_name;
    } else {
        compiler_error(type_tok, "Expected a type name after ':', got '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
    }
    // Array type suffix: `a : i64[5] = ...`. The element type is `declared`
    // and the fixed length is the integer inside the brackets.
    s64 array_len = -1;
    if (lexer.peak().type == Tok_LSquare) {
        if (ptr_depth > 0)
            compiler_error(type_tok, "Arrays of pointers are not supported yet\n");
        if (declared == TYPE_VOID)
            compiler_error(type_tok, "Variable cannot have type 'void'\n");
        lexer.next();
        auto len_tok = lexer.next();
        if (len_tok.type != Tok_IntLit)
            compiler_error(len_tok, "Expected a fixed element count inside '[...]', got '" SV_FORMAT "'\n", SV_ARG(len_tok.val));
        u64 raw = 0;
        if (!parse_u64_literal(len_tok.val, raw) || raw > 0x7FFFFFFFll)
            compiler_error(len_tok, "Invalid array length: '" SV_FORMAT "'\n", SV_ARG(len_tok.val));
        array_len = (s64)raw;
        auto close = lexer.next();
        if (close.type != Tok_RSquare)
            compiler_error(close, "Expected ']' in the array type, got '" SV_FORMAT "'\n", SV_ARG(close.val));
    }
    auto eq = lexer.next();
    if (eq.type != Tok_Equals)
        compiler_error(eq, "Expected '=' after the type in a typed declaration, got '" SV_FORMAT "'\n", SV_ARG(eq.val));
    auto* inner = parse_expression(lexer);
    if (!inner)
        compiler_error(eq, "Expected an expression after '='\n");
    skip_semicolon_if_exist(lexer);
    auto* assign = new_assignment_expr(variable, inner, is_local, eq);
    assign->declared_type = declared;
    assign->declared_struct_name = declared_struct_name;
    assign->declared_struct_module = declared_struct_module;
    if (array_len >= 0) {
        assign->declared_type = TYPE_ARRAY;
        assign->declared_array_elem = declared;
        assign->declared_array_len = array_len;
    }
    if (ptr_depth > 0) {
        assign->declared_pointee = declared;
        assign->declared_type = TYPE_PTR;
        assign->declared_ptr_depth = ptr_depth;
        assign->declared_pointee_struct_name = declared_struct_name;
        assign->declared_pointee_struct_module = declared_struct_module;
    }
    return assign;
}


// `mod :: name`: a module-qualified reference to a function, struct or global
// variable declared in module `tok`. Consumes the two ':' tokens, then builds
// the same node the unqualified form would, tagged with the module.
Expression* parse_qualified_ref(Token mod_tok, Lexer& lexer)
{
    lexer.skip(); // ':'
    lexer.skip(); // ':'
    auto name_tok = lexer.next();
    if (name_tok.type != Tok_Ident)
        compiler_error(name_tok, "Expected a name after '" SV_FORMAT "::', got '" SV_FORMAT "'\n", SV_ARG(mod_tok.val), SV_ARG(name_tok.val));
    if (!module_symbol_visible(mod_tok.val))
        compiler_error(mod_tok,
            "Module '" SV_FORMAT "' is not visible from module '" SV_FORMAT "': declare or import it before use\n",
            SV_ARG(mod_tok.val), SV_ARG(g_current_module_name));

    auto next = lexer.peak();
    if (next.type == Tok_LParen) {
        lexer.next();
        auto* call = new_call_expr(name_tok);
        call->module_name = mod_tok.val;
        auto close = lexer.peak();
        if (close.type == Tok_RParen) {
            lexer.next();
            return call;
        }
        do {
            Expression* inner = parse_expression(lexer);
            if (!inner) break;
            call->args.push(CallArg{inner});
            close = lexer.next();
        } while (close.type == Tok_Comma);
        if (close.type != Tok_RParen) {
            compiler_error(close, "Expected ')', got '" SV_FORMAT "'\n", SV_ARG(close.val));
        }
        return call;
    }
    if (next.type == Tok_LBracket) {
        // `{` opens a struct init (`geo::Point { ... }`) but also an
        // if/loop/block after a plain global read (`if cfg::flag { ... }`).
        // Structs are registered at parse time, so a struct name is
        // unambiguous; anything else falls through to the global-read path
        // (an undeclared-struct misuse is caught later during translation).
        if (find_visible_struct(name_tok.val, mod_tok.val) != Array<DeclaredStruct>::INVALID_INDEX)
            return parse_struct_init(lexer, name_tok, mod_tok.val);
    }
    if (next.type == Tok_Equals || next.type == Tok_ColonEquals) {
        // `mod::gx = 5`: assign to a global variable in another module.
        auto* is_already_exist = find_global_variable(name_tok.val, mod_tok.val);
        check_assignment(name_tok, next, is_already_exist != nullptr, false);
        auto* assign = parse_assignment(name_tok, false, lexer);
        assign->module_name = mod_tok.val;
        return assign;
    }
    // A plain (read) reference to a qualified global variable.
    auto* var_ref = new_binary_expr(name_tok);
    var_ref->module_name = mod_tok.val;
    return var_ref;
}

Expression* parse_ident(Lexer& lexer, bool allow_assignment) {
    Token tok = lexer.next();
    if (tok.type != Tok_Ident) {
        compiler_error(tok, "Expected Identifier, but got '" SV_FORMAT "'\n", SV_ARG(tok.val));
    }

    auto next = lexer.peak();
    if (next.type == Tok_DColon && lexer.peak_next().type == Tok_DColon) {
        // Module-qualified reference: `mod :: name` (two ':' tokens).
        return parse_qualified_ref(tok, lexer);
    }
    if (next.type == Tok_Equals || next.type == Tok_ColonEquals) {
        // Inside a unary operand (`&x` / `*x`) an `=` belongs to the outer
        // store, not to the variable, so `*x = 20` is a dereference-store.
        if (!allow_assignment)
            return new_binary_expr(tok);
        auto* is_already_exist = get_variable(tok, false);
        check_assignment(tok, next, is_already_exist != nullptr, false);
        bool is_local = true;
        if (is_already_exist)
            is_local = is_already_exist->is_local;
        return parse_assignment(tok, is_local, lexer);
    }
    if (next.type == Tok_DColon && allow_assignment) {
        // Typed declaration: `x : u8 = 300`. Inside a unary operand the `:`
        // belongs to nothing else, so it must appear at statement level.
        return parse_typed_declaration(tok, true, lexer);
    }
    if (next.type == Tok_LParen) {
        lexer.next();
        auto* call = new_call_expr(tok);
        auto close = lexer.peak();
        if (close.type == Tok_RParen) {
            lexer.next();
            return call;
        }
        do {
            Expression* inner = parse_expression(lexer);
            if (!inner) break;
            call->args.push(CallArg{inner});
            close = lexer.next();
        } while (close.type == Tok_Comma);
        if (close.type != Tok_RParen) {
            compiler_error(close, "Expected ')', got '" SV_FORMAT "'\n", SV_ARG(close.val));
        }
        return call;
    }
    // if (next.type == Tok_LBracket) {
    //     lexer.skip();
    //     auto close_tok = lexer.next();
    //     if (close_tok.type != Tok_RBracket) {
    //         compiler_error(close_tok, "Expected } at the end of struct initilization, but got: '"
    //                         SV_FORMAT
    //                         "' (You cannot pass args for now)\n", SV_ARG(close_tok.val));
    //     }
    //     skip_semicolon_if_exist(lexer);
    //     return new_struct_init_expr(tok);
    // }
    if (next.type == Tok_LBracket && find_visible_struct(tok.val, "") != Array<DeclaredStruct>::INVALID_INDEX) {
        return parse_struct_init(lexer, tok);
    }
    if (next.type == Tok_Dot) {
        lexer.skip();
        auto field = lexer.next();
        if (field.type != Tok_Ident) {
           compiler_error(field, "Expected member field, but got: '" SV_FORMAT "'\n", SV_ARG(field.val));
        }
        if (lexer.peak().type == Tok_Equals || lexer.peak().type == Tok_ColonEquals) {
            return parse_assignment(tok, true, lexer, field);
        }
        return new_member_call(tok, field);
    }
    return new_binary_expr(tok);
}
StructExpr* parse_struct(Lexer& lexer) {
    auto tok = lexer.next();
    if (tok.type != Tok_Struct) {
        compiler_error(tok, "Expected struct definition, but got: '" SV_FORMAT "'\n", SV_ARG(tok.val));
        exit(1);
    }
    auto struct_name = lexer.next();
    if (struct_name.type != Tok_Ident) {
        compiler_error(struct_name, "Expected struct name, but got: '" SV_FORMAT "'\n", SV_ARG(struct_name.val));
    }
    check_reserved_prefix(struct_name, "Struct name");
    auto* struct_expr = new_struct_expr(struct_name);

    tok = lexer.peak();
    if (tok.type != Tok_LBracket) {
        compiler_error(tok, "Expected { at the start of the struct declaration, but got: '" SV_FORMAT "'\n", SV_ARG(tok.val));
    }
    lexer.skip();
    if (lexer.peak().type == Tok_RBracket) {
        lexer.skip();
        skip_semicolon_if_exist(lexer);
        struct_expr->total_size = 0;
        struct_expr->align = 1;
        return struct_expr;
    }
    u32 cur_off = 0;
    u32 max_align = 1;
    while (true) {
        auto field_tok = lexer.next();
        if (field_tok.type != Tok_Ident) {
            compiler_error(field_tok, "Expected member name in struct declaration, but got: '" SV_FORMAT "'\n", SV_ARG(field_tok.val));
        }
        check_reserved_prefix(field_tok, "Struct field");
        if (lexer.peak().type != Tok_DColon) {
            compiler_error(lexer.peak(), "Expected ':' and a type after member '" SV_FORMAT "', but got: '" SV_FORMAT "'\n", SV_ARG(field_tok.val), SV_ARG(lexer.peak().val));
        }
        lexer.next(); // consume ':'
        auto type_tok = lexer.next();
        // A module-qualified struct type: `field : mod :: Foo`.
        StrView type_module = "";
        if (type_tok.type == Tok_Ident && lexer.peak().type == Tok_DColon && lexer.peak_next().type == Tok_DColon) {
            type_module = type_tok.val;
            lexer.skip();
            lexer.skip();
            type_tok = lexer.next();
            if (type_tok.type != Tok_Ident)
                compiler_error(type_tok, "Expected a type name after '" SV_FORMAT "::', got '" SV_FORMAT "'\n", SV_ARG(type_module), SV_ARG(type_tok.val));
        }
        u8 ptr_depth = 0;
        while (lexer.peak().type == Tok_Caret) {
            ++ptr_depth;
            lexer.next();
        }
        Field field{};
        field.name = field_tok.val;
        if (type_tok.type == Tok_Keyword) {
            if (type_module.size > 0)
                compiler_error(type_tok, "Expected a struct type after '" SV_FORMAT "::'\n", SV_ARG(type_module));
            ValueType base = str_to_value_type(type_tok.val);
            if (base == TYPE_NOP || base == TYPE_VOID)
                compiler_error(type_tok, "Unknown field type: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
            if (ptr_depth > 0) {
                field.pointee = base;
                field.type = TYPE_PTR;
                field.ptr_depth = ptr_depth;
            } else {
                field.type = base;
            }
        } else if (type_tok.type == Tok_Ident) {
            usize s_idx = find_visible_struct(type_tok.val, type_module);
            if (s_idx == Array<DeclaredStruct>::INVALID_INDEX)
                compiler_error(type_tok, "Unknown field type: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
            if (ptr_depth > 0) {
                field.pointee = TYPE_STRUCT;
                field.type = TYPE_PTR;
                field.ptr_depth = ptr_depth;
                field.pointee_struct_name = type_tok.val;
                field.pointee_struct_module = g_structs[s_idx].module_name;
            } else {
                field.type = TYPE_STRUCT;
                field.struct_name = type_tok.val;
                field.struct_module = g_structs[s_idx].module_name;
            }
        } else {
            compiler_error(type_tok, "Unknown field type: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
        }
    // C layout: the field lands at its natural alignment (a struct-typed
    // field stores an 8-byte heap pointer, so it is 8-aligned like any
    // pointer) and the struct grows to the largest member alignment.
    field.size = type_size(field.type);
    u32 align = MAX(field.size, 1);
    cur_off = (cur_off + align - 1) / align * align;
    field.offset = cur_off;
    cur_off += field.size;
    max_align = MAX(max_align, align);
    add_field_or_report_if_exist(struct_expr, field);
        auto next_tok = lexer.peak();
        if (next_tok.type == Tok_Comma) {
            lexer.skip();
            // Trailing comma: `x : i64, }` closes the struct declaration.
            if (lexer.peak().type == Tok_RBracket) {
                lexer.skip();
                skip_semicolon_if_exist(lexer);
                struct_expr->total_size = (cur_off + max_align - 1) / max_align * max_align;
                struct_expr->align = max_align;
                return struct_expr;
            }
            continue;
        }
        if (next_tok.type == Tok_RBracket) {
            lexer.skip();
            skip_semicolon_if_exist(lexer);
            struct_expr->total_size = (cur_off + max_align - 1) / max_align * max_align;
            struct_expr->align = max_align;
            return struct_expr;
        }
        compiler_error(next_tok, "Expected ',' or '}' after member in struct declaration, but got: '" SV_FORMAT "'\n", SV_ARG(next_tok.val));
    }
}

Expression* parse_struct_init(Lexer& lexer, Token struct_tok, StrView module_name)
{
    // struct_tok is the struct name; `{` is the next token.
    auto* init = new_struct_init_expr(struct_tok);
    init->module_name = module_name;
    lexer.next(); // consume '{'
    if (lexer.peak().type == Tok_RBracket) {
        lexer.skip();
        skip_semicolon_if_exist(lexer);
        return init;
    }
    const usize struct_index = find_visible_struct(struct_tok.val, module_name);
    if (!g_structs.is_valid_index(struct_index)) {
        compiler_error(struct_tok, "Use of undeclared struct: '" SV_FORMAT "'\n", SV_ARG(struct_tok.val));
    }
    // Custom zero-initialization: `Foo {0}` fills every member with its zero
    // value. Any other single value inside the braces is invalid.
    if (lexer.peak().type == Tok_IntLit && lexer.peak().val == "0") {
        lexer.skip(); // consume '0'
        auto end = lexer.peak();
        if (end.type == Tok_Comma) {
            lexer.skip();
            end = lexer.peak();
        }
        if (end.type != Tok_RBracket)
            compiler_error(end, "Expected '}' after '0' in struct zero initialization, but got: '" SV_FORMAT "'\n", SV_ARG(end.val));
        lexer.skip();
        skip_semicolon_if_exist(lexer);
        init->zero_init = true;
        return init;
    }
    Array<bool> seen;
    seen.reserve(g_structs[struct_index].expr->fields.count());
    seen.set_count(g_structs[struct_index].expr->fields.count());
    seen.memzero();
    while (true) {
        auto field_tok = lexer.next();
        if (field_tok.type != Tok_Ident) {
            compiler_error(field_tok, "Expected a member name in struct initialization, but got: '" SV_FORMAT "'\n", SV_ARG(field_tok.val));
        }
        if (lexer.peak().type != Tok_DColon) {
            compiler_error(lexer.peak(), "Expected ':' after member '" SV_FORMAT "' in struct initialization, but got: '" SV_FORMAT "'\n", SV_ARG(field_tok.val), SV_ARG(lexer.peak().val));
        }
        lexer.next(); // consume ':'
        auto* value = parse_expression(lexer);
        if (!value)
            compiler_error(field_tok, "Expected an expression after ':'\n");
        bool found = false;
        for (usize i = 0; i < g_structs[struct_index].expr->fields.count(); ++i) {
            if (g_structs[struct_index].expr->fields[i].name == field_tok.val) {
                if (seen[i])
                    compiler_error(field_tok, "Duplicate initialization of member '" SV_FORMAT "'\n", SV_ARG(field_tok.val));
                seen[i] = true;
                found = true;
                break;
            }
        }
        if (!found)
            compiler_error(field_tok, "Struct '" SV_FORMAT "' doesnt contain '" SV_FORMAT "' field\n", SV_ARG(struct_tok.val), SV_ARG(field_tok.val));
        init->field_names.push(field_tok.val);
        init->field_values.push(value);
        auto next_tok = lexer.peak();
        if (next_tok.type == Tok_Comma) {
            lexer.skip();
            // Trailing comma: `Foo { x: 4, }` closes the struct literal.
            if (lexer.peak().type == Tok_RBracket) {
                lexer.skip();
                skip_semicolon_if_exist(lexer);
                return init;
            }
            continue;
        }
        if (next_tok.type == Tok_RBracket) {
            lexer.skip();
            skip_semicolon_if_exist(lexer);
            return init;
        }
        compiler_error(next_tok, "Expected ',' or '}' in struct initialization, but got: '" SV_FORMAT "'\n", SV_ARG(next_tok.val));
    }
}

// Parse `name(params) -> ret` (everything of a function declaration except the
// body) into a FunctionExpr. Shared by `fn foo() { ... }` definitions and
// `extern fn foo(...) -> T;` C FFI declarations.
FunctionExpr* parse_function_signature(Lexer& lexer) {
    auto name = lexer.next();
    if (name.type != Tok_Ident) {
        compiler_error(name, "Expected function name, got '" SV_FORMAT "'\n", SV_ARG(name.val));
    }
    Array<Expression*> args;
    Array<FunctionArgument> arg_types;
    auto args_tok = lexer.next();
    if (args_tok.type == Tok_LParen) {
        Token close = lexer.peak();
        if (close.type != Tok_RParen) {
            do {
                auto name_tok = lexer.next();
                if (name_tok.type != Tok_Ident)
                    compiler_error(name_tok, "Expected parameter name, got '" SV_FORMAT "'\n", SV_ARG(name_tok.val));
                check_reserved_prefix(name_tok, "Parameter");
                args.push(new_binary_expr(name_tok));
                Expression* type = nullptr;
                FunctionArgument arg{};
                if (lexer.peak().type == Tok_DColon) {
                    lexer.next();
                    auto type_tok = lexer.next();
                    // A module-qualified struct type: `p : mod :: Foo`.
                    StrView type_module = "";
                    if (type_tok.type == Tok_Ident && lexer.peak().type == Tok_DColon && lexer.peak_next().type == Tok_DColon) {
                        type_module = type_tok.val;
                        lexer.skip();
                        lexer.skip();
                        type_tok = lexer.next();
                        if (type_tok.type != Tok_Ident)
                            compiler_error(type_tok, "Expected a type name after '" SV_FORMAT "::', got '" SV_FORMAT "'\n", SV_ARG(type_module), SV_ARG(type_tok.val));
                    }
                    // A `u8^` / `bool^^` pointer type: the base keyword token
                    // followed by one or more `^` suffix tokens.
                    u8 ptr_depth = 0;
                    while (lexer.peak().type == Tok_Caret) {
                        ++ptr_depth;
                        lexer.next();
                    }
                    if (type_tok.type == Tok_Keyword) {
                        if (type_module.size > 0)
                            compiler_error(type_tok, "Expected a struct type after '" SV_FORMAT "::'\n", SV_ARG(type_module));
                        type = parse_type(type_tok);
                        if (!type)
                            compiler_error(type_tok, "Unknown parameter type: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
                        arg.expr = type;
                        arg.type = str_to_value_type(type_tok.val);
                        if (arg.type == TYPE_VOID && ptr_depth == 0)
                            compiler_error(type_tok, "Parameter cannot have type 'void'\n");
                        if (ptr_depth > 0) {
                            arg.pointee = arg.type;
                            arg.type = TYPE_PTR;
                            arg.ptr_depth = ptr_depth;
                        }
                    } else if (type_tok.type == Tok_Ident) {
                        usize s_idx = find_visible_struct(type_tok.val, type_module);
                        if (s_idx == Array<DeclaredStruct>::INVALID_INDEX)
                            compiler_error(type_tok, "Unknown parameter type: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
                        if (ptr_depth > 0) {
                            // `p : Foo^` parameter: TYPE_PTR with the pointee
                            // struct's name kept for call-site identity checks.
                            // `expr` is set so the type-check and body-var
                            // setup treat it as a typed parameter.
                            arg.expr = new_binary_expr(Token{Tok_Type, type_tok.val});
                            arg.pointee = TYPE_STRUCT;
                            arg.type = TYPE_PTR;
                            arg.ptr_depth = ptr_depth;
                            arg.pointee_struct_name = type_tok.val;
                            arg.pointee_struct_module = g_structs[s_idx].module_name;
                        } else {
                            // The param's FunctionArgument.type is TYPE_STRUCT; the
                            // name is kept for call-site identity checking. `expr`
                            // is set so the type-check loop treats it as typed.
                            arg.expr = new_binary_expr(Token{Tok_Type, type_tok.val});
                            arg.type = TYPE_STRUCT;
                            arg.struct_name = type_tok.val;
                            arg.struct_module = g_structs[s_idx].module_name;
                        }
                    } else {
                        compiler_error(type_tok, "Unknown parameter type: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
                    }
                } else {
                    compiler_error(name_tok, "Parameter '" SV_FORMAT "' must have an explicit type (e.g. 'name : i64')\n", SV_ARG(name_tok.val));
                }
                arg_types.push(arg);
                close = lexer.next();
            } while (close.type == Tok_Comma);
            if (close.type != Tok_RParen) {
                compiler_error(close, "Expected ')' after '(', got '" SV_FORMAT "'\n", SV_ARG(close.val));
            }
        } else {
            lexer.next();
        }
    }
    // Optional explicit return type: `fn foo() -> u8 { ... }`.
    // A `u8^` / `bool^^` pointer return type: base keyword + `^` suffixes.
    // A `-> Foo` returns a struct (by reference: the heap pointer).
    ValueType ret_type = TYPE_NOP;
    StrView ret_struct_name = "";
    StrView ret_struct_module = "";
    ValueType fun_ret_pointee = TYPE_NOP;
    u8 fun_ret_ptr_depth = 0;
    StrView fun_ret_pointee_struct = "";
    StrView fun_ret_pointee_struct_module = "";
    if (lexer.peak().type == Tok_Arrow) {
        lexer.next();
        auto type_tok = lexer.next();
        // A module-qualified struct return type: `-> mod :: Foo`.
        StrView ret_module = "";
        if (type_tok.type == Tok_Ident && lexer.peak().type == Tok_DColon && lexer.peak_next().type == Tok_DColon) {
            ret_module = type_tok.val;
            lexer.skip();
            lexer.skip();
            type_tok = lexer.next();
            if (type_tok.type != Tok_Ident)
                compiler_error(type_tok, "Expected a type name after '" SV_FORMAT "::', got '" SV_FORMAT "'\n", SV_ARG(ret_module), SV_ARG(type_tok.val));
        }
        u8 ret_ptr_depth = 0;
        while (lexer.peak().type == Tok_Caret) {
            ++ret_ptr_depth;
            lexer.next();
        }
        if (type_tok.type == Tok_Keyword) {
            if (ret_module.size > 0)
                compiler_error(type_tok, "Expected a struct type after '" SV_FORMAT "::'\n", SV_ARG(ret_module));
            ret_type = str_to_value_type(type_tok.val);
            if (ret_type == TYPE_NOP)
                compiler_error(type_tok, "Unknown return type: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
            if (ret_ptr_depth > 0) {
                fun_ret_pointee = ret_type;
                fun_ret_ptr_depth = ret_ptr_depth;
                ret_type = TYPE_PTR;
            }
        } else if (type_tok.type == Tok_Ident) {
            usize s_idx = find_visible_struct(type_tok.val, ret_module);
            if (s_idx == Array<DeclaredStruct>::INVALID_INDEX)
                compiler_error(type_tok, "Unknown return type: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
            if (ret_ptr_depth > 0) {
                ret_type = TYPE_PTR;
            } else {
                ret_type = TYPE_STRUCT;
                ret_struct_name = type_tok.val;
                ret_struct_module = g_structs[s_idx].module_name;
            }
            fun_ret_pointee = TYPE_STRUCT;
            fun_ret_ptr_depth = ret_ptr_depth;
            fun_ret_pointee_struct = type_tok.val;
            fun_ret_pointee_struct_module = g_structs[s_idx].module_name;
        } else {
            compiler_error(type_tok, "Unknown return type: '" SV_FORMAT "'\n", SV_ARG(type_tok.val));
        }
    }
    auto* fun_expr = new_function_expr(name, args, arg_types, nullptr);
    fun_expr->return_type = ret_type;
    fun_expr->return_struct_name = ret_struct_name;
    fun_expr->return_struct_module = ret_struct_module;
    fun_expr->return_pointee = fun_ret_pointee;
    fun_expr->return_ptr_depth = fun_ret_ptr_depth;
    fun_expr->return_pointee_struct_name = fun_ret_pointee_struct;
    fun_expr->return_pointee_struct_module = fun_ret_pointee_struct_module;
    return fun_expr;
}

FunctionExpr* parse_function(Lexer& lexer) {
    auto decl = lexer.next();
    if (decl.type != Tok_Function) {
        compiler_error(decl, "Expected function declaration, got '" SV_FORMAT "'\n", SV_ARG(decl.val));
    }
    auto* fun_expr = parse_function_signature(lexer);
    fun_expr->block = parse_block(lexer);
    return fun_expr;
}

// `extern fn foo(params) -> ret;`: a C FFI declaration. No body is parsed and
// none is emitted; the symbol is resolved by the C linker at link time.
FunctionExpr* parse_extern_function(Lexer& lexer) {
    auto extern_tok = lexer.next();
    if (extern_tok.type != Tok_Keyword || extern_tok.val != "extern") {
        compiler_error(extern_tok, "Expected 'extern', got '" SV_FORMAT "'\n", SV_ARG(extern_tok.val));
    }
    auto fn_tok = lexer.next();
    if (fn_tok.type != Tok_Function) {
        compiler_error(fn_tok, "Expected 'fn' after 'extern', got '" SV_FORMAT "'\n", SV_ARG(fn_tok.val));
    }
    auto* fun_expr = parse_function_signature(lexer);
    fun_expr->is_extern = true;
    // A body is not allowed: `extern fn` is a pure declaration.
    if (lexer.peak().type == Tok_LBracket)
        compiler_error(fun_expr->tok, "extern function cannot have a body (use 'fn " SV_FORMAT "' for a definition)\n", SV_ARG(fun_expr->tok.val));
    // Struct values don't cross the C ABI boundary as a plain pointer: this
    // language returns structs through a caller-reserved slot and represents
    // values as heap pointers, which no C signature matches. Pointers to
    // structs (`Foo^`) are fine.
    for (auto& arg : fun_expr->arg_types)
        if (arg.type == TYPE_STRUCT)
            compiler_error(fun_expr->tok, "extern function parameter of struct type is not supported (use a '^' pointer instead)\n");
    if (fun_expr->return_struct_name.size > 0)
        compiler_error(fun_expr->tok, "extern function returning a struct is not supported (use a '^' pointer instead)\n");
    if (is_windows_target())
        compiler_error(fun_expr->tok, "extern (C FFI) functions are not supported on the Windows target yet\n");
    if (lexer.peak().type == Tok_SemiColon)
        lexer.next();
    return fun_expr;
}

Expression* parse_primary(Lexer& lexer) {
    auto next_tok = lexer.peak();
    if (next_tok.type == Tok_Struct) {
        auto* struc = parse_struct(lexer);
        ASSERT_NOT_NULL(struc);
        add_struct_or_report_if_exit(struc);
        return struc;
    } else if (next_tok.type == Tok_Keyword && next_tok.val == "extern") {
        auto* fun = parse_extern_function(lexer);
        ASSERT_NOT_NULL(fun);
        add_function_or_report_if_exit(fun);
        return fun;
    } else if (next_tok.type == Tok_Function) {
        auto* fun = parse_function(lexer);
        ASSERT_NOT_NULL(fun);
        add_function_or_report_if_exit(fun);
        return fun;
    } else if (next_tok.type == Tok_Module) {
        lexer.skip();
        auto module_name = lexer.next();
        if (module_name.type != Tok_Ident) {
            compiler_error(module_name, "Expected module name, but got '" SV_FORMAT "'\n", SV_ARG(module_name.val));
        }
        skip_semicolon_if_exist(lexer);

        auto* module_expr = new_module_expr(module_name);
        ASSERT_NOT_NULL(module_expr);
        return module_expr;
    } else if (next_tok.type == Tok_ImportModule) {
        lexer.skip();
        auto module_name = lexer.next();
        if (module_name.type != Tok_Ident) {
            compiler_error(module_name, "Expected module name, but got '" SV_FORMAT "'\n", SV_ARG(module_name.val));
        }
        skip_semicolon_if_exist(lexer);

        auto* module_import_expr = new_module_import_expr(module_name, module_name.val);
        ASSERT_NOT_NULL(module_import_expr);
        return module_import_expr;
    } else if (next_tok.type == Tok_Directive) {
        // Top-level `#assert` (the reflection directives only appear inside
        // expressions): routed through the keyword parser like the statement
        // forms; translation checks it at compile time.
        return parse_keyword(lexer);
    } else if (next_tok.type == Tok_Ident) {
        lexer.skip();
        auto oper = lexer.peak();
        if (oper.type == Tok_DColon && lexer.peak_next().type == Tok_DColon) {
            // Module-qualified top-level statement: `mod::gx = 5`,
            // `mod::foo()`.
            return parse_qualified_ref(next_tok, lexer);
        }
        if (oper.type == Tok_DColon) {
            // Typed global declaration: `gx : i64 = 5`.
            auto* assign = parse_typed_declaration(next_tok, false, lexer);
            ASSERT_NOT_NULL(assign);
            return assign;
        }
        bool is_already_exist = get_variable(next_tok, false) != nullptr;
        check_assignment(next_tok, oper, is_already_exist);
        auto* assign = parse_assignment(next_tok, false, lexer);
        ASSERT_NOT_NULL(assign);
        // get_and_add_variable(assign->tok, false, 8);
        return assign;
    } else {
        compiler_error(next_tok, "Expected module, function, global variable, or struct declaration at the top block, but got '" SV_FORMAT "'\n", SV_ARG(next_tok.val));
    }
    return nullptr;
}

Expression* parse_expression(Lexer& lexer, Expression* lhs, int min_prec, bool allow_assignment) {
    if (!lhs) {
        lhs = parse_binary_expr(lexer, allow_assignment);
        if (!lhs) return nullptr;
        // Postfix array indexing binds tighter than any binary operator:
        // `arr[i] + 1` is `(arr[i]) + 1`. `[` is not a binary operator
        // (precedence 0), so it never enters the operator loop below.
        while (lexer.peak().type == Tok_LSquare) {
            lexer.next();
            Expression* idx = parse_expression(lexer);
            if (!idx)
                compiler_error(lhs->tok, "Expected an index expression after '['\n");
            Token close = lexer.next();
            if (close.type != Tok_RSquare)
                compiler_error(close, "Expected ']' after array index, got '" SV_FORMAT "'\n", SV_ARG(close.val));
            auto* index_expr = new_expr<IndexExpr>(lhs->tok, Expr_Index);
            index_expr->base = lhs;
            index_expr->index = idx;
            lhs = index_expr;
        }
    }

    while (!lexer._tokens.is_empty()) {
       Token op = lexer.peak();
       auto op_index = is_operator(op);
       if (op_index == 0 || prec(op_index) < min_prec) break;
       // An assignment (and the `:=` of a for-loop counter) is a complete
       // statement; never chain a binary operator after it. Otherwise the
       // next statement's `*`/`&` would be consumed as a binary operator.
       // The same applies to other statement-level constructs (if/for/return/
       // function/struct/block), whose leading `^`/`&` must start a fresh
       // statement instead of being chained as an operator.
        if (lhs->type == Expr_Assignment || lhs->type == Expr_If
            || lhs->type == Expr_For || lhs->type == Expr_Return
            || lhs->type == Expr_Break || lhs->type == Expr_Continue
            || lhs->type == Expr_Function || lhs->type == Expr_Struct
            || lhs->type == Expr_Block || lhs->type == Expr_Assert
            || lhs->type == Expr_Run || lhs->type == Expr_ComptimeLib)
            break;

       lexer.next();
       Expression* rhs = parse_expression(lexer, nullptr, prec(op_index) + 1, allow_assignment);
       if (!rhs) {
           compiler_error(op, "Expected expression after operator: '" SV_FORMAT "'\n", SV_ARG(op.val));
       }
        if (op.type == Tok_Cast && rhs->type == Expr_Binary) {
            // `expr as u8^` / `expr as bool^^`: the `^` suffixes after the cast
            // type keyword are the pointer indirection depth, so `x as u8^` is a
            // pointer re-interpretation cast, never `(x as u8) ^ something`.
            // Struct names lex as identifiers, so `as Pair^` must resolve the
            // name against visible structs too, or its `^` leaks into the next
            // statement and garbles the parse.
            auto* cast_type = static_cast<BinaryExpr*>(rhs);
            const bool names_type = cast_type->tok.type == Tok_Type
                || (cast_type->tok.type == Tok_Ident && !cast_type->lhs
                    && find_visible_struct(cast_type->tok.val, cast_type->module_name) != Array<DeclaredStruct>::INVALID_INDEX);
            if (names_type) {
                while (lexer.peak().type == Tok_Caret) {
                    lexer.next();
                    ++cast_type->ptr_depth;
                }
            }
        }
       lhs = new_binary_expr(op, lhs, rhs);
    }

    // `arr[i] = v`: an indexed store.
    if (allow_assignment && lhs && lhs->type == Expr_Index) {
        auto next = lexer.peak();
        if (next.type == Tok_Equals || next.type == Tok_ColonEquals) {
            lexer.next();
            auto* rhs = parse_expression(lexer);
            if (!rhs)
                compiler_error(next, "Expected expression after assignment\n");
            skip_semicolon_if_exist(lexer);
            auto* idx = static_cast<IndexExpr*>(lhs);
            auto* assign = new_expr<IndexAssignExpr>(idx->tok, Expr_IndexAssign);
            assign->base = idx->base;
            assign->index = idx->index;
            assign->rhs = rhs;
            return assign;
        }
    }

    // `^p = v`, `^^p = v`, `^(&x) = v`: the leading `^` is the store marker,
    // so the store's target is the deref chain with one level stripped.
    // This runs only at statement/expression level (not inside a unary
    // operand), so the `=` after a full `^^p` chain is handled here.
    if (allow_assignment && lhs && lhs->type == Expr_Deref) {
        auto next = lexer.peak();
        if (next.type == Tok_Equals || next.type == Tok_ColonEquals) {
            lexer.next();
            auto* rhs = parse_expression(lexer);
            if (!rhs)
                compiler_error(next, "Expected expression after assignment\n");
            skip_semicolon_if_exist(lexer);
            auto* deref = static_cast<DerefExpr*>(lhs);
            auto* assign = new_expr<DerefAssignExpr>(deref->tok, Expr_DerefAssign);
            assign->target = deref->target;
            assign->rhs = rhs;
            return assign;
        }
    }
    // `=` / `:=` after anything that isn't a variable (handled by
    // parse_ident) or a deref chain (handled above) is an rvalue: reject it
    // with a clear lvalue error instead of a confusing token error.
    if (allow_assignment) {
        auto next = lexer.peak();
        if (next.type == Tok_Equals || next.type == Tok_ColonEquals)
            compiler_error(next, "Cannot assign to a non-lvalue expression\n");
    }
    return lhs;
}

DeclaredString* get_string(StrView name, usize* index)
{
    for (usize i = 0; i < g_strings.count(); ++i) {
        auto& str = g_strings[i];
        if (str.name == name) {
            if (index) *index = i;
            return &str;
        }
    }
    return nullptr;
}

// Byte length of a string literal with escapes collapsed, matching how
// append_hex emits bytes: `\n`/`\"`/`\\` each count as one byte, any other
// character counts as itself.
s64 string_literal_byte_len(StrView str)
{
    s64 len = 0;
    usize i = 0;
    while (i < str.size) {
        char c = str.data[i];
        if (c == '\\' && i + 1 < str.size) {
            char esc = str.data[i + 1];
            if (esc == 'n' || esc == '"' || esc == '\\')
                ++i;
        }
        ++len;
        ++i;
    }
    return len;
}

// Append a new string literal and assign its offset. Strings are append-only
// and every offset equals the cumulative sum of the preceding (name.size + 1)
// terms, so the next offset is O(1) from the last entry rather than a rescan.
DeclaredString* push_string(StrView name, BinaryExpr* expr, usize* index)
{
    u64 offset = 0;
    if (!g_strings.is_empty())
        offset = g_strings.last().offset + g_strings.last().name.size + 1;
    g_strings.push(DeclaredString{name, expr, offset});
    if (index) *index = g_strings.count() - 1;
    return &g_strings.last();
}

void add_string(BinaryExpr* expr)
{
    auto name = expr->tok.val;
    if (get_string(name) == nullptr)
        push_string(name, expr, nullptr);
}

DeclaredString* get_or_add_string(StrView name, usize* index)
{
    DeclaredString* str = get_string(name, index);
    if (str) return str;
    return push_string(name, nullptr, index);
}

void add_struct_or_report_if_exit(StructExpr* expr)
{
    for (auto& struc : g_structs) {
        if (struc.name == expr->tok.val && struc.module_name == g_current_module_name) {
            compiler_error(expr->tok, "Struct '" SV_FORMAT "' already declared in module '" SV_FORMAT "'\n", SV_ARG(struc.name), SV_ARG(g_current_module_name));
        }
    }
    g_structs.push(DeclaredStruct{expr->tok.val, expr});
    g_structs.last().module_name = g_current_module_name;
}

void add_function_or_report_if_exit(FunctionExpr* expr)
{
    for (auto& fun : g_functions) {
        if (fun.name == expr->tok.val && fun.module_name == g_current_module_name) {
            compiler_error(expr->tok, "Function '" SV_FORMAT "' already declared in module '" SV_FORMAT "'\n", SV_ARG(fun.name), SV_ARG(g_current_module_name));
        }
    }
    g_functions.push(DeclaredFunction{expr->tok.val, expr, {}, {}, expr->return_type, expr->return_struct_name, expr->return_struct_module, expr->return_pointee, expr->return_ptr_depth, expr->return_pointee_struct_name, expr->return_pointee_struct_module});
    g_functions.last().is_extern = expr->is_extern;
    g_functions.last().module_name = g_current_module_name;
    // Remember the declaring file so errors during body translation (which
    // runs after all files are parsed) report the right file and line.
    g_functions.last().src_path = src_path;
    g_functions.last().src_content = src_content;

    // `main` is special: __entry forwards the OS command line to it, so its
    // signature is constrained to `(argc : i64)` and/or `(argc : i64,
    // argv : ptr)`. Validated at declaration time (not in compile_program) so
    // the error is reported reliably by the test harness.
    if (expr->tok.val == "main") {
        auto& args = expr->arg_types;
        auto arg_count = args.count();
        if (arg_count > 2)
            compiler_error(expr->tok, "'main' takes at most 2 arguments (argc : i64, argv : str^)\n");
        if (arg_count >= 1 && args[0].type != TYPE_I64)
            compiler_error(expr->tok, "First parameter of 'main' must be 'i64'\n");
        if (arg_count >= 2) {
            if (args[1].type != TYPE_PTR || args[1].pointee != TYPE_STR || args[1].ptr_depth != 1)
                compiler_error(expr->tok, "Second parameter of 'main' must be 'str^'\n");
        } 
        if (is_windows_target() && arg_count > 0)
            compiler_error(expr->tok, "'main' with command line arguments is not supported on the Windows target yet\n");
    }
}

bool parse(Lexer& lexer, Array<Expression*>& exprs) {
    // Every file starts in the global module; a `module` declaration as the
    // first expression switches the file into a named module.
    g_current_module_name = "";
    Expression* expr = nullptr;
    bool is_first = true;
    do {
        expr = parse_primary(lexer);
        if (!expr) return false;
        if (expr->type == Expr_Module) {
            auto* me = static_cast<ModuleExpr*>(expr);
            if (!is_first)
                compiler_error(me->tok, "Module declaration must be the first expression in a file\n");
            if (me->tok.val == "global")
                compiler_error(me->tok, "'global' is a reserved name and cannot be used as a module name\n");
            g_current_module_name = me->tok.val;
            find_or_add_module(g_current_module_name);
        } else if (expr->type == Expr_ImportModule) {
            auto* ie = static_cast<ModuleImportExpr*>(expr);
            if (ie->tok.val == g_current_module_name)
                compiler_error(ie->tok, "Module '" SV_FORMAT "' cannot import itself\n", SV_ARG(ie->tok.val));
            auto* mod = find_or_add_module(g_current_module_name);
            if (!mod->imports.contains(ie->tok.val))
                mod->imports.push(ie->tok.val);
        }
        exprs.push(expr);
        is_first = false;
    } while (!lexer._tokens.is_empty() && expr);

    return true;
}


bool is_valid_operation(InstructionType operation_type, ValueType lhs, ValueType rhs, ValueType& result_type)
{
    switch(operation_type) {
        case OP_MINUS:
        case OP_PLUS:
        case OP_MULT:
        case OP_DIVIDE:
        case OP_MOD: {
            if (lhs == TYPE_BOOL || rhs == TYPE_BOOL)
                return false;
            if (is_numeric_type(lhs) && is_numeric_type(rhs)) {
                result_type = promote_type(lhs, rhs);
                return true;
            }
            // Pointer arithmetic: exactly one pointer operand mixed with a
            // number, and only for +/- (`argv + i`, `p - 8`). Multiplying,
            // dividing, or modulo-ing addresses (and any use of strings,
            // arrays or structs here) is invalid.
            if ((operation_type == OP_PLUS || operation_type == OP_MINUS)
                && ((lhs == TYPE_PTR && is_numeric_type(rhs))
                    || (rhs == TYPE_PTR && is_numeric_type(lhs)))) {
                result_type = TYPE_PTR;
                return true;
            }
            return false;
        }

        case OP_GREATER_EQUALS:
        case OP_GREATER:
        case OP_LESS_EQUALS:
        case OP_LESS: {
            if (lhs == TYPE_BOOL || rhs == TYPE_BOOL)
                return false;
            if (lhs == TYPE_PTR || rhs == TYPE_PTR) {
                // All relational operations order pointers by their raw
                // addresses (compared as unsigned). Mixing a pointer with a
                // non-pointer operand stays invalid.
                if (lhs != TYPE_PTR || rhs != TYPE_PTR)
                    return false;
                result_type = TYPE_BOOL;
                return true;
            }
            if (!is_numeric_type(lhs) && !is_numeric_type(rhs))
                return false;
            result_type = TYPE_BOOL;
            return true;
        } break;

        case OP_EQUALS:
        case OP_NOT_EQUALS: {
            if (lhs == TYPE_PTR || rhs == TYPE_PTR) {
                // Equality/inequality orders pointers by their raw addresses.
                if (lhs != TYPE_PTR || rhs != TYPE_PTR)
                    return false;
                result_type = TYPE_BOOL;
                return true;
            }
            // Equality is defined for numbers, for booleans (which hold 0/1),
            // and for a type name used as a constant value (e.g. a string type
            // id compared via `type_id(x) == string`). Mixed bool/numeric and
            // everything else (two strings, void, ...) is not.
            const bool lhs_bool = lhs == TYPE_BOOL, rhs_bool = rhs == TYPE_BOOL;
            if (lhs_bool && rhs_bool) {
                result_type = TYPE_BOOL;
                return true;
            }
            if (lhs_bool != rhs_bool)
                return false;
            const bool lhs_num = is_numeric_type(lhs), rhs_num = is_numeric_type(rhs);
            if (lhs_num && rhs_num) {
                result_type = TYPE_BOOL;
                return true;
            }
            if ((lhs_num && rhs == TYPE_STR) || (lhs == TYPE_STR && rhs_num)) {
                result_type = TYPE_BOOL;
                return true;
            }
            return false;
        } break;

        case OP_CAST: {
            // A same-type cast is a no-op (e.g. reassigning `p = &y` re-casts
            // a PTR value to the PTR target type).
            if (lhs == rhs) {
                result_type = rhs;
                return true;
            }
            switch (rhs) {
                case TYPE_I8:
                case TYPE_I16:
                case TYPE_I32:
                case TYPE_I64:
                case TYPE_U8:
                case TYPE_U16:
                case TYPE_U32:
                case TYPE_U64: {
                    if (!is_numeric_type(lhs) && lhs != TYPE_BOOL)
                        return false;
                } break;

                case TYPE_STR: {
                    if (lhs != TYPE_STR)
                        return false;
                } break;

                case TYPE_BOOL: {
                    if (!is_numeric_type(lhs) && lhs != TYPE_BOOL)
                        return false;
                } break;

                default: {
                    // void/ptr/struct/nop cannot be cast targets.
                    return false;
                } break;
            }
            result_type = rhs;
            return true;
        }

        default: TODO(inst_type_to_str(operation_type));
    }
    return false;
}

VirtualReg& allocate_reg(Array<VirtualReg>& regs)
{
    // Offsets are assigned later by update_all_offsets, which lays out only the
    // spilled, visited registers (args and locals share one frame).
    regs.push(VirtualReg{regs.count(), 0});
    return regs.last();
}

void update_all_offsets(Array<VirtualReg>& regs)
{
    // Every spilled (phys == PR_NONE) visited register gets a unique offset
    // into the callee's own stack frame. Registers allocated to a physical
    // register reuse no slot: nothing ever writes their stack home while the
    // value lives in the register (PUSH targets are always comp-time regs,
    // which are never allocated). Args are copied into this frame at the
    // function prologue, so they must live in the same offset space as locals
    // (a shared counter avoids an arg and a local aliasing the same slot).
    u32 offset = 0;
    for (auto& reg : regs) {
        if (!reg.is_visited) continue;
        // Global bindings keep their __globals offset; only frame slots are
        // laid out here.
        if (reg.is_global) { reg.phys = PR_NONE; continue; }
        if (reg.phys != PR_NONE) { reg.offset = 0; continue; }
        offset += STACK_REGISTER_SIZE;
        reg.offset = offset;
    }
}

// Bind a variable for use with the current `regs` array. Real locals are
// returned as-is. Globals are bound into `local_vars` as a per-function copy
// with a fresh virtual register that addresses the shared __globals slot
// (is_global + offset = the global's fixed slot). Returns nullptr when the
// variable does not exist.
Variable* bind_global_variable(Token tok, Array<Variable>* local_vars, Array<VirtualReg>& regs, StrView module_name = "")
{
    if (!local_vars) return nullptr;
    // A local shadows a global only for the plain (unqualified) form; a
    // qualified reference always resolves to the named module's global.
    Variable* found = module_name.size > 0
        ? find_global_variable(tok.val, module_name)
        : get_variable(tok, false, local_vars);
    if (!found) return nullptr;
    if (found->is_local) return found;
    // Reuse the per-function copy of a global so the same variable keeps one
    // virtual register. The match is on name *and* module: `a::gx` and `b::gx`
    // are different globals that only share a name.
    for (auto& lv : *local_vars)
        if (lv.name == tok.val && lv.module_name == found->module_name && lv.is_accesible && !lv.is_local)
            return &lv;
    local_vars->push(*found);
    auto& copy = local_vars->last();
    copy.is_local = false;
    auto& g = allocate_reg(regs);
    g.is_global = true;
    g.is_visited = true;
    g.type = found->type;
    g.offset = (u32)found->global_offset;
    copy.reg_index = g.index;
    return &local_vars->last();
}

// Fill `uses[r]` with the number of visited instructions that read register r
// as an operand (not as their result/definition). Built once per function in a
// single pass so compile_ops can test a comparison result's single consumer in
// O(1) for cmp+jcc fusion.
void compute_reg_uses(Array<Instruction>& ops, usize reg_count, Array<usize>& uses)
{
    uses.reserve(reg_count);
    uses.set_count(reg_count);
    uses.memzero();
    for (auto& inst : ops) {
        if (!inst.is_visited) continue;
        switch (inst.type) {
            case OP_PLUS:
            case OP_MINUS:
            case OP_MOD:
            case OP_DIVIDE:
            case OP_MULT:
            case OP_EQUALS:
            case OP_NOT_EQUALS:
            case OP_LESS:
            case OP_LESS_EQUALS:
            case OP_GREATER_EQUALS:
            case OP_GREATER: {
                ++uses[inst.binop.lhs_index];
                ++uses[inst.binop.rhs_index];
            } break;
            case OP_INC: {
                ++uses[inst.binop.lhs_index];
            } break;
            case OP_NOT: {
                ++uses[inst.binop.lhs_index];
            } break;
            case OP_CAST: {
                ++uses[inst.binop.lhs_index];
            } break;
            case OP_JMP_IF:
            case OP_PRINT:
            case OP_RET: {
                ++uses[inst.reg_index];
            } break;
            case OP_STORE: {
                ++uses[inst.reg_index];
            } break;
            case OP_LEA: {
                ++uses[inst.target.var.reg_index];
            } break;
            case OP_LOAD_PTR: {
                ++uses[inst.binop.lhs_index];
            } break;
            case OP_STORE_PTR: {
                ++uses[inst.binop.lhs_index];
                ++uses[inst.binop.rhs_index];
            } break;
            case OP_CALL: {
                for (auto& arg : inst.call.args)
                    ++uses[arg.reg_index];
            } break;
            default: break;
        }
    }
}


template <typename T>
bool is_power_of_two(T n) {
    return n > 0 && (n & (n - 1)) == 0;
}

template <typename T>
T get_power_of_two_exponent(T n) {
    T exponent = 0;
    while (n > 1) {
        n >>= 1;  // Right shift by 1 (same as n /= 2)
        exponent++;
    }
    return exponent;
}

// `(1 << k) - 1` computed in u64 so k == 63 (shifting into the sign bit)
// stays well-defined.
static s64 pow2_mask(s64 k) {
    return (s64)(((u64)1 << k) - 1);
}

// Peephole optimization for generated function bodies. The codegen routes
// every value through rax (load_reg / store_reg), which leaves redundant
// moves behind around pure register traffic. Two rewrites, both of which
// leave rax holding the value it held before the sequence:
//   store->reload:  mov <dst>, rax \n mov rax, <dst>  -> keep only the store
//   load->forward:  mov rax, <src> \n mov <dst>, rax  -> mov <dst>, <src>
// The forward merge additionally requires that the line following the moved
// value does not reference rax, so rax's loaded value is never expected to
// survive past the instruction that consumes it.
static void peephole_asm(StrBuilder& builder, usize start) {
    const char* s = builder.data();
    const usize end = builder.count();

    StrBuilder out;
    defer(out.cleanup());
    out.reserve(end - start + 1);

    auto parse_mov = [](const char* l, usize len, usize& a_off, usize& a_len,
                        usize& b_off, usize& b_len) -> bool {
        usize k = 0;
        while (k < len && (l[k] == ' ' || l[k] == '\t')) ++k;
        if (len - k < 4 || memcmp(l + k, "mov ", 4) != 0) return false;
        k += 4;
        while (k < len && l[k] == ' ') ++k;
        a_off = k;
        while (k < len && l[k] != ',') ++k;
        if (k >= len) return false;
        a_len = k - a_off;
        if (a_len == 0) return false;
        ++k;
        while (k < len && (l[k] == ' ' || l[k] == '\t')) ++k;
        b_off = k;
        b_len = len - k;
        while (b_len > 0 && (l[b_off + b_len - 1] == ' ' ||
                             l[b_off + b_len - 1] == '\t' ||
                             l[b_off + b_len - 1] == '\r'))
            --b_len;
        return b_len > 0;
    };
    auto is_rax = [](const char* l, usize len) -> bool {
        return len == 3 && memcmp(l, "rax", 3) == 0;
    };
    auto contains_rax = [](const char* l, usize len) -> bool {
        for (usize k = 0; k + 3 <= len; ++k)
            if (l[k] == 'r' && l[k + 1] == 'a' && l[k + 2] == 'x') return true;
        return false;
    };
    auto is_reg64 = [](const char* l, usize len) -> bool {
        static const char* regs[] = {"rdi", "rsi", "rdx", "rcx", "r8",  "r9",
                                     "r10", "r11", "r12", "r13", "r14", "r15",
                                     "rbx", "rbp", "rsp"};
        for (const char* r : regs) {
            usize rl = strlen(r);
            if (len == rl && memcmp(l, r, rl) == 0) return true;
        }
        return false;
    };
    // Register names that accept `xor r, r` (64-bit and their 32-bit aliases;
    // rax is included here since it is only ever a scratch in user code).
    auto is_xor_reg = [](const char* l, usize len) -> bool {
        static const char* regs[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi",
                                     "rbp", "rsp", "r8",  "r9",  "r10", "r11",
                                     "r12", "r13", "r14", "r15", "eax", "ebx",
                                     "ecx", "edx", "esi", "edi", "ebp", "r8d",
                                     "r9d", "r10d", "r11d", "r12d", "r13d",
                                     "r14d", "r15d"};
        for (const char* r : regs) {
            usize rl = strlen(r);
            if (len == rl && memcmp(l, r, rl) == 0) return true;
        }
        return false;
    };

    // Parse `add <reg>, 1` or `sub <reg>, 1` (exactly, trailing whitespace
    // aside). Returns +1 for add, -1 for sub, 0 otherwise, and on success
    // fills the destination operand's offset/length.
    auto parse_add_sub_one = [](const char* l, usize len, usize* a_off,
                                usize* a_len) -> int {
        usize k = 0;
        while (k < len && (l[k] == ' ' || l[k] == '\t')) ++k;
        if (len - k < 4) return 0;
        int sign;
        if (memcmp(l + k, "add ", 4) == 0) sign = 1;
        else if (memcmp(l + k, "sub ", 4) == 0) sign = -1;
        else return 0;
        k += 4;
        usize ds = k;
        while (k < len && ((l[k] >= 'a' && l[k] <= 'z') ||
                           (l[k] >= '0' && l[k] <= '9')))
            ++k;
        if (k == ds) return 0;
        *a_off = ds;
        *a_len = k - ds;
        // Require exactly `, 1` after the destination.
        if (len - k != 3 || l[k] != ',' || l[k + 1] != ' ' || l[k + 2] != '1')
            return 0;
        return sign;
    };

    // Parse a jump to an internal label. Returns 1 for a conditional
    // `j<cc> .lab_<n>` (filling the cc mnemonic), 2 for an unconditional
    // `jmp .lab_<n>`, and 0 otherwise.
    auto parse_jump = [](const char* l, usize len, char cc[4],
                         s64* target) -> int {
        usize k = 0;
        while (k < len && (l[k] == ' ' || l[k] == '\t')) ++k;
        if (len - k < 4 || l[k] != 'j') return 0;
        usize j = k + 1;
        usize e = j;
        while (e < len && ((l[e] >= 'a' && l[e] <= 'z') ||
                           (l[e] >= 'A' && l[e] <= 'Z')))
            ++e;
        if (e == j || e >= len || l[e] != ' ') return 0;
        const int kind = (e - j == 2 && memcmp(l + j, "mp", 2) == 0) ? 2 : 1;
        if (kind == 1 && e - j > 2) return 0;
        if (len - (e + 1) < 5 || memcmp(l + (e + 1), ".lab_", 5) != 0) return 0;
        usize p = e + 6;
        s64 num = 0;
        bool any = false;
        while (p < len && l[p] >= '0' && l[p] <= '9') {
            num = num * 10 + (l[p] - '0');
            ++p;
            any = true;
        }
        if (!any || p != len) return 0;
        if (kind == 1) {
            cc[0] = l[j];
            if (e - j == 2) cc[1] = l[j + 1];
            cc[e - j] = 0;
        }
        *target = num;
        return kind;
    };

    // Parse an internal label definition `.lab_<n>:`. Returns its number.
    auto parse_label = [](const char* l, usize len, s64* target) -> bool {
        usize k = 0;
        while (k < len && (l[k] == ' ' || l[k] == '\t')) ++k;
        if (len - k < 6 || memcmp(l + k, ".lab_", 5) != 0) return false;
        usize p = k + 5;
        s64 num = 0;
        bool any = false;
        while (p < len && l[p] >= '0' && l[p] <= '9') {
            num = num * 10 + (l[p] - '0');
            ++p;
            any = true;
        }
        if (!any || p >= len || l[p] != ':') return false;
        ++p;
        while (p < len && (l[p] == ' ' || l[p] == '\t')) ++p;
        if (p != len) return false;
        *target = num;
        return true;
    };

    // Inverse of a conditional-jump condition suffix (the letters after `j`),
    // or nullptr when unknown.
    auto invert_jcc = [](const char* cc) -> const char* {
        struct Inv { const char* from; const char* to; };
        static const Inv tab[] = {
            {"e", "ne"},  {"ne", "e"},  {"z", "nz"},  {"nz", "z"},
            {"b", "ae"},  {"ae", "b"},  {"l", "ge"},  {"ge", "l"},
            {"be", "a"},  {"a", "be"},  {"le", "g"},  {"g", "le"},
            {"s", "ns"},  {"ns", "s"},  {"c", "nc"},  {"nc", "c"},
            {"p", "np"},  {"np", "p"},  {"o", "no"},  {"no", "o"},
        };
        for (auto& entry : tab)
            if (strcmp(entry.from, cc) == 0) return entry.to;
        return nullptr;
    };

    usize i = start;
    while (i < end) {
        usize clen = 0;
        while (i + clen < end && s[i + clen] != '\n') ++clen;
        usize nl = (i + clen < end) ? 1 : 0;

        usize j1 = i + clen + nl;
        usize nlen = 0;
        while (j1 + nlen < end && s[j1 + nlen] != '\n') ++nlen;
        usize nl1 = (j1 + nlen < end) ? 1 : 0;

        usize j2 = j1 + nlen + nl1;
        usize nnlen = 0;
        while (j2 + nnlen < end && s[j2 + nnlen] != '\n') ++nnlen;
        usize nl2 = (j2 + nnlen < end) ? 1 : 0;

        const char* L1 = s + i;
        const char* L2 = s + j1;
        const char* L3 = s + j2;

        usize a1o, a1l, b1o, b1l, a2o, a2l, b2o, b2l;
        bool m1 = parse_mov(L1, clen, a1o, a1l, b1o, b1l);
        bool m2 = nl1 > 0 && parse_mov(L2, nlen, a2o, a2l, b2o, b2l);

        // store->reload: mov <dst>, rax \n mov rax, <dst> -> drop the reload
        if (m1 && m2 && is_rax(L1 + b1o, b1l) && is_rax(L2 + a2o, a2l) &&
            a1l == b2l && memcmp(L1 + a1o, L2 + b2o, a1l) == 0 &&
            !is_rax(L1 + a1o, a1l)) {
            out.append(L1, clen);
            out.append('\n');
            i = j1 + nlen + nl1;
            continue;
        }

        // load->forward: mov rax, <src> \n mov <dst>, rax -> mov <dst>, <src>
        if (m1 && m2 && is_rax(L1 + a1o, a1l) && is_rax(L2 + b2o, b2l) &&
            is_reg64(L2 + a2o, a2l) && !contains_rax(L1 + b1o, b1l)) {
            // The merge drops rax's loaded value, so it may only fire when rax
            // is dead after the forward: the next instruction is a call (which
            // clobbers rax) or an unconditional jump (which leaves this code
            // path), or nothing follows at all.
            bool rax_dead = j2 >= end;
            if (!rax_dead) {
                usize k = 0;
                while (k < nnlen && (L3[k] == ' ' || L3[k] == '\t')) ++k;
                const char* l3 = L3 + k;
                usize l3len = nnlen - k;
                if ((l3len >= 5 && memcmp(l3, "call ", 5) == 0) ||
                    (l3len >= 4 && memcmp(l3, "jmp ", 4) == 0))
                    rax_dead = true;
            }
            if (rax_dead) {
                // Forwarding a literal zero into a plain register is best
                // emitted as xor, which never reads the flags register (the
                // compiler always emits explicit test/cmp before a flag read).
                if (b1l == 1 && L1[b1o] == '0' && is_xor_reg(L2 + a2o, a2l)) {
                    out.append('\t');
                    out.append("xor ", 4);
                    out.append(L2 + a2o, a2l);
                    out.append(", ", 2);
                    out.append(L2 + a2o, a2l);
                    out.append('\n');
                } else {
                    out.append('\t');
                    out.append("mov ", 4);
                    out.append(L2 + a2o, a2l);
                    out.append(", ", 2);
                    out.append(L1 + b1o, b1l);
                    out.append('\n');
                }
                i = j1 + nlen + nl1;
                continue;
            }
        }

        // add reg, 1 -> inc reg ; sub reg, 1 -> dec reg. inc/dec leave the
        // carry flag untouched, but the compiler never reads flags from
        // arithmetic (always an explicit test/cmp first), so this is safe.
        {
            usize a_off, a_len;
            int sign = parse_add_sub_one(L1, clen, &a_off, &a_len);
            if (sign != 0 && is_xor_reg(L1 + a_off, a_len)) {
                out.append(sign > 0 ? "\tinc " : "\tdec ", 5);
                out.append(L1 + a_off, a_len);
                out.append('\n');
                i = j1;
                continue;
            }
        }

        // jcc .lab_A \n jmp .lab_B \n .lab_A: -> invert the jcc to .lab_B and
        // let the not-taken path fall through into .lab_A, dropping the jmp.
        if (nl1 > 0) {
            char cc1[4];
            s64 tA = -1;
            s64 tB = -1;
            char cc2[4];
            int k1 = parse_jump(L1, clen, cc1, &tA);
            int k2 = parse_jump(L2, nlen, cc2, &tB);
            s64 lab = -1;
            if (k1 == 1 && k2 == 2 && parse_label(L3, nnlen, &lab) && tA == lab) {
                const char* inv = invert_jcc(cc1);
                if (inv) {
                    out.append('\t');
                    out.append('j');
                    out.append(inv, strlen(inv));
                    out.append(" .lab_") << tB << '\n';
                    out.append(L3, nnlen);
                    if (nl2) out.append('\n');
                    i = j1 + nlen + nl1 + nnlen + nl2;
                    continue;
                }
            }
        }

        // mov reg, 0 -> xor reg, reg (shorter; only for plain register
        // destinations, and rax is a safe scratch in user code).
        if (m1 && b1l == 1 && L1[b1o] == '0' && is_xor_reg(L1 + a1o, a1l)) {
            out.append('\t');
            out.append("xor ", 4);
            out.append(L1 + a1o, a1l);
            out.append(", ", 2);
            out.append(L1 + a1o, a1l);
            out.append('\n');
            i = j1;
            continue;
        }

        out.append(L1, clen);
        if (nl) out.append('\n');
        i = j1;
    }

    builder.set_count(start);
    builder.append(out.data(), out.count());
}

// Emit `mov dst, <reg value>` where the value is reg's compile-time constant,
// its allocated physical register, or the content of its stack slot.
static void load_reg(StrBuilder& builder, const char* dst, const VirtualReg& reg) {
    if (reg.is_comp_time)
        (builder.append("\tmov ").append(dst)).append(", ") << reg.int_val << '\n';
    else if (reg.phys != PR_NONE)
        (builder.append("\tmov ").append(dst)).append(", ") << phys_name(reg.phys) << '\n';
    else if (reg.is_global)
        ((builder.append("\tmov ").append(dst)).append(", [__globals + ") << reg.offset).append("]\n");
    else
        ((builder.append("\tmov ").append(dst)).append(", [rbp - ") << reg.offset).append("]\n");
}

// Emit `mov <res location>, src` where the result goes to res's physical
// register (nothing is emitted if it is already there) or to its stack slot.
static void store_reg(StrBuilder& builder, const VirtualReg& res, const char* src) {
    if (res.phys != PR_NONE) {
        auto dst = phys_name(res.phys);
        if (strcmp(src, dst.data) != 0)
            (builder.append("\tmov ").append(dst)).append(", ") << src << '\n';
    } else if (res.is_global) {
        (builder.append("\tmov [__globals + ") << res.offset).append("], ") << src << '\n';
    } else {
        (builder.append("\tmov [rbp - ") << res.offset).append("], ") << src << '\n';
    }
}

// Like load_reg, but skips the move when reg already lives in dst.
static void load_reg_if_needed(StrBuilder& builder, const char* dst, const VirtualReg& reg) {
    if (reg.phys != PR_NONE && strcmp(phys_name(reg.phys).data, dst) == 0) return;
    load_reg(builder, dst, reg);
}

// Truncate/reinterpret the value in rax to `type` (zero- or sign-extending
// from the smaller of the source and target widths when from_width > 0).
// No conversion for bool/i64/u64/strings/pointers (the stored pattern already
// matches, or a cast to those types was never emitted).
static void emit_truncate(StrBuilder& builder, ValueType type, u32 from_width) {
    switch (type) {
        case TYPE_I8:   builder.append("\tmovsx rax, al\n"); break;
        case TYPE_I16:  builder.append("\tmovsx rax, ax\n"); break;
        case TYPE_I32:  builder.append("\tmovsxd rax, eax\n"); break;
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64: {
            const u32 w = from_width > 0 ? MIN(from_width, type_size(type)) : type_size(type);
            if (w == 1)      builder.append("\tmovzx rax, al\n");
            else if (w == 2) builder.append("\tmovzx rax, ax\n");
            else if (w < 8)  builder.append("\tmov eax, eax\n"); // writing eax zero-extends to rax
            // w == 8 (u64): the full 64-bit value is already in rax; no-op.
        } break;
        default: break;
    }
}

// Truncate a value sitting in 64-bit register `dest` to a 32-bit result type.
// Binary operations on sub-32-bit operands promote to i32/u32, and the low 32
// bits of the result are already correct, so i32 sign-extends (movsxd) and u32
// zero-extends (mov edst,edst) — keeping the stored value canonical.
// Map a 64-bit register name to its 32-bit alias (rax -> eax, r12 -> r12d).
static void append_reg32(StrBuilder& builder, const char* dest) {
    if (dest[1] >= '0' && dest[1] <= '9')
        builder.append(dest).append('d'); // r8..r15 -> r8d..r15d
    else
        builder.append('e').append(dest + 1); // rax/rsi/rdi -> eax/esi/edi
}

static void emit_truncate_dest(StrBuilder& builder, const char* dest, ValueType type) {
    if (type_size(type) != 4) return;
    builder.append(type == TYPE_I32 ? "\tmovsxd " : "\tmov ");
    if (type == TYPE_I32)
        builder.append(dest); // movsxd needs the 64-bit destination (rax, r12)
    else
        append_reg32(builder, dest); // mov eax, eax zero-extends
    builder.append(", ");
    append_reg32(builder, dest);
    builder.append('\n');
}

// Append the second operand of a binary instruction after the leading
// "op <dst>, " text, using reg's actual location: an immediate, its physical
// register, or its stack slot (a register destination disambiguates size).
static void emit_rhs_operand(StrBuilder& builder, const VirtualReg& rhs) {
    if (rhs.is_comp_time)
        builder << rhs.int_val << '\n';
    else if (rhs.phys != PR_NONE)
        builder.append(phys_name(rhs.phys)).append('\n');
    else if (rhs.is_global)
        (builder.append("[__globals + ") << rhs.offset).append("]\n");
    else
        (builder.append("[rbp - ") << rhs.offset).append("]\n");
}

// Emit `mov dst, <lhs>; <op> dst, <rhs>; store res` using the operands'
// actual locations instead of always shuffling through the rax/rbx scratch
// pair: the result is computed in res's own register when it does not alias
// rhs (loading lhs there), falling back to rax otherwise. Commutativity is
// irrelevant; only correctness of the destination choice matters.
static void emit_binary_op(StrBuilder& builder, const char* op,
                           const VirtualReg& lhs, const VirtualReg& rhs, const VirtualReg& res) {
    const char* dest = "rax";
    if (res.phys != PR_NONE) {
        const char* res_name = phys_name(res.phys).data;
        bool clobbers_rhs = rhs.phys != PR_NONE && strcmp(phys_name(rhs.phys).data, res_name) == 0;
        if (!clobbers_rhs)
            dest = res_name;
    }
    load_reg_if_needed(builder, dest, lhs);
    builder.append('\t').append(op).append(' ').append(dest).append(", ");
    emit_rhs_operand(builder, rhs);
    emit_truncate_dest(builder, dest, res.type);
    store_reg(builder, res, dest);
}

// Emit `cmp lhs, rhs` using the operands' actual locations: lhs is compared in
// place when it lives in a register (cmp only reads), otherwise loaded into
// rax. rhs is used as an immediate, register, or memory operand directly.
static void emit_cmp(StrBuilder& builder, const VirtualReg& lhs, const VirtualReg& rhs) {
    if (lhs.phys != PR_NONE) {
        builder.append("\tcmp ").append(phys_name(lhs.phys)).append(", ");
        emit_rhs_operand(builder, rhs);
    } else {
        load_reg(builder, "rax", lhs);
        builder.append("\tcmp rax, ");
        emit_rhs_operand(builder, rhs);
    }
}

// Try to strength-reduce add/sub/mult into a single `lea` (or a trivial copy /
// xor when the op degenerates), matching the exact results of the mov+op
// fallbacks. The compiler never consumes arithmetic flags (it always emits an
// explicit `test`/`cmp` before a flag-reading instruction), so dropping the
// flag writes of add/sub/imul is safe. `op`: 0 = add, 1 = sub, 2 = imul.
// Returns true when the operation was emitted.
static bool emit_lea_opt(StrBuilder& builder, int op,
                         const VirtualReg& lhs, const VirtualReg& rhs, const VirtualReg& res) {
    const char* dest = (res.phys != PR_NONE) ? phys_name(res.phys).data : "rax";

    // --- add/sub against a compile-time constant ---------------------------
    if (op == 0 || op == 1) {
        const VirtualReg* var = nullptr;
        s64 c = 0;
        if (rhs.is_comp_time) { var = &lhs; c = rhs.int_val; }
        else if (op == 0 && lhs.is_comp_time) { var = &rhs; c = lhs.int_val; }

        if (!var) {
            // add reg, reg with a register result distinct from both operands:
            // lea dest, [a + b] folds the load+add into one instruction.
            if (op == 0 && lhs.phys != PR_NONE && rhs.phys != PR_NONE &&
                res.phys != PR_NONE) {
                const char* a = phys_name(lhs.phys).data;
                const char* b = phys_name(rhs.phys).data;
                if (strcmp(dest, a) != 0 && strcmp(dest, b) != 0) {
                    (builder.append("\tlea ").append(dest)).append(", [").append(a).append(" + ").append(b).append("]\n");
                    emit_truncate_dest(builder, dest, res.type);
                    store_reg(builder, res, dest);
                    return true;
                }
            }
            return false;
        }
        if (var->is_comp_time) return false; // both constants; folded upstream
        if (op == 1) c = -c;

        if (c == 0) {
            // x + 0 / x - 0 is a pure copy: emit the move, not `add dest, 0`.
            if (var->phys != PR_NONE) {
                if (strcmp(dest, phys_name(var->phys).data) != 0)
                    (builder.append("\tmov ").append(dest)).append(", ") << phys_name(var->phys) << '\n';
            } else if (var->is_global) {
                (builder.append("\tmov ").append(dest)).append(", [__globals + ") << var->offset << "]\n";
            } else {
                (builder.append("\tmov ").append(dest)).append(", [rbp - ") << var->offset << "]\n";
            }
            emit_truncate_dest(builder, dest, res.type);
            store_reg(builder, res, dest);
            return true;
        }

        // The lea address is the sum of the operand's location and the
        // displacement, so it only equals the arithmetic result when the
        // operand is a register (a memory location would add the slot's
        // address, not its stored value). The displacement must fit a signed
        // 32-bit immediate.
        if (var->phys == PR_NONE) return false;
        const char* base = phys_name(var->phys).data;
        s64 disp = c;
        if (disp < INT32_MIN || disp > INT32_MAX) return false;

        builder.append("\tlea ").append(dest).append(", [").append(base);
        if (disp > 0) { builder.append(" + "); builder << disp; }
        else if (disp < 0) { builder.append(" - "); builder << -disp; }
        builder.append("]\n");
        emit_truncate_dest(builder, dest, res.type);
        store_reg(builder, res, dest);
        return true;
    }

    // --- mult by a small compile-time constant ------------------------------
    const VirtualReg* var = nullptr;
    s64 c = 0;
    if (rhs.is_comp_time) { var = &lhs; c = rhs.int_val; }
    else if (lhs.is_comp_time) { var = &rhs; c = lhs.int_val; }
    else return false;
    if (var->is_comp_time) return false;
    if (var->phys == PR_NONE) return false; // lea index must be a register

    const char* v = phys_name(var->phys).data;
    switch (c) {
        case 0:
            builder.append("\txor ").append(dest).append(", ").append(dest).append('\n');
            break;
        case 1:
            if (strcmp(dest, v) != 0)
                (builder.append("\tmov ").append(dest)).append(", ") << v << '\n';
            break;
        case 2: case 4: case 8:
            // lea dest, [v*2] / [v*4] / [v*8]
            (builder.append("\tlea ").append(dest)).append(", [").append(v);
            builder.append('*') << c;
            builder.append("]\n");
            break;
        case 3: case 5: case 9:
            // lea dest, [v + v*2] / [v + v*4] / [v + v*8]
            (builder.append("\tlea ").append(dest)).append(", [").append(v);
            (builder.append(" + ").append(v)).append('*') << (c - 1);
            builder.append("]\n");
            break;
        default:
            return false;
    }
    emit_truncate_dest(builder, dest, res.type);
    store_reg(builder, res, dest);
    return true;
}

// Strength-reduce arithmetic against a power-of-two constant into shifts,
// matching the exact results of the imul/idiv fallbacks:
//   x *  2^k   ->  x << k
//   x * -2^k   ->  -(x << k)
//   x /  2^k   ->  (x + ((x >> 63) & (2^k - 1))) >> k   (rounds toward zero)
//   x / -2^k   ->  -(x / 2^k)
//   x % +/-2^k ->  x >= 0 ? x & (2^k - 1) : -(|x| & (2^k - 1))
// Unsigned DIV/MOD (res.type unsigned) use the plain logical forms:
//   u / 2^k -> u >> k,   u % 2^k -> u & (2^k - 1)
// MULT is commutative, so the constant may be on either side; DIV/MOD only
// reduce when the divisor is the compile-time constant.
bool shift_math_optimization(MathType type, StrBuilder& builder, VirtualReg& lhs, VirtualReg& rhs, VirtualReg& res) {
    s64 value;
    const VirtualReg* var;
    if (type == SHIFT_MULT) {
        if (rhs.is_comp_time) {
            value = rhs.int_val;
            var = &lhs;
        } else if (lhs.is_comp_time) {
            value = lhs.int_val;
            var = &rhs;
        } else {
            return false;
        }
    } else {
        if (!rhs.is_comp_time)
            return false;
        value = rhs.int_val;
        var = &lhs;
    }

    // |value| must be a positive power of two; |INT64_MIN| is 2^63, which is
    // only representable in u64, so the magnitude is computed in u64. For
    // unsigned operations the value is interpreted as u64 (a divisor with the
    // high bit set is huge, not negative).
    const bool is_unsigned = is_unsigned_type(promote_type(lhs.type, rhs.type));
    const u64 mag = is_unsigned ? (u64)value : (value < 0 ? (u64)(-(value + 1)) + 1 : (u64)value);
    if (!is_power_of_two(mag))
        return false;
    const bool negative = !is_unsigned && value < 0;
    const s64 k = (s64)get_power_of_two_exponent(mag);

    // Compute in res's own register when possible (falling back to rax); the
    // source is moved there first, so a destination that aliases the source is
    // always safe for the shift/mod forms (unlike add/sub, the destination is
    // only ever read after the source copy).
    const char* dest = (res.phys != PR_NONE) ? phys_name(res.phys).data : "rax";
    load_reg_if_needed(builder, dest, *var);

    switch (type) {
        case SHIFT_MULT: {
            if (k > 0)
                (builder.append("\tshl ").append(dest)).append(", ") << k << '\n';
            if (negative)
                builder.append("\tneg ").append(dest) << '\n';
        } break;

        case SHIFT_DIV: {
            if (is_unsigned) {
                // Logical shift right: truncates toward zero as unsigned div.
                (builder.append("\tshr ").append(dest)).append(", ") << k << '\n';
            } else {
                // Round toward zero (idiv semantics) instead of toward -infinity:
                // sar rcx,63 leaves 0 or -1; ANDing with the mask adds 2^k - 1
                // to negative dividends only. Correct even for INT64_MIN / -2^63.
                builder.append("\tmov rcx, ").append(dest).append('\n');
                builder.append("\tsar rcx, 63\n");
                builder.append("\tmov rdx, ") << pow2_mask(k) << '\n';
                builder.append("\tand rcx, rdx\n");
                builder.append("\tadd ").append(dest).append(", rcx\n");
                (builder.append("\tsar ").append(dest)).append(", ") << k << '\n';
                if (negative)
                    builder.append("\tneg ").append(dest) << '\n';
            }
        } break;

        case SHIFT_MOD: {
            if (is_unsigned) {
                // Mask off the low k bits: unsigned remainder.
                builder.append("\tmov rdx, ") << pow2_mask(k) << '\n';
                builder.append("\tand ").append(dest).append(", rdx\n");
            } else {
                // Remainder keeps the dividend's sign (idiv semantics); the
                // divisor's sign does not matter. |x| via the xor/sub idiom,
                // mask off the low k bits, then re-apply the sign.
                builder.append("\tmov rcx, ").append(dest).append('\n');
                builder.append("\tsar rcx, 63\n");
                builder.append("\txor ").append(dest).append(", rcx\n"); // ~x when x < 0
                builder.append("\tsub ").append(dest).append(", rcx\n"); // +1 when x < 0  => |x|
                builder.append("\tmov rdx, ") << pow2_mask(k) << '\n';
                builder.append("\tand ").append(dest).append(", rdx\n"); // |x| % 2^k
                builder.append("\txor ").append(dest).append(", rcx\n");
                builder.append("\tsub ").append(dest).append(", rcx\n"); // negate when x < 0
            }
        } break;

        default: UNREACHABLE("shift_math_optimization");
    }

    emit_truncate_dest(builder, dest, res.type);
    store_reg(builder, res, dest);
    return true;
}

// FASM reserves instruction mnemonics (add, test, mov, ...) and register
// names as symbols, so every emitted user-function label gets a `__` prefix
// (the std helpers and __entry/__exit already carry one). C FFI (`extern fn`)
// symbols are resolved by the C linker and must keep their exact name, so
// calls to those stay unprefixed.
static bool fn_is_extern(StrView name)
{
    for (auto& func : g_functions)
        if (func.name == name && func.is_extern) return true;
    return false;
}

static void append_fn_label(StrBuilder& builder, StrView name, StrView module_name = "")
{
    if (module_name.size > 0) {
        builder.append("__") << module_name << "_" << name;
        return;
    }
    if (!name.starts_with("__"))
        builder.append("__");
    builder.append(name);
}

bool compile_ops(StrBuilder& builder, Array<Instruction>& ops, Array<VirtualReg>& regs, bool has_frame)
{
    usize ip = 0;
    // Frame size (in bytes of spilled, visited register slots) must match
    // compile_function's computation: OP_ALLOC's struct area starts below the
    // spill slots, and each reservation pads its base to the struct's C
    // alignment, so the region [rbp - registers_size - struct_area_size, rbp -
    // registers_size) is exactly the frame's bottom (never touched by spills
    // or pushed registers).
    usize registers_size = 0;
    for (auto& reg : regs)
        if (reg.is_visited && reg.phys == PR_NONE && !reg.is_global)
            registers_size = reg.offset + STACK_REGISTER_SIZE;
    usize struct_area_used = 0;
    // Operand use-counts, precomputed once so the cmp+jcc fusion check below is
    // O(1) instead of a full scan per comparison.
    Array<usize> reg_uses;
    defer(reg_uses.cleanup());
    compute_reg_uses(ops, regs.count(), reg_uses);
    // Callee-saved registers that hold allocated values. The prologue pushed
    // them (before rbp) and every return must pop them, in reverse order.
    bool cs_used[PR_COUNT] = {};
    for (auto& reg : regs)
        if (reg.phys >= 0 && reg.phys < PR_COUNT && phys_callee_saved(reg.phys))
            cs_used[reg.phys] = true;
    // Must match compile_function's `cs_push_count`: the prologue pushes these
    // registers before rbp, so a register is pushed iff a callee-saved phys is
    // in use anywhere in the body.
    usize cs_push_count = 0;
    for (s8 p = 0; p < PR_COUNT; ++p)
        if (cs_used[p]) ++cs_push_count;
    auto emit_epilogue_pops = [&](StrBuilder& b) {
        for (s8 p = PR_COUNT - 1; p >= 0; --p)
            if (cs_used[p])
                (b.append("\tpop ") << phys_name(p)) << '\n';
    };
    while(ip++ < ops.count()) {
        auto current_inst = ops.get(ip - 1);
        if (!current_inst.is_visited)
            continue;
        switch(current_inst.type)
        {
            case OP_PUSH_I64:
            case OP_PUSH_I32:
            case OP_PUSH_I16:
            case OP_PUSH_I8: {
                auto& reg = regs[current_inst.reg_index];
                if (!reg.is_visited) continue;
                builder.append("\tmov QWORD [rbp - ") << reg.offset << ']' << ',';
                builder << reg.int_val << '\n';
            } break;

            case OP_PUSH_BOOL: {
                auto& reg = regs[current_inst.reg_index];
                if (!reg.is_visited) continue;
                builder.append("\tmov QWORD [rbp - ") << reg.offset << ']' << ',';
                builder << (u8)reg.bool_val << '\n';
            } break;

            case OP_ENTRY_ARGC:
            case OP_ENTRY_ARGV: {
                auto& reg = regs[current_inst.reg_index];
                if (!reg.is_visited) continue;
                // The OS starts the process with argc at [rsp] and the argument
                // array at [rsp+8]. The prologue pushes the callee-saved
                // registers and rbp (never shrinking the OS area), so those
                // values still sit at [rbp + 8 + 8*cs_push_count] /
                // [rbp + 16 + 8*cs_push_count] once the body runs. On Windows
                // the PE loader leaves no such layout, so main(argc, argv) is
                // rejected at compile time and this path is SysV-only.
                const usize entry_base = 8 + 8 * cs_push_count;
                const usize entry_off = current_inst.type == OP_ENTRY_ARGC ? entry_base : entry_base + 8;
                if (current_inst.type == OP_ENTRY_ARGC) {
                    if (reg.phys != PR_NONE) {
                        (builder.append("\tmov ").append(phys_name(reg.phys))).append(", [rbp + ");
                        builder << entry_off << ']' << '\n';
                    } else {
                        builder.append("\tmov rax, [rbp + ");
                        builder << entry_off << ']' << '\n';
                        (builder.append("\tmov [rbp - ") << reg.offset).append("], rax\n");
                    }
                } else {
                    if (reg.phys != PR_NONE) {
                        (builder.append("\tlea ").append(phys_name(reg.phys))).append(", [rbp + ");
                        builder << entry_off << ']' << '\n';
                    } else {
                        builder.append("\tlea rax, [rbp + ");
                        builder << entry_off << ']' << '\n';
                        (builder.append("\tmov [rbp - ") << reg.offset).append("], rax\n");
                    }
                }
            } break;

            case OP_PUSH_STR: {
                auto& reg = regs[current_inst.reg_index];
                if (!reg.is_visited) continue;
                auto& str = g_strings[reg.str_val];
                if (reg.phys != PR_NONE)
                    (builder.append("\tmov ").append(phys_name(reg.phys))).append(", __strings + ") << str.offset << '\n';
                else {
                    builder.append("\tlea rax, [rbp - ") << reg.offset << ']' << '\n';
                    builder.append("\tmov rbx, __strings + ") << str.offset << '\n';
                    builder.append("\tmov [rax], rbx\n");
                }
            } break;

            case OP_EQUALS:
            case OP_NOT_EQUALS:
            case OP_LESS:
            case OP_LESS_EQUALS:
            case OP_GREATER_EQUALS:
            case OP_GREATER: {
                auto& lhs = regs[current_inst.binop.lhs_index];
                auto& rhs = regs[current_inst.binop.rhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;

                // Compare-then-branch fusion: when this comparison's result is
                // consumed exactly once, by the next visited instruction which
                // is a JMP_IF on that result, emit `cmp` + `jcc` directly and
                // skip the JMP_IF. Avoids the setcc/movzx/spill and the
                // reload/test of a transient boolean.
                usize next_ip = ip; // ip points one past the current inst
                while (next_ip < ops.count() && !ops[next_ip].is_visited)
                    ++next_ip;
                if (next_ip < ops.count()
                    && ops[next_ip].type == OP_JMP_IF
                    && ops[next_ip].reg_index == current_inst.reg_index
                    && reg_uses[current_inst.reg_index] == 1) {
                    emit_cmp(builder, lhs, rhs);
                    const bool is_unsigned_cmp = is_unsigned_type(promote_type(lhs.type, rhs.type));
                    const char* jcc;
                    if (current_inst.type == OP_EQUALS) jcc = "je";
                    else if (current_inst.type == OP_NOT_EQUALS) jcc = "jne";
                    else if (current_inst.type == OP_LESS) jcc = is_unsigned_cmp ? "jb" : "jl";
                    else if (current_inst.type == OP_LESS_EQUALS) jcc = is_unsigned_cmp ? "jbe" : "jle";
                    else if (current_inst.type == OP_GREATER) jcc = is_unsigned_cmp ? "ja" : "jg";
                    else jcc = is_unsigned_cmp ? "jae" : "jge";
                    builder.append('\t').append(jcc).append(" .lab_");
                    builder << ops[next_ip].label.ip << '\n';
                    ip = next_ip + 1; // skip the fused JMP_IF
                    continue;
                }

                emit_cmp(builder, lhs, rhs);
                const bool is_unsigned_cmp = is_unsigned_type(promote_type(lhs.type, rhs.type));
                if (current_inst.type == OP_EQUALS)
                    builder.append("\tsete al\n");
                else if (current_inst.type == OP_NOT_EQUALS)
                    builder.append("\tsetne al\n");
                else if (current_inst.type == OP_LESS)
                    builder.append(is_unsigned_cmp ? "\tsetb al\n" : "\tsetl al\n");
                else if (current_inst.type == OP_LESS_EQUALS)
                    builder.append(is_unsigned_cmp ? "\tsetbe al\n" : "\tsetle al\n");
                else if (current_inst.type == OP_GREATER)
                    builder.append(is_unsigned_cmp ? "\tseta al\n" : "\tsetg al\n");
                else
                    builder.append(is_unsigned_cmp ? "\tsetae al\n" : "\tsetge al\n");
                builder.append("\tmovzx rax, al\n");
                store_reg(builder, res, "rax");
            } break;

            case OP_NOT: {
                auto& operand = regs[current_inst.binop.lhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;
                load_reg(builder, "rax", operand);
                builder.append("\ttest rax, rax\n");
                builder.append("\tsete al\n");
                builder.append("\tmovzx rax, al\n");
                store_reg(builder, res, "rax");
            } break;

            case OP_PRINT: {
                const char* func;
                auto& reg = regs[current_inst.reg_index];
                if (current_inst.target.var.type == TYPE_STR) func = "__print_str";
                else if (current_inst.target.var.type == TYPE_BOOL) func = "__print_bool";
                else if (current_inst.target.var.type == TYPE_PTR) func = "__print_ptr";
                else if (is_unsigned_type(current_inst.target.var.type)) func = "__print_unum";
                else func = "__print_num";

                if (reg.is_comp_time) {
                    if (reg.type == TYPE_BOOL)
                        builder.append("\tmov rax, ") << (u8)reg.bool_val << '\n';
                    else
                        builder.append("\tmov rax, ") << reg.int_val << '\n';
                    emit_truncate(builder, reg.type);
                    if (is_windows_target())
                        builder.append("\tmov rcx, rax\n");
                    else
                        builder.append("\tmov rdi, rax\n");
                    builder.append("\tcall ").append(func) << '\n';
                }
                else {
                    // Truncate the value to the width of its type so the print
                    // helpers see the low bits of the stored slot: a u8 holding 300
                    // (e.g. an un-truncated typed parameter) must print 44, and a
                    // signed value must be re-sign-extended from its width.
                    const bool trunc_needed = reg.type == TYPE_I8 || reg.type == TYPE_I16 ||
                                              reg.type == TYPE_I32 || reg.type == TYPE_U8 ||
                                              reg.type == TYPE_U16 || reg.type == TYPE_U32;
                    const char* arg = is_windows_target() ? "rcx" : "rdi";
                    if (!trunc_needed && reg.phys != PR_NONE &&
                        strcmp(phys_name(reg.phys).data, arg) == 0) {
                        // 64-bit value already in the argument register: call
                        // straight away, no rax round-trip.
                        builder.append("\tcall ").append(func) << '\n';
                    } else {
                        load_reg(builder, "rax", reg);
                        emit_truncate(builder, reg.type);
                        (builder.append("\tmov ").append(arg)).append(", rax\n");
                        builder.append("\tcall ").append(func) << '\n';
                    }
                }
            } break;

            case OP_CAST: {
                auto& lhs = regs[current_inst.binop.lhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;

                load_reg(builder, "rax", lhs);
                if (res.type == TYPE_BOOL)
                    builder.append("\ttest rax, rax\n\tsetne al\n\tmovzx rax, al\n");
                else
                    emit_truncate(builder, res.type, is_numeric_type(lhs.type) ? type_size(lhs.type) : 8);
                store_reg(builder, res, "rax");
            } break;

            case OP_STORE: {
                ASSERT(current_inst.data_type == TYPE_VARIABLE, "Invalid data type\n");
                auto& var = current_inst.target.var;
                auto& rhs = regs[current_inst.reg_index];
                auto& reg = regs[var.reg_index];

                if (var.is_local) {
                    if (rhs.phys != PR_NONE)
                        store_reg(builder, reg, phys_name(rhs.phys).data);
                    else {
                        load_reg(builder, "rax", rhs);
                        store_reg(builder, reg, "rax");
                    }
                } else {
                    if (rhs.phys != PR_NONE)
                        (builder.append("\tmov [__globals + ") << reg.offset).append("], ") << phys_name(rhs.phys) << '\n';
                    else {
                        load_reg(builder, "rax", rhs);
                        (builder.append("\tmov [__globals + ") << reg.offset).append("], rax\n");
                    }
                }            } break;

            case OP_INC: {
                auto& lhs = regs[current_inst.binop.lhs_index];
                if (lhs.phys != PR_NONE)
                    builder.append("\tinc ") << phys_name(lhs.phys) << '\n';
                else if (lhs.is_global)
                    builder.append("\tinc qword [__globals + ") << lhs.offset << "]\n";
                else
                    builder.append("\tinc qword [rbp - ") << lhs.offset << ']' << '\n';
            } break;

            case OP_LEA: {
                ASSERT(current_inst.data_type == TYPE_VARIABLE, "Invalid data type\n");
                auto& var = current_inst.target.var;
                auto& target_reg = regs[var.reg_index];
                auto& res = regs[current_inst.reg_index];
                // Address-of is a pure `lea`; emit it straight into the result
                // register when one is allocated (saves `lea rax, ...; mov res, rax`).
                if (res.phys != PR_NONE) {
                    if (var.is_local)
                        (builder.append("\tlea ") << phys_name(res.phys)).append(", [rbp - ") << target_reg.offset;
                    else
                        (builder.append("\tlea ") << phys_name(res.phys)).append(", [__globals + ") << target_reg.offset;
                    builder.append("]\n");
                } else {
                    if (var.is_local)
                        (builder.append("\tlea rax, [rbp - ") << target_reg.offset).append("]\n");
                    else
                        (builder.append("\tlea rax, [__globals + ") << target_reg.offset).append("]\n");
                    store_reg(builder, res, "rax");
                }
            } break;

            case OP_LOAD_PTR: {
                auto& ptr = regs[current_inst.binop.lhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;
                // int_val (when non-zero) is the byte displacement from the
                // base pointer: a struct member load `[ptr + offset]`.
                // byte_size selects the access width (0 = full 8 bytes).
                const u32 width = current_inst.byte_size ? current_inst.byte_size : 8;
                if (res.phys != PR_NONE && ptr.phys != PR_NONE) {
                    // Fold the pointer fetch and the dereference into one
                    // memory load (also fine when res == ptr: the address is
                    // read before the register is overwritten).
                    StrBuilder mem{};
                    (mem.append("[") << phys_name(ptr.phys));
                    if (current_inst.int_val)
                        (mem.append(" + ") << current_inst.int_val);
                    mem.append(']');
                    emit_width_load(builder, phys_name(res.phys).data, mem.to_string_view(false), width, res.type);
                    mem.cleanup();
                } else if (res.phys != PR_NONE) {
                    load_reg(builder, "rax", ptr);
                    StrBuilder mem{};
                    mem.append("[rax");
                    if (current_inst.int_val)
                        (mem.append(" + ") << current_inst.int_val);
                    mem.append(']');
                    emit_width_load(builder, phys_name(res.phys).data, mem.to_string_view(false), width, res.type);
                    mem.cleanup();
                } else {
                    load_reg(builder, "rax", ptr);
                    StrBuilder mem{};
                    mem.append("[rax");
                    if (current_inst.int_val)
                        (mem.append(" + ") << current_inst.int_val);
                    mem.append(']');
                    emit_width_load(builder, "rax", mem.to_string_view(false), width, res.type);
                    mem.cleanup();
                    store_reg(builder, res, "rax");
                }
            } break;

            case OP_STORE_PTR: {
                auto& ptr = regs[current_inst.binop.lhs_index];
                auto& value = regs[current_inst.binop.rhs_index];
                // int_val (when non-zero) is the byte displacement from the
                // base pointer: a struct member store `[ptr + offset]`.
                // byte_size selects the store width (0 = full 8 bytes).
                const u32 width = current_inst.byte_size ? current_inst.byte_size : 8;
                const char* suffix = mem_width_suffix(width);
                if (ptr.phys != PR_NONE) {
                    // The pointer is in a register: store straight into
                    // [ptr(+off)]. A compile-time value folds to an immediate,
                    // so `Foo {x: 123}` emits `mov qword [r12], 123` instead of
                    // a rax/rbx round-trip.
                    if (value.is_comp_time) {
                        // An immediate outside the signed 32-bit range cannot be
                        // folded into `mov [mem], imm` (NASM silently truncates
                        // it), so materialize the full 64-bit value in rax first.
                        const s64 imm = value.int_val;
                        if (imm < -0x7FFFFFFFll - 1 || imm > 0x7FFFFFFFll) {
                            (builder.append("\tmov rax, ") << imm).append('\n');
                            (builder.append("\tmov ") << suffix).append(" [") << phys_name(ptr.phys);
                            if (current_inst.int_val)
                                (builder.append(" + ") << current_inst.int_val).append("], rax\n");
                            else
                                builder.append("], rax\n");
                        } else {
                            (builder.append("\tmov ") << suffix).append(" [") << phys_name(ptr.phys);
                            if (current_inst.int_val)
                                (builder.append(" + ") << current_inst.int_val).append("], ");
                            else
                                builder.append("], ");
                            builder << imm << '\n';
                        }
                    } else if (value.phys != PR_NONE) {
                        (builder.append("\tmov ") << suffix).append(" [") << phys_name(ptr.phys);
                        if (current_inst.int_val)
                            (builder.append(" + ") << current_inst.int_val).append("], ");
                        else
                            builder.append("], ");
                        builder.append(reg_subname(phys_name(value.phys).data, width)) << '\n';
                    } else {
                        // Value spilled or global: load into a scratch that is
                        // not the pointer's register, then store.
                        const char* scratch = phys_name(ptr.phys) == "rax" ? "rbx" : "rax";
                        load_reg(builder, scratch, value);
                        (builder.append("\tmov ") << suffix).append(" [") << phys_name(ptr.phys);
                        if (current_inst.int_val)
                            (builder.append(" + ") << current_inst.int_val).append("], ");
                        else
                            builder.append("], ");
                        builder.append(reg_subname(scratch, width)) << '\n';
                    }
                } else {
                    load_reg(builder, "rax", value);
                    load_reg(builder, "rbx", ptr);
                    (builder.append("\tmov ") << suffix).append(" [rbx");
                    if (current_inst.int_val)
                        (builder.append(" + ") << current_inst.int_val).append("], ");
                    else
                        builder.append("], ");
                    builder.append(reg_subname("rax", width)) << '\n';
                }
            } break;

            case OP_ALLOC: {
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;
                // Reserve the struct's bytes in this function's frame, below
                // the spill slots: struct values are just addresses into the
                // frame, so no heap is involved. The struct sits just below
                // everything already laid out (its base subtracts its own size
                // too), so consecutive OP_ALLOCs pack contiguously down the
                // frame and never overlap the spill slots or pushed registers.
                // The same op also reserves the caller's return slot for a
                // struct-returning call.
                // C alignment: the running struct-area offset is padded (the
                // base = rbp - (registers_size + offset + size) must be a
                // multiple of the struct's align). registers_size is always a
                // multiple of 8, and struct align is at most 8, so aligning the
                // struct-area offset alone is enough.
                const usize a = current_inst.align ? current_inst.align : 1;
                const usize aligned_total = (struct_area_used + current_inst.int_val + a - 1) / a * a;
                struct_area_used = aligned_total - current_inst.int_val;
                const usize slot_off = registers_size + struct_area_used + current_inst.int_val;
                if (res.phys != PR_NONE) {
                    (builder.append("\tlea ") << phys_name(res.phys)).append(", [rbp - ");
                    (builder << slot_off).append("]\n");
                } else {
                    (builder.append("\tlea rax, [rbp - ") << slot_off).append("]\n");
                    store_reg(builder, res, "rax");
                }
                struct_area_used = aligned_total;
            } break;

            case OP_JMP: {
                ASSERT(current_inst.data_type == TYPE_ADDRESS, "Invalid data type\n");
                builder.append("\tjmp .lab_") << current_inst.label.ip << '\n';
            } break;

            case OP_JMP_IF: {
                ASSERT(current_inst.data_type == TYPE_ADDRESS, "Invalid data type\n");
                auto& cond = regs[current_inst.reg_index];

                if (cond.is_comp_time) {
                    if (cond.int_val)
                        builder.append("\tjmp .lab_") << current_inst.label.ip << '\n';
                    break;
                }
                if (cond.phys != PR_NONE) {
                    builder.append("\ttest ").append(phys_name(cond.phys)).append(", ").append(phys_name(cond.phys)).append('\n');
                } else {
                    load_reg(builder, "rax", cond);
                    builder.append("\ttest rax, rax\n");
                }
                builder.append("\tjnz .lab_") << current_inst.label.ip << '\n';
            } break;

            case OP_LABEL: {
                builder.append(".lab_") << current_inst.label.ip << ':' << '\n';
            } break;

            case OP_RET: {
                auto& res = regs[current_inst.reg_index];
                if (res.is_comp_time)
                    builder.append("\tmov rax, ") << res.int_val << '\n';
                else
                    load_reg(builder, "rax", res);
                if (has_frame) {
                    builder.append("\tmov rsp, rbp\n");
                    builder.append("\tpop rbp\n");
                }
                emit_epilogue_pops(builder);
                builder.append("\tret\n");
            } break;

            case OP_CALL: {
                ASSERT(current_inst.data_type == TYPE_CALL, "Invalid data type\n");
                // Platform calling convention. SysV (Linux): the first six
                // arguments travel in rdi/rsi/rdx/rcx/r8/r9; any further
                // arguments go to the caller-reserved area below rsp, which the
                // callee reads at [callee_rbp + 16 + 8*cs_pushes + (i-6)*8].
                // Microsoft x64 (Windows): the first four travel in
                // rcx/rdx/r8/r9, further arguments go below rsp behind a
                // 32-byte shadow space, which the callee reads at
                // [callee_rbp + 16 + 8*cs_pushes + 32 + (i-4)*8]. Both require
                // rsp 16-byte aligned at the call, so the reserved area is
                // padded to a multiple of 16 on Windows. Loading a register
                // argument cannot clobber a live value: the allocator keeps
                // call-crossing values in callee-saved registers or their stack
                // slot, so no live value sits in a caller-saved register at
                // the call.
                const usize arg_count = current_inst.call.args.count();
                const int reg_arg_n = arg_reg_count();
                usize stack_args_bytes = 0;
                for (usize i = reg_arg_n; i < arg_count; ++i) stack_args_bytes += current_inst.call.args[i].size;
                const usize shadow = (usize)shadow_space_bytes();
                usize total_stack = stack_args_bytes + shadow;
                if (is_windows_target() && (total_stack & 15) != 0) total_stack += 8;
                if (total_stack > 0)
                    builder.append("\tsub rsp, ") << total_stack << '\n';
                for (usize i = 0; i < arg_count && (int)i < reg_arg_n; ++i) {
                    auto& res = regs[current_inst.call.args[i].reg_index];
                    load_reg_if_needed(builder, arg_regs()[i], res);
                }
                auto current_arg_offset = (int)shadow;
                for (usize i = reg_arg_n; i < arg_count; ++i) {
                    auto& res = regs[current_inst.call.args[i].reg_index];
                    if (res.phys != PR_NONE)
                        (builder.append("\tmov [rsp+") << current_arg_offset << ']').append(", ") << phys_name(res.phys) << '\n';
                    else {
                        load_reg(builder, "rax", res);
                        (builder.append("\tmov [rsp+") << current_arg_offset << ']').append(", rax\n");
                    }
                    current_arg_offset += current_inst.call.args[i].size;
                }
                if (fn_is_extern(current_inst.call.name.val)) {
                    // Compile-time emission resolves FFI targets to concrete
                    // addresses (see g_comptime_externs): a shared object
                    // cannot reference undefined symbols with PC32 relocs.
                    void* resolved = nullptr;
                    if (g_comptime_emitting) {
                        for (auto& ce : g_comptime_externs) {
                            if (ce.name == current_inst.call.name.val) {
                                resolved = ce.address;
                                break;
                            }
                        }
                    }
                    if (resolved) {
                        (builder.append("\tmov rax, ") << (usize)resolved).append('\n');
                        builder.append("\tcall rax\n");
                    } else
                        (builder.append("\tcall ")).append(current_inst.call.name.val).append('\n');
                }
                else {
                    builder.append("\tcall ");
                    append_fn_label(builder, current_inst.call.name.val, current_inst.call.module);
                    builder.append('\n');
                }
                if (total_stack > 0)
                    builder.append("\tadd rsp, ") << total_stack << '\n';

                auto& res = regs[current_inst.reg_index];
                // Only spill the result when it is actually consumed. A
                // discarded result register is never visited and keeps its
                // stale allocation-time offset; storing to it would alias a
                // live slot that update_all_offsets reassigned.
                if (res.is_visited) {
                    store_reg(builder, res, "rax");
                }
            } break;

            case OP_PLUS:
            case OP_MINUS:
            case OP_MULT: {
                auto& lhs = regs[current_inst.binop.lhs_index];
                auto& rhs = regs[current_inst.binop.rhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;

                const char* asm_op = "add";
                MathType shift = SHIFT_NONE;
                switch (current_inst.type) {
                    case OP_PLUS:  asm_op = "add"; break;
                    case OP_MINUS: asm_op = "sub"; break;
                    default:       asm_op = "imul"; shift = SHIFT_MULT; break;
                }
                // Two-operand imul does not touch rdx:rax, so add/sub/imul all
                // compute directly into res's register (or rax). Prefer a single
                // lea (add/sub against a constant, add of two registers, mult by
                // a small constant) over the mov+op fallback, then shifts for
                // power-of-two multipliers.
                const int lea_op = (current_inst.type == OP_PLUS) ? 0 :
                                   (current_inst.type == OP_MINUS) ? 1 : 2;
                if (emit_lea_opt(builder, lea_op, lhs, rhs, res))
                    break;
                if (shift != SHIFT_NONE && shift_math_optimization(shift, builder, lhs, rhs, res))
                    break;
                emit_binary_op(builder, asm_op, lhs, rhs, res);
            } break;

            case OP_MOD:
            case OP_DIVIDE: {
                auto& lhs = regs[current_inst.binop.lhs_index];
                auto& rhs = regs[current_inst.binop.rhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;

                const MathType math_type = (current_inst.type == OP_MOD) ? SHIFT_MOD : SHIFT_DIV;
                if (shift_math_optimization(math_type, builder, lhs, rhs, res))
                    break;

                // idiv requires the dividend in rdx:rax; the divisor may be any
                // register or memory operand, so only load it when it is a
                // compile-time constant (idiv has no immediate form).
                load_reg(builder, "rax", lhs);
                const bool is_unsigned = is_unsigned_type(promote_type(lhs.type, rhs.type));
                if (is_unsigned)
                    builder.append("\txor edx, edx\n");
                else
                    builder.append("\tcqo\n");
                if (rhs.is_comp_time) {
                    load_reg(builder, "rcx", rhs);
                    builder.append(is_unsigned ? "\tdiv rcx\n" : "\tidiv rcx\n");
                } else if (rhs.phys != PR_NONE) {
                    builder.append(is_unsigned ? "\tdiv " : "\tidiv ");
                    builder.append(phys_name(rhs.phys)) << '\n';
                } else if (rhs.is_global) {
                    builder.append(is_unsigned ? "\tdiv qword [__globals + " : "\tidiv qword [__globals + ");
                    builder << rhs.offset << ']' << '\n';
                } else {
                    builder.append(is_unsigned ? "\tdiv qword [rbp - " : "\tidiv qword [rbp - ");
                    builder << rhs.offset << ']' << '\n';
                }
                emit_truncate_dest(builder, current_inst.type == OP_MOD ? "rdx" : "rax", res.type);
                if (current_inst.type == OP_MOD)
                    store_reg(builder, res, "rdx");
                else
                    store_reg(builder, res, "rax");
            } break;

            case OP_NOP: UNREACHABLE("compile_ops");
            default: TODO(inst_type_to_str(current_inst.type));
        }
    }
    return true;
}


bool compile_function(StrBuilder& builder, DeclaredFunction func)
{
    append_fn_label(builder, func.name, func.module_name);
    builder.append(":\n");
    const bool is_entry = func.name == "__entry";
    // __entry's body is the global-statement ops (assigned in compile_program)
    // plus the calls to main/__exit appended below; it is compiled exactly like
    // any other function body.
    auto& ops = func.ops;
    auto& regs = func.regs;

    // Frame size: only spilled, visited registers occupy stack slots.
    // update_all_offsets assigns offsets monotonically in array order, so the
    // last spilled reg ends at the highest address. (Reading offset of an
    // unvisited reg would be uninitialized memory.)
    usize registers_size = 0;
    for (auto& reg : regs)
        if (reg.is_visited && reg.phys == PR_NONE && !reg.is_global)
            registers_size = reg.offset + STACK_REGISTER_SIZE;

    // The struct area lives below the spill slots, laid out by compile_ops in
    // the same order: each visited OP_ALLOC reserves its int_val bytes, and
    // each region's base is padded to the struct's alignment (so a region
    // pushed at running offset `t` ends at align_up(t + size, align)). Both
    // walks must agree so the reserved region [rbp - registers_size -
    // struct_area_size, rbp - registers_size) is exactly the frame's bottom.
    usize struct_area_size = 0;
    for (auto& op : ops) {
        if (op.type == OP_ALLOC && op.is_visited) {
            const usize a = op.align ? op.align : 1;
            struct_area_size = (struct_area_size + op.int_val + a - 1) / a * a;
        }
    }

    // Skip the prologue/epilogue entirely for functions that touch no stack
    // slots. A function must keep a frame if it performs calls: the prologue
    // realigns rsp for the callee. __entry keeps one too (it is the binary's
    // entry point).
    bool has_frame = registers_size > 0 || struct_area_size > 0;
    if (!has_frame) {
        for (auto& op : ops) {
            if (!op.is_visited) continue;
            // OP_CALL forces a frame so the prologue realigns rsp for the
            // callee. On Windows, OP_PRINT too: the print helpers call the
            // Win32 APIs, which require 16-byte stack alignment (the Linux
            // helpers only make syscalls, which do not). OP_ALLOC reserves
            // bytes in this frame, so it forces a frame as well.
            if (op.type == OP_CALL || op.type == OP_ALLOC || (is_windows_target() && op.type == OP_PRINT)) {
                has_frame = true;
                break;
            }
        }
    }
    if (is_entry)
        has_frame = true;
    // Arguments arrive in registers, so a frame is only needed when one of
    // them spills to a stack slot (the prologue copies it to [rbp - offset])
    // or when more arguments than the platform's register args are passed on
    // the stack. The hidden struct-return slot argument counts like any other.
    const usize hidden_args = (func.expr && func.expr->return_struct_name.size > 0) ? 1 : 0;
    if (func.expr && (int)func.expr->args.count() + (int)hidden_args > arg_reg_count())
        has_frame = true;
    if (func.expr && func.expr->args.count() + hidden_args > 0) {
        for (usize i = 0; i < func.expr->args.count() + hidden_args && i < regs.count(); ++i) {
            if (regs[i].is_visited && regs[i].phys == PR_NONE) {
                has_frame = true;
                break;
            }
        }
    }

    // Preserve any callee-saved registers that hold allocated values. Pushed
    // before rbp so argument offsets ([rbp + 16 + i*8]) stay unchanged, and in
    // ascending phys order so the descending pops in OP_RET invert them.
    bool cs_pushed[PR_COUNT] = {};
    for (auto& reg : regs)
        if (reg.phys >= 0 && reg.phys < PR_COUNT && phys_callee_saved(reg.phys))
            cs_pushed[reg.phys] = true;
    usize cs_push_count = 0;
    for (s8 p = 0; p < PR_COUNT; ++p)
        if (cs_pushed[p]) {
            (builder.append("\tpush ") << phys_name(p)) << '\n';
            ++cs_push_count;
        }

    if (has_frame) {
        builder.append("\tpush rbp\n");
        builder.append("\tmov rbp, rsp\n");
        // __entry is entered by the OS with rsp 16-byte aligned and no return
        // address on the stack, so unlike a normally-called function (entered
        // with rsp ≡ 8 mod 16, i.e. ≡ 0 mod 16 right after `push rbp`) its
        // prologue leaves rsp ≡ 8 mod 16. Always realign it so every `call` in
        // the body (e.g. the synthesized `call main`) happens with rsp
        // 16-byte aligned, as the SysV/Microsoft ABIs require. Ordinary
        // functions need no `and rsp`: their frame subtract (below) is padded
        // to keep rsp aligned through the body.
        if (is_entry || is_windows_target())
            builder.append("\tand rsp, -16\n");
        // Align the reserved frame size so every call inside the body starts
        // from an aligned rsp (the op-call site only pads the argument area;
        // the `mov rsp, rbp` epilogue is unaffected). Microsoft x64 wants the
        // frame itself 16-byte aligned. SysV realigns rsp instead: a
        // normally-called function is entered with rsp ≡ 8 mod 16 and the
        // prologue pushed cs_push_count registers then rbp, so the subtract
        // must satisfy (8*(cs_push_count+1) + frame_sub) ≡ 8 (mod 16), i.e.
        // frame_sub ≡ 8*cs_push_count (mod 16). __entry already ran
        // `and rsp, -16` above, so it only needs frame_sub ≡ 0 (mod 16).
        usize frame_sub = registers_size + struct_area_size;
        if (is_windows_target()) {
            frame_sub = (registers_size + struct_area_size + 15) & ~(usize)15;
        } else {
            const usize mod_target = is_entry ? 0 : (8 * cs_push_count) & 15;
            const usize cur_mod = frame_sub & 15;
            if (cur_mod != mod_target)
                frame_sub += (mod_target - cur_mod) & 15;
        }
        if (frame_sub > 0)
            builder.append("\tsub rsp, ") << frame_sub << '\n';
    }
    if (func.expr && func.expr->args.count() + hidden_args > 0) {
        // The first arg_reg_count() arguments arrive in the platform's
        // integer argument registers; further arguments sit on the stack at
        // [rbp + arg_base + (i - arg_reg_count())*8] (the caller reserved that
        // area below its rsp before the call, preceded on Windows by the
        // 32-byte shadow space). Copy each argument into its allocated
        // register or slot. The incoming registers overlap the allocator pool,
        // so the copy is resolved as a parallel move: copying in argument
        // order could overwrite an argument still waiting in its incoming
        // register. Dead parameters (never referenced) are skipped entirely.
        usize arg_base = 16 + 8 * cs_push_count + (usize)shadow_space_bytes();

        struct ArgMove {
            s8  src_is_reg; // 1: source is arg_regs[src_id]; 0: source is memory [rbp + src_disp]
            s8  src_id;
            s64 src_disp;
            s8  dst_reg;    // PR_NONE: destination is memory [rbp - dst_disp]
            s64 dst_disp;
        };
        Array<ArgMove> moves;
        defer(moves.cleanup());
        for (usize i = 0; i < func.expr->args.count() + hidden_args && i < regs.count(); ++i) {
            auto& arg = regs[i];
            if (!arg.is_visited) continue;
            ArgMove m{};
            if ((int)i < arg_reg_count()) {
                m.src_is_reg = 1;
                m.src_id = (s8)i;
            } else {
                m.src_is_reg = 0;
                m.src_disp = arg_base + (i - (usize)arg_reg_count()) * 8;
            }
            if (arg.phys != PR_NONE) {
                m.dst_reg = arg.phys;
            } else {
                m.dst_reg = PR_NONE;
                m.dst_disp = arg.offset;
            }
            if (m.src_is_reg && arg_reg_pool_id()[m.src_id] == m.dst_reg) continue; // already in place
            moves.push(m);
        }

        Array<bool> done;
        done.reserve(moves.count());
        done.set_count(moves.count());
        done.memzero();
        usize remaining = moves.count();

        auto emit_move = [&](usize idx) {
            auto& m = moves[idx];
            // find_safe guaranteed this move's destination is not any pending
            // move's source, so a direct mov cannot clobber a value still
            // needed. rax is only required to shuttle memory-to-memory copies.
            if (m.src_is_reg) {
                if (m.dst_reg != PR_NONE)
                    (builder.append("\tmov ") << phys_name(m.dst_reg)).append(", ") << arg_regs()[m.src_id] << '\n';
                else
                    (builder.append("\tmov [rbp - ") << m.dst_disp).append("], ") << arg_regs()[m.src_id] << '\n';
            } else if (m.dst_reg != PR_NONE) {
                (builder.append("\tmov ") << phys_name(m.dst_reg)).append(", [rbp + ") << m.src_disp << ']' << '\n';
            } else {
                builder.append("\tmov rax, [rbp + ") << m.src_disp << ']' << '\n';
                (builder.append("\tmov [rbp - ") << m.dst_disp).append("], rax\n");
            }
        };

        auto find_safe = [&]() -> s64 {
            for (usize i = 0; i < moves.count(); ++i) {
                if (done[i]) continue;
                if (moves[i].dst_reg == PR_NONE) return (s64)i; // unique slot: never clobbers a register
                bool conflict = false;
                for (usize j = 0; j < moves.count(); ++j) {
                    if (done[j] || j == i) continue;
                    if (moves[j].src_is_reg && arg_reg_pool_id()[moves[j].src_id] == moves[i].dst_reg) {
                        conflict = true;
                        break;
                    }
                }
                if (!conflict) return (s64)i;
            }
            return -1;
        };

        while (remaining > 0) {
            s64 safe = find_safe();
            if (safe >= 0) {
                emit_move((usize)safe);
                done[(usize)safe] = true;
                --remaining;
                continue;
            }
            // No move can run: every remaining destination register is another
            // move's incoming register, i.e. a pure cycle of register-to-
            // register moves. Break it with rax: save cycle[0]'s value, then
            // move the rest in reverse, then store rax into cycle[0]'s target.
            LocalArray<usize> cycle;
            usize first = 0;
            while (done[first]) ++first;
            cycle.push(first);
            for (;;) {
                s8 target = moves[cycle.last()].dst_reg;
                s64 next = -1;
                for (usize j = 0; j < moves.count(); ++j) {
                    if (done[j]) continue;
                    if (moves[j].src_is_reg && arg_reg_pool_id()[moves[j].src_id] == target) {
                        next = (s64)j;
                        break;
                    }
                }
                if (next < 0) break;
                usize nid = (usize)next;
                bool closed = false;
                for (usize k = 0; k < cycle.count(); ++k)
                    if (cycle[k] == nid) { closed = true; break; }
                if (closed) break;
                cycle.push(nid);
            }
            builder.append("\tmov rax, ") << arg_regs()[moves[cycle[0]].src_id] << '\n';
            for (usize k = cycle.count() - 1; k >= 1; --k) {
                auto& m = moves[cycle[k]];
                (builder.append("\tmov ") << phys_name(m.dst_reg)).append(", ") << arg_regs()[m.src_id] << '\n';
            }
            (builder.append("\tmov ") << phys_name(moves[cycle[0]].dst_reg)).append(", rax\n");
            for (usize k = 0; k < cycle.count(); ++k) {
                done[cycle[k]] = true;
                --remaining;
            }
        }
    }
	bool contains_return = false;
	for (auto op : func.ops) {
	    if (op.type == OP_RET) {
            contains_return = true;
            break;
		}
	}
	// A void function (no explicit `return`) still needs a terminal OP_RET so
	// it never falls off the end of its body; translate_function_body appends
	// one (with `return 0`) for exactly this case. Keep a defensive fallback
	// for functions that are never translated. __entry is excluded: its body
	// ends in `call __exit`, which never returns.
	if (!contains_return && !is_entry) {
        auto& ret_reg = allocate_reg(func.regs);
        ret_reg.is_comp_time = true;
        ret_reg.is_visited = true;
        ret_reg.int_val = 0;
        func.ops.push(Instruction{.type = OP_RET, .location = func.expr ? func.expr->tok : eof_token(), .reg_index= ret_reg.index, .is_visited = true,});
	}
	// __entry's calls to main and __exit, plus the OP_ENTRY_ARGC/OP_ENTRY_ARGV
	// captures of the OS command line, are synthesized in compile_program before
	// the optimization passes run, so they are laid out like any other body.
	compile_ops(builder, func.ops, func.regs, has_frame);

	return true;
}

void append_hex(StrBuilder& builder, StrView str, u32& append_nulls) {
    usize iter = 0;
    while (iter < str.size) {
        unsigned char c = (unsigned char)str.data[iter];
        builder.append("0x", 2);
        if (c == '\\' && iter + 1 < str.size) {
            char esc = str.data[iter + 1];
            if (esc == 'n') {
                c = '\n';
                ++iter;
                ++append_nulls;
            } else if (esc == '"') {
                c = '"';
                ++iter;
                ++append_nulls;
            } else if (esc == '\\') {
                c = '\\';
                ++iter;
                ++append_nulls;
            }
        }
        builder.append(hex_chars[c >> 4]);
        builder.append(hex_chars[c & 0x0F]);
        builder.append(',');
        ++iter;
    }
}

void dead_code(DeclaredFunction& func) {
    auto& ops = func.ops;
    auto& regs = func.regs;

    if (ops.count() == 0) return;
    Array<usize> worklist;
    Array<bool> visited;
    visited.reserve(ops.count());
    visited.set_count(ops.count());
    visited.memzero();

    // Start from function entry
    worklist.push(0);

    while (!worklist.is_empty()) {
        usize ip = worklist.pop();
        if (visited[ip]) continue;
        visited[ip] = true;

        auto& inst = ops[ip];
        inst.is_visited = true;

        // Mark registers as before (your existing switch logic)
        switch (inst.type) {

            case OP_JMP:
            case OP_JMP_IF: {
                // A label is placed at g_labels[..].ip + 1; `.ip` is the index of
                // the instruction right before it. For a label at index 0 (the
                // very first instruction, e.g. `for <cond> {}` at the top of a
                // function) `.ip` is 0 - 1, so the target is the label itself.
                usize target_ip = g_labels[inst.label.ip].ip + 1;
                if (target_ip < ops.count() && !visited[target_ip]) worklist.push(target_ip);
                if (ip + 1 < ops.count() && !visited[ip + 1]) worklist.push(ip + 1);
                // A compile-time condition is inlined by JMP_IF codegen (never
                // loaded from memory), so it must not be marked visited: that
                // would force its OP_PUSH to materialize the constant uselessly.
                if (inst.type == OP_JMP_IF) {
                    auto& cond = regs[inst.reg_index];
                    if (!cond.is_comp_time) cond.is_visited = true;
                }
                continue;
            }

            case OP_PUSH_STR:
            case OP_PRINT:
            case OP_RET:
            case OP_PUSH_BOOL:
            case OP_PUSH_I64:
            case OP_PUSH_I32:
            case OP_PUSH_I16:
            case OP_PUSH_I8:
            case OP_ENTRY_ARGC:
            case OP_ENTRY_ARGV: {
                auto& reg = regs[inst.reg_index];
                if (!reg.is_comp_time)
                    reg.is_visited = true;
            } break;

            case OP_PLUS:
            case OP_MINUS:
            case OP_MOD:
            case OP_DIVIDE:
            case OP_MULT:
            case OP_CAST:
            case OP_EQUALS:
            case OP_NOT_EQUALS:
            case OP_LESS:
            case OP_LESS_EQUALS:
            case OP_GREATER_EQUALS:
            case OP_GREATER: {
                auto& lhs = regs[inst.binop.lhs_index];
                auto& rhs = regs[inst.binop.rhs_index];
                if (!lhs.is_comp_time)
                    lhs.is_visited = true;
                if (inst.type != OP_CAST && !rhs.is_comp_time)
                    rhs.is_visited = true;
            } break;

            case OP_NOT: {
                auto& operand = regs[inst.binop.lhs_index];
                if (!operand.is_comp_time)
                    operand.is_visited = true;
            } break;

            case OP_STORE: {
                auto& reg = regs[inst.reg_index];
                auto& rhs = regs[inst.target.var.reg_index];
                if (!rhs.is_comp_time)
                    rhs.is_visited = true;
                if (!reg.is_comp_time)
                    reg.is_visited = true;
            } break;

            case OP_LEA: {
                // The address-taken variable needs a stack slot even when its
                // value is never read directly.
                auto& target = regs[inst.target.var.reg_index];
                target.is_visited = true;
                auto& res = regs[inst.reg_index];
                if (!res.is_comp_time)
                    res.is_visited = true;
            } break;

            case OP_LOAD_PTR: {
                auto& ptr = regs[inst.binop.lhs_index];
                if (!ptr.is_comp_time)
                    ptr.is_visited = true;
                auto& res = regs[inst.reg_index];
                if (!res.is_comp_time)
                    res.is_visited = true;
            } break;

            case OP_STORE_PTR: {
                auto& ptr = regs[inst.binop.lhs_index];
                if (!ptr.is_comp_time)
                    ptr.is_visited = true;
                auto& val = regs[inst.binop.rhs_index];
                if (!val.is_comp_time)
                    val.is_visited = true;
            } break;

            case OP_ALLOC: {
                // The reservation is always kept: its result register is used
                // by the following stores and gets a slot/register like any
                // other (and compile_ops sizes the struct area from visited
                // OP_ALLOCs, so every one must stay).
                auto& res = regs[inst.reg_index];
                if (!res.is_comp_time)
                    res.is_visited = true;
            } break;

            case OP_INC: {
                auto& res = regs[inst.binop.lhs_index];
                res.is_visited = true;
                res.is_comp_time = false;
            } break;

            case OP_CALL: {
                for (usize i = 0; i < inst.call.args.count(); ++i) {
                    auto& call_arg = inst.call.args[i];
                    auto& arg = regs[call_arg.reg_index];
                    // A compile-time argument is inlined by the caller's
                    // load_reg, so visiting it would only force a dead store of
                    // the constant into a stack slot (every other consumer
                    // skips comp-time registers for the same reason).
                    if (!arg.is_comp_time)
                        arg.is_visited = true;
                }
            } break;

            case OP_LABEL:
                break;

            case OP_NOP: UNREACHABLE("dead_code");
            default: TODO(inst_type_to_str(inst.type));
        }

        // Add fallthrough for non-jump instructions
        if (ip + 1 < ops.count() && !visited[ip + 1]) {
            worklist.push(ip + 1);  // Fallthrough
        }
    }
}

// Registers read by `inst` (gen set) and written (kill set) for liveness.
// Returns the gen set by marking `gen[reg]` and marks `kill[reg]` for written
// (redefined) registers.
void reg_reads_and_writes(Instruction& inst, Array<bool>& gen, Array<bool>& kill)
{
    auto read = [&](usize r) {
        if (r < gen.count()) gen[r] = true;
    };
    auto write = [&](usize r) {
        if (r < kill.count()) kill[r] = true;
    };
    switch (inst.type) {
        case OP_PLUS:
        case OP_MINUS:
        case OP_MULT:
        case OP_DIVIDE:
        case OP_MOD:
        case OP_EQUALS:
        case OP_NOT_EQUALS:
        case OP_LESS:
        case OP_LESS_EQUALS:
        case OP_GREATER_EQUALS:
        case OP_GREATER: {
            read(inst.binop.lhs_index);
            read(inst.binop.rhs_index);
            write(inst.reg_index);
        } break;
        case OP_NOT: {
            read(inst.binop.lhs_index);
            write(inst.reg_index);
        } break;
        case OP_INC: {
            read(inst.binop.lhs_index);
            write(inst.binop.lhs_index);
        } break;
        case OP_CAST: {
            read(inst.binop.lhs_index);
            write(inst.reg_index);
        } break;
        case OP_JMP_IF:
        case OP_PRINT:
        case OP_RET: {
            read(inst.reg_index);
        } break;
        case OP_STORE: {
            read(inst.reg_index);
            write(inst.target.var.reg_index);
        } break;
        case OP_LEA: {
            read(inst.target.var.reg_index);
            write(inst.reg_index);
        } break;
        case OP_LOAD_PTR: {
            // rhs_index is an unused sentinel; only the pointer is read.
            read(inst.binop.lhs_index);
            write(inst.reg_index);
        } break;
        case OP_STORE_PTR: {
            read(inst.binop.lhs_index);
            read(inst.binop.rhs_index);
        } break;
        case OP_ALLOC: {
            write(inst.reg_index);
        } break;
        case OP_CALL: {
            for (auto& arg : inst.call.args)
                read(arg.reg_index);
            write(inst.reg_index);
        } break;
        case OP_PUSH_I8:
        case OP_PUSH_I16:
        case OP_PUSH_I32:
        case OP_PUSH_I64:
        case OP_PUSH_PTR:
        case OP_PUSH_STR:
        case OP_PUSH_BOOL:
        case OP_ENTRY_ARGC:
        case OP_ENTRY_ARGV: {
            write(inst.reg_index);
        } break;
        default: break;
    }
}

// Backward liveness analysis. Fills `live[i * reg_count + r]` = register r
// may be read before being written again on some path through instruction i's
// successors (live-in). Shared by dead_store_elim and the register allocator.
void compute_liveness(DeclaredFunction& func, Array<bool>& live)
{
    auto& ops = func.ops;
    auto& regs = func.regs;
    const usize reg_count = regs.count();

    live.reserve(ops.count() * reg_count);
    live.set_count(ops.count() * reg_count);
    live.memzero();

    Array<bool> live_out;
    live_out.reserve(reg_count);
    live_out.set_count(reg_count);
    Array<bool> new_live;
    new_live.reserve(reg_count);
    new_live.set_count(reg_count);
    Array<bool> kill;
    kill.reserve(reg_count);
    kill.set_count(reg_count);
    Array<bool> gen;
    gen.reserve(reg_count);
    gen.set_count(reg_count);

    bool changed = true;
    while (changed) {
        changed = false;
        for (usize i = ops.count(); i-- > 0;) {
            auto& inst = ops[i];
            if (!inst.is_visited) continue;

            // live_out[i] = union of live_in[successor]
            live_out.memzero();
            auto add_succ = [&](usize succ) {
                if (succ >= ops.count() || !ops[succ].is_visited) return;
                for (usize r = 0; r < reg_count; ++r)
                    if (live[succ * reg_count + r]) live_out[r] = true;
            };
            // Labels are pushed right after set_label, so a jump target is the
            // OP_LABEL at g_labels[..].ip + 1.
            if (inst.type == OP_JMP) {
                add_succ(g_labels[inst.label.ip].ip + 1);
            } else if (inst.type == OP_JMP_IF) {
                add_succ(g_labels[inst.label.ip].ip + 1);
                add_succ(i + 1);
            } else if (inst.type != OP_RET) {
                add_succ(i + 1);
            }

            // live_in[i] = gen[i] | (live_out[i] - kill[i]). Apply kills
            // before gens so a read-modify-write (OP_INC reads and writes the
            // same register) keeps its operand live-in.
            for (usize r = 0; r < reg_count; ++r) new_live[r] = live_out[r];
            kill.memzero();
            gen.memzero();
            reg_reads_and_writes(inst, gen, kill);
            for (usize r = 0; r < reg_count; ++r)
                if (kill[r]) new_live[r] = false;
            for (usize r = 0; r < reg_count; ++r)
                if (gen[r]) new_live[r] = true;

            bool same = true;
            for (usize r = 0; r < reg_count; ++r) {
                if (new_live[r] != live[i * reg_count + r]) { same = false; break; }
            }
            if (!same) {
                for (usize r = 0; r < reg_count; ++r)
                    live[i * reg_count + r] = new_live[r];
                changed = true;
            }
        }
    }
}

// Backward liveness pass: removes STOREs to local variables whose value is
// never read again before the variable is reassigned or the function ends.
// Global stores are kept (their effect is visible to other functions).
// Runs after dead_code, which has already marked reachable instructions and
// live registers.
void dead_store_elim(DeclaredFunction& func)
{
    auto& ops = func.ops;
    auto& regs = func.regs;
    if (ops.count() == 0) return;
    const usize reg_count = regs.count();
    if (reg_count == 0) return;

    Array<bool> live;
    compute_liveness(func, live);

    // Second pass: eliminate stores whose target is not live on exit (never
    // read again before being reassigned or the function ends).
    for (usize i = 0; i < ops.count(); ++i) {
        auto& inst = ops[i];
        if (inst.type != OP_STORE || !inst.is_visited) continue;
        auto& var = inst.target.var;
        if (!var.is_local) continue;

        bool target_live = false;
        auto succ_live = [&](usize succ) {
            if (succ >= ops.count() || !ops[succ].is_visited) return;
            if (live[succ * reg_count + var.reg_index]) target_live = true;
        };
        if (inst.type == OP_JMP) {
            succ_live(g_labels[inst.label.ip].ip + 1);
        } else if (inst.type == OP_JMP_IF) {
            succ_live(g_labels[inst.label.ip].ip + 1);
            succ_live(i + 1);
        } else if (inst.type != OP_RET) {
            succ_live(i + 1);
        }
        if (!target_live) {
            inst.is_visited = false;
        }
    }
}

// Peephole pass removing control flow that can never change behavior. Runs
// after dead_code/dead_store_elim and before codegen:
//  1. Label runs: when OP_LABEL A is directly followed by OP_LABEL B, every
//     jump to A can instead go to B, so A is dropped.
//  2. Jumps to the next instruction: an OP_JMP whose target, after labels are
//     collapsed, is the same instruction it would fall through to anyway is
//     dropped.
void simplify_control_flow(DeclaredFunction& func)
{
    auto& ops = func.ops;
    if (ops.count() == 0) return;

    Array<s64> redirect;
    defer(redirect.cleanup());
    redirect.reserve(g_labels.count());
    redirect.set_count(g_labels.count());
    for (usize i = 0; i < g_labels.count(); ++i)
        redirect[i] = (s64)i;

    // Collapse runs of adjacent labels into their last member (later ones are
    // unmarked and skipped by codegen).
    for (usize i = 0; i < ops.count(); ++i) {
        if (ops[i].type != OP_LABEL || !ops[i].is_visited) continue;
        usize run_end = i;
        while (run_end + 1 < ops.count() && ops[run_end + 1].type == OP_LABEL && ops[run_end + 1].is_visited)
            ++run_end;
        for (usize k = i; k < run_end; ++k) {
            redirect[ops[k].label.ip] = (s64)ops[run_end].label.ip;
            ops[k].is_visited = false;
        }
        i = run_end;
    }

    // Retarget jumps that referenced a collapsed label.
    for (auto& inst : ops) {
        if (inst.type != OP_JMP && inst.type != OP_JMP_IF) continue;
        usize id = (usize)inst.label.ip;
        while (redirect[id] != (s64)id)
            id = (usize)redirect[id];
        inst.label.ip = (s64)id;
    }

    // Position of each surviving label in the ops array.
    Array<s64> label_pos;
    defer(label_pos.cleanup());
    label_pos.reserve(g_labels.count());
    label_pos.set_count(g_labels.count());
    for (usize i = 0; i < g_labels.count(); ++i)
        label_pos[i] = -1;
    for (usize i = 0; i < ops.count(); ++i)
        if (ops[i].type == OP_LABEL && ops[i].is_visited)
            label_pos[ops[i].label.ip] = (s64)i;

    // "Next emitted" skips labels and dead ops.
    auto next_emitted = [&](usize from) -> s64 {
        for (usize j = from + 1; j < ops.count(); ++j)
            if (ops[j].is_visited && ops[j].type != OP_LABEL)
                return (s64)j;
        return (s64)ops.count();
    };

    // Drop an OP_JMP whose target lands on the instruction it would fall
    // through to anyway.
    for (usize i = 0; i < ops.count(); ++i) {
        if (ops[i].type != OP_JMP || !ops[i].is_visited) continue;
        s64 target = label_pos[ops[i].label.ip];
        if (target >= 0 && next_emitted(i) == next_emitted((usize)target))
            ops[i].is_visited = false;
    }
}

// --- VirtualReg allocation (linear scan) -------------------------------
// Maps virtual registers to physical registers (rsi..r15), keeping hot values
// out of memory. rax/rbx/rcx/rdx stay reserved as codegen scratch. Values
// whose interval crosses an OP_CALL/OP_PRINT may only use the callee-saved
// registers (r12..r15), which the prologue/epilogue push/pop; everything else
// spills to its existing stack slot.


// Does instruction `inst` read or write virtual register `r`? Unlike
// reg_reads_and_writes (which only tracks what liveness needs), defs are
// included so an interval covers [first def, last use].
static bool reg_is_touched(Instruction& inst, usize r) {
    switch (inst.type) {
        case OP_ALLOC:
        case OP_PUSH_I8:
        case OP_PUSH_I16:
        case OP_PUSH_I32:
        case OP_PUSH_I64:
        case OP_PUSH_PTR:
        case OP_PUSH_STR:
        case OP_PUSH_BOOL:
        case OP_LABEL:
        case OP_JMP:
        case OP_ENTRY_ARGC:
        case OP_ENTRY_ARGV:
            return inst.reg_index == r;
        case OP_INC:
            return inst.binop.lhs_index == r;
        case OP_CAST:
        case OP_PLUS:
        case OP_MINUS:
        case OP_MULT:
        case OP_DIVIDE:
        case OP_MOD:
        case OP_EQUALS:
        case OP_NOT_EQUALS:
        case OP_LESS:
        case OP_LESS_EQUALS:
        case OP_GREATER_EQUALS:
        case OP_GREATER:
            return inst.binop.lhs_index == r || inst.binop.rhs_index == r || inst.reg_index == r;
        case OP_NOT:
            return inst.binop.lhs_index == r || inst.reg_index == r;
        case OP_JMP_IF:
        case OP_PRINT:
        case OP_RET:
            return inst.reg_index == r;
        case OP_STORE:
            return inst.reg_index == r || inst.target.var.reg_index == r;
        case OP_LEA:
            return inst.reg_index == r || inst.target.var.reg_index == r;
        case OP_LOAD_PTR:
            // rhs_index is an unused sentinel for LOAD_PTR (codegen reads only
            // the pointer operand); counting it would make reg 0 appear used by
            // every load and stretch intervals past their true end.
            return inst.reg_index == r || inst.binop.lhs_index == r;
        case OP_STORE_PTR:
            return inst.reg_index == r || inst.binop.lhs_index == r || inst.binop.rhs_index == r;
        case OP_CALL:
            if (inst.reg_index == r) return true;
            for (auto& arg : inst.call.args)
                if (arg.reg_index == r) return true;
            return false;
        default:
            return false;
    }
}


void build_intervals(DeclaredFunction& func, Array<LiveInterval>& out)
{
    auto& ops = func.ops;
    auto& regs = func.regs;

    // Global variables live in __globals memory, so their virtual register
    // never holds the value and must not be allocated.
    Array<bool> never_alloc;
    never_alloc.reserve(regs.count());
    never_alloc.set_count(regs.count());
    never_alloc.memzero();
    for (auto& inst : ops) {
        if (inst.type == OP_STORE && !inst.target.var.is_local
            && inst.target.var.reg_index < never_alloc.count())
            never_alloc[inst.target.var.reg_index] = true;
        // Address-taken variables must live in their stack slot so &var
        // points at a stable location. Globals are already covered above.
        if (inst.type == OP_LEA && inst.target.var.reg_index < never_alloc.count())
            never_alloc[inst.target.var.reg_index] = true;
    }

    // Backward liveness already accounts for backedges: a value read only at a
    // loop header stays live across the whole body, so its interval must span
    // the loop too (otherwise caller-saved registers would be clobbered by a
    // print/call inside the body between the header and the backedge).
    Array<bool> live;
    compute_liveness(func, live);

    for (usize r = 0; r < regs.count(); ++r) {
        auto& reg = regs[r];
        if (!reg.is_visited || reg.is_comp_time || never_alloc[r] || reg.is_global) {
            reg.phys = PR_NONE;
            continue;
        }
        usize start = ops.count(), end = 0;
        for (usize i = 0; i < ops.count(); ++i) {
            if (!ops[i].is_visited) continue;
            if (!reg_is_touched(ops[i], r) && !live[i * regs.count() + r]) continue;
            if (i < start) start = i;
            end = i;
        }
        if (start == ops.count()) { // dead: never touched nor live
            reg.phys = PR_NONE;
            continue;
        }
        out.push(LiveInterval{r, start, end, reg.hint});
    }

    // Sort by start (insertion sort; intervals are few per function).
    for (usize i = 1; i < out.count(); ++i) {
        LiveInterval key = out[i];
        usize j = i;
        while (j > 0 && out[j - 1].start > key.start) { out[j] = out[j - 1]; --j; }
        out[j] = key;
    }
}

void allocate_registers(DeclaredFunction& func)
{
    auto& regs = func.regs;
    auto& ops = func.ops;
    if (!g_register_allocation) return;

    Array<LiveInterval> intervals;
    defer(intervals.cleanup());
    build_intervals(func, intervals);
    if (intervals.count() == 0) return;

    struct ActiveReg { s8 phys; usize end; };
    LocalArray<ActiveReg> active;

    // True if a call or print sits in [start, end] of the instruction stream.
    // A value live across one can only survive in a callee-saved register; the
    // runtime helpers clobber the rest. (The prologue preserves any
    // callee-saved register we use here.)
    auto interval_crosses_call = [&](usize start, usize end) {
        for (usize i = start; i <= end && i < ops.count(); ++i) {
            if (!ops[i].is_visited) continue;
            if (ops[i].type == OP_CALL || ops[i].type == OP_PRINT || ops[i].type == OP_ALLOC) return true;
        }
        return false;
    };

    bool used[PR_COUNT] = {};
    for (auto& cur : intervals) {
        // Release registers whose interval ended before this one begins.
        for (usize j = 0; j < active.count();) {
            if (active[j].end < cur.start) {
                used[active[j].phys] = false;
                active.remove_unordered(j);
            } else {
                ++j;
            }
        }

        bool crosses_call = interval_crosses_call(cur.start, cur.end);

        s8 phys = PR_NONE;
        // Loop-counter hints are honored first; a busy hint (nested loops
        // cycle through the same set) falls through to the general case.
        if (!crosses_call && cur.hint != PR_NONE && cur.hint >= 0 && cur.hint < PR_COUNT
            && !used[cur.hint])
            phys = cur.hint;
        if (phys == PR_NONE) {
            for (s8 p = 0; p < PR_COUNT; ++p) {
                if (used[p]) continue;
                if (crosses_call && !phys_callee_saved(p)) continue;
                phys = p;
                break;
            }
        }
        regs[cur.reg].phys = phys;
        if (phys != PR_NONE) {
            used[phys] = true;
            active.push(ActiveReg{phys, cur.end});
        }
    }

    // Coalesce values consumed solely by one operation into the register that
    // operation reads them from, eliminating the copy in between:
    //   - call arguments  -> their calling-convention register. `test(&x)`
    //     becomes `lea rdi, [rbp-8]` + `call test` instead of the previous
    //     `lea rax, ...; mov r12, rax; mov rdi, r12`.
    //   - print           -> rdi/rcx (the value then already sits in the arg
    //     register, so the print helper is called straight away).
    //   - `v = rhs` stores-> the destination variable's register. `p := &x`
    //     becomes a bare `lea rdi, [rbp-8]` instead of `lea rsi,...; mov rdi,rsi`.
    // Safe only when the value dies at the consuming op (its interval ends
    // exactly there) and no other call or print sits inside its live range to
    // clobber the caller-saved target register.
    auto try_coalesce = [&](usize p, usize res_idx, s8 target, usize exclude_idx) {
        if (res_idx >= regs.count() || target == PR_NONE) return;
        auto& res = regs[res_idx];
        if (!res.is_visited || res.phys == PR_NONE || res.phys == target) return;

        const LiveInterval* L = nullptr;
        for (auto& iv : intervals)
            if (iv.reg == res_idx) { L = &iv; break; }
        if (!L || L->end != p) return; // still live after the consuming op

        bool unsafe = false;
        for (usize i2 = L->start; i2 < p && !unsafe; ++i2) {
            if (!ops[i2].is_visited) continue;
            if (ops[i2].type == OP_CALL || ops[i2].type == OP_PRINT)
                unsafe = true; // calls/prints clobber every caller-saved reg
            // OP_ALLOC is safe: it writes only its own result register (which
            // after coalescing *is* the target, so `lea target, [rbp-..]` puts
            // the value exactly where it belongs) and rax as scratch, and rax
            // is outside the allocatable pool, so no target is ever clobbered.
            // This lets `a := Foo {...}` fuse the struct's base register into
            // `a`'s own register instead of emitting `mov r13, r12`.
        }
        if (unsafe) return; // must survive an intermediate call

        for (auto& iv : intervals) {
            if (iv.reg == res_idx || iv.reg == exclude_idx) continue;
            // No conflict when ranges only touch at a boundary: a value ending
            // at L->start is read before res is written there, and a value
            // starting at p is written after res is consumed there.
            if (iv.end <= L->start || iv.start >= p) continue;
            if (iv.reg < regs.count() && regs[iv.reg].phys == target) {
                unsafe = true;
                break;
            }
        }
        if (unsafe) return;

        res.phys = target;
    };

    for (usize p = 0; p < ops.count(); ++p) {
        auto& inst = ops[p];
        if (!inst.is_visited) continue;
        if (inst.type == OP_CALL) {
            const usize arg_n = inst.call.args.count();
            for (usize i = 0; i < arg_n && (int)i < arg_reg_count(); ++i)
                try_coalesce(p, inst.call.args[i].reg_index, arg_phys(i), (usize)-1);
        } else if (inst.type == OP_PRINT) {
            try_coalesce(p, inst.reg_index, arg_phys(0), (usize)-1);
        } else if (inst.type == OP_STORE && inst.data_type == TYPE_VARIABLE) {
            const usize var_idx = inst.target.var.reg_index;
            if (var_idx < regs.count() && regs[var_idx].phys != PR_NONE)
                try_coalesce(p, inst.reg_index, regs[var_idx].phys, var_idx);
        }
    }

    // Coalesce single-use values into their consumer's register. For
    //   res = lhs op rhs
    // where lhs dies exactly at this op (its interval ends here; interval ends
    // already reflect loop backedges, so loop-carried values are excluded),
    // the lhs -> res copy (the prologue argument move or emit_binary_op's load)
    // is pure overhead: the op reads lhs and immediately overwrites it with the
    // result, so lhs can live in res's own register. Rewrite lhs.phys to
    // res.phys when no other live value occupies that register across lhs's
    // interval. Runs after the consumer coalescing above so a call/print
    // argument that already moved into its target register pulls its operand
    // in as well (`print(x*8)` computes the shl directly in rdi).
    auto is_binary_op = [](InstructionType t) {
        switch (t) {
            case OP_PLUS:    case OP_MINUS:   case OP_MULT:
            case OP_DIVIDE:  case OP_MOD:
            case OP_EQUALS:  case OP_LESS:    case OP_LESS_EQUALS:
            case OP_GREATER: case OP_GREATER_EQUALS:
                return true;
            default: return false;
        }
    };
    for (usize p = 0; p < ops.count(); ++p) {
        auto& inst = ops[p];
        if (!inst.is_visited || !is_binary_op(inst.type)) continue;
        usize lhs_idx = inst.binop.lhs_index;
        usize res_idx = inst.reg_index;
        if (lhs_idx >= regs.count() || res_idx >= regs.count()) continue;
        auto& lhs = regs[lhs_idx];
        auto& res = regs[res_idx];
        if (!lhs.is_visited || !res.is_visited) continue;
        if (lhs.is_comp_time || res.is_comp_time) continue;
        if (lhs.phys == PR_NONE || res.phys == PR_NONE) continue;
        if (lhs.phys == res.phys) continue;

        const LiveInterval* L = nullptr;
        for (auto& iv : intervals)
            if (iv.reg == lhs_idx) { L = &iv; break; }
        if (!L || L->end != p) continue; // lhs stays live after the op

        // The merged value occupies res.phys over [L->start, res.end]. res's
        // own range already survived in res.phys, but the extended front part
        // [L->start, p] is new: if a call/print sits inside it, the shared
        // register must be callee-saved (e.g. fib's f(n-1) result feeds the
        // final add across the f(n-2) call).
        if (!phys_callee_saved(res.phys) && interval_crosses_call(L->start, p))
            continue;

        bool conflict = false;
        for (auto& iv : intervals) {
            if (iv.reg == lhs_idx || iv.reg == res_idx) continue;
            if (iv.start > p || iv.end < L->start) continue; // no overlap
            if (iv.reg < regs.count() && regs[iv.reg].phys == res.phys) {
                conflict = true;
                break;
            }
        }
        if (conflict) continue;

        lhs.phys = res.phys;
    }
}

void add_std_library(StrBuilder& builder, bool is_windows, bool has_extern)
{
    if (is_windows) {
        builder.append("__print_num:\n");
        builder.append("\tpush	rsi\n");
        builder.append("\tpush	rdi\n");
        builder.append("\tsub	rsp, 88\n");
        builder.append("\tmov	word [rsp + 78], 10\n");
        builder.append("\ttest	rcx, rcx\n");
        builder.append("je	.LBB0_1\n");
        builder.append("\tmov	r8, rcx\n");
        builder.append("\tneg	r8\n");
        builder.append("\tcmovs	r8, rcx\n");
        builder.append("\tlea	rsi, [rsp + 77]\n");
        builder.append("\tmov	r9, -3689348814741910323\n");
        builder.append(".LBB0_3:\n");
        builder.append("\tmov	rax, r8\n");
        builder.append("\tmul	r9\n");
        builder.append("\tshr	rdx, 3\n");
        builder.append("\tlea	eax, [rdx + rdx]\n");
        builder.append("\tlea	eax, [rax + 4*rax]\n");
        builder.append("\tmov	r10d, r8d\n");
        builder.append("\tsub	r10d, eax\n");
        builder.append("\tor	r10b, 48\n");
        builder.append("\tmov	byte [rsi], r10b\n");
        builder.append("\tdec	rsi\n");
        builder.append("\tcmp	r8, 9\n");
        builder.append("\tmov	r8, rdx\n");
        builder.append("\tja	.LBB0_3\n");
        builder.append("\ttest	rcx, rcx\n");
        builder.append("\tjs	.LBB0_6\n");
        builder.append("\tinc	rsi\n");
        builder.append("\tjmp	.LBB0_7\n");
        builder.append(".LBB0_1:\n");
        builder.append("\tlea	rsi, [rsp + 77]\n");
        builder.append("\tmov	byte [rsp + 77], 48\n");
        builder.append("\tjmp	.LBB0_7\n");
        builder.append(".LBB0_6:\n");
        builder.append("\tmov	byte [rsi], 45\n");
        builder.append(".LBB0_7:\n");
        builder.append("\tlea	rdi, [rsp + 48]\n");
        builder.append("\tmov	ecx, -11\n");
        builder.append("\tcall [GetStdHandle]\n");
        builder.append("\tmov	ecx, esi\n");
        builder.append("\tnot	ecx\n");
        builder.append("\tlea	r8d, [rdi + rcx]\n");
        builder.append("\tadd	r8d, 32\n");
        builder.append("\tmov	qword [rsp + 32], 0\n");
        builder.append("\tlea	r9, [rsp + 44]\n");
        builder.append("\tmov	rcx, rax\n");
        builder.append("\tmov	rdx, rsi\n");
        builder.append("\tcall [WriteConsoleA]\n");
        builder.append("\tnop\n");
        builder.append("\tadd	rsp, 88\n");
        builder.append("\tpop	rdi\n");
        builder.append("\tpop	rsi\n");
        builder.append("\tret\n");
        builder.append("__print_unum:\n");
        builder.append("\tpush	rsi\n");
        builder.append("\tpush	rdi\n");
        builder.append("\tsub	rsp, 88\n");
        builder.append("\tmov	word [rsp + 78], 10\n");
        builder.append("\ttest	rcx, rcx\n");
        builder.append("je	.LU0_1\n");
        builder.append("\tmov	r8, rcx\n");
        builder.append("\tlea	rsi, [rsp + 77]\n");
        builder.append("\tmov	r9, -3689348814741910323\n");
        builder.append(".LU0_3:\n");
        builder.append("\tmov	rax, r8\n");
        builder.append("\tmul	r9\n");
        builder.append("\tshr	rdx, 3\n");
        builder.append("\tlea	eax, [rdx + rdx]\n");
        builder.append("\tlea	eax, [rax + 4*rax]\n");
        builder.append("\tmov	r10d, r8d\n");
        builder.append("\tsub	r10d, eax\n");
        builder.append("\tor	r10b, 48\n");
        builder.append("\tmov	byte [rsi], r10b\n");
        builder.append("\tdec	rsi\n");
        builder.append("\tcmp	r8, 9\n");
        builder.append("\tmov	r8, rdx\n");
        builder.append("\tja	.LU0_3\n");
        builder.append("\tinc	rsi\n");
        builder.append("\tjmp	.LU0_7\n");
        builder.append(".LU0_1:\n");
        builder.append("\tlea	rsi, [rsp + 77]\n");
        builder.append("\tmov	byte [rsp + 77], 48\n");
        builder.append(".LU0_7:\n");
        builder.append("\tlea	rdi, [rsp + 48]\n");
        builder.append("\tmov	ecx, -11\n");
        builder.append("\tcall [GetStdHandle]\n");
        builder.append("\tmov	ecx, esi\n");
        builder.append("\tnot	ecx\n");
        builder.append("\tlea	r8d, [rdi + rcx]\n");
        builder.append("\tadd	r8d, 32\n");
        builder.append("\tmov	qword [rsp + 32], 0\n");
        builder.append("\tlea	r9, [rsp + 44]\n");
        builder.append("\tmov	rcx, rax\n");
        builder.append("\tmov	rdx, rsi\n");
        builder.append("\tcall [WriteConsoleA]\n");
        builder.append("\tnop\n");
        builder.append("\tadd	rsp, 88\n");
        builder.append("\tpop	rdi\n");
        builder.append("\tpop	rsi\n");
        builder.append("\tret\n");
        builder.append("__print_ptr:\n");
        builder.append("\tpush	rsi\n");
        builder.append("\tpush	rdi\n");
        builder.append("\tsub	rsp, 88\n");
        builder.append("\tmov	word [rsp + 78], 10\n");
        builder.append("\tlea	rsi, [rsp + 77]\n");
        builder.append("\ttest	rcx, rcx\n");
        builder.append("\tje	.LPT0_1\n");
        builder.append("\tmov	r8, rcx\n");
        builder.append(".LPT0_3:\n");
        builder.append("\tmov	rdx, r8\n");
        builder.append("\tand	edx, 15\n");
        builder.append("\tcmp	dl, 10\n");
        builder.append("\tjb	.LPT0_4\n");
        builder.append("\tadd	dl, 87\n");
        builder.append("\tjmp	.LPT0_5\n");
        builder.append(".LPT0_4:\n");
        builder.append("\tadd	dl, 48\n");
        builder.append(".LPT0_5:\n");
        builder.append("\tmov	byte [rsi], dl\n");
        builder.append("\tdec	rsi\n");
        builder.append("\tshr	r8, 4\n");
        builder.append("\tjnz	.LPT0_3\n");
        builder.append("\tinc	rsi\n");
        builder.append("\tjmp	.LPT0_6\n");
        builder.append(".LPT0_1:\n");
        builder.append("\tlea	rsi, [rsp + 77]\n");
        builder.append("\tmov	byte [rsp + 77], 48\n");
        builder.append(".LPT0_6:\n");
        builder.append("\tmov	byte [rsi - 1], 120\n");
        builder.append("\tmov	byte [rsi - 2], 48\n");
        builder.append("\tlea	rsi, [rsi - 2]\n");
        builder.append("\tlea	rdi, [rsp + 48]\n");
        builder.append("\tmov	ecx, -11\n");
        builder.append("\tcall [GetStdHandle]\n");
        builder.append("\tmov	ecx, esi\n");
        builder.append("\tnot	ecx\n");
        builder.append("\tlea	r8d, [rdi + rcx]\n");
        builder.append("\tadd	r8d, 32\n");
        builder.append("\tmov	qword [rsp + 32], 0\n");
        builder.append("\tlea	r9, [rsp + 44]\n");
        builder.append("\tmov	rcx, rax\n");
        builder.append("\tmov	rdx, rsi\n");
        builder.append("\tcall [WriteConsoleA]\n");
        builder.append("\tnop\n");
        builder.append("\tadd	rsp, 88\n");
        builder.append("\tpop	rdi\n");
        builder.append("\tpop	rsi\n");
        builder.append("\tret\n");
        builder.append("__print_str:\n");
        builder.append("\tpush	rsi\n");
        builder.append("\tpush	rdi\n");
        builder.append("\tsub	rsp, 56\n");
        builder.append("\ttest	rcx, rcx\n");
        builder.append("\tje	.LBB_str_null\n");
        builder.append("\tmov	rsi, rcx\n");
        builder.append("\tcmp	byte [rcx], 0\n");
        builder.append("\tje	.LBB1_1\n");
        builder.append("\txor	edi, edi\n");
        builder.append(".LBB1_3:\n");
        builder.append("\tcmp	byte [rsi + rdi + 1], 0\n");
        builder.append("\tlea	rdi, [rdi + 1]\n");
        builder.append("\tjne	.LBB1_3\n");
        builder.append("\tjmp	.LBB1_4\n");
        builder.append(".LBB1_1:\n");
        builder.append("\txor	edi, edi\n");
        builder.append(".LBB1_4:\n");
        builder.append("\tmov	ecx, -11\n");
        builder.append("\tcall	[GetStdHandle]\n");
        builder.append("\tmov	qword [rsp + 32], 0\n");
        builder.append("\tlea	r9, [rsp + 52]\n");
        builder.append("\tmov	rcx, rax\n");
        builder.append("\tmov	rdx, rsi\n");
        builder.append("\tmov	r8d, edi\n");
        builder.append("\tcall	[WriteConsoleA]\n");
        builder.append("\tnop\n");
        builder.append("\tadd	rsp, 56\n");
        builder.append("\tpop	rdi\n");
        builder.append("\tpop	rsi\n");
        builder.append("\tret\n");
        builder.append(".LBB_str_null:\n");
        builder.append("\tmov	ecx, -11\n");
        builder.append("\tcall	[GetStdHandle]\n");
        builder.append("\tmov	qword [rsp + 32], 0\n");
        builder.append("\tlea	r9, [rsp + 52]\n");
        builder.append("\tmov	rcx, rax\n");
        builder.append("\tmov	rdx, __str_null\n");
        builder.append("\tmov	r8d, 6\n");
        builder.append("\tcall	[WriteConsoleA]\n");
        builder.append("\tnop\n");
        builder.append("\tadd	rsp, 56\n");
        builder.append("\tpop	rdi\n");
        builder.append("\tpop	rsi\n");
        builder.append("\tret\n");
        builder.append("__print_bool:\n");
        builder.append("\tpush	rbx\n");
        builder.append("\tsub	rsp, 48\n");
        builder.append("\tmov	ebx, ecx\n");
        builder.append("\tmov	ecx, -11\n");
        builder.append("\tcall	[GetStdHandle]\n");
        builder.append("\ttest	bl, bl\n");
        builder.append("\tje	.LBB2_2\n");
        builder.append("\tmov	qword [rsp + 32], 0\n");
        builder.append("\tmov	rdx, __str_true\n");
        builder.append("\tlea	r9, [rsp + 44]\n");
        builder.append("\tmov	rcx, rax\n");
        builder.append("\tmov	r8d, 5\n");
        builder.append("\tjmp	.LBB2_3\n");
        builder.append(".LBB2_2:\n");
        builder.append("\tmov	qword [rsp + 32], 0\n");
        builder.append("\tmov	rdx, __str_false\n");
        builder.append("\tlea	r9, [rsp + 44]\n");
        builder.append("\tmov	rcx, rax\n");
        builder.append("\tmov	r8d, 6\n");
        builder.append(".LBB2_3:\n");
        builder.append("\tcall	[WriteConsoleA]\n");
        builder.append("\tnop\n");
        builder.append("\tadd	rsp, 48\n");
        builder.append("\tpop	rbx\n");
        builder.append("\tret\n");
        builder.append("__exit:\n");
        builder.append("\tsub	rsp, 40\n");
        builder.append("\tmov	rcx, rax\n");
        builder.append("\tcall	[ExitProcess]\n");
    } else {
        builder.append("__print_num:\n");
        builder.append("\tlea	rcx, [rsp - 9]\n");
        builder.append("\tmov	word [rsp - 10], 10\n");
        builder.append("\ttest	rdi, rdi\n");
        builder.append("\tje	.LBB0_1\n");
        builder.append("\tmov	r8, rdi\n");
        builder.append("\tneg	r8\n");
        builder.append("\tcmovs	r8, rdi\n");
        builder.append("\tlea	rsi, [rsp - 11]\n");
        builder.append("\tmov	r9, -3689348814741910323\n");
        builder.append(".LBB0_3:\n");
        builder.append("\tmov	rax, r8\n");
        builder.append("\tmul	r9\n");
        builder.append("\tshr	rdx, 3\n");
        builder.append("\tlea	eax, [rdx + rdx]\n");
        builder.append("\tlea	eax, [rax + 4*rax]\n");
        builder.append("\tmov	r10d, r8d\n");
        builder.append("\tsub	r10d, eax\n");
        builder.append("\tor	r10b, 48\n");
        builder.append("\tmov	byte [rsi], r10b\n");
        builder.append("\tdec	rsi\n");
        builder.append("\tcmp	r8, 9\n");
        builder.append("\tmov	r8, rdx\n");
        builder.append("\tja	.LBB0_3\n");
        builder.append("\ttest	rdi, rdi\n");
        builder.append("\tjs	.LBB0_6\n");
        builder.append("\tinc	rsi\n");
        builder.append("\tjmp	.LBB0_7\n");
        builder.append(".LBB0_1:\n");
        builder.append("\tlea	rsi, [rsp - 11]\n");
        builder.append("\tmov	byte [rsp - 11], 48\n");
        builder.append("\tjmp	.LBB0_7\n");
        builder.append(".LBB0_6:\n");
        builder.append("\tmov	byte [rsi], 45\n");
        builder.append(".LBB0_7:\n");
        builder.append("\tsub	rcx, rsi\n");
        builder.append("\tmov	eax, 1\n");
        builder.append("\tmov	edi, 1\n");
        builder.append("\tmov	rdx, rcx\n");
        builder.append("\tsyscall\n");
        builder.append("\tret\n");
        builder.append("__print_unum:\n");
        builder.append("\tlea	rcx, [rsp - 9]\n");
        builder.append("\tmov	word [rsp - 10], 10\n");
        builder.append("\ttest	rdi, rdi\n");
        builder.append("\tje	.LU0_1\n");
        builder.append("\tmov	r8, rdi\n");
        builder.append("\tlea	rsi, [rsp - 11]\n");
        builder.append("\tmov	r9, -3689348814741910323\n");
        builder.append(".LU0_3:\n");
        builder.append("\tmov	rax, r8\n");
        builder.append("\tmul	r9\n");
        builder.append("\tshr	rdx, 3\n");
        builder.append("\tlea	eax, [rdx + rdx]\n");
        builder.append("\tlea	eax, [rax + 4*rax]\n");
        builder.append("\tmov	r10d, r8d\n");
        builder.append("\tsub	r10d, eax\n");
        builder.append("\tor	r10b, 48\n");
        builder.append("\tmov	byte [rsi], r10b\n");
        builder.append("\tdec	rsi\n");
        builder.append("\tcmp	r8, 9\n");
        builder.append("\tmov	r8, rdx\n");
        builder.append("\tja	.LU0_3\n");
        builder.append("\tinc	rsi\n");
        builder.append("\tjmp	.LU0_7\n");
        builder.append(".LU0_1:\n");
        builder.append("\tlea	rsi, [rsp - 11]\n");
        builder.append("\tmov	byte [rsp - 11], 48\n");
        builder.append(".LU0_7:\n");
        builder.append("\tsub	rcx, rsi\n");
        builder.append("\tmov	eax, 1\n");
        builder.append("\tmov	edi, 1\n");
        builder.append("\tmov	rdx, rcx\n");
        builder.append("\tsyscall\n");
        builder.append("\tret\n");
        builder.append("__print_ptr:\n");
        builder.append("\tlea	rcx, [rsp - 9]\n");
        builder.append("\tmov	word [rsp - 10], 10\n");
        builder.append("\ttest	rdi, rdi\n");
        builder.append("\tje	.LPT0_1\n");
        builder.append("\tmov	r8, rdi\n");
        builder.append("\tlea	rsi, [rsp - 11]\n");
        builder.append(".LPT0_3:\n");
        builder.append("\tmov	rdx, r8\n");
        builder.append("\tand	edx, 15\n");
        builder.append("\tcmp	dl, 10\n");
        builder.append("\tjb	.LPT0_4\n");
        builder.append("\tadd	dl, 87\n");
        builder.append("\tjmp	.LPT0_5\n");
        builder.append(".LPT0_4:\n");
        builder.append("\tadd	dl, 48\n");
        builder.append(".LPT0_5:\n");
        builder.append("\tmov	byte [rsi], dl\n");
        builder.append("\tdec	rsi\n");
        builder.append("\tshr	r8, 4\n");
        builder.append("\tjnz	.LPT0_3\n");
        builder.append("\tinc	rsi\n");
        builder.append("\tjmp	.LPT0_6\n");
        builder.append(".LPT0_1:\n");
        builder.append("\tlea	rsi, [rsp - 11]\n");
        builder.append("\tmov	byte [rsp - 11], 48\n");
        builder.append(".LPT0_6:\n");
        builder.append("\tmov	byte [rsi - 1], 120\n");
        builder.append("\tmov	byte [rsi - 2], 48\n");
        builder.append("\tlea	rsi, [rsi - 2]\n");
        builder.append("\tsub	rcx, rsi\n");
        builder.append("\tmov	eax, 1\n");
        builder.append("\tmov	edi, 1\n");
        builder.append("\tmov	rdx, rcx\n");
        builder.append("\tsyscall\n");
        builder.append("\tret\n");
        builder.append("__print_str:\n");
        builder.append("\ttest	rdi, rdi\n");
        builder.append("\tje	.LBB_str_null\n");
        builder.append("\tmov	rsi, rdi\n");
        builder.append("\tcmp	byte [rdi], 0\n");
        builder.append("\tje	.LBB1_1\n");
        builder.append("\txor	eax, eax\n");
        builder.append(".LBB1_3:\n");
        builder.append("\tlea	rdx, [rax + 1]\n");
        builder.append("\tcmp	byte [rsi + rax + 1], 0\n");
        builder.append("\tmov	rax, rdx\n");
        builder.append("\tjne	.LBB1_3\n");
        builder.append("\tmov	eax, 1\n");
        builder.append("\tmov	edi, 1\n");
        builder.append("\tsyscall\n");
        builder.append("\tret\n");
        builder.append(".LBB1_1:\n");
        builder.append("\txor	edx, edx\n");
        builder.append("\tmov	eax, 1\n");
        builder.append("\tmov	edi, 1\n");
        builder.append("\tsyscall\n");
        builder.append("\tret\n");
        builder.append(".LBB_str_null:\n");
        builder.append("\tmov	rsi, __str_null\n");
        builder.append("\tmov	edx, 6\n");
        builder.append("\tmov	eax, 1\n");
        builder.append("\tmov	edi, 1\n");
        builder.append("\tsyscall\n");
        builder.append("\tret\n");
        builder.append("__print_bool:\n");
        builder.append("\ttest	edi, edi\n");
        builder.append("\tje	.LBB2_2\n");
        builder.append("\tmov	rsi, __str_true\n");
        builder.append("\tmov	eax, 1\n");
        builder.append("\tmov	edx, 5\n");
        builder.append("\tmov	edi, 1\n");
        builder.append("\tsyscall\n");
        builder.append("\tret\n");
        builder.append(".LBB2_2:\n");
        builder.append("\tmov	rsi, __str_false\n");
        builder.append("\tmov	eax, 1\n");
        builder.append("\tmov	edx, 6\n");
        builder.append("\tmov	edi, 1\n");
        builder.append("\tsyscall\n");
        builder.append("\tret\n");
        builder.append("__exit:\n");
        if (has_extern) {
            // libc is linked, so hand control back through libc's exit(): the
            // raw exit_group syscall would skip stdio flushing, silently
            // dropping anything printf/puts buffered. (Nul's own print helpers
            // write with raw syscalls and need no flush.) __exit is entered
            // with rsp ≡ 8 mod 16 like any callee, so sub rsp, 8 makes the
            // call site 16-byte aligned for libc.
            builder.append("\tmov	rdi, rax\n");
            builder.append("\tsub	rsp, 8\n");
            builder.append("\tcall	exit\n");
        } else {
            builder.append("\tmov	rdi, rax\n");
            builder.append("\tmov	rax, 60\n");
            builder.append("\tsyscall\n");
        }
    }
}

// ------------------------------------------------------------------
// Emit the string-literal table into a `.data` section.
static void append_string_data(StrBuilder& builder)
{
    if (g_strings.count() != 0) {
        builder.append("\t__strings: db ");
        for (usize s = 0; s < g_strings.count(); ++s) {
            u32 append_nulls = 0;
            append_hex(builder, g_strings[s].name, append_nulls);
            for (u32 i = 0; i < append_nulls; ++i)
                builder.append("0,");
            builder.append("0");
            if (s + 1 < g_strings.count())
                builder.append(", ");
        }
        builder.append('\n');
    } else {
        builder.append("\t__strings: db 0\n");
    }
    builder.append("\t__str_null: db 0x28,0x4E,0x55,0x4C,0x4C,0x29\n");
    // true/false go out as raw hex (like __strings) because the assembler does
    // not process C-style \n escapes inside db strings.
    builder.append("\t__str_true: db 0x74,0x72,0x75,0x65,0x0A\n");
    builder.append("\t__str_false: db 0x66,0x61,0x6C,0x73,0x65,0x0A\n");
}

// FASM output. The Linux target is `format ELF64` (a relocatable object that
// `ld` then links into a dynamically-linked executable; C FFI `extern fn`s are
// declared `extrn` and resolved against libc). The Windows target is
// `format PE64` (a self-contained .exe, no linker step): the format directive
// emits the PE headers, and the .idata import table resolves the Win32 entry
// points (GetStdHandle / WriteConsoleA / ExitProcess), which the std library
// calls indirectly through `call [symbol]`.
// ------------------------------------------------------------------

// Runs a toolchain command with its stdout/stderr going nowhere: both are
// redirected into a memory pipe that the parent drains while the child runs.
// Nothing is printed on success; on failure the captured output is echoed so
// real diagnostics (fasm errors, ld errors) still reach the user. Returns
// false when the process exited with an error.
static bool execute_quietly(Cmd& cmd)
{
    int fds[2];
    if (pipe(fds) != 0)
        return cmd.execute().wait(); // cannot capture: fall back to a noisy run
    fflush(stdout);
    fflush(stderr);
    CmdOptions opt{};
    opt.print_command = false;
    opt.stdout_desc = &fds[1];
    opt.stderr_desc = &fds[1];
    Process proc = cmd.execute(opt);
    // The parent's copy of the write end must go away before reading,
    // otherwise EOF never arrives even after the child exits.
    close(fds[1]);
    StrBuilder captured{};
    char buffer[4096];
    for (;;) {
        ssize_t read_bytes = read(fds[0], buffer, sizeof(buffer));
        if (read_bytes <= 0)
            break;
        captured.append(StrView(buffer, (usize)read_bytes));
    }
    close(fds[0]);
    const bool ok = proc.wait();
    if (!ok && captured.count() > 0)
        fwrite(captured.data(), 1, captured.count(), stdout);
    return ok;
}

bool compile_program(Array<Instruction>& global_ops, Array<VirtualReg>& global_regs)
{
    const bool is_windows = is_windows_target();
    // Any `extern fn`? C FFI symbols must be resolvable, so the binary links
    // libc and is loaded through the dynamic linker.
    bool has_extern = false;
    for (auto& func : g_functions)
        if (func.is_extern) {
            has_extern = true;
            break;
        }
    StrBuilder builder{};
    if (is_windows) {
        builder.append("format PE64 console\n");
        builder.append("entry __entry\n\n");
        builder.append("section '.data' data readable writeable\n");
        append_string_data(builder);
        builder.append("\nsection '.code' code readable executable\n");
        add_std_library(builder, true, has_extern);
    } else {
        builder.append("format ELF64\n\n");
        builder.append("section '.text' executable\n");
        builder.append("public __entry\n");
        // C FFI: each `extern fn` is resolved by the C linker, so its symbol
        // must be declared in the object file's symbol table. (extern fn is
        // rejected on the Windows target, so only the ELF path emits these.)
        for (auto& func : g_functions)
            if (func.is_extern)
                (builder.append("extrn ").append(func.name)).append('\n');
        // When libc is linked, __exit routes through its exit() to flush stdio.
        if (has_extern)
            builder.append("extrn exit\n");
        builder.append('\n');
        add_std_library(builder, false, has_extern);
    }

    // Global-statement ops become the __entry function's body, so they go
    // through the same optimization/register-allocation passes as every other
    // function body, and __entry is compiled as a single function.
    for (usize i = 0; i < g_functions.count(); ++i) {
        if (g_functions[i].name != "__entry") continue;
        auto& entry = g_functions[i];

        // Resolve `main` regardless of which module declares it: __entry
        // (global module) calls it through its mangled label. If main declares
        // parameters, they are the OS command line: argc and argv.
        auto main_module = StrView("");
        auto main_index = find_function_any_module("main");
        if (main_index != Array<DeclaredFunction>::INVALID_INDEX)
            main_module = g_functions[main_index].module_name;
        usize main_argc = 0;
        if (main_index != Array<DeclaredFunction>::INVALID_INDEX && g_functions[main_index].expr)
            main_argc = g_functions[main_index].expr->args.count();
        // main's signature is validated at declaration time
        // (add_function_or_report_if_exit); here we only need the arg count to
        // forward argc/argv from __entry.

        // Build __entry's body: capture argc/argv from the entry stack, run the
        // global-statement ops, then call main and __exit. The whole stream is
        // built up front so every op and register runs through dead_code,
        // register allocation and offset layout like any other function body.
        Array<Instruction> entry_ops{};
        usize argc_reg_index = (usize)-1;
        usize argv_reg_index = (usize)-1;
        if (main_argc >= 1) {
            global_regs.push(VirtualReg{global_regs.count(), 0});
            auto& argc_reg = global_regs.last();
            argc_reg.is_visited = true;
            argc_reg.type = TYPE_I64;
            argc_reg_index = argc_reg.index;
            entry_ops.push(Instruction{.type = OP_ENTRY_ARGC, .location = entry.expr ? entry.expr->tok : eof_token(), .reg_index = argc_reg.index, .is_visited = true});
        }
        if (main_argc >= 2) {
            global_regs.push(VirtualReg{global_regs.count(), 0});
            auto& argv_reg = global_regs.last();
            argv_reg.is_visited = true;
            argv_reg.type = TYPE_PTR;
            argv_reg_index = argv_reg.index;
            entry_ops.push(Instruction{.type = OP_ENTRY_ARGV, .location = entry.expr ? entry.expr->tok : eof_token(), .reg_index = argv_reg.index, .is_visited = true});
        }
        for (auto& op : global_ops)
            entry_ops.push(op);
        auto main_reg = allocate_reg(global_regs);
        Instruction main_call{.type = OP_CALL, .data_type = TYPE_CALL, .call = {Token{Tok_StrLit, "main"}, 0, false, main_module}, .reg_index = main_reg.index, .is_visited = true};
        if (main_argc >= 1)
            main_call.call.args.push(CallArg{nullptr, TYPE_I64, argc_reg_index, STACK_REGISTER_SIZE});
        if (main_argc >= 2)
            main_call.call.args.push(CallArg{nullptr, TYPE_PTR, argv_reg_index, STACK_REGISTER_SIZE});
        entry_ops.push(main_call);
        auto exit_reg = allocate_reg(global_regs);
        entry_ops.push(Instruction{.type = OP_CALL, .data_type = TYPE_CALL, .location = eof_token(), .call = {Token{Tok_StrLit, "__exit"}, 0}, .reg_index = exit_reg.index, .is_visited = true});

        entry.ops = entry_ops;  // takes ownership of entry_ops' buffer
        global_ops.cleanup();   // free the original caller-provided buffer
        entry.regs = global_regs;
        break;
    }

    const usize user_code_start = builder.count();
    for (auto& func : g_functions) {
        // Extern (C FFI) declarations have no body: their label would collide
        // with the C definition the linker resolves, so emit nothing for them.
        if (func.is_extern)
            continue;
        // `#run` may have already run the passes while building its scratch
        // shared library; never run them twice.
        if (!func.passes_done) {
            dead_code(func);
            dead_store_elim(func);
            simplify_control_flow(func);
            allocate_registers(func);
            update_all_offsets(func.regs);
            func.passes_done = true;
        }
        compile_function(builder, func);
    }
    peephole_asm(builder, user_code_start);

    // String literals live in .data; global variables live in .bss. Struct
    // values are reserved on each function's stack, so no heap arena is
    // needed. On Windows the .data is emitted up front (the .code section's
    // import-table calls must come after it), so only the remaining sections
    // are finished here.
    if (is_windows) {
        usize globals_bytes = g_globals_size < 1 ? 1 : g_globals_size;
        builder.append("\nsection '.bss' data readable writeable\n");
        (builder.append("\t__globals: rb ") << globals_bytes).append('\n');
        // PE import table: an image-import-descriptor pointing at the import
        // lookup table (kernel_table), whose entries the loader rewrites to the
        // actual function addresses. `dq RVA _name` creates one IAT slot per
        // function; the std library's `call [GetStdHandle]` style calls go
        // through these slots.
        builder.append("\nsection '.idata' import data readable writeable\n");
        builder.append("\tdd 0, 0, 0, RVA kernel_name, RVA kernel_table\n");
        builder.append("\tdd 0, 0, 0, 0, 0\n\n");
        builder.append("\tkernel_table:\n");
        builder.append("\t\tExitProcess dq RVA _ExitProcess\n");
        builder.append("\t\tGetStdHandle dq RVA _GetStdHandle\n");
        builder.append("\t\tWriteConsoleA dq RVA _WriteConsoleA\n");
        builder.append("\t\tdq 0\n\n");
        builder.append("\tkernel_name db 'KERNEL32.DLL', 0\n\n");
        builder.append("\t_ExitProcess db 0, 0, 'ExitProcess', 0\n");
        builder.append("\t_GetStdHandle db 0, 0, 'GetStdHandle', 0\n");
        builder.append("\t_WriteConsoleA db 0, 0, 'WriteConsoleA', 0\n");
    } else {
        builder.append("\nsection '.data' writeable\n");
        append_string_data(builder);
        usize globals_bytes = g_globals_size < 1 ? 1 : g_globals_size;
        builder.append("\nsection '.bss' writeable\n");
        (builder.append("\t__globals: rb ") << globals_bytes).append('\n');
    }

    StrBuilder out_asm_b{};
    out_asm_b.append(out_path).append(".asm").append_null(false);
    auto out_asm = out_asm_b.to_string_view(true);
    FILE* file = fopen(out_asm.data, "wb");
    if (!file) {
        log_error("Failed to open output file '" SV_FORMAT "'\n", SV_ARG(out_asm));
        return false;
    }
    fwrite(builder.data(), 1, builder.count(), file);
    fclose(file);

    Cmd cmd{};
    // FASM builds the final output directly from the `format` directive in the
    // source: PE64 -> .exe (self-contained, no linker), ELF64 -> relocatable
    // object that `ld` links below. (fasm is repo-local; on a native Windows
    // host the same binary is fasm.exe.)
    StrBuilder out_out_b{};
    out_out_b.append(out_path);
    if (is_windows)
        out_out_b.append(".exe");
    else
        out_out_b.append(".obj");
    out_out_b.append_null(false);
    auto out_out = out_out_b.to_string_view(true);
    if (is_windows)
        cmd.push("./deps/fasm/fasm.exe");
    else
        cmd.push("./deps/fasm/fasm");

    cmd.append(out_asm).append(' ');
    cmd.append(out_out);
    if (!execute_quietly(cmd)) {
        log_error("Failed to assemble '" SV_FORMAT "'\n", SV_ARG(out_asm));
        return false;
    }
    if (is_windows)
        return true;
    cmd.clear();
    cmd.push("ld", "--entry", "__entry", "-o");
    cmd.append(out_path).append(' ');
    cmd.append(out_out).append(' ');
    if (has_extern) {
        // C FFI: resolve `extern fn` symbols against the C runtime. The
        // dynamic linker makes the result a normal dynamically-linked
        // executable (a bare `ld -lc` has no interp and cannot load libc).
        cmd.append("-lc --dynamic-linker /lib64/ld-linux-x86-64.so.2 ");
    }
    // Libraries registered with `#libc` / `#lib("path")` are linked into the
    // final binary as well, so its own extern calls resolve at load time.
    for (usize i = 0; i < g_comptime_libs.count(); ++i) {
        StrBuilder lb{};
        lb.append(g_comptime_libs[i]).append(' ');
        cmd.append(lb.to_string_view(true));
    }
    if (!execute_quietly(cmd)) {
        log_error("Failed to link '" SV_FORMAT "'\n", SV_ARG(out_asm));
        return false;
    }

    // Run the compiled program
    if (g_run_compiled) {
        Cmd run_cmd{};
        StrBuilder out_run_b{};
        if (is_windows) {
            // PE64 output is a self-contained .exe, runnable on a Windows host.
            out_run_b.append(out_path).append(".exe").append_null(false);
        } else {
            if (!out_path.starts_with("/"))
                out_run_b.append("./");
            out_run_b.append(out_path).append_null(false);
        }
        run_cmd.push(out_run_b.to_string_view(true));
        if (!run_cmd.execute().wait())
            log_error("Program '" SV_FORMAT "' exited with an error.\n", SV_ARG(out_asm));
    }

    return true;
}

// Size of a type in bytes, as reported by the `type_size` builtin.
// Bools are 1 byte; only variables and non-fused arithmetic/compare/cast
// results ever touch the stack, and those are always allocated at 8 bytes,
// so a 1-byte bool register never aliases a live slot.
u32 type_size(ValueType type)
{
    switch (type) {
        case TYPE_VOID: return 0;
        case TYPE_BOOL: return 1;
        case TYPE_I8:   return 1;
        case TYPE_I16:  return 2;
        case TYPE_I32:  return 4;
        case TYPE_I64:  return 8;
        case TYPE_U8:   return 1;
        case TYPE_U16:  return 2;
        case TYPE_U32:  return 4;
        case TYPE_U64:  return 8;
        case TYPE_PTR:  return 8;
        case TYPE_STR:  return 8;
        case TYPE_STRUCT: return 8;
        case TYPE_ARRAY: return 8;
        default:
            UNREACHABLE("type_size");
    }
}

// Truncate a 64-bit value to the given integer type, keeping the signed
// interpretation of the result: e.g. 300 as i8 -> 44, 200 as i8 -> -56,
// 0x100000000 as i32 -> 0. Unsigned types keep the zero-extended value.
s64 truncate_value(s64 val, ValueType type)
{
    switch (type) {
        case TYPE_I8: {
            u8 u = (u8)(val & 0xFF);
            return u >= 0x80 ? (s64)u - 0x100 : (s64)u;
        }
        case TYPE_I16: {
            u16 u = (u16)(val & 0xFFFF);
            return u >= 0x8000 ? (s64)u - 0x10000 : (s64)u;
        }
        case TYPE_I32: {
            u32 u = (u32)(val & 0xFFFFFFFFu);
            return u >= 0x80000000u ? (s64)u - 0x100000000ll : (s64)u;
        }
        case TYPE_U8:
            return (s64)(u8)(val & 0xFF);
        case TYPE_U16:
            return (s64)(u16)(val & 0xFFFF);
        case TYPE_U32:
            return (s64)(u32)(val & 0xFFFFFFFFu);
        case TYPE_U64:
        case TYPE_I64:
        default:
            return val;
    }
}

// A numeric cast is free (no runtime instruction) when the value's 64-bit
// stack representation needs no change for the target interpretation. Signed
// values live sign-extended, unsigned values zero-extended, so:
//   - widening any signed/unsigned mix is free (unsigned -> signed wider is
//     always representable; signed -> unsigned wider needs zero-extension,
//     which is a no-op only for the full 64-bit width),
//   - same-width signed<->unsigned needs an instruction (movzx/movsx),
//   - narrowing always needs an instruction.
bool cast_is_free(ValueType src, ValueType dst)
{
    if (src == dst) return true;
    if (src == TYPE_BOOL && is_numeric_type(dst)) return true;
    if (dst == TYPE_BOOL) return false; // numeric -> bool needs a test/setne
    if (!is_numeric_type(src) || !is_numeric_type(dst)) return false;
    const u32 src_w = type_size(src);
    const u32 dst_w = type_size(dst);
    if (dst_w < src_w) return false;
    if (is_unsigned_type(src)) return dst_w > src_w;
    if (is_signed_type(dst)) return true;
    return src_w == 8; // signed src -> unsigned dst wider: free only at 64-bit
}

// Allocate a register holding a known compile-time constant. No instruction
// is emitted for it; every consumer reads the value inline (codegen checks
// `is_comp_time`), so the constant never needs a stack slot.
VirtualReg& make_const(Array<VirtualReg>& regs, ValueType type, s64 val)
{
    auto& reg = allocate_reg(regs);
    reg.is_comp_time = true;
    reg.type = type;
    reg.int_val = val;
    if (type == TYPE_BOOL)
        reg.bool_val = val != 0;
    return reg;
}

void eval_binary(InstructionType instruction_type, ValueType type, VirtualReg& lhs, VirtualReg& rhs, VirtualReg& res)
{
    res.is_comp_time = true;
    res.type = type;
    const bool is_unsigned = is_unsigned_type(type);

    if (type == TYPE_BOOL) {
        // Comparisons use the promoted operand type for signedness: an
        // unsigned operand that doesn't promote to a signed type (u32 vs
        // i32, u64 vs i64) makes the comparison unsigned.
        const bool u = is_unsigned_type(promote_type(lhs.type, rhs.type));
        if (u) {
            u64 l = (u64)lhs.int_val, r = (u64)rhs.int_val;
            switch(instruction_type) {
                case OP_EQUALS:         res.int_val = l == r; break;
                case OP_NOT_EQUALS:     res.int_val = l != r; break;
                case OP_LESS:           res.int_val = l <  r; break;
                case OP_LESS_EQUALS:    res.int_val = l <= r; break;
                case OP_GREATER:        res.int_val = l >  r; break;
                case OP_GREATER_EQUALS: res.int_val = l >= r; break;
                default: UNREACHABLE("eval_binary bool");
            }
        } else {
            s64 l = lhs.int_val, r = rhs.int_val;
            switch(instruction_type) {
                case OP_EQUALS:         res.int_val = l == r; break;
                case OP_NOT_EQUALS:     res.int_val = l != r; break;
                case OP_LESS:           res.int_val = l <  r; break;
                case OP_LESS_EQUALS:    res.int_val = l <= r; break;
                case OP_GREATER:        res.int_val = l >  r; break;
                case OP_GREATER_EQUALS: res.int_val = l >= r; break;
                default: UNREACHABLE("eval_binary bool");
            }
        }
        res.bool_val = res.int_val != 0;
        return;
    }

    switch(instruction_type) {
        case OP_PLUS:    res.int_val = is_unsigned ? (s64)((u64)lhs.int_val + (u64)rhs.int_val) : lhs.int_val + rhs.int_val; break;
        case OP_MINUS:   res.int_val = is_unsigned ? (s64)((u64)lhs.int_val - (u64)rhs.int_val) : lhs.int_val - rhs.int_val; break;
        case OP_MULT:    res.int_val = is_unsigned ? (s64)((u64)lhs.int_val * (u64)rhs.int_val) : lhs.int_val * rhs.int_val; break;
        case OP_DIVIDE:  res.int_val = is_unsigned ? (s64)((u64)lhs.int_val / (u64)rhs.int_val) : lhs.int_val / rhs.int_val; break;
        case OP_MOD:     res.int_val = is_unsigned ? (s64)((u64)lhs.int_val % (u64)rhs.int_val) : lhs.int_val % rhs.int_val; break;
        default: UNREACHABLE("eval_binary");
    }
    // Keep 32-bit results canonical (wrap to the operand type), matching the
    // runtime codegen so `i32 + i32` folds and executes identically.
    if (type_size(type) == 4)
        res.int_val = truncate_value(res.int_val, type);
}

// Parse a non-negative decimal literal into a u64. Returns false when the
// value exceeds 2^64-1, which cannot be represented even as a u64 literal.
bool parse_u64_literal(StrView text, u64& out)
{
    u64 v = 0;
    usize base = 10;
    usize start = 0;
    if (text.size >= 2 && text.data[0] == '0' && (text.data[1] == 'x' || text.data[1] == 'X')) {
        base = 16;
        start = 2;
    } else if (text.size >= 2 && text.data[0] == '0' && (text.data[1] == 'b' || text.data[1] == 'B')) {
        base = 2;
        start = 2;
    }
    if (start == text.size)
        return false; // empty digit string after a prefix
    for (usize i = start; i < text.size; ++i) {
        char c = text.data[i];
        u64 digit;
        if (c >= '0' && c <= '9')
            digit = (u64)(c - '0');
        else if (base == 16 && c >= 'a' && c <= 'f')
            digit = (u64)(c - 'a' + 10);
        else if (base == 16 && c >= 'A' && c <= 'F')
            digit = (u64)(c - 'A' + 10);
        else if (base == 2)
            digit = (u64)(c - '0'); // 0/1 already validated by the lexer
        else
            return false;
        if (v > ((~(u64)0) - digit) / base)
            return false; // would overflow u64
        v = v * base + digit;
    }
    out = v;
    return true;
}




// Translate a function body into its ops/regs and record its return type.
// Convert a `name : type` parameter type token to a ValueType, or TYPE_NOP
// for an untyped parameter.
ValueType str_to_value_type(StrView type) {
    if (type == "i8")          return TYPE_I8;
    else if (type == "i16")    return TYPE_I16;
    else if (type == "i32")    return TYPE_I32;
    else if (type == "i64")    return TYPE_I64;
    else if (type == "u8")     return TYPE_U8;
    else if (type == "u16")    return TYPE_U16;
    else if (type == "u32")    return TYPE_U32;
    else if (type == "u64")    return TYPE_U64;
    else if (type == "bool")   return TYPE_BOOL;
    else if (type == "string") return TYPE_STR;
    else if (type == "str")    return TYPE_STR;
    else if (type == "void")   return TYPE_VOID;
    return TYPE_NOP;
}

// Struct name of the value an expression evaluates to ("" when it is not a
// struct value). Used to tag struct variables and to check struct arguments
// against `foo : Foo` parameters at call sites.
// Struct name and declaring module of the value an expression evaluates to.
// Used to tag struct variables and to check struct arguments against
// `foo : Foo` parameters at call sites.
static void struct_identity_of(Expression* target, Array<Variable>& local_vars, StrView& struct_name, StrView& struct_module)
{
    struct_name = "";
    struct_module = "";
    if (target->type == Expr_StructInit) {
        // The init may be unqualified (`Foo { ... }`) while the struct lives in
        // a module; resolve its actual declaring module, not the qualifier
        // string ("" for an unqualified init).
        auto* init = static_cast<StructInitExpr*>(target);
        struct_name = init->tok.val;
        auto index = find_visible_struct(init->tok.val, init->module_name);
        struct_module = g_structs.is_valid_index(index) ? g_structs[index].module_name : init->module_name;
        return;
    }
    if (target->type == Expr_MemberCall) {
        auto* member = static_cast<MemberCallExpr*>(target);
        auto* var = get_variable(member->tok, false, &local_vars);
        if (!var) return;
        StrView owner_struct = var->struct_name;
        StrView owner_module = var->struct_module;
        if (var->type == TYPE_PTR && var->pointee == TYPE_STRUCT) {
            owner_struct = var->pointee_struct_name;
            owner_module = var->pointee_struct_module;
        }
        auto index = find_visible_struct(owner_struct, owner_module);
        if (!g_structs.is_valid_index(index)) return;
        for (auto& field : g_structs[index].expr->fields) {
            if (field.name == member->field.val) {
                struct_name = field.struct_name;
                struct_module = field.struct_module;
                return;
            }
        }
        return;
    }
    if (target->type == Expr_Deref) {
        // `^p` where `p` is a pointer to a struct evaluates to that struct.
        auto* deref = static_cast<DerefExpr*>(target);
        struct_name = deref->pointee_struct_name;
        struct_module = deref->pointee_struct_module;
        return;
    }
    if (target->type == Expr_Call) {
        auto index = find_visible_function(target->tok, static_cast<CallExpr*>(target)->module_name);
        if (index != Array<DeclaredFunction>::INVALID_INDEX) {
            struct_name = g_functions[index].return_struct_name;
            struct_module = g_functions[index].return_struct_module;
        }
        return;
    }
    auto* var = get_variable(target->tok, false, &local_vars);
    if (!var) return;
    struct_name = var->struct_name;
    struct_module = var->struct_module;
}

StrView struct_name_of(Expression* target, Array<Variable>& local_vars)
{
    StrView name = "", module = "";
    struct_identity_of(target, local_vars, name, module);
    return name;
}

StrView struct_module_of(Expression* target, Array<Variable>& local_vars)
{
    StrView name = "", module = "";
    struct_identity_of(target, local_vars, name, module);
    return module;
}

// Emit every registered `defer` call right before a `return`, in reverse
// registration order (LIFO, like Go). Each call is guarded by its flag
// register: the flag is 0 unless the `defer` statement executed, so a defer
// inside a branch that wasn't taken never fires (and its garbage arguments are
// never read). JMP_IF jumps when its register is nonzero, so the flag is
// inverted first to skip on 0.
static void emit_deferred_calls(Array<Instruction>& ops, Array<VirtualReg>& regs)
{
    for (s64 i = (s64)g_deferred_calls.count() - 1; i >= 0; --i) {
        auto& dc = g_deferred_calls[(usize)i];
        auto& not_flag = allocate_reg(regs);
        not_flag.type = TYPE_BOOL;
        ops.push(Instruction{.type = OP_NOT, .location = dc.call.location, .binop = {OP_NOT, dc.flag_reg, 0}, .reg_index = not_flag.index});
        auto skip_label = allocate_label();
        ops.push(Instruction{.type = OP_JMP_IF, .data_type = TYPE_ADDRESS, .location = dc.call.location, .label = {skip_label.label.ip}, .reg_index = not_flag.index, .is_visited = true});
        ops.push(dc.call);
        set_label(skip_label, ops);
        ops.push(skip_label);
    }
}

// Snapshot a deferred call's argument registers into fresh registers at the
// defer site, so the call runs at function exit with the argument *values the
// `defer` statement saw* (Go's capture semantics), not whatever the variables
// hold at return time. Constants are inlined by the call codegen and need no
// copy.
static void snapshot_deferred_args(Array<Instruction>& ops, Array<VirtualReg>& regs, Instruction& call)
{
    auto snapshot = [&](usize& reg_idx) {
        auto& src = regs[reg_idx];
        if (src.is_comp_time) return;
        auto& dst = allocate_reg(regs);
        dst.type = src.type;
        ops.push(Instruction{.type = OP_CAST, .location = call.location, .binop = {OP_CAST, src.index, src.index}, .reg_index = dst.index});
        reg_idx = dst.index;
    };
    if (call.type == OP_CALL) {
        for (auto& arg : call.call.args)
            snapshot(arg.reg_index);
    } else if (call.type == OP_PRINT) {
        snapshot(call.reg_index);
    }
}

// Translate a function body into its ops/regs and record its return type.
// Shared by Expr_Function and Expr_Call (which lazily translates call targets
// that haven't been translated yet, so a call's result type is known even when
// the callee is declared after the call site).
void translate_function_body(DeclaredFunction& fun)
{
    if (!fun.expr || !fun.expr->block) return;
    if (g_translating_functions.contains(fun.name)) return;
    g_translating_functions.push(fun.name);

    // Stable index of `fun` inside g_functions: body translation can execute
    // `#run` blocks, whose synthetic wrappers get appended to g_functions —
    // reallocating it and dangling any held reference. Everything after the
    // translate_to_instruction call below re-resolves through this index.
    usize self_index = 0;
    for (usize i = 0; i < g_functions.count(); ++i) {
        if (&g_functions[i] == &fun) {
            self_index = i;
            break;
        }
    }

    // Symbol resolution inside the body happens against the function's own
    // module, regardless of which file declared it.
    StrView prev_module = g_current_module_name;
    g_current_module_name = fun.module_name;

    // Error attribution likewise: point compiler_error back at the file the
    // function was parsed from (translation runs after every file is parsed,
    // so the globals would otherwise name whichever file was parsed last).
    StrView prev_src_path = src_path;
    const char* prev_src_content = src_content;
    src_path = fun.src_path;
    src_content = fun.src_content;

    // Declared return type (`fn f() -> type`), or TYPE_NOP when none is
    // written: the type is then *inferred* from the body's `return` statements
    // (the first `return` fixes it; a bare `return;` or no return at all makes
    // the function void).
    const ValueType declared_ret = fun.expr->return_type;

    ValueType prev_live = g_live_function_return_type;
    g_live_function_return_type = declared_ret;

    Array<Instruction> body_ops{};
    Array<VirtualReg> body_regs{};
    Array<Variable> body_vars{};
    for (usize i = 0; i < fun.expr->args.count(); ++i) {
        auto& arg = fun.expr->args[i];
        auto& reg = allocate_reg(body_regs);
        ValueType reg_type = TYPE_I64;
        ValueType pointee = TYPE_NOP;
        u8 ptr_depth = 0;
        if (i < fun.expr->arg_types.count() && fun.expr->arg_types[i].expr) {
            auto& fa = fun.expr->arg_types[i];
            reg_type = fa.type;
            pointee = fa.pointee;
            ptr_depth = fa.ptr_depth;
        }
        reg.type = reg_type;
        StrView struct_name = "";
        StrView struct_module = "";
        StrView pointee_struct_name = "";
        StrView pointee_struct_module = "";
        if (i < fun.expr->arg_types.count() && fun.expr->arg_types[i].expr) {
            struct_name = fun.expr->arg_types[i].struct_name;
            struct_module = fun.expr->arg_types[i].struct_module;
            pointee_struct_name = fun.expr->arg_types[i].pointee_struct_name;
            pointee_struct_module = fun.expr->arg_types[i].pointee_struct_module;
        }
        body_vars.push(Variable{.name = arg->tok.val, .type = reg_type, .reg_index = reg.index, .is_local = true, .is_argument = true, .struct_name = struct_name, .struct_module = struct_module, .pointee = pointee, .ptr_depth = ptr_depth, .pointee_struct_name = pointee_struct_name, .pointee_struct_module = pointee_struct_module});
    }
    // Struct-returning functions receive a hidden return-slot argument after
    // the visible parameters: the caller's reserved frame region where the
    // callee materializes its return value. Its register is the base for every
    // struct built by a `return`, so it must exist before the body translates.
    const bool returns_struct = fun.expr->return_struct_name.size > 0;
    usize hidden_slot_reg = (usize)-1;
    if (returns_struct) {
        auto& slot = allocate_reg(body_regs);
        slot.type = TYPE_PTR;
        hidden_slot_reg = slot.index;
    }

    // Save and restore the return-area translation context: translate_function_body
    // runs lazily from a call site inside another body, which is mid-return.
    // The `defer` list is function-scoped the same way (its flags and saved
    // calls belong to the function being translated).
    const usize prev_slot = g_return_slot_reg;
    const usize prev_hidden = g_function_hidden_slot;
    const usize prev_off = g_return_area_off;
    const usize prev_max = g_return_area_max;
    g_function_hidden_slot = hidden_slot_reg;
    g_return_slot_reg = (usize)-1; // only active while translating a `return` value
    g_return_area_off = 0;
    g_return_area_max = 0;

    Array<DeferredCall> prev_deferred = g_deferred_calls;
    g_deferred_calls.clear();

    // Typed parameters arrive with a full-width value (the caller's register
    // is passed as-is). Truncate each narrow parameter to its declared type at
    // entry so comparisons/arithmetic see the narrow value; print truncates on
    // its own, but an i8 param holding 300 must compare equal to 44.
    for (usize i = 0; i < fun.expr->args.count(); ++i) {
        auto& reg = body_regs[i];
        if (reg.type == TYPE_NOP || reg.type == TYPE_I64 || reg.type == TYPE_U64
            || reg.type == TYPE_STR || reg.type == TYPE_PTR || reg.type == TYPE_VOID
            || reg.type == TYPE_STRUCT) // a struct param is a full-width pointer
            continue;
        body_ops.push(Instruction{.type = OP_CAST, .location = fun.expr->tok, .binop = {OP_CAST, reg.index, reg.index}, .reg_index = reg.index});
    }
    const usize after_param_casts = body_ops.count();
    ValueType body_ret = TYPE_NOP;
    auto body_value = translate_to_instruction(body_ops, body_regs, body_vars, fun.expr->block, body_ret);
    // Re-resolve: the translation above may have run `#run` blocks whose
    // synthetic wrappers reallocated g_functions (see the index capture at
    // the top).
    DeclaredFunction& self = g_functions[self_index];

    // Every defer flag starts at 0, so a `defer` in a branch that never ran
    // cannot fire. The inits are placed right after the param-truncation casts
    // (all execution paths flow through them). Each init uses its own constant
    // source register (OP_CAST's value comes from that source, so two writes to
    // the same flag register can hold different values).
    if (g_deferred_calls.count() > 0) {
        Array<Instruction> pre_ops{};
        for (usize i = 0; i < after_param_casts; ++i)
            pre_ops.push(body_ops[i]);
        for (auto& dc : g_deferred_calls) {
            auto& zero = make_const(body_regs, TYPE_BOOL, 0);
            pre_ops.push(Instruction{.type = OP_CAST, .location = self.expr->tok, .binop = {OP_CAST, zero.index, zero.index}, .reg_index = dc.flag_reg, .is_visited = true});
        }
        for (usize i = after_param_casts; i < body_ops.count(); ++i)
            pre_ops.push(body_ops[i]);
        body_ops = pre_ops;
    }

    // Append a trailing return so a void function (or one whose last statement
    // isn't a `return`) never falls off the end of its body. The implicit
    // `return 0` also gives `main` exit code 0 when it has no explicit return.
    // A `#run` wrapper returns the block's value instead: the trailing
    // expression (`#run { 40 + 2 }`) is the result even without an explicit
    // `return`.
    if (body_ops.count() == 0 || body_ops[body_ops.count() - 1].type != OP_RET) {
        emit_deferred_calls(body_ops, body_regs);
        VirtualReg zero_value = make_const(body_regs, TYPE_I64, 0);
        const bool use_body_value = self.is_comptime_wrapper && body_ret != TYPE_NOP && body_ret != TYPE_VOID;
        VirtualReg& ret_source = use_body_value ? body_value : zero_value;
        body_ops.push(Instruction{.type = OP_RET, .location = self.expr->tok, .reg_index = ret_source.index, .is_visited = true});
    }
    g_deferred_calls = prev_deferred;

    if (declared_ret != TYPE_NOP)
        self.return_type = declared_ret;
    else {
        // A `#run` wrapper with no explicit `return` still has a type: the
        // trailing expression's (`#run { 40 + 2 }` yields i64), so the block
        // result can carry bool/narrow-int types too.
        if (self.is_comptime_wrapper && g_live_function_return_type == TYPE_NOP && body_ret != TYPE_NOP)
            g_live_function_return_type = body_ret;
        // No `-> type`: the return type was inferred from the body's `return`
        // statements. Still TYPE_NOP (no `return` in the body) means void.
        self.return_type = g_live_function_return_type != TYPE_NOP ? g_live_function_return_type : TYPE_VOID;
    }

    // Callers must reserve at least the declared struct's bytes, and every
    // `return` may append nested struct regions past that, so keep the max.
    if (returns_struct) {
        usize declared_total = 0;
        u32 declared_align = 1;
        auto sidx = find_visible_struct(self.expr->return_struct_name, self.expr->return_struct_module);
        if (g_structs.is_valid_index(sidx) && g_structs[sidx].expr) {
            declared_total = g_structs[sidx].expr->total_size;
            declared_align = g_structs[sidx].expr->align;
        }
        // Align the fallback total so the caller's reserved slot is a multiple
        // of the struct's alignment (g_return_area_max already aligns each
        // nested region during emission).
        declared_total = (declared_total + declared_align - 1) / declared_align * declared_align;
        self.return_area_size = MAX(g_return_area_max, declared_total);
    }

    self.ops = body_ops;
    self.regs = body_regs;

    g_return_slot_reg = prev_slot;
    g_function_hidden_slot = prev_hidden;
    g_return_area_off = prev_off;
    g_return_area_max = prev_max;
    g_live_function_return_type = prev_live;
    g_translating_functions.pop();
    g_current_module_name = prev_module;
    src_path = prev_src_path;
    src_content = prev_src_content;
}

// Array metadata (element type and fixed length) of the *value* produced by
// `target`. For a literal it reads the promoted element type recorded during
// translation; for a variable it reads the variable's stored metadata. Returns
// `len` = -1 when `target` is not a statically-known array (so callers can
// report a clear error).
static void array_metadata(Expression* target, Array<Variable>& local_vars, ValueType& elem, s64& len)
{
    elem = TYPE_NOP;
    len = -1;
    if (!target) return;
    if (target->type == Expr_ArrayLit) {
        auto* arr = static_cast<ArrayLitExpr*>(target);
        elem = arr->elem_type;
        len = (s64)arr->elements.count();
        return;
    }
    if (target->type == Expr_Binary && target->tok.type == Tok_Ident) {
        auto* var = get_variable(target->tok, false, &local_vars);
        if (var && var->type == TYPE_ARRAY) {
            elem = var->array_elem;
            len = var->array_len;
        }
        return;
    }
}

// Pointer metadata (base pointee type and remaining indirection depth) of the
// *value* produced by `target`, which must already be translated. For a
// variable it reads the variable's stored metadata; for a DerefExpr it reuses
// the metadata the inner deref recorded while translating (its `ptr_depth` is
// one less than its own target's, so the levels below it remain intact); for
// `&x` the pointee is the addressed variable's own type.
static void pointer_metadata(Expression* target, Array<Variable>& local_vars, ValueType& pointee, u8& depth, StrView& pointee_struct, StrView& pointee_struct_module)
{
    // `null` is an opaque void*: it carries no pointee info but is still a
    // single-level pointer (so it satisfies a `Foo^`/`i64^` param's depth).
    if (target->tok.type == Tok_NullLit) {
        pointee = TYPE_NOP;
        depth = 1;
        pointee_struct = "";
        pointee_struct_module = "";
        return;
    }
    if (target->type == Expr_Deref) {
        auto* inner = static_cast<DerefExpr*>(target);
        pointee = inner->pointee;
        depth = inner->ptr_depth;
        pointee_struct = inner->pointee_struct_name;
        pointee_struct_module = inner->pointee_struct_module;
        return;
    }
    if (target->type == Expr_AddressOf) {
        auto* addr = static_cast<AddressOfExpr*>(target);
        if (addr->operand->type == Expr_Deref) {
            // `&^p` ≡ `p`: peeling the deref leaves the pointer's own
            // metadata (`&^p` of an i64^ is an i64^, of an i64^^ an i64^).
            auto* deref = static_cast<DerefExpr*>(addr->operand);
            pointer_metadata(deref->target, local_vars, pointee, depth, pointee_struct, pointee_struct_module);
            return;
        }
        if (addr->operand->type == Expr_MemberCall) {
            // `&f.x`: address of a member. A plain field yields a single-level
            // pointer to the field's type; a pointer field gains one level
            // (`&f.p` of an i64^ field is i64^^). A struct-typed member
            // yields a single-level pointer to that struct (the address of
            // the member's slot).
            auto* member = static_cast<MemberCallExpr*>(addr->operand);
            auto* var = get_variable(member->tok, false, &local_vars);
            if (!var || !var->is_accesible) {
                compiler_error(member->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(member->tok.val));
            }
            StrView owner_struct = var->struct_name;
            StrView owner_module = var->struct_module;
            if (var->type == TYPE_PTR && var->pointee == TYPE_STRUCT) {
                owner_struct = var->pointee_struct_name;
                owner_module = var->pointee_struct_module;
            }
            auto index = find_visible_struct(owner_struct, owner_module);
            if (!g_structs.is_valid_index(index)) {
                compiler_error(member->tok, "Cannot find Struct with name: '" SV_FORMAT "'\n", SV_ARG(owner_struct));
            }
            for (auto& field : g_structs[index].expr->fields) {
                if (field.name == member->field.val) {
                    if (field.type == TYPE_PTR) {
                        pointee = field.pointee;
                        depth = (u8)(field.ptr_depth + 1);
                        pointee_struct = field.pointee_struct_name;
                        pointee_struct_module = field.pointee_struct_module;
                    } else if (field.type == TYPE_STRUCT) {
                        pointee = TYPE_STRUCT;
                        depth = 1;
                        pointee_struct = field.struct_name;
                        pointee_struct_module = field.struct_module;
                    } else {
                        pointee = field.type;
                        depth = 1;
                        pointee_struct = "";
                        pointee_struct_module = "";
                    }
                    return;
                }
            }
            compiler_error(member->tok, "Struct '" SV_FORMAT "' doesnt contain '" SV_FORMAT "' field\n", SV_ARG(owner_struct), SV_ARG(member->field.val));
            return;
        }
        auto* var = get_variable(addr->operand->tok, false, &local_vars);
        if (!var || !var->is_accesible) {
            compiler_error(addr->operand->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(addr->operand->tok.val));
        }
        pointee = var->type;
        depth = 1;
        pointee_struct = "";
        pointee_struct_module = "";
        // `&p` where `p` is itself a pointer keeps the chain: depth grows by
        // one and the base pointee is preserved (so `q := &p` + `^^q` works).
        if (var->type == TYPE_PTR) {
            pointee = var->pointee;
            depth = (u8)(var->ptr_depth + 1);
            pointee_struct = var->pointee_struct_name;
            pointee_struct_module = var->pointee_struct_module;
        }
        // `&f` where `f` is a struct value: a pointer to the struct.
        else if (var->type == TYPE_STRUCT) {
            pointee = TYPE_STRUCT;
            pointee_struct = var->struct_name;
            pointee_struct_module = var->struct_module;
        }
        return;
    }
    if (target->type == Expr_Call) {
        // A function returning a pointer exposes its declared pointee metadata
        // (for `-> Foo^` the struct name, otherwise nothing: an opaque
        // single-level pointer).
        auto index = find_visible_function(target->tok, static_cast<CallExpr*>(target)->module_name);
        if (index != Array<DeclaredFunction>::INVALID_INDEX) {
            pointee = g_functions[index].return_pointee;
            depth = g_functions[index].return_ptr_depth;
            pointee_struct = g_functions[index].return_pointee_struct_name;
            pointee_struct_module = g_functions[index].return_pointee_struct_module;
            return;
        }
        pointee = TYPE_NOP;
        depth = 1;
        pointee_struct = "";
        pointee_struct_module = "";
        return;
    }
    if (target->type == Expr_MemberCall) {
        // `foo.p` where `p` is a pointer field: the field's declared pointee
        // and depth carry over, so `^foo.p` and `&foo.p` type-check.
        auto* member = static_cast<MemberCallExpr*>(target);
        auto* var = get_variable(member->tok, false, &local_vars);
        if (!var || !var->is_accesible) {
            compiler_error(member->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(member->tok.val));
        }
        // `s.data` on a string: a pointer to the literal's first byte.
        if (var->type == TYPE_STR) {
            if (member->field.val == "data") {
                pointee = TYPE_U8;
                depth = 1;
                pointee_struct = "";
                pointee_struct_module = "";
            } else {
                // `.len` is not a pointer.
                pointee = TYPE_NOP;
                depth = 0;
                pointee_struct = "";
                pointee_struct_module = "";
            }
            return;
        }
        StrView owner_struct = var->struct_name;
        StrView owner_module = var->struct_module;
        if (var->type == TYPE_PTR && var->pointee == TYPE_STRUCT) {
            owner_struct = var->pointee_struct_name;
            owner_module = var->pointee_struct_module;
        }
        auto index = find_visible_struct(owner_struct, owner_module);
        if (!g_structs.is_valid_index(index))
            compiler_error(member->tok, "Cannot find Struct with name: '" SV_FORMAT "'\n", SV_ARG(owner_struct));
        for (auto& field : g_structs[index].expr->fields) {
            if (field.name == member->field.val) {
                pointee = field.pointee;
                depth = field.ptr_depth;
                pointee_struct = field.pointee_struct_name;
                pointee_struct_module = field.pointee_struct_module;
                return;
            }
        }
        pointee = TYPE_NOP;
        depth = 0;
        pointee_struct = "";
        pointee_struct_module = "";
        return;
    }
    if (target->type == Expr_Binary) {
        // `expr as u8^` / `expr as bool^^`: the cast's declared pointee and
        // depth win over the source value's metadata. This lets the pointer
        // re-interpretation cast flow into arg checks, derefs and `type_of`.
        auto* bin = static_cast<BinaryExpr*>(target);
        if (bin->tok.type == Tok_Cast && bin->rhs && bin->rhs->type == Expr_Binary) {
            auto* cast_type = static_cast<BinaryExpr*>(bin->rhs);
            // Builtin type names lex as Tok_Type; struct names as Tok_Ident.
            const bool names_type = cast_type->tok.type == Tok_Type
                || (cast_type->tok.type == Tok_Ident && !cast_type->lhs
                    && find_visible_struct(cast_type->tok.val, cast_type->module_name) != Array<DeclaredStruct>::INVALID_INDEX);
            if (names_type && cast_type->ptr_depth > 0) {
                pointee = str_to_value_type(cast_type->tok.val);
                if (pointee == TYPE_NOP) {
                    pointee = TYPE_STRUCT;
                    pointee_struct = cast_type->tok.val;
                    pointee_struct_module = cast_type->module_name;
                } else {
                    pointee_struct = "";
                    pointee_struct_module = "";
                }
                depth = cast_type->ptr_depth;
                return;
            }
        }
        // `p + n`, `n + p`, `p - n` pointer arithmetic: the result keeps the
        // pointer operand's metadata (`x := argv + i` infers x as `str^`).
        if ((bin->tok.type == Tok_Plus || bin->tok.type == Tok_Minus)
            && bin->lhs && bin->rhs) {
            ValueType l_pointee = TYPE_NOP, r_pointee = TYPE_NOP;
            u8 l_depth = 0, r_depth = 0;
            StrView l_struct = "", l_module = "", r_struct = "", r_module = "";
            pointer_metadata(bin->lhs, local_vars, l_pointee, l_depth, l_struct, l_module);
            pointer_metadata(bin->rhs, local_vars, r_pointee, r_depth, r_struct, r_module);
            if (l_depth > 0) {
                pointee = l_pointee; depth = l_depth;
                pointee_struct = l_struct; pointee_struct_module = l_module;
                return;
            }
            if (r_depth > 0) {
                pointee = r_pointee; depth = r_depth;
                pointee_struct = r_struct; pointee_struct_module = r_module;
                return;
            }
        }
    }
    // Only a bare identifier names a variable here. Anything else reaching
    // the fallthrough (a literal operand of pointer arithmetic, an index
    // expression, ...) simply carries no pointer metadata.
    if (target->tok.type != Tok_Ident) {
        pointee = TYPE_NOP;
        depth = 0;
        pointee_struct = "";
        pointee_struct_module = "";
        return;
    }
    auto* var = get_variable(target->tok, false, &local_vars);
    if (!var || !var->is_accesible) {
        compiler_error(target->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(target->tok.val));
    }
    pointee = var->pointee;
    depth = var->ptr_depth;
    pointee_struct = var->pointee_struct_name;
    pointee_struct_module = var->pointee_struct_module;
}

// Build a "pointee^^..." name for a pointer type (e.g. `u8^`, `bool^^`), so
// `type_of` prints the inner type with one `^` per indirection level instead
// of the generic "Ptr". Returns "Ptr" when the pointee is unknown (an opaque
// function result). The result is heap-allocated and intentionally never
// freed (the string pool holds the StrView by pointer).
static const char* pointer_type_name(ValueType pointee, u8 depth)
{
    if (pointee == TYPE_NOP) return "Ptr";
    const char* base = value_type_to_str(pointee);
    const usize base_len = strlen(base);
    char* buf = new char[base_len + depth + 1];
    memcpy(buf, base, base_len);
    for (u8 i = 0; i < depth; ++i)
        buf[base_len + i] = '^';
    buf[base_len + depth] = '\0';
    return buf;
}

static usize struct_total_size(StrView name, StrView module_name = "")
{
    auto idx = find_visible_struct(name, module_name);
    if (!g_structs.is_valid_index(idx) || !g_structs[idx].expr) return 0;
    return g_structs[idx].expr->total_size;
}

// The C alignment of a declared struct (0 for unknown, falls back to 1).
static u32 struct_align(StrView name, StrView module_name = "")
{
    auto idx = find_visible_struct(name, module_name);
    if (!g_structs.is_valid_index(idx) || !g_structs[idx].expr) return 1;
    return g_structs[idx].expr->align;
}

// Deep-copy the struct whose address is `src_reg` into the function's hidden
// return slot, used for `return <value>` where the value is not a fresh struct
// literal (a variable, member or call result that may point into this frame,
// which is gone once `ret` runs). Scalar/str/ptr fields are copied into the
// struct's region; struct-typed fields get a fresh region past the parent and
// the parent's member slot is pointed at it, so nested data survives too.
static void copy_struct_into(Array<Instruction>& ops, Array<VirtualReg>& regs, usize src_reg, StrView struct_name, StrView struct_module = "")
{
    auto idx = find_visible_struct(struct_name, struct_module);
    if (!g_structs.is_valid_index(idx) || !g_structs[idx].expr) return;
    auto* def = g_structs[idx].expr;
    // Align the region's base to the struct's C alignment, then reserve it.
    // The hidden slot's own address is a multiple of the top-level struct's
    // alignment, so aligning the relative offset keeps the base aligned too.
    const usize a = def->align ? def->align : 1;
    g_return_area_off = (g_return_area_off + a - 1) / a * a;
    const usize my_off = g_return_area_off;
    g_return_area_off += def->total_size;
    g_return_area_max = MAX(g_return_area_max, g_return_area_off);
    usize dst_base = g_return_slot_reg;
    if (my_off > 0) {
        auto& off_const = make_const(regs, TYPE_PTR, (s64)my_off);
        auto& base = allocate_reg(regs);
        base.type = TYPE_PTR;
        ops.push(Instruction{.type = OP_PLUS, .location = eof_token(), .binop = {OP_PLUS, g_return_slot_reg, off_const.index}, .reg_index = base.index});
        dst_base = base.index;
    }
    for (usize f = 0; f < def->fields.count(); ++f) {
        auto& field = def->fields[f];
        const usize off = f * STACK_REGISTER_SIZE;
        if (field.type == TYPE_STRUCT) {
            auto& nested_off = make_const(regs, TYPE_PTR, (s64)g_return_area_off);
            auto& nested_dst = allocate_reg(regs);
            nested_dst.type = TYPE_PTR;
            ops.push(Instruction{.type = OP_PLUS, .location = eof_token(), .binop = {OP_PLUS, g_return_slot_reg, nested_off.index}, .reg_index = nested_dst.index});
            auto& src_val = allocate_reg(regs);
            src_val.type = TYPE_STRUCT;
            ops.push(Instruction{.type = OP_LOAD_PTR, .location = eof_token(), .binop = {OP_LOAD_PTR, src_reg, src_val.index}, .reg_index = src_val.index, .int_val = off});
            ops.push(Instruction{.type = OP_STORE_PTR, .location = eof_token(), .binop = {OP_STORE_PTR, dst_base, nested_dst.index}, .reg_index = nested_dst.index, .int_val = off});
            copy_struct_into(ops, regs, src_val.index, field.struct_name, field.struct_module);
        } else {
            auto& src_val = allocate_reg(regs);
            src_val.type = field.type;
            ops.push(Instruction{.type = OP_LOAD_PTR, .location = eof_token(), .binop = {OP_LOAD_PTR, src_reg, src_val.index}, .reg_index = src_val.index, .int_val = off});
            ops.push(Instruction{.type = OP_STORE_PTR, .location = eof_token(), .binop = {OP_STORE_PTR, dst_base, src_val.index}, .reg_index = src_val.index, .int_val = off});
        }
    }
}

// Zero-initialize every member of the struct at `base_index`, used for the
// `Foo {0}` literal form: numbers and bools store 0, str fields store the
// empty string, and struct-typed fields reserve a fresh region (from the same
// return area / frame struct area the surrounding literal uses), zero-init it
// recursively, and store its pointer.
// Zero-initialize one struct field: numeric/bool fields become 0, str fields
// become NULL (a raw 0), and nested struct fields are recursively zeroed. The
// zero value is stored into [base + field_index * 8].
static void zero_init_members(Array<Instruction>& ops, Array<VirtualReg>& regs, Token tok, StructExpr* def, usize base_index);

static void zero_init_field(Array<Instruction>& ops, Array<VirtualReg>& regs, Token tok, StructExpr* def, usize field_index, usize base_index)
{
    auto& field = def->fields[field_index];
    usize value_index;
    if (field.type == TYPE_STRUCT) {
        auto idx = find_visible_struct(field.struct_name, field.struct_module);
        if (!g_structs.is_valid_index(idx) || !g_structs[idx].expr)
            compiler_error(tok, "Use of undeclared struct: '" SV_FORMAT "'\n", SV_ARG(field.struct_name));
        auto* nested_def = g_structs[idx].expr;
        usize nested_base;
        if (g_return_slot_reg != (usize)-1) {
            // Align the nested region's base to the nested struct's alignment,
            // like copy_struct_into and the top-level struct-init path.
            const usize na = nested_def->align ? nested_def->align : 1;
            g_return_area_off = (g_return_area_off + na - 1) / na * na;
            const usize my_off = g_return_area_off;
            g_return_area_off += nested_def->total_size;
            g_return_area_max = MAX(g_return_area_max, g_return_area_off);
            if (my_off == 0) {
                nested_base = g_return_slot_reg;
            } else {
                auto& off_const = make_const(regs, TYPE_PTR, (s64)my_off);
                auto& base = allocate_reg(regs);
                base.type = TYPE_PTR;
                ops.push(Instruction{.type = OP_PLUS, .location = tok, .binop = {OP_PLUS, g_return_slot_reg, off_const.index}, .reg_index = base.index});
                nested_base = base.index;
            }
        } else {
            auto& alloc_reg = allocate_reg(regs);
            alloc_reg.type = TYPE_STRUCT;
            nested_base = alloc_reg.index;
            ops.push(Instruction{.type = OP_ALLOC, .location = tok, .reg_index = alloc_reg.index, .int_val = nested_def->total_size, .align = nested_def->align});
        }
        zero_init_members(ops, regs, tok, nested_def, nested_base);
        value_index = nested_base;
    } else {
        // str zero-init is NULL (0), not a pointer to the empty string.
        auto& reg = make_const(regs, field.type, 0);
        value_index = reg.index;
    }
    ops.push(Instruction{.type = OP_STORE_PTR, .location = tok, .binop = {OP_STORE_PTR, base_index, value_index}, .reg_index = value_index, .int_val = field.offset, .byte_size = (u8)field.size});
}

static void zero_init_members(Array<Instruction>& ops, Array<VirtualReg>& regs, Token tok, StructExpr* def, usize base_index)
{
    for (usize f = 0; f < def->fields.count(); ++f)
        zero_init_field(ops, regs, tok, def, f, base_index);
}

// Emit a deep copy of a fixed-size array: a fresh block is allocated and every
// element is copied from `src_base` to `dst_base` (element width = elem).
static void copy_array_into(Array<Instruction>& ops, Array<VirtualReg>& regs, Token tok, usize src_base, usize dst_base, ValueType elem, s64 len)
{
    const u32 elem_size = type_size(elem);
    for (s64 i = 0; i < len; ++i) {
        auto& tmp = allocate_reg(regs);
        tmp.type = elem;
        ops.push(Instruction{.type = OP_LOAD_PTR, .location = tok, .binop = {OP_LOAD_PTR, src_base, 0}, .reg_index = tmp.index, .int_val = (usize)(i * elem_size), .byte_size = (u8)elem_size});
        ops.push(Instruction{.type = OP_STORE_PTR, .location = tok, .binop = {OP_STORE_PTR, dst_base, tmp.index}, .reg_index = tmp.index, .int_val = (usize)(i * elem_size), .byte_size = (u8)elem_size});
    }
}

// Resolve the string literal index an assignment's rhs currently holds, so the
// target variable's `.len` can be folded at compile time. Returns -1 when the
// value is not a statically known literal (function calls, params, ...).
static s64 str_literal_index_of(Expression* rhs, Array<Variable>* local_vars)
{
    if (!rhs || rhs->type != Expr_Binary)
        return -1;
    auto* bin = static_cast<BinaryExpr*>(rhs);
    if (bin->tok.type == Tok_StrLit) {
        usize index = (usize)-1;
        if (get_string(bin->tok.val, &index))
            return (s64)index;
        return -1;
    }
    if (bin->tok.type == Tok_Ident) {
        Variable* src = get_variable(bin->tok, false, local_vars);
        if (src && src->type == TYPE_STR)
            return src->str_literal_index;
    }
    return -1;
}

// ---- Compile-time execution (`#run`) ----
// Nesting guard: a `#run` block translates its body through the ordinary
// pipeline, so a `#run` inside it (directly or in a callee) would re-enter
// this executor and mutate g_functions mid-iteration.
static usize g_comptime_depth = 0;

// Persistent storage for the synthetic functions' names (the StrView outlives
// the local StrBuilder).
static Array<StrBuilder>& comptime_name_pool()
{
    static Array<StrBuilder> pool{};
    return pool;
}


// Execute one `#run { ... }` block at compile time:
//   1. wrap the block in a synthetic function (`__comptime_run<N>`) and
//      translate it through the ordinary body pipeline,
//   2. emit every translated function into a scratch ELF64 shared object,
//      assembled with fasm and linked with `ld -shared`,
//   3. preload the libraries registered by `#libc` / `#lib("path")` with
//      RTLD_GLOBAL, so the block's `extern fn` calls resolve against them,
//   4. dlopen the scratch library in the compiler process, dlsym the
//      synthetic function and call it; its i64 return value becomes a
//      compile-time constant.
static s64 execute_comptime_run(RunExpr* run, ValueType& out_type)
{
    out_type = TYPE_I64;
#if !defined(__unix__) && !defined(__APPLE__)
    compiler_error(run->tok, "#run is only supported on POSIX hosts (the block executes inside the compiler process)\n");
    return 0;
#else
    if (g_comptime_depth > 0)
        compiler_error(run->tok, "#run blocks cannot be nested\n");
    g_comptime_depth++;

    // 1. Synthetic wrapper function: empty arg list, return type inferred
    //    from the block's `return`s (i64 when a value is returned, void
    //    otherwise).
    auto* fn_expr = new_expr<FunctionExpr>(run->tok, Expr_Function);
    fn_expr->block = run->block;
    comptime_name_pool().push(StrBuilder{});
    auto& name_builder = comptime_name_pool().last();
    (name_builder.append("__comptime_run") << g_comptime_counter).append_null(false);
    const usize this_counter = g_comptime_counter;
    g_comptime_counter++;
    // Reserve before pushing: the caller frames above (translate_function_body,
    // Expr_Call's callee binding) hold references into g_functions, and the
    // append must never reallocate the array out from under them.
    g_functions.reserve(g_functions.count() + 1);
    g_functions.push(DeclaredFunction{name_builder.to_string_view(true), fn_expr, {}, {}, TYPE_NOP, StrView(""), StrView(""), TYPE_NOP, 0, StrView(""), StrView("")});
    const usize fun_index = g_functions.count() - 1;
    g_functions.last().module_name = g_current_module_name;
    g_functions.last().src_path = src_path;
    g_functions.last().src_content = src_content;
    g_functions.last().is_comptime_wrapper = true;
    translate_function_body(g_functions[fun_index]);
    // Callee bodies need no forcing: every call site translates its callee
    // eagerly (see the Expr_Call case), so the whole call graph reachable from
    // this block is complete after the one translate_function_body call.
    // Functions it never calls stay untranslated here and are emitted as
    // label-only stubs in the scratch library (never executed).

    // 2. Shared-object assembly: the executable path's ELF emission minus the
    //    __entry synthesis (a library has no entry point).
    bool has_extern = false;
    for (usize i = 0; i < g_functions.count(); ++i)
        if (g_functions[i].is_extern) {
            has_extern = true;
            break;
        }
    StrBuilder label_b{};
    append_fn_label(label_b, g_functions[fun_index].name, g_functions[fun_index].module_name);
    label_b.append_null(false);

    // 3. Preload registered libraries into the global scope: the scratch
    //    library keeps its extern symbols undefined, and the dynamic loader
    //    resolves them against everything loaded RTLD_GLOBAL (libc included).
    for (usize i = 0; i < g_comptime_libs.count(); ++i) {
        StrBuilder lib_b{};
        lib_b.append(g_comptime_libs[i]).append_null(false);
        auto lib_path = lib_b.to_string_view(true);
        // A bare name ("test.dll", "mylib.so") follows the loader's standard
        // search paths; a relative path without a directory separator also
        // tries the compiler's working directory.
        void* lib_handle = dlopen(lib_path.data, RTLD_NOW | RTLD_GLOBAL);
        if (!lib_handle && !lib_path.starts_with("/")) {
            bool has_slash = false;
            for (usize c = 0; c < lib_path.size; ++c)
                if (lib_path.data[c] == '/') has_slash = true;
            if (!has_slash) {
                StrBuilder local_b{};
                local_b.append("./").append(lib_path).append_null(false);
                lib_handle = dlopen(local_b.to_string_view(true).data, RTLD_NOW | RTLD_GLOBAL);
            }
        }
        if (!lib_handle) {
            const char* err = dlerror();
            compiler_error(run->tok, "#lib failed to load '" SV_FORMAT "': %s\n", SV_ARG(lib_path), err ? err : "unknown dlopen error");
        }
    }

    // Resolve every extern symbol now, inside the compiler process: the
    // preloaded libraries (and libc, always present) are searched through the
    // global scope. Resolving here also turns a missing library into a clear
    // compile error instead of a runtime dlopen failure.
    g_comptime_externs.set_count(0);
    for (usize i = 0; i < g_functions.count(); ++i) {
        if (!g_functions[i].is_extern)
            continue;
        StrBuilder sym_b{};
        sym_b.append(g_functions[i].name).append_null(false);
        void* address = dlsym(RTLD_DEFAULT, sym_b.to_string_view(true).data);
        if (!address) {
            const char* err = dlerror();
            compiler_error(run->tok, "#run could not resolve extern function '" SV_FORMAT "' from #libc or #lib libraries%s%s\n", SV_ARG(g_functions[i].name), err ? ": " : "", err ? err : "");
        }
        g_comptime_externs.push(ComptimeExternAddress{g_functions[i].name, address});
    }
    g_comptime_emitting = true;

    StrBuilder builder{};
    builder.append("format ELF64\n\nsection '.text' executable\n");
    builder.append("public ").append(label_b.to_string_view(true)).append('\n');
    for (usize i = 0; i < g_functions.count(); ++i)
        if (g_functions[i].is_extern)
            (builder.append("extrn ").append(g_functions[i].name)).append('\n');
    // The std helpers are emitted with their raw-syscall paths (no `call
    // exit`): a shared object cannot carry PC32 relocations against undefined
    // symbols, and the block's own calls go through resolved addresses.
    builder.append('\n');
    add_std_library(builder, false, false);
    for (usize i = 0; i < g_functions.count(); ++i) {
        auto& func = g_functions[i];
        if (func.is_extern)
            continue;
        // Only bodies that finished translating are compiled here. A function
        // currently being translated (the one containing this very `#run`) or
        // not yet reached (declared later in the file) still has empty ops:
        // running the passes now would freeze that partial state
        // (passes_done=true blocks the real run later), so it is left out of
        // the scratch library entirely — nothing there can call it anyway,
        // and `ld -shared` tolerates the undefined references.
        if (func.ops.count() == 0)
            continue;
        // The passes run exactly once per function: here for the scratch
        // library, and compile_program skips anything already prepared.
        if (!func.passes_done) {
            dead_code(func);
            dead_store_elim(func);
            simplify_control_flow(func);
            allocate_registers(func);
            update_all_offsets(func.regs);
            func.passes_done = true;
        }
        compile_function(builder, func);
    }
    g_comptime_emitting = false;
    builder.append("\nsection '.data' writeable\n");
    append_string_data(builder);
    usize globals_bytes = g_globals_size < 1 ? 1 : g_globals_size;
    builder.append("\nsection '.bss' writeable\n");
    (builder.append("\t__globals: rb ") << globals_bytes).append('\n');

    // Scratch artifacts live next to the output path and are removed after
    // the block finishes.
    StrBuilder asm_b{}, obj_b{}, so_b{};
    asm_b.append(out_path) << ".ct" << this_counter << ".asm";
    obj_b.append(out_path) << ".ct" << this_counter << ".obj";
    so_b.append(out_path) << ".ct" << this_counter << ".so";
    asm_b.append_null(false);
    obj_b.append_null(false);
    so_b.append_null(false);
    auto asm_path = asm_b.to_string_view(true);
    auto obj_path = obj_b.to_string_view(true);
    auto so_path = so_b.to_string_view(true);

    FILE* file = fopen(asm_path.data, "wb");
    if (!file) {
        log_error("Failed to open scratch file '" SV_FORMAT "'\n", SV_ARG(asm_path));
        exit(1);
    }
    fwrite(builder.data(), 1, builder.count(), file);
    fclose(file);

    Cmd fasm_cmd{};
    fasm_cmd.push("./deps/fasm/fasm");
    fasm_cmd.append(asm_path).append(' ');
    fasm_cmd.append(obj_path);
    if (!execute_quietly(fasm_cmd)) {
        log_error("Failed to assemble compile-time code '" SV_FORMAT "'\n", SV_ARG(asm_path));
        exit(1);
    }
    Cmd ld_cmd{};
    ld_cmd.push("ld", "-shared", "-o");
    ld_cmd.append(so_path).append(' ');
    ld_cmd.append(obj_path);
    if (!execute_quietly(ld_cmd)) {
        log_error("Failed to link compile-time code '" SV_FORMAT "'\n", SV_ARG(so_path));
        exit(1);
    }

    // 4. Load and call. dlopen only treats the argument as a path when it
    //    contains a slash — a bare "prog.ct0.so" next to the output would
    //    search the loader's library paths instead of the current directory
    //    (unlike Windows). Make it explicit in that case.
    StrBuilder dlopen_b{};
    dlopen_b.append(so_path);
    bool has_slash = false;
    for (usize c = 0; c < so_path.size; ++c)
        if (so_path.data[c] == '/') has_slash = true;
    if (!has_slash) {
        StrBuilder local_b{};
        local_b.append("./").append(so_path).append_null(false);
        dlopen_b = local_b;
    }
    dlopen_b.append_null(false);
    void* handle = dlopen(dlopen_b.to_string_view(true).data, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* err = dlerror();
        compiler_error(run->tok, "#run failed to load compiled block '%s': %s\n", so_path.data, err ? err : "unknown dlopen error");
    }
    void* symbol = dlsym(handle, label_b.to_string_view(true).data);
    if (!symbol) {
        const char* err = dlerror();
        dlclose(handle);
        compiler_error(run->tok, "#run block symbol '" SV_FORMAT "' not found: %s\n", SV_ARG(label_b.to_string_view(true)), err ? err : "unknown dlsym error");
    }
    // The wrapper's inferred return type decides how rax is interpreted:
    // integers and bools come back in rax; anything else (structs, strings)
    // is not a compile-time constant.
    const ValueType wrapper_type = g_functions[fun_index].return_type;
    s64 result = 0;
    switch (wrapper_type) {
        case TYPE_BOOL:
        case TYPE_I8: case TYPE_U8:
        case TYPE_I16: case TYPE_U16:
        case TYPE_I32: case TYPE_U32:
        case TYPE_I64: case TYPE_U64:
            result = reinterpret_cast<s64 (*)(void)>(symbol)();
            break;
        case TYPE_VOID: case TYPE_NOP:
            // Statement-style block (`#run { print(1); }`): no value, but the
            // side effects must still happen.
            reinterpret_cast<s64 (*)(void)>(symbol)();
            break;
        default:
            dlclose(handle);
            compiler_error(run->tok, "#run block returns %s, which cannot be a compile-time constant\n", value_type_to_str(wrapper_type));
    }
    out_type = wrapper_type;
    dlclose(handle);
    unlink(asm_path.data);
    unlink(obj_path.data);
    unlink(so_path.data);

    g_comptime_depth--;
    return result;
#endif
}

VirtualReg translate_to_instruction(Array<Instruction>& ops, Array<VirtualReg>& regs, Array<Variable>& local_vars, Expression* expr, ValueType& return_type)
{
    if (!expr) return {};
    switch(expr->type)
    {
        case Expr_Binary: {
            auto* bin_expr = dynamic_cast<BinaryExpr*>(expr);
            ASSERT_NOT_NULL(bin_expr);

            // `&&` / `||`: short-circuit logical operators. The right operand
            // is translated only when the left operand does not decide the
            // result (`a && b` skips b when a is false, `a || b` skips b when
            // a is true), so side-effecting operands behave like C. The result
            // is always a normalized bool (0/1).
            if (bin_expr->tok.type == Tok_AndAnd || bin_expr->tok.type == Tok_OrOr) {
                const bool is_and = bin_expr->tok.type == Tok_AndAnd;
                auto lhs_reg = translate_to_instruction(ops, regs, local_vars, bin_expr->lhs, return_type);
                if (return_type == TYPE_VOID)
                    compiler_error(bin_expr->tok, "Cannot use a void value as a logical operand\n");
                if (return_type != TYPE_BOOL && !is_numeric_type(return_type))
                    compiler_error(bin_expr->tok, "Invalid operand to %s operation (%s)\n", is_and ? "&&" : "||", value_type_to_str(return_type));

                // A compile-time left operand folds the expression statically.
                if (lhs_reg.is_comp_time) {
                    const bool a = lhs_reg.int_val != 0;
                    if ((is_and && !a) || (!is_and && a))
                        return make_const(regs, TYPE_BOOL, is_and ? 0 : 1);
                    auto rhs_reg = translate_to_instruction(ops, regs, local_vars, bin_expr->rhs, return_type);
                    if (return_type == TYPE_VOID)
                        compiler_error(bin_expr->tok, "Cannot use a void value as a logical operand\n");
                    if (return_type != TYPE_BOOL && !is_numeric_type(return_type))
                        compiler_error(bin_expr->tok, "Invalid operand to %s operation (%s)\n", is_and ? "&&" : "||", value_type_to_str(return_type));
                    if (rhs_reg.is_comp_time)
                        return make_const(regs, TYPE_BOOL, rhs_reg.int_val != 0);
                    // Runtime rhs with a decided lhs: normalize to bool.
                    auto& norm = allocate_reg(regs);
                    norm.type = TYPE_BOOL;
                    ops.push(Instruction{.type = OP_NOT, .location = bin_expr->tok, .binop = {OP_NOT, rhs_reg.index, 0}, .reg_index = norm.index});
                    auto& norm2 = allocate_reg(regs);
                    norm2.type = TYPE_BOOL;
                    ops.push(Instruction{.type = OP_NOT, .location = bin_expr->tok, .binop = {OP_NOT, norm.index, 0}, .reg_index = norm2.index});
                    return norm2;
                }

                // `&&` short-circuits when lhs is false (jump on !lhs); `||`
                // short-circuits when lhs is true (jump on lhs). The fall-through
                // path evaluates rhs; both paths write the same result register.
                auto short_label = allocate_label();
                auto end_label = allocate_label();
                auto& neg = allocate_reg(regs);
                neg.type = TYPE_BOOL;
                ops.push(Instruction{.type = OP_NOT, .location = bin_expr->tok, .binop = {OP_NOT, lhs_reg.index, 0}, .reg_index = neg.index});
                ops.push(Instruction{.type = OP_JMP_IF, .data_type = TYPE_ADDRESS, .location = bin_expr->tok, .label = {short_label.label.ip}, .reg_index = is_and ? neg.index : lhs_reg.index, .is_visited = true});

                auto rhs_reg = translate_to_instruction(ops, regs, local_vars, bin_expr->rhs, return_type);
                if (return_type == TYPE_VOID)
                    compiler_error(bin_expr->tok, "Cannot use a void value as a logical operand\n");
                if (return_type != TYPE_BOOL && !is_numeric_type(return_type))
                    compiler_error(bin_expr->tok, "Invalid operand to %s operation (%s)\n", is_and ? "&&" : "||", value_type_to_str(return_type));
                auto& norm = allocate_reg(regs);
                norm.type = TYPE_BOOL;
                ops.push(Instruction{.type = OP_NOT, .location = bin_expr->tok, .binop = {OP_NOT, rhs_reg.index, 0}, .reg_index = norm.index});
                auto& norm2 = allocate_reg(regs);
                norm2.type = TYPE_BOOL;
                ops.push(Instruction{.type = OP_NOT, .location = bin_expr->tok, .binop = {OP_NOT, norm.index, 0}, .reg_index = norm2.index});

                auto& res = allocate_reg(regs);
                res.type = TYPE_BOOL;
                auto& zero = make_const(regs, TYPE_BOOL, 0);
                ops.push(Instruction{.type = OP_PLUS, .location = bin_expr->tok, .binop = {OP_PLUS, norm2.index, zero.index}, .reg_index = res.index});
                ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .label = {end_label.label.ip}, .is_visited = true});

                set_label(short_label, ops);
                ops.push(short_label);
                auto& one = make_const(regs, TYPE_BOOL, 1);
                ops.push(Instruction{.type = OP_PLUS, .location = bin_expr->tok, .binop = {OP_PLUS, is_and ? zero.index : one.index, zero.index}, .reg_index = res.index});
                set_label(end_label, ops);
                ops.push(end_label);
                return_type = TYPE_BOOL;
                return res;
            }

            ValueType lhs_type = TYPE_NOP;
            ValueType rhs_type = TYPE_NOP;
            // `expr as Pair^`: a struct name as cast target is not a variable
            // reference. Like builtin type names it evaluates to a compile-time
            // type marker; the indirection depth stays on the node for
            // pointer_metadata.
            bool struct_cast_target = false;
            if (bin_expr->tok.type == Tok_Cast && bin_expr->rhs && bin_expr->rhs->type == Expr_Binary) {
                auto* cast_type = static_cast<BinaryExpr*>(bin_expr->rhs);
                struct_cast_target = cast_type->tok.type == Tok_Ident && !cast_type->lhs
                    && find_visible_struct(cast_type->tok.val, cast_type->module_name) != Array<DeclaredStruct>::INVALID_INDEX;
            }
            auto lhs_reg = translate_to_instruction(ops, regs, local_vars, bin_expr->lhs, lhs_type);
            VirtualReg rhs_reg = struct_cast_target
                ? make_const(regs, TYPE_I64, (s64)TYPE_STRUCT)
                : translate_to_instruction(ops, regs, local_vars, bin_expr->rhs, rhs_type);
            if (struct_cast_target) {
                auto* cast_type = static_cast<BinaryExpr*>(bin_expr->rhs);
                rhs_type = cast_type->ptr_depth > 0 ? TYPE_PTR : TYPE_STRUCT;
                if (cast_type->ptr_depth > 0) {
                    // A raw pointer does not directly produce a `Foo^`: that
                    // value class is the address of a slot holding a struct's
                    // heap base (what `&struct_var` yields), so member access
                    // and deref both fetch the base through it. Wrap the raw
                    // pointer in an anonymous reference cell to conform.
                    if (lhs_type != TYPE_PTR)
                        compiler_error(bin_expr->tok, "Invalid operands to CAST operation (%s and %s)\n", value_type_to_str(lhs_type), value_type_to_str(rhs_type));
                    auto& cell = allocate_reg(regs);
                    cell.type = TYPE_PTR;
                    ops.push(Instruction{.type = OP_ALLOC, .location = bin_expr->tok, .reg_index = cell.index, .int_val = STACK_REGISTER_SIZE, .align = (u32)STACK_REGISTER_SIZE});
                    ops.push(Instruction{.type = OP_STORE_PTR, .location = bin_expr->tok, .binop = {OP_STORE_PTR, cell.index, lhs_reg.index}, .reg_index = lhs_reg.index});
                    return_type = TYPE_PTR;
                    return cell;
                }
            }
            // u32 _index = regs.count() - 1;
            auto& operation = bin_expr->tok.type;
            if (operation == Tok_IntLit) {
                // Parse as an unsigned magnitude: u64 literals reach 2^64-1,
                // which does not fit in s64 (and would silently clamp with
                // atoll). Anything larger overflows even u64 and is an error.
                u64 raw = 0;
                if (!parse_u64_literal(bin_expr->tok.val, raw))
                    compiler_error(bin_expr->tok, "Integer literal '" SV_FORMAT "' is out of range\n", SV_ARG(bin_expr->tok.val));

                // Values in [2^63, 2^64-1] only fit in u64, so such literals
                // are typed u64 (the bit pattern is preserved in int_val);
                // smaller literals are plain i64.
                const bool is_u64 = raw > (u64)0x7FFFFFFFFFFFFFFFll;
                return_type = is_u64 ? TYPE_U64 : TYPE_I64;

                auto& reg = allocate_reg(regs);
                reg.is_comp_time = true;
                reg.type = return_type;
                reg.int_val = (s64)raw;
                ops.push(Instruction{.type = OP_PUSH_I64, .location = bin_expr->tok, .reg_index = reg.index});
                return reg;
            }
            else if (operation == Tok_BoolLit) {
                return_type = TYPE_BOOL;

                auto& reg = allocate_reg(regs);
                reg.is_comp_time = true;
                reg.type = return_type;
                reg.bool_val = bin_expr->tok.val == "true" ? true : false;
                reg.int_val = reg.bool_val;
                ops.push(Instruction{.type = OP_PUSH_BOOL, .location = bin_expr->tok, .reg_index = reg.index});
                return reg;
            }
            else if (operation == Tok_NullLit) {
                // `null` is a void* whose value is 0; it casts freely to any
                // pointer type (all pointers share the TYPE_PTR ValueType).
                return_type = TYPE_PTR;

                auto& reg = allocate_reg(regs);
                reg.is_comp_time = true;
                reg.type = TYPE_PTR;
                reg.int_val = 0;
                ops.push(Instruction{.type = OP_PUSH_I64, .location = bin_expr->tok, .reg_index = reg.index});
                return reg;
            }
            else if (operation == Tok_StrLit) {
                usize index = 0;
                auto* str = get_string(bin_expr->tok.val, &index);
                if (!str) TODO("report error");
                return_type = TYPE_STR;

                auto& reg = allocate_reg(regs);
                reg.type = return_type;
                reg.str_val = index;
                ops.push(Instruction{.type = OP_PUSH_STR, .location = bin_expr->tok, .reg_index = reg.index});
                return reg;
            }
            else if (operation == Tok_Type) {
                // A standalone type name is a compile-time constant equal to
                // its type id and typed as that type, so `#type_id(i32)`,
                // `#type_size(i32)`, `#type_of(i32)` and `#type_id(x) == i32`
                // all work. The `as` cast path reads only rhs_type and is
                // unaffected by the constant register produced here.
                ValueType type = str_to_value_type(bin_expr->tok.val);
                if (type == TYPE_NOP)
                    compiler_error(bin_expr->tok, "Unknown cast type '" SV_FORMAT "'\n", SV_ARG(bin_expr->tok.val));
                // `u8^` / `bool^^` as a cast target is a pointer type; the
                // pointee and depth live on the node for pointer_metadata.
                if (bin_expr->ptr_depth > 0) {
                    if (type == TYPE_VOID)
                        compiler_error(bin_expr->tok, "Type 'void' cannot be a pointer\n");
                    type = TYPE_PTR;
                }
                // The register holds the type id as a plain i64 (int_val/bool_val
                // share a union, so a bool-typed const would truncate the id to
                // 0/1); the *type* is reported separately via return_type, which
                // is what type_id/type_size/type_of and casts consume.
                return_type = type;
                return make_const(regs, TYPE_I64, (s64)type);
            }
            else if (operation == Tok_Ident) {
                auto* var = bind_global_variable(bin_expr->tok, &local_vars, regs, bin_expr->module_name);
                if (!var || !var->is_accesible) {
                    compiler_error(bin_expr->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(bin_expr->tok.val));
                }
                return_type = var->type;
                // A compile-time constant variable (`x := #run { ... }`) folds
                // to its value; the PUSH materializes it only if a runtime
                // context (print, store) actually consumes the read.
                if (var->is_comp_time) {
                    auto& const_reg = allocate_reg(regs);
                    const_reg.is_comp_time = true;
                    const_reg.type = var->type;
                    const_reg.int_val = var->comp_time_val;
                    ops.push(Instruction{.type = var->type == TYPE_BOOL ? OP_PUSH_BOOL : OP_PUSH_I64, .location = bin_expr->tok, .reg_index = const_reg.index});
                    return const_reg;
                }
                // auto& reg = allocate_reg(regs);
                auto& var_reg = regs[var->reg_index];
                return var_reg;
            } else {
                auto instruction_type = operator_to_instruction(operation);
                if (instruction_type == OP_NOP) {
                    compiler_error(bin_expr->tok, "Unknown operation: '" SV_FORMAT "' (%s)\n", SV_ARG(bin_expr->tok.val), tok_type_to_str(operation));
                }
                if (!is_valid_operation(instruction_type, lhs_type, rhs_type, return_type)) {
                    compiler_error(bin_expr->tok, "Invalid operands to %s operation (%s and %s)\n", inst_type_to_str(instruction_type), value_type_to_str(lhs_type), value_type_to_str(rhs_type));
                }
                if (instruction_type != OP_CAST) {
                    // Algebraic identities. These hold for all operand values
                    // and let the result reuse an existing register (or fold
                    // to a constant) instead of emitting a runtime operation.
                    if (lhs_reg.index == rhs_reg.index) {
                        switch(instruction_type) {
                            case OP_EQUALS:
                            case OP_LESS_EQUALS:
                            case OP_GREATER_EQUALS:
                                return_type = TYPE_BOOL;
                                return make_const(regs, TYPE_BOOL, 1);
                            case OP_NOT_EQUALS:
                            case OP_LESS:
                            case OP_GREATER:
                                return_type = TYPE_BOOL;
                                return make_const(regs, TYPE_BOOL, 0);
                            case OP_MINUS:
                                return make_const(regs, return_type, 0);
                            default: break;
                        }
                    }
                    if (rhs_reg.is_comp_time) {
                        switch(instruction_type) {
                            case OP_PLUS:
                            case OP_MINUS:
                                if (rhs_reg.int_val == 0) return lhs_reg;
                                break;
                            case OP_MULT:
                                if (rhs_reg.int_val == 0) return make_const(regs, return_type, 0);
                                if (rhs_reg.int_val == 1) return lhs_reg;
                                break;
                            case OP_DIVIDE:
                                if (rhs_reg.int_val == 1) return lhs_reg;
                                break;
                            case OP_MOD:
                                if (rhs_reg.int_val == 1) return make_const(regs, return_type, 0);
                                break;
                            default: break;
                        }
                    }
                    if (lhs_reg.is_comp_time) {
                        switch(instruction_type) {
                            case OP_PLUS:
                                if (lhs_reg.int_val == 0) return rhs_reg;
                                break;
                            case OP_MULT:
                                if (lhs_reg.int_val == 0) return make_const(regs, return_type, 0);
                                if (lhs_reg.int_val == 1) return rhs_reg;
                                break;
                            default: break;
                        }
                    }
                    auto& res_reg = allocate_reg(regs);
                    res_reg.type = return_type;
                    if (lhs_reg.is_comp_time && rhs_reg.is_comp_time) {
                        // Division/modulo by a compile-time zero must be a clean
                        // error, not a SIGFPE in the compiler.
                        if ((instruction_type == OP_DIVIDE || instruction_type == OP_MOD) && rhs_reg.int_val == 0)
                            compiler_error(bin_expr->tok, "Division by zero in constant expression\n");
                        // INT64_MIN / -1 overflows (SIGFPE on x86); reject it too.
                        if (instruction_type == OP_DIVIDE && !is_unsigned_type(return_type)
                            && lhs_reg.int_val == INT64_MIN && rhs_reg.int_val == -1)
                            compiler_error(bin_expr->tok, "Integer overflow in constant expression\n");
                        eval_binary(instruction_type, return_type, lhs_reg, rhs_reg, res_reg);
                    }

                    ops.push(Instruction{.type = instruction_type, .location = bin_expr->tok, .binop = {instruction_type, lhs_reg.index, rhs_reg.index}, .reg_index = res_reg.index});
                    return res_reg;
                }
                else {
                    // OP_CAST: convert lhs_reg's value to the target type
                    // `rhs_type` (validated above, return_type == rhs_type).
                    // Compile-time operands are folded to a truncated constant;
                    // runtime operands get an OP_CAST that truncates in codegen.
                    if (lhs_reg.is_comp_time) {
                        s64 v = lhs_reg.int_val;
                        if (rhs_type == TYPE_BOOL)
                            v = v != 0;
                        else
                            v = truncate_value(v, rhs_type);
                        return make_const(regs, rhs_type, v);
                    }
                    // Strings need no numeric conversion; pass the value through.
                    if (rhs_type == TYPE_STR)
                        return lhs_reg;

                    // Widening casts are free when the value's stored bit
                    // pattern already matches the target interpretation (see
                    // cast_is_free); otherwise emit an OP_CAST to re-interpret
                    // (zero-/sign-extend or truncate) in codegen.
                    if (cast_is_free(lhs_type, rhs_type))
                        return lhs_reg;

                    // Re-interpreting/truncating cast: emit an OP_CAST.
                    auto& cast_reg = allocate_reg(regs);
                    cast_reg.type = rhs_type;
                    ops.push(Instruction{.type = OP_CAST, .location = bin_expr->tok, .binop = {OP_CAST, lhs_reg.index, lhs_reg.index}, .reg_index = cast_reg.index});
                    return cast_reg;
                }
            }
        } break;

        case Expr_Function: {
            auto* fun_expr = dynamic_cast<FunctionExpr*>(expr);
            ASSERT_NOT_NULL(fun_expr);
            // An extern (FFI) declaration has no body to translate; its
            // signature is final and the symbol comes from the C linker.
            if (fun_expr->is_extern)
                break;
            auto index = find_visible_function(fun_expr->tok, g_current_module_name);
            if (index != Array<DeclaredFunction>::INVALID_INDEX) {
                translate_function_body(g_functions[index]);
            } else {
                // should never happened, probably compiler bug if hits
                compiler_error(fun_expr->tok, "Undeclared function '" SV_FORMAT "'\n", SV_ARG(fun_expr->tok.val));
            }
        } break;

        case Expr_Block: {
            auto* block_expr = dynamic_cast<BlockExpr*>(expr);
            ASSERT_NOT_NULL(block_expr);
            auto before_vars = local_vars.count();
            // The block's value is its last statement's value (`#run { 40 + 2 }`
            // evaluates to 42); statements after the last overwrite it.
            VirtualReg last_value{};
            for (auto& statement : block_expr->exprs) {
                last_value = translate_to_instruction(ops, regs, local_vars, statement, return_type);
            }
            for (; before_vars < local_vars.count(); ++before_vars)
                local_vars[before_vars].is_accesible = false;
            return last_value;
        } break;

        case Expr_Return: {
            auto* ret_expr = dynamic_cast<ReturnExpr*>(expr);
            ASSERT_NOT_NULL(ret_expr);
            // The function's declared or so-far-inferred return type. Every
            // `return` must agree with it: numeric types auto-promote/cast to
            // it, anything else must match exactly, and void functions cannot
            // return a value.
            ValueType expected = g_live_function_return_type;

            if (ret_expr->rhs == nullptr) {
                // Bare `return;` (void return).
                if (expected == TYPE_NOP)
                    g_live_function_return_type = TYPE_VOID;
                else if (expected != TYPE_VOID)
                    compiler_error(ret_expr->tok, "Inconsistent return type: function returns %s but 'return;' returns nothing\n", value_type_to_str(expected));
                return_type = TYPE_VOID;
                auto& ret_reg = make_const(regs, TYPE_I64, 0);
                emit_deferred_calls(ops, regs);
                ops.push(Instruction{.type = OP_RET, .location = ret_expr->tok, .reg_index = ret_reg.index});
                break;
            }

            // Struct returns materialize the value in the function's hidden
            // caller-reserved return slot (not the local struct area): only the
            // caller's frame survives the call. The slot address is what gets
            // returned, so callers receive a pointer into their own frame.
            const bool is_struct_return = g_function_hidden_slot != (usize)-1
                && struct_name_of(ret_expr->rhs, local_vars).size > 0;
            if (is_struct_return) {
                if (expected == TYPE_NOP)
                    g_live_function_return_type = TYPE_STRUCT;
                else if (expected != TYPE_STRUCT)
                    compiler_error(ret_expr->tok, "Inconsistent return type: expected %s, got %s\n", value_type_to_str(expected), value_type_to_str(TYPE_STRUCT));
                const usize saved_off = g_return_area_off;
                const usize saved_max = g_return_area_max;
                g_return_slot_reg = g_function_hidden_slot;
                g_return_area_off = 0;
                if (ret_expr->rhs->type == Expr_StructInit) {
                    // A literal: translate it directly into the return slot
                    // (Expr_StructInit allocates from the return area while the
                    // context is active). The top-level struct's address is the
                    // slot register itself.
                    translate_to_instruction(ops, regs, local_vars, ret_expr->rhs, return_type);
                } else {
                    // A variable/member/call result may point into this frame;
                    // deep-copy its fields into the return slot instead.
                    ValueType rhs_type;
                    auto src = translate_to_instruction(ops, regs, local_vars, ret_expr->rhs, rhs_type);
                    copy_struct_into(ops, regs, src.index, struct_name_of(ret_expr->rhs, local_vars), struct_module_of(ret_expr->rhs, local_vars));
                    return_type = TYPE_STRUCT;
                }
                g_return_area_off = saved_off;
                g_return_area_max = MAX(g_return_area_max, saved_max);
                emit_deferred_calls(ops, regs);
                ops.push(Instruction{.type = OP_RET, .location = ret_expr->tok, .reg_index = g_return_slot_reg});
                g_return_slot_reg = (usize)-1;
                break;
            }

            auto res_reg = translate_to_instruction(ops, regs, local_vars, ret_expr->rhs, return_type);
            if (return_type == TYPE_VOID)
                compiler_error(ret_expr->tok, "Cannot return a void value\n");

            if (expected == TYPE_NOP) {
                // First value return: it fixes the inferred function type.
                g_live_function_return_type = return_type;
            } else if (expected == TYPE_VOID) {
                compiler_error(ret_expr->tok, "Cannot return a value from a void function\n");
            } else if (expected != return_type) {
                // Auto promotion/casting applies only between numeric types.
                if (!is_numeric_type(expected) || !is_numeric_type(return_type))
                    compiler_error(ret_expr->tok, "Inconsistent return type: expected %s, got %s\n", value_type_to_str(expected), value_type_to_str(return_type));
                if (res_reg.is_comp_time) {
                    res_reg = make_const(regs, expected, truncate_value(res_reg.int_val, expected));
                } else if (!cast_is_free(return_type, expected)) {
                    // Runtime value: emit a cast so the returned register holds
                    // the value truncated/re-interpreted as the return type.
                    auto& cast_reg = allocate_reg(regs);
                    cast_reg.type = expected;
                    ops.push(Instruction{.type = OP_CAST, .location = ret_expr->tok, .binop = {OP_CAST, res_reg.index, res_reg.index}, .reg_index = cast_reg.index});
                    res_reg = cast_reg;
                }
            }

            emit_deferred_calls(ops, regs);
            ops.push(Instruction{.type = OP_RET, .location = ret_expr->tok, .reg_index = res_reg.index});
        } break;

        case Expr_Break:
        case Expr_Continue: {
            const bool is_break = expr->type == Expr_Break;
            if (g_loop_stack.is_empty())
                compiler_error(expr->tok, is_break
                    ? "Cannot use 'break' outside of a loop\n"
                    : "Cannot use 'continue' outside of a loop\n");
            auto& ctx = g_loop_stack.last();
            ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .location = expr->tok,
                .label = {is_break ? ctx.break_label : ctx.continue_label}, .is_visited = true});
            return_type = TYPE_VOID;
        } break;

        case Expr_Defer: {
            auto* defer_expr = dynamic_cast<DeferExpr*>(expr);
            ASSERT_NOT_NULL(defer_expr);
            return_type = TYPE_VOID;
            if (defer_expr->expr->type != Expr_Call && defer_expr->expr->type != Expr_Print)
                compiler_error(defer_expr->tok, "Only function calls can be deferred\n");

            // Translate the deferred expression inline: its arguments are
            // evaluated (captured) now, when the `defer` statement executes.
            // The call itself is the last instruction pushed; pop it and
            // remember it so every `return` re-emits it (LIFO). Arguments live
            // in registers that liveness keeps alive until the deferred call.
            const usize ops_before = ops.count();
            translate_to_instruction(ops, regs, local_vars, defer_expr->expr, return_type);
            if (ops.count() == ops_before)
                compiler_error(defer_expr->tok, "Nothing to defer\n");
            Instruction deferred_call = ops.pop();
            snapshot_deferred_args(ops, regs, deferred_call);

            // Registration flag: 1 when this `defer` statement runs (a branch
            // that didn't execute leaves it 0 from the entry init). The flag
            // register is written by an OP_CAST from a fresh constant source so
            // it is a runtime value the return-point guard can branch on.
            auto& flag = allocate_reg(regs);
            flag.type = TYPE_BOOL;
            auto& one = make_const(regs, TYPE_BOOL, 1);
            ops.push(Instruction{.type = OP_CAST, .location = defer_expr->tok, .binop = {OP_CAST, one.index, one.index}, .reg_index = flag.index});

            g_deferred_calls.push(DeferredCall{deferred_call, flag.index});
        } break;

        case Expr_Print: {
            auto* print_expr = dynamic_cast<PrintExpr*>(expr);
            ASSERT_NOT_NULL(print_expr);
            ValueType rhs_type;
            auto rhs = translate_to_instruction(ops, regs, local_vars, print_expr->rhs, rhs_type);
            if (rhs_type == TYPE_VOID)
                compiler_error(print_expr->tok, "Cannot print a void value\n");
            if (rhs_type == TYPE_STRUCT)
                compiler_error(print_expr->rhs->tok, "Cannot print a struct value\n");
            if (rhs_type == TYPE_ARRAY)
                compiler_error(print_expr->rhs->tok, "Cannot print an array value\n");

            return_type = TYPE_VOID;
            Variable var = {print_expr->rhs->tok.val, rhs_type};
            ops.push(Instruction{.type = OP_PRINT, .location = print_expr->tok, .target = {var}, .reg_index = rhs.index});
        } break;

        case Expr_Assert: {
            auto* assert_expr = dynamic_cast<AssertExpr*>(expr);
            ASSERT_NOT_NULL(assert_expr);
            // `#assert <expr>`: translate the operand into this scratch
            // context; constant folding leaves a compile-time register whose
            // value decides the check. Nothing is emitted into `ops`.
            ValueType value_type = TYPE_NOP;
            auto value = translate_to_instruction(ops, regs, local_vars, assert_expr->rhs, value_type);
            if (value_type != TYPE_BOOL && !is_numeric_type(value_type))
                compiler_error(assert_expr->rhs->tok, "#assert expression must be of a numeric or bool type, got %s\n", value_type_to_str(value_type));
            if (!value.is_comp_time)
                compiler_error(assert_expr->rhs->tok, "#assert expression must be a compile-time constant\n");
            const bool failed = value_type == TYPE_U64
                ? (u64)value.int_val < 1
                : value.int_val < 1;
            if (failed)
                compiler_error(assert_expr->tok, "#assert failed: expression evaluated to %lld\n", (long long)value.int_val);
        } break;

        case Expr_ComptimeLib: {
            // `#libc` / `#lib("path")`: registered at parse time; nothing to
            // emit.
        } break;

        case Expr_Run: {
            // `#run { ... }`: execute the block in the compiler process right
            // now; the result becomes a compile-time constant of the block's
            // inferred type (any integer width or bool), so
            // `x := #run { return 6 * 7; }` folds x to 42 with zero runtime
            // code emitted for the block itself.
            auto* run = dynamic_cast<RunExpr*>(expr);
            ASSERT_NOT_NULL(run);
            ValueType run_type = TYPE_I64;
            const s64 result = execute_comptime_run(run, run_type);
            return_type = run_type == TYPE_NOP ? TYPE_I64 : run_type;
            // Like a literal: the register folds at compile time AND
            // materializes through its OP_PUSH when a runtime context needs it
            // (e.g. `print(#run { return 1 })`).
            auto& reg = allocate_reg(regs);
            reg.is_comp_time = true;
            reg.type = return_type;
            reg.int_val = result;
            if (return_type == TYPE_BOOL)
                reg.bool_val = result != 0;
            ops.push(Instruction{.type = return_type == TYPE_BOOL ? OP_PUSH_BOOL : OP_PUSH_I64, .location = run->tok, .reg_index = reg.index});
            return reg;
        } break;

        case Expr_AddressOf: {
            auto* addr = dynamic_cast<AddressOfExpr*>(expr);
            ASSERT_NOT_NULL(addr);
            if (addr->operand->type == Expr_MemberCall) {
                // `&f.x`: a member is an lvalue, so its address is valid. The
                // struct value is a heap pointer held in the variable's
                // register; the member address is that base plus the field's
                // byte offset.
                auto* member = static_cast<MemberCallExpr*>(addr->operand);
                auto* var = bind_global_variable(member->tok, &local_vars, regs);
                if (!var || !var->is_accesible) {
                    compiler_error(member->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(member->tok.val));
                }
                StrView owner_struct = var->struct_name;
                StrView owner_module = var->struct_module;
                if (var->type == TYPE_PTR && var->pointee == TYPE_STRUCT) {
                    owner_struct = var->pointee_struct_name;
                    owner_module = var->pointee_struct_module;
                }
                else if (var->type != TYPE_STRUCT) {
                    compiler_error(member->tok, "Cannot take the address of a member of a non-struct value (variable '" SV_FORMAT "')\n", SV_ARG(var->name));
                }
                auto struct_index = find_visible_struct(owner_struct, owner_module);
                if (!g_structs.is_valid_index(struct_index)) {
                    compiler_error(member->tok, "Cannot find Struct with name: '" SV_FORMAT "'\n", SV_ARG(owner_struct));
                }
                usize field_index = (usize)-1;
                for (usize fi = 0; fi < g_structs[struct_index].expr->fields.count(); ++fi) {
                    if (g_structs[struct_index].expr->fields[fi].name == member->field.val) { field_index = fi; break; }
                }
                if (field_index == (usize)-1) {
                    compiler_error(member->tok, "Struct '" SV_FORMAT "' doesnt contain '" SV_FORMAT "' field\n", SV_ARG(owner_struct), SV_ARG(member->field.val));
                }
                auto& base = regs[var->reg_index];
                auto& field = g_structs[struct_index].expr->fields[field_index];
                // `&p.x` where `p` is a pointer to a struct: the address of the
                // member slot is the pointee's heap base plus the offset, so
                // fetch the heap base through the pointer first.
                if (var->type == TYPE_PTR && var->pointee == TYPE_STRUCT) {
                    auto& heap_base = allocate_reg(regs);
                    heap_base.type = TYPE_STRUCT;
                    ops.push(Instruction{.type = OP_LOAD_PTR, .location = member->tok, .binop = {OP_LOAD_PTR, base.index, 0}, .reg_index = heap_base.index});
                    auto& offset = make_const(regs, TYPE_PTR, (s64)field.offset);
                    auto& res = allocate_reg(regs);
                    res.type = TYPE_PTR;
                    ops.push(Instruction{.type = OP_PLUS, .location = member->tok, .binop = {OP_PLUS, heap_base.index, offset.index}, .reg_index = res.index});
                    return_type = TYPE_PTR;
                    return res;
                }
                auto& offset = make_const(regs, TYPE_PTR, (s64)field.offset);
                auto& res = allocate_reg(regs);
                res.type = TYPE_PTR;
                ops.push(Instruction{.type = OP_PLUS, .location = member->tok, .binop = {OP_PLUS, base.index, offset.index}, .reg_index = res.index});
                return_type = TYPE_PTR;
                return res;
            }
            if (addr->operand->type == Expr_Deref) {
                // `&^p` / `&^^p` = the address of the pointee location, i.e.
                // the pointer being dereferenced with one level stripped
                // (`&*p` ≡ `p`). No lea or load is emitted: the pointer value
                // IS the address. The deref is only valid if its target is a
                // real pointer, so validate before re-using its register.
                auto* deref = static_cast<DerefExpr*>(addr->operand);
                ValueType ptr_type = TYPE_NOP;
                auto ptr = translate_to_instruction(ops, regs, local_vars, deref->target, ptr_type);
                if (ptr_type != TYPE_PTR) {
                    compiler_error(addr->tok, "Cannot dereference a non-pointer value\n");
                }
                ValueType pointee;
                u8 depth;
                StrView pointee_struct = "";
                StrView pointee_struct_module = "";
                pointer_metadata(deref->target, local_vars, pointee, depth, pointee_struct, pointee_struct_module);
                if (depth == 0) {
                    compiler_error(addr->tok, "Cannot dereference a non-pointer value\n");
                }
                return_type = TYPE_PTR;
                return ptr;
            }
            // Only a plain variable reference is an lvalue: a bare identifier
            // (lhs == rhs == nullptr). A literal (`10`, `true`, `"s"`) also
            // produces a BinaryExpr with null operands, so require the token to
            // actually be an identifier — otherwise `&10` would be "address of
            // an undeclared variable named 10" instead of an rvalue error.
            auto* operand_binary = dynamic_cast<BinaryExpr*>(addr->operand);
            if (!operand_binary || operand_binary->lhs != nullptr || operand_binary->rhs != nullptr
                || operand_binary->tok.type != Tok_Ident)
                compiler_error(addr->tok, "Cannot take the address of a non-lvalue expression\n");
            auto* var = bind_global_variable(addr->operand->tok, &local_vars, regs);
            if (!var || !var->is_accesible) {
                compiler_error(addr->operand->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(addr->operand->tok.val));
            }
            // `&f` where `f` is a struct value is the address of the variable's
            // slot (a `Foo^`): the same lea used for scalar variables.
            auto& res = allocate_reg(regs);
            res.type = TYPE_PTR;
            ops.push(Instruction{.type = OP_LEA, .data_type = TYPE_VARIABLE, .location = addr->tok, .target = {*var}, .reg_index = res.index});
            return_type = TYPE_PTR;
            return res;
        } break;

        case Expr_Deref: {
            auto* deref = dynamic_cast<DerefExpr*>(expr);
            ASSERT_NOT_NULL(deref);
            ValueType ptr_type = TYPE_NOP;
            auto ptr = translate_to_instruction(ops, regs, local_vars, deref->target, ptr_type);
            if (ptr_type != TYPE_PTR)
                compiler_error(deref->tok, "Cannot dereference a non-pointer value\n");
            ValueType pointee;
            u8 depth;
            StrView pointee_struct = "";
            StrView pointee_struct_module = "";
            pointer_metadata(deref->target, local_vars, pointee, depth, pointee_struct, pointee_struct_module);
            if (depth == 0)
                compiler_error(deref->tok, "Cannot dereference a non-pointer value\n");
            deref->pointee = pointee;
            deref->ptr_depth = depth - 1;
            deref->pointee_struct_name = pointee_struct;
            deref->pointee_struct_module = pointee_struct_module;
            ValueType result_type = (deref->ptr_depth == 0) ? pointee : TYPE_PTR;
            auto& res = allocate_reg(regs);
            res.type = result_type;
            ops.push(Instruction{.type = OP_LOAD_PTR, .location = deref->tok, .binop = {OP_LOAD_PTR, ptr.index, 0}, .reg_index = res.index});
            return_type = result_type;
            return res;
        } break;

        case Expr_Not: {
            auto* not_expr = dynamic_cast<NotExpr*>(expr);
            ASSERT_NOT_NULL(not_expr);
            ValueType operand_type = TYPE_NOP;
            auto operand = translate_to_instruction(ops, regs, local_vars, not_expr->operand, operand_type);
            if (operand_type == TYPE_VOID)
                compiler_error(not_expr->tok, "Cannot negate a void value\n");
            if (operand_type != TYPE_BOOL && !is_numeric_type(operand_type))
                compiler_error(not_expr->tok, "Invalid operand to NOT operation (%s)\n", value_type_to_str(operand_type));
            return_type = TYPE_BOOL;
            // Fold compile-time operands: `!x` == `(x == 0)`.
            if (operand.is_comp_time) {
                auto& const_reg = make_const(regs, TYPE_BOOL, operand.int_val == 0);
                return const_reg;
            }
            auto& res = allocate_reg(regs);
            res.type = TYPE_BOOL;
            ops.push(Instruction{.type = OP_NOT, .location = not_expr->tok, .binop = {OP_NOT, operand.index, 0}, .reg_index = res.index});
            return res;
        } break;

        case Expr_DerefAssign: {
            auto* assign = dynamic_cast<DerefAssignExpr*>(expr);
            ASSERT_NOT_NULL(assign);
            // assign->target is the pointer expression to store through:
            // `p` for `^p = v`, `^p` for `^^p = v`, `&x` for `^(&x) = v`.
            // `p` for `^p = v`, `^p` for `^^p = v`, `&x` for `^(&x) = v`.
            ValueType ptr_type = TYPE_NOP;
            auto ptr = translate_to_instruction(ops, regs, local_vars, assign->target, ptr_type);
            if (ptr_type != TYPE_PTR)
                compiler_error(assign->tok, "Cannot dereference a non-pointer value\n");
            ValueType pointee;
            u8 depth;
            StrView pointee_struct = "";
            StrView pointee_struct_module = "";
            pointer_metadata(assign->target, local_vars, pointee, depth, pointee_struct, pointee_struct_module);
            if (depth == 0)
                compiler_error(assign->tok, "Cannot dereference a non-pointer value\n");
            ValueType rhs_type = TYPE_NOP;
            auto rhs = translate_to_instruction(ops, regs, local_vars, assign->rhs, rhs_type);
            if (rhs_type == TYPE_VOID)
                compiler_error(assign->rhs->tok, "Cannot assign a void value\n");
            ops.push(Instruction{.type = OP_STORE_PTR, .location = assign->tok, .binop = {OP_STORE_PTR, ptr.index, rhs.index}, .reg_index = rhs.index});
            return_type = TYPE_VOID;
            return {};
        } break;

        case Expr_Assignment: {
            auto* assign = dynamic_cast<AssignmentExpr*>(expr);
            ASSERT_NOT_NULL(assign);

            // `foo.x = v`: a store through the struct's heap pointer at the
            // field's byte offset.
            if (assign->field_name.type == Tok_Ident) {
                auto* struct_var = bind_global_variable(assign->tok, &local_vars, regs);
                if (!struct_var || !struct_var->is_accesible) {
                    compiler_error(assign->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(assign->tok.val));
                }
                StrView owner_struct = struct_var->struct_name;
                StrView owner_module = struct_var->struct_module;
                usize base_reg = struct_var->reg_index;
                if (struct_var->type == TYPE_PTR && struct_var->pointee == TYPE_STRUCT) {
                    // `p.x = v` where `p` is a pointer to a struct: fetch the
                    // pointee's heap base through the pointer first, then store
                    // the member value at its offset.
                    owner_struct = struct_var->pointee_struct_name;
                    owner_module = struct_var->pointee_struct_module;
                    auto& heap_base = allocate_reg(regs);
                    heap_base.type = TYPE_STRUCT;
                    ops.push(Instruction{.type = OP_LOAD_PTR, .location = assign->tok, .binop = {OP_LOAD_PTR, struct_var->reg_index, 0}, .reg_index = heap_base.index});
                    base_reg = heap_base.index;
                } else if (struct_var->type != TYPE_STRUCT) {
                    compiler_error(assign->tok, "Cannot assign a member of a non-struct value\n");
                }
                auto sindex = find_visible_struct(owner_struct, owner_module);
                if (!g_structs.is_valid_index(sindex)) {
                    compiler_error(assign->tok, "Cannot find Struct with name: '" SV_FORMAT "'\n", SV_ARG(owner_struct));
                }
                usize field_index = (usize)-1;
                for (usize f = 0; f < g_structs[sindex].expr->fields.count(); ++f) {
                    if (g_structs[sindex].expr->fields[f].name == assign->field_name.val) { field_index = f; break; }
                }
                if (field_index == (usize)-1) {
                    compiler_error(assign->tok, "Struct '" SV_FORMAT "' doesnt contain '" SV_FORMAT "' field\n", SV_ARG(g_structs[sindex].name), SV_ARG(assign->field_name.val));
                }
                auto& field = g_structs[sindex].expr->fields[field_index];
                ValueType rhs_type = TYPE_NOP;
                auto rhs = translate_to_instruction(ops, regs, local_vars, assign->rhs, rhs_type);
                if (rhs_type == TYPE_VOID)
                    compiler_error(assign->rhs->tok, "Cannot assign a void value\n");
                if (rhs_type != field.type) {
                    if (!is_numeric_type(rhs_type) || !is_numeric_type(field.type))
                        compiler_error(assign->rhs->tok, "Cannot assign %s to %s field\n", value_type_to_str(rhs_type), value_type_to_str(field.type));
                    if (!cast_is_free(rhs_type, field.type)) {
                        auto& cast_reg = allocate_reg(regs);
                        cast_reg.type = field.type;
                        ops.push(Instruction{.type = OP_CAST, .location = assign->tok, .binop = {OP_CAST, rhs.index, rhs.index}, .reg_index = cast_reg.index});
                        rhs = cast_reg;
                    }
                }
                ops.push(Instruction{.type = OP_STORE_PTR, .location = assign->tok, .binop = {OP_STORE_PTR, base_reg, rhs.index}, .reg_index = rhs.index, .int_val = field.offset, .byte_size = (u8)field.size});
                return_type = TYPE_VOID;
                return {};
            }

            // A fresh top-level declaration (`gx := v`, not module-qualified)
            // owns its name in the *current* module: the unqualified visibility
            // scan would also see this module's own pass-1 registration (fine)
            // or other modules' same-named globals, which must not count as
            // redeclarations or become ambiguous here.
            Variable* is_already_exist = nullptr;
            if (assign->module_name.size > 0) {
                is_already_exist = find_global_variable(assign->tok.val, assign->module_name);
            } else if (assign->oper.type == Tok_ColonEquals && !assign->is_local) {
                is_already_exist = find_global_variable(assign->tok.val, g_current_module_name);
            } else {
                is_already_exist = get_variable(assign->tok, false, &local_vars);
            }
            bool var_is_local = assign->is_local;
            bool already_exist = false;
            if (is_already_exist) {
                var_is_local = is_already_exist->is_local;
                already_exist = true;
            }
            // A typed declaration (`x : T = v`) is a fresh variable, so the
            // `=`-on-undeclared check must not fire.
            check_assignment(assign->tok, assign->oper, already_exist, assign->declared_type == TYPE_NOP && var_is_local);

            auto rhs = translate_to_instruction(ops, regs, local_vars, assign->rhs, return_type);
            if (return_type == TYPE_VOID)
                compiler_error(assign->rhs->tok, "Cannot assign a void value\n");

            // Force the declared type: numeric values promote/shrink to it
            // (like an implicit cast), anything else must match exactly.
            if (assign->declared_type != TYPE_NOP && return_type != assign->declared_type) {
                if (!is_numeric_type(return_type) || !is_numeric_type(assign->declared_type))
                    compiler_error(assign->rhs->tok, "Cannot assign %s to %s variable\n", value_type_to_str(return_type), value_type_to_str(assign->declared_type));
                if (!cast_is_free(return_type, assign->declared_type)) {
                    // A compile-time constant keeps its value through the
                    // declared type: there is no runtime code to cast, and
                    // replacing the register would lose it (the capture below
                    // reads rhs.int_val).
                    auto& cast_reg = allocate_reg(regs);
                    cast_reg.type = assign->declared_type;
                    if (rhs.is_comp_time) {
                        cast_reg.is_comp_time = true;
                        cast_reg.int_val = rhs.int_val;
                        if (assign->declared_type == TYPE_BOOL)
                            cast_reg.bool_val = rhs.int_val != 0;
                    } else {
                        ops.push(Instruction{.type = OP_CAST, .location = assign->tok, .binop = {OP_CAST, rhs.index, rhs.index}, .reg_index = cast_reg.index});
                    }
                    rhs = cast_reg;
                }
                return_type = assign->declared_type;
            }

            // Arrays copy deeply: `b := a` (and any array reassignment) gets its
            // own block, so later element writes through one alias never leak
            // into the other. An array literal is already a fresh block, so it
            // needs no copy.
            if (return_type == TYPE_ARRAY && assign->rhs->type != Expr_ArrayLit) {
                ValueType elem = TYPE_NOP; s64 arr_len = -1;
                array_metadata(assign->rhs, local_vars, elem, arr_len);
                if (elem == TYPE_NOP || arr_len < 0)
                    compiler_error(assign->rhs->tok, "Cannot copy an array whose length is unknown\n");
                auto& dst_base = allocate_reg(regs);
                dst_base.type = TYPE_ARRAY;
                ops.push(Instruction{.type = OP_ALLOC, .location = assign->tok, .reg_index = dst_base.index, .int_val = (usize)(type_size(elem) * arr_len), .align = (u32)MIN(type_size(elem), 8u)});
                copy_array_into(ops, regs, assign->tok, rhs.index, dst_base.index, elem, arr_len);
                rhs = dst_base;
            }

            // A typed struct declaration (`f : Foo = ...`) must be initialized
            // with a value of that exact struct type. The type check above sees
            // TYPE_STRUCT on both sides, so the struct names (and their modules)
            // are compared here; without it, `f : Foo = Foo1 { ... }` would
            // silently compile.
            if (assign->declared_type == TYPE_STRUCT && assign->declared_struct_name.size > 0) {
                StrView rhs_struct = struct_name_of(assign->rhs, local_vars);
                StrView rhs_module = struct_module_of(assign->rhs, local_vars);
                if (rhs_struct.size > 0 && (rhs_struct != assign->declared_struct_name || rhs_module != assign->declared_struct_module)) {
                    compiler_error(assign->rhs->tok, "Cannot assign " SV_FORMAT " to " SV_FORMAT " variable\n", SV_ARG(rhs_struct), SV_ARG(assign->declared_struct_name));
                }
            }

            if (is_already_exist) {
                if (!is_valid_operation(OP_CAST, is_already_exist->type, return_type, return_type)) {
                    compiler_error(assign->rhs->tok, "Cannot assign %s to %s variable\n", value_type_to_str(return_type), value_type_to_str(is_already_exist->type));
                }
                // Reassigning a struct variable must keep the same struct type.
                if (is_already_exist->type == TYPE_STRUCT && return_type == TYPE_STRUCT
                    && is_already_exist->struct_name.size > 0) {
                    StrView rhs_struct = struct_name_of(assign->rhs, local_vars);
                    StrView rhs_module = struct_module_of(assign->rhs, local_vars);
                    if (rhs_struct.size > 0 && (rhs_struct != is_already_exist->struct_name || rhs_module != is_already_exist->struct_module)) {
                        compiler_error(assign->rhs->tok, "Cannot assign " SV_FORMAT " to " SV_FORMAT " variable\n", SV_ARG(rhs_struct), SV_ARG(is_already_exist->struct_name));
                    }
                }
            }

            // All variables share full-width stack slots (the whole language
            // moves QWORDs at a time), so allocate at STACK_REGISTER_SIZE
            // regardless of the variable's type to avoid overlapping slots.
            auto& res = allocate_reg(regs);
            res.type = return_type;
            Variable* var = nullptr;
            if (assign->module_name.size > 0) {
                var = find_global_variable(assign->tok.val, assign->module_name);
            } else if (assign->oper.type == Tok_ColonEquals && !var_is_local) {
                // A fresh top-level declaration: the variable belongs to the
                // current module only (see the already-exist lookup above).
                check_reserved_prefix(assign->tok, "Variable");
                var = find_global_variable(assign->tok.val, g_current_module_name);
                if (!var) {
                    g_vars.push(Variable{assign->tok.val, return_type, res.index, false});
                    var = &g_vars.last();
                    var->module_name = g_current_module_name;
                    var->global_offset = g_globals_size;
                    g_globals_size += STACK_REGISTER_SIZE;
                }
            } else {
                var = get_and_add_variable(assign->tok, var_is_local, return_type, res.index, &local_vars);
            }
            if (!var || !var->is_accesible) {
                compiler_error(assign->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(assign->tok.val));
            }
            var->type = return_type;
            // Track the string literal this variable holds so `.len` can fold
            // to a constant; -1 means the value is not statically known.
            if (var->type == TYPE_STR)
                var->str_literal_index = str_literal_index_of(assign->rhs, &local_vars);
            // Array variables record their element type and fixed length so
            // `len()` and indexed access resolve statically. The declared type
            // (`a : i64[5] = ...`) wins over the inferred metadata.
            if (var->type == TYPE_ARRAY) {
                if (assign->declared_type == TYPE_ARRAY && assign->declared_array_len >= 0) {
                    ValueType elem = TYPE_NOP; s64 arr_len = -1;
                    array_metadata(assign->rhs, local_vars, elem, arr_len);
                    if (arr_len >= 0 && arr_len != assign->declared_array_len)
                        compiler_error(assign->rhs->tok, "Array of %lld elements cannot be assigned to a %lld-element array\n", (long long)arr_len, (long long)assign->declared_array_len);
                    var->array_elem = assign->declared_array_elem;
                    var->array_len = assign->declared_array_len;
                } else {
                    ValueType elem = TYPE_NOP; s64 arr_len = -1;
                    array_metadata(assign->rhs, local_vars, elem, arr_len);
                    var->array_elem = elem;
                    var->array_len = arr_len;
                }
            }
            // Struct variables tag their declared/inferred struct name so
            // member access and struct-arg checks can resolve the layout. The
            // declared type (`foo : Foo = ...`) wins over the inferred name.
            if (var->type == TYPE_STRUCT) {
                if (assign->declared_type == TYPE_STRUCT && assign->declared_struct_name.size > 0) {
                    var->struct_name = assign->declared_struct_name;
                    var->struct_module = assign->declared_struct_module;
                } else if (var->struct_name.size == 0) {
                    var->struct_name = struct_name_of(assign->rhs, local_vars);
                    var->struct_module = struct_module_of(assign->rhs, local_vars);
                }
            }
            // Propagate pointer metadata so `p := &x` / `q := p` keep the
            // pointee type and depth needed by later dereferences. A declared
            // pointer type (`p : u8^ = ...`) wins over the inferred metadata.
            // A `null` reassignment must NOT clobber an existing variable's
            // metadata (the value is just cleared, type is unchanged).
            if (var->type == TYPE_PTR && assign->rhs->type != Expr_StructInit) {
                if (assign->declared_type == TYPE_PTR && assign->declared_ptr_depth > 0) {
                    var->pointee = assign->declared_pointee;
                    var->ptr_depth = assign->declared_ptr_depth;
                    var->pointee_struct_name = assign->declared_pointee_struct_name;
                    var->pointee_struct_module = assign->declared_pointee_struct_module;
                } else if (assign->rhs->tok.type != Tok_NullLit) {
                    ValueType pointee = TYPE_NOP;
                    u8 depth = 0;
                    StrView pointee_struct = "";
                    StrView pointee_struct_module = "";
                    pointer_metadata(assign->rhs, local_vars, pointee, depth, pointee_struct, pointee_struct_module);
                    var->pointee = pointee;
                    var->ptr_depth = depth;
                    var->pointee_struct_name = pointee_struct;
                    var->pointee_struct_module = pointee_struct_module;
                }
            }
            // A global's slot lives in __globals, so the store must go through
            // the per-function binding (its register carries the __globals
            // offset). Keep the canonical g_vars entry's metadata in sync so
            // other functions resolve the same struct/pointer type.
            if (!var->is_local) {
                for (auto& gv : g_vars) {
                    if (gv.name == var->name && (assign->module_name.size == 0 || gv.module_name == assign->module_name)) {
                        gv.struct_name = var->struct_name;
                        gv.struct_module = var->struct_module;
                        gv.pointee = var->pointee;
                        gv.ptr_depth = var->ptr_depth;
                        gv.pointee_struct_name = var->pointee_struct_name;
                        gv.pointee_struct_module = var->pointee_struct_module;
                        gv.str_literal_index = var->str_literal_index;
                        gv.array_elem = var->array_elem;
                        gv.array_len = var->array_len;
                        break;
                    }
                }
                var = bind_global_variable(assign->tok, &local_vars, regs, assign->module_name);
                if (!var || !var->is_accesible) {
                    compiler_error(assign->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(assign->tok.val));
                }
            }
            // ONLY a direct `x := #run { ... }` makes the variable itself a
            // compile-time constant: every read folds to the value and no slot
            // store is emitted; reassignment requires another `#run`. Plain
            // literals keep ordinary runtime semantics — folding them broke
            // loop counters (`for i := 0 .. n` never stored i, so reads of the
            // counter folded to its initial 0 forever).
            if (assign->rhs->type == Expr_Run || (var && var->is_comp_time)) {
                if (assign->rhs->type != Expr_Run)
                    compiler_error(assign->rhs->tok, "Cannot assign a runtime value to compile-time constant '" SV_FORMAT "'\n", SV_ARG(var->name));
                var->is_comp_time = true;
                var->comp_time_val = rhs.int_val;
                if (!var->is_local) {
                    for (auto& gv : g_vars) {
                        if (gv.name == var->name && (assign->module_name.size == 0 || gv.module_name == assign->module_name)) {
                            gv.is_comp_time = true;
                            gv.comp_time_val = rhs.int_val;
                            break;
                        }
                    }
                }
                return_type = TYPE_VOID;
                return res;
            }
            ops.push(Instruction{.type = OP_STORE, .data_type = TYPE_VARIABLE, .location = assign->tok, .target = {*var}, .reg_index = rhs.index});
            return_type = TYPE_VOID;
            return res;
        } break;

        case Expr_Call: {
            auto* call_expr = dynamic_cast<CallExpr*>(expr);
            ASSERT_NOT_NULL(call_expr);
            // Built-in compile-time reflection functions.
            if (call_expr->tok.val == "offset_of" || call_expr->tok.val == "align_of") {
                const bool is_offset = call_expr->tok.val == "offset_of";
                const usize want = is_offset ? 2 : 1;
                if (call_expr->args.count() != want) {
                    compiler_error(call_expr->tok, "Builtin '" SV_FORMAT "' expects exactly %d arguments\n", SV_ARG(call_expr->tok.val), (int)want);
                }
                // First argument is a bare struct name: `offset_of(Foo, "x")`.
                // It is not an expression to evaluate, so it is never
                // translated, only looked up.
                StrView struct_name = call_expr->args[0].expr->tok.val;
                auto sidx = find_visible_struct(struct_name, "");
                if (!g_structs.is_valid_index(sidx) || !g_structs[sidx].expr) {
                    compiler_error(call_expr->args[0].expr->tok, "Cannot find struct with name: '" SV_FORMAT "'\n", SV_ARG(struct_name));
                }
                auto* struct_def = g_structs[sidx].expr;
                return_type = TYPE_I64;
                if (!is_offset) {
                    return make_const(regs, TYPE_I64, (s64)struct_def->align);
                }
                Token field_tok = call_expr->args[1].expr->tok;
                if (field_tok.type != Tok_StrLit) {
                    compiler_error(field_tok, "Builtin 'offset_of' expects a field name string literal as its second argument\n");
                }
                for (auto& field : struct_def->fields) {
                    if (field.name == field_tok.val)
                        return make_const(regs, TYPE_I64, (s64)field.offset);
                }
                compiler_error(field_tok, "Struct '" SV_FORMAT "' has no field named '" SV_FORMAT "'\n", SV_ARG(struct_name), SV_ARG(field_tok.val));
            }
            if (call_expr->tok.val == "#type_id" || call_expr->tok.val == "#type_size"
                || call_expr->tok.val == "#type_of") {
                if (call_expr->args.count() != 1) {
                    compiler_error(call_expr->tok, "Builtin '" SV_FORMAT "' expects exactly 1 argument\n", SV_ARG(call_expr->tok.val));
                }
                ValueType arg_type = TYPE_NOP;
                translate_to_instruction(ops, regs, local_vars, call_expr->args[0].expr, arg_type);
                if (call_expr->tok.val == "#type_id") {
                    return_type = TYPE_I64;
                    return make_const(regs, TYPE_I64, (s64)arg_type);
                }
                if (call_expr->tok.val == "#type_size") {
                    return_type = TYPE_I64;
                    return make_const(regs, TYPE_I64, (s64)type_size(arg_type));
                }
                usize index = 0;
                if (arg_type == TYPE_PTR) {
                    ValueType pointee = TYPE_NOP;
                    u8 depth = 0;
                    StrView pointee_struct = "";
                    StrView pointee_struct_module = "";
                    pointer_metadata(call_expr->args[0].expr, local_vars, pointee, depth, pointee_struct, pointee_struct_module);
                    get_or_add_string(StrView(pointer_type_name(pointee, depth)), &index);
                } else {
                    get_or_add_string(StrView(value_type_to_str(arg_type)), &index);
                }
                return_type = TYPE_STR;
                auto& reg = allocate_reg(regs);
                reg.type = TYPE_STR;
                reg.str_val = index;
                ops.push(Instruction{.type = OP_PUSH_STR, .location = call_expr->tok, .reg_index = reg.index});
                return reg;
            }
            if (call_expr->tok.val == "len") {
                // Compile-time array length: `len(arr)`. No runtime code is
                // emitted; the length is the array's fixed element count.
                if (call_expr->args.count() != 1)
                    compiler_error(call_expr->tok, "Builtin 'len' expects exactly 1 argument\n");
                ValueType elem = TYPE_NOP; s64 arr_len = -1;
                array_metadata(call_expr->args[0].expr, local_vars, elem, arr_len);
                if (arr_len < 0)
                    compiler_error(call_expr->args[0].expr->tok, "Builtin 'len' expects an array argument\n");
                return_type = TYPE_I64;
                return make_const(regs, TYPE_I64, arr_len);
            }
            auto index = find_visible_function(call_expr->tok, call_expr->module_name);
            if (index != Array<DeclaredFunction>::INVALID_INDEX) {
                DeclaredFunction& callee = g_functions[index];
                // Ensure the callee's body is translated so its return type is
                // known even when the callee is declared after the call site.
                // Extern (FFI) declarations have no body to translate: their
                // signature is final, so they are never deferred here.
                if (callee.ops.count() == 0 && callee.expr && !callee.expr->is_extern) {
                    if (g_translating_functions.contains(callee.name))
                        // Recursive call: use the return type inferred so far
                        // from earlier `return` statements in the body.
                        callee.return_type = g_live_function_return_type;
                    else
                        translate_function_body(callee);
                }
                const u32 expected = callee.expr->args.count();
                const auto got = call_expr->args.count();
                if (expected != got) {
                    compiler_error(call_expr->tok, "Not enough arguments provided to a function '" SV_FORMAT"', expected %d, got %d\n", SV_ARG(call_expr->tok.val), expected, got);
                }
                for (usize i = 0; i < call_expr->args.count(); ++i) {
                    auto& arg = call_expr->args[i];
                    ValueType arg_type;
                    VirtualReg arg_reg = translate_to_instruction(ops, regs, local_vars, arg.expr, arg_type);
                    if (arg_type == TYPE_VOID)
                        compiler_error(arg.expr->tok, "Cannot pass a void value as an argument\n");
                    arg.reg_index = arg_reg.index;
                    arg.size = STACK_REGISTER_SIZE;
                    arg.type = arg_type;
                    // C FFI: a `str` holds the address of its (nul-terminated)
                    // character buffer, so it converts freely to a `char*`
                    // (any pointer param), like C's implicit const char*.
                    const bool str_to_ffi_ptr = callee.expr->is_extern
                        && arg_type == TYPE_STR
                        && callee.expr->arg_types[i].type == TYPE_PTR;
                    // C FFI: a struct value holds the address of its storage
                    // bytes, so it converts freely to any pointer param, like
                    // C's implicit `&x` when passing a struct by pointer.
                    // (`foo(f)` for an extern `fn foo(p : void^)`.)
                    const bool struct_to_ffi_ptr = callee.expr->is_extern
                        && arg_type == TYPE_STRUCT
                        && callee.expr->arg_types[i].type == TYPE_PTR;
                    if (i < callee.expr->arg_types.count()
                        && callee.expr->arg_types[i].expr
                        && !str_to_ffi_ptr
                        && !struct_to_ffi_ptr
                        && !types_compatible(arg_type, callee.expr->arg_types[i].type)) {
                        compiler_error(call_expr->tok, "Argument type mismatch for function '" SV_FORMAT "', expected " SV_FORMAT ", got " SV_FORMAT "\n", SV_ARG(call_expr->tok.val), SV_ARG(callee.expr->arg_types[i].expr->tok.val), SV_ARG(StrView(value_type_to_str(arg_type))));
                    }
                    // Struct arguments must match the param's struct identity
                    // (`foo : Foo` won't accept a Bar value).
                    if (arg_type == TYPE_STRUCT && i < callee.expr->arg_types.count()
                        && callee.expr->arg_types[i].type == TYPE_STRUCT
                        && callee.expr->arg_types[i].struct_name.size > 0) {
                        StrView arg_struct = struct_name_of(arg.expr, local_vars);
                        StrView arg_struct_module = struct_module_of(arg.expr, local_vars);
                        if (arg_struct.size > 0 && (arg_struct != callee.expr->arg_types[i].struct_name || arg_struct_module != callee.expr->arg_types[i].struct_module)) {
                            compiler_error(call_expr->tok, "Argument type mismatch for function '" SV_FORMAT "', expected " SV_FORMAT ", got " SV_FORMAT "\n", SV_ARG(call_expr->tok.val), SV_ARG(callee.expr->arg_types[i].struct_name), SV_ARG(arg_struct));
                        }
                    }
                    // Pointer args must also match the param's pointee type and
                    // indirection depth (`&x` of an i64 can't be passed to a
                    // `u8^` param, and `&x` of a single-level pointer can't be
                    // passed to a `bool^^` param). An opaque pointer (a
                    // function result) carries no pointee metadata, so its
                    // pointee is not enforced, only its depth.
                    if (arg_type == TYPE_PTR && i < callee.expr->arg_types.count()
                        && callee.expr->arg_types[i].type == TYPE_PTR) {
                        const auto& param = callee.expr->arg_types[i];
                        ValueType arg_pointee = TYPE_NOP;
                        u8 arg_depth = 0;
                        StrView arg_pointee_struct = "";
                        StrView arg_pointee_struct_module = "";
                        pointer_metadata(arg.expr, local_vars, arg_pointee, arg_depth, arg_pointee_struct, arg_pointee_struct_module);
                        if (arg_depth != param.ptr_depth) {
                            compiler_error(call_expr->tok, "Argument type mismatch for function '" SV_FORMAT "', expected a %d-level pointer, got a %d-level pointer\n", SV_ARG(call_expr->tok.val), param.ptr_depth, arg_depth);
                        }
                        if (param.pointee != TYPE_NOP && arg_pointee != TYPE_NOP
                            && param.pointee != arg_pointee
                            // `void^` converts to/from any pointer (C's void*).
                            && param.pointee != TYPE_VOID && arg_pointee != TYPE_VOID) {
                            compiler_error(call_expr->tok, "Argument type mismatch for function '" SV_FORMAT "', expected a pointer to " SV_FORMAT ", got a pointer to " SV_FORMAT "\n", SV_ARG(call_expr->tok.val), SV_ARG(StrView(value_type_to_str(param.pointee))), SV_ARG(StrView(value_type_to_str(arg_pointee))));
                        }
                        // Pointer-to-struct args must also match the param's
                        // pointee struct identity (`&f` of a Foo can't be passed
                        // to a `Bar^` param, even though both are structs).
                        if (param.pointee_struct_name.size > 0 && arg_pointee_struct.size > 0
                            && (param.pointee_struct_name != arg_pointee_struct || param.pointee_struct_module != arg_pointee_struct_module)) {
                            compiler_error(call_expr->tok, "Argument type mismatch for function '" SV_FORMAT "', expected a pointer to " SV_FORMAT ", got a pointer to " SV_FORMAT "\n", SV_ARG(call_expr->tok.val), SV_ARG(param.pointee_struct_name), SV_ARG(arg_pointee_struct));
                        }
                    }
                }
                return_type = callee.return_type;
                auto& reg = allocate_reg(regs);
                reg.type = return_type;
                // Struct returns: reserve the return slot in *this* function's
                // frame (the callee writes its result there) and pass its
                // address as a hidden argument after the visible ones. The call
                // result register doubles as the slot address, which the callee
                // returns in rax.
                if (return_type == TYPE_STRUCT && callee.return_struct_name.size > 0) {
                    reg.type = TYPE_PTR;
                    const usize ret_area = callee.return_area_size > 0
                        ? callee.return_area_size : struct_total_size(callee.return_struct_name, callee.return_struct_module);
                    ops.push(Instruction{.type = OP_ALLOC, .location = call_expr->tok, .reg_index = reg.index, .int_val = ret_area, .align = struct_align(callee.return_struct_name, callee.return_struct_module)});
                }
                ops.push(Instruction{.type = OP_CALL, .data_type = TYPE_CALL, .location = call_expr->tok, .call = {call_expr->tok, call_expr->args, false, callee.module_name}, .reg_index = reg.index});
                if (return_type == TYPE_STRUCT && callee.return_struct_name.size > 0)
                    ops.last().call.args.push(CallArg{nullptr, TYPE_PTR, reg.index, STACK_REGISTER_SIZE});
                return reg;
            } else {
                compiler_error(call_expr->tok, "Call to undeclared function '" SV_FORMAT"'\n", SV_ARG(call_expr->tok.val));
            }
            // return_type = g_function[index].type;
        } break;

        case Expr_If: {
            auto* if_expr = dynamic_cast<IfExpr*>(expr);
            ASSERT_NOT_NULL(if_expr);


            auto if_reg = translate_to_instruction(ops, regs, local_vars, if_expr->condition, return_type);
            if (return_type == TYPE_VOID)
                compiler_error(if_expr->condition->tok, "Cannot use a void value as an if condition\n");

            // A compile-time condition makes the branch statically known:
            // translate only the taken path, so the dead branch, its jumps and
            // any constant spills are never emitted.
            if (if_reg.is_comp_time) {
                if (if_reg.int_val)
                    translate_to_instruction(ops, regs, local_vars, if_expr->if_block, return_type);
                else
                    translate_to_instruction(ops, regs, local_vars, if_expr->else_block, return_type);
                return_type = TYPE_VOID;
                break;
            }

            auto body_label = allocate_label();
            auto else_label = allocate_label();
            auto end_label = allocate_label();

            ops.push(Instruction{.type = OP_JMP_IF, .data_type = TYPE_ADDRESS, .location=if_expr->condition->tok, .label = {body_label.label.ip}, .reg_index = if_reg.index});
            ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .label = {else_label.label.ip}});
            set_label(body_label, ops);
            ops.push(body_label);
            translate_to_instruction(ops, regs, local_vars, if_expr->if_block, return_type);
            ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .label = {end_label.label.ip}});

            set_label(else_label, ops);
            ops.push(else_label);
            translate_to_instruction(ops, regs, local_vars, if_expr->else_block, return_type);
            set_label(end_label, ops);
            ops.push(end_label);
            return_type = TYPE_VOID;
        } break;

        case Expr_For: {
            auto* for_expr = dynamic_cast<ForExpr*>(expr);
            ASSERT_NOT_NULL(for_expr);

            // `for <cond> { }`: a condition-driven loop. The condition is
            // translated at the top of each iteration; the block runs while it
            // holds. A compile-time `true` condition makes the JMP_IF a plain
            // unconditional jump, producing an infinite loop (`for true { }`).
            if (for_expr->is_condition) {
                auto cond_label = allocate_label();
                auto block_label = allocate_label();
                auto end_label = allocate_label();
                usize before_vars = local_vars.count();

                set_label(cond_label, ops);
                ops.push(cond_label);
                auto cond_reg = translate_to_instruction(ops, regs, local_vars, for_expr->left_cond, return_type);
                if (return_type == TYPE_VOID)
                    compiler_error(for_expr->left_cond->tok, "Cannot use a void value as a loop condition\n");
                ops.push(Instruction{.type = OP_JMP_IF, .data_type = TYPE_ADDRESS, .location = for_expr->left_cond->tok, .label = {block_label.label.ip}, .reg_index = cond_reg.index, .is_visited = true});
                ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .label = {end_label.label.ip}, .is_visited = true});

                set_label(block_label, ops);
                ops.push(block_label);
                ++g_for_depth;
                g_loop_stack.push(LoopContext{.break_label = end_label.label.ip, .continue_label = cond_label.label.ip});
                translate_to_instruction(ops, regs, local_vars, for_expr->block, return_type);
                g_loop_stack.pop();
                --g_for_depth;
                ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .label = {cond_label.label.ip}, .is_visited = true});

                set_label(end_label, ops);
                ops.push(end_label);
                for (; before_vars < local_vars.count(); ++before_vars)
                    local_vars[before_vars].is_accesible = false;
                return_type = TYPE_VOID;
                break;
            }

            auto cond_label = allocate_label();
            auto block_label = allocate_label();
            auto end_label = allocate_label();
            auto continue_label = allocate_label();

            s64 before_everything = ops.count();
            usize before_vars = local_vars.count();
            auto left_reg = translate_to_instruction(ops, regs, local_vars, for_expr->left_cond, return_type);

            // Hint the loop counter's register to a callee-saved register,
            // cycling through r12..r15 by nesting depth so nested counters
            // stay in separate registers (up to 4 deep).
            if (left_reg.index < regs.count() && g_for_depth >= 0)
                regs[left_reg.index].hint = PR_R12 + (g_for_depth % (PR_R15 - PR_R12 + 1));

            // Evaluate the loop bound ONCE, before the loop header. If it were
            // translated inside the loop, a side-effecting bound (e.g. a
            // function call) would run on every iteration and, when it
            // changes the value, never let the counter catch up (infinite
            // loop). Its register stays live across the whole body, so the
            // allocator keeps it in a callee-saved register past any call.
            auto right_reg = translate_to_instruction(ops, regs, local_vars, for_expr->right_cond, return_type);

            set_label(cond_label, ops);
            ops.push(cond_label);
            auto& res = allocate_reg(regs);

            auto compare_type = for_expr->is_inclusive ? OP_LESS_EQUALS : OP_LESS;
            ValueType cmp_result = TYPE_NOP;
            // The bound's logical type is the translated return type (a free
            // `as i64` cast of a bool keeps its register typed BOOL, but the
            // expression is an int). A raw bool bound is a compile error.
            if (!is_valid_operation(compare_type, left_reg.type, return_type, cmp_result))
                compiler_error(for_expr->right_cond->tok, "Invalid operands to loop bound operation (%s and %s)\n", value_type_to_str(left_reg.type), value_type_to_str(return_type));
            ops.push(Instruction{.type = compare_type, .binop = {compare_type, left_reg.index, right_reg.index}, .reg_index = res.index, .is_visited = true,});
            ops.push(Instruction{.type = OP_JMP_IF, .data_type = TYPE_ADDRESS, .location=for_expr->tok, .label = {block_label.label.ip}, .reg_index = res.index, .is_visited = true,});
            ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .label = {end_label.label.ip}, .is_visited = true,});

            auto before_block = ops.count();
            set_label(block_label, ops);
            ops.push(block_label);
            ++g_for_depth;
            // `continue` in a range loop must still advance the counter, so it
            // jumps to the increment (continue_label), not the loop condition.
            g_loop_stack.push(LoopContext{.break_label = end_label.label.ip, .continue_label = continue_label.label.ip});
            translate_to_instruction(ops, regs, local_vars, for_expr->block, return_type);
            g_loop_stack.pop();
            --g_for_depth;
            if (before_block != ops.count()) {
                set_label(continue_label, ops);
                ops.push(continue_label);
                ops.push(Instruction{.type = OP_INC, .binop = {OP_INC, left_reg.index}, .is_visited = true,});
                ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .label = {cond_label.label.ip}, .is_visited = true,});
                set_label(end_label, ops);
                ops.push(end_label);
            } else {
                // Reset all operations if block is empty
                ops.set_count(before_everything);
            }
            // The loop counter is scoped to the loop; drop it after the loop ends.
            for (; before_vars < local_vars.count(); ++before_vars)
                local_vars[before_vars].is_accesible = false;
            return_type = TYPE_VOID;
        } break;

        case Expr_StructInit: {
            auto* struct_expr = dynamic_cast<StructInitExpr*>(expr);
            ASSERT_NOT_NULL(struct_expr);
            auto struct_index = find_visible_struct(struct_expr->tok.val, struct_expr->module_name);
            if (!g_structs.is_valid_index(struct_index)) {
                compiler_error(struct_expr->tok, "Use of undeclared struct: '" SV_FORMAT "'\n", SV_ARG(struct_expr->tok.val));
            }
            auto* struct_def = g_structs[struct_index].expr;
            // Reserve total_size bytes and store every field at its byte offset
            // (each slot matches the field's C width). Local literals reserve
            // bytes in this function's frame struct area; a literal inside a
            // `return` value is materialized directly in the caller-reserved
            // return slot instead, so the returned address survives the function.
            usize base_index;
            if (g_return_slot_reg != (usize)-1) {
                // Align the region's base to the struct's C alignment so the
                // caller's slot address is a multiple of the struct's align.
                const usize a = struct_def->align ? struct_def->align : 1;
                g_return_area_off = (g_return_area_off + a - 1) / a * a;
                const usize my_off = g_return_area_off;
                g_return_area_off += struct_def->total_size;
                g_return_area_max = MAX(g_return_area_max, g_return_area_off);
                if (my_off == 0) {
                    base_index = g_return_slot_reg; // top-level return struct
                } else {
                    auto& off_const = make_const(regs, TYPE_PTR, (s64)my_off);
                    auto& base = allocate_reg(regs);
                    base.type = TYPE_PTR;
                    ops.push(Instruction{.type = OP_PLUS, .location = struct_expr->tok, .binop = {OP_PLUS, g_return_slot_reg, off_const.index}, .reg_index = base.index});
                    base_index = base.index;
                }
            } else {
                auto& alloc_reg = allocate_reg(regs);
                alloc_reg.type = TYPE_STRUCT;
                base_index = alloc_reg.index;
                ops.push(Instruction{.type = OP_ALLOC, .location = struct_expr->tok, .reg_index = alloc_reg.index, .int_val = struct_def->total_size, .align = struct_def->align});
            }
            if (struct_expr->zero_init) {
                // `Foo {0}`: every member gets its zero value.
                zero_init_members(ops, regs, struct_expr->tok, struct_def, base_index);
            } else {
                // Every unspecified member gets its zero value too: `Foo { x: 5 }`
                // zeroes y and z rather than leaving them as garbage.
                Array<bool> seen;
                seen.reserve(struct_def->fields.count());
                seen.set_count(struct_def->fields.count());
                seen.memzero();
                for (usize i = 0; i < struct_expr->field_names.count(); ++i) {
                    usize field_index = (usize)-1;
                    for (usize f = 0; f < struct_def->fields.count(); ++f) {
                        if (struct_def->fields[f].name == struct_expr->field_names[i]) { field_index = f; break; }
                    }
                    if (field_index == (usize)-1) {
                        compiler_error(struct_expr->tok, "Struct '" SV_FORMAT "' doesnt contain '" SV_FORMAT "' field\n", SV_ARG(struct_expr->tok.val), SV_ARG(struct_expr->field_names[i]));
                    }
                    seen[field_index] = true;
                    auto& field = struct_def->fields[field_index];
                    ValueType field_type = TYPE_NOP;
                    auto value = translate_to_instruction(ops, regs, local_vars, struct_expr->field_values[i], field_type);
                    if (field_type == TYPE_VOID)
                        compiler_error(struct_expr->field_values[i]->tok, "Cannot store a void value in a struct field\n");
                    if (field_type != field.type) {
                        if (!is_numeric_type(field_type) || !is_numeric_type(field.type))
                            compiler_error(struct_expr->field_values[i]->tok, "Cannot assign %s to %s field\n", value_type_to_str(field_type), value_type_to_str(field.type));
                        if (!cast_is_free(field_type, field.type)) {
                            auto& cast_reg = allocate_reg(regs);
                            cast_reg.type = field.type;
                            ops.push(Instruction{.type = OP_CAST, .location = struct_expr->tok, .binop = {OP_CAST, value.index, value.index}, .reg_index = cast_reg.index});
                            value = cast_reg;
                        }
                    }
                    ops.push(Instruction{.type = OP_STORE_PTR, .location = struct_expr->tok, .binop = {OP_STORE_PTR, base_index, value.index}, .reg_index = value.index, .int_val = field.offset, .byte_size = (u8)field.size});
                }
                for (usize f = 0; f < struct_def->fields.count(); ++f) {
                    if (!seen[f])
                        zero_init_field(ops, regs, struct_expr->tok, struct_def, f, base_index);
                }
            }
            return_type = TYPE_STRUCT;
            return regs[base_index];
        } break;

        case Expr_MemberCall: {
            auto* member_expr = dynamic_cast<MemberCallExpr*>(expr);
            ASSERT_NOT_NULL(member_expr);
            auto* var = bind_global_variable(member_expr->tok, &local_vars, regs);
            if (!var || !var->is_accesible) {
                compiler_error(member_expr->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(member_expr->tok.val));
            }
            // String `.len` / `.data`: compile-time string accessors. `.len` is
            // the literal's byte count (a constant), `.data` is the buffer's
            // address (the string value itself re-typed as a pointer). Neither
            // emits runtime code; a statically unknown value (e.g. a function
            // return) is an error for `.len` because no literal backs it.
            if (var->type == TYPE_STR) {
                if (member_expr->field.val == "len") {
                    if (var->str_literal_index < 0 || (usize)var->str_literal_index >= g_strings.count())
                        compiler_error(member_expr->tok, "Cannot take 'len' of a string whose value is not a literal\n");
                    auto& str = g_strings[var->str_literal_index];
                    return_type = TYPE_I64;
                    return make_const(regs, TYPE_I64, string_literal_byte_len(str.name));
                }
                if (member_expr->field.val == "data") {
                    return_type = TYPE_PTR;
                    return regs[var->reg_index];
                }
                compiler_error(member_expr->tok, "String type has no member '" SV_FORMAT "'\n", SV_ARG(member_expr->field.val));
            }
            // A struct value's register holds its heap base. A pointer to a
            // struct (`p : Foo^`) holds the address of that base, so fetch the
            // heap base through it before the member load.
            StrView owner_struct = var->struct_name;
            StrView owner_module = var->struct_module;
            usize base_reg = var->reg_index;
            if (var->type == TYPE_PTR && var->pointee == TYPE_STRUCT) {
                owner_struct = var->pointee_struct_name;
                owner_module = var->pointee_struct_module;
                auto& heap_base = allocate_reg(regs);
                heap_base.type = TYPE_STRUCT;
                ops.push(Instruction{.type = OP_LOAD_PTR, .location = member_expr->tok, .binop = {OP_LOAD_PTR, var->reg_index, 0}, .reg_index = heap_base.index});
                base_reg = heap_base.index;
            } else if (var->type != TYPE_STRUCT) {
                compiler_error(member_expr->tok, "Cannot access a member of a non-struct value (variable '" SV_FORMAT "')\n", SV_ARG(var->name));
            }
            auto index = find_visible_struct(owner_struct, owner_module);
            if (!g_structs.is_valid_index(index)) {
                compiler_error(member_expr->tok, "Cannot find Struct with name: '" SV_FORMAT "'\n", SV_ARG(owner_struct));
            }
            usize field_index = (usize)-1;
            for (usize f = 0; f < g_structs[index].expr->fields.count(); ++f) {
                if (g_structs[index].expr->fields[f].name == member_expr->field.val) { field_index = f; break; }
            }
            if (field_index == (usize)-1) {
                compiler_error(member_expr->tok, "Struct '" SV_FORMAT "' doesnt contain '" SV_FORMAT "' field\n", SV_ARG(g_structs[index].name), SV_ARG(member_expr->field.val));
            }
            auto& field = g_structs[index].expr->fields[field_index];
            auto& res = allocate_reg(regs);
            res.type = field.type;
            ops.push(Instruction{.type = OP_LOAD_PTR, .location = member_expr->tok, .binop = {OP_LOAD_PTR, base_reg, 0}, .reg_index = res.index, .int_val = field.offset, .byte_size = (u8)field.size});
            return_type = field.type;
            return res;
        } break;

        case Expr_Struct: {
            // probably doesnt need to exist
        } break;

        case Expr_ArrayLit: {
            auto* arr = dynamic_cast<ArrayLitExpr*>(expr);
            ASSERT_NOT_NULL(arr);
            if (arr->elements.count() == 0)
                compiler_error(arr->tok, "Empty array literals are not supported\n");
            // Translate every element first so the element type can be promoted
            // across all of them, then store each as that width.
            Array<usize> elem_regs;
            ValueType elem_type = TYPE_NOP;
            for (usize i = 0; i < arr->elements.count(); ++i) {
                ValueType t = TYPE_NOP;
                auto reg = translate_to_instruction(ops, regs, local_vars, arr->elements[i], t);
                if (t == TYPE_VOID)
                    compiler_error(arr->elements[i]->tok, "Cannot store a void value in an array\n");
                if (!is_numeric_type(t) && t != TYPE_BOOL)
                    compiler_error(arr->elements[i]->tok, "Invalid array element type (%s)\n", value_type_to_str(t));
                elem_regs.push(reg.index);
                elem_type = (elem_type == TYPE_NOP) ? t : promote_type(elem_type, t);
            }
            arr->elem_type = elem_type;
            const u32 elem_size = type_size(elem_type);
            const usize total = elem_size * arr->elements.count();
            auto& base = allocate_reg(regs);
            base.type = TYPE_ARRAY;
            ops.push(Instruction{.type = OP_ALLOC, .location = arr->tok, .reg_index = base.index, .int_val = total, .align = (u32)MIN(elem_size, 8u)});
            for (usize i = 0; i < arr->elements.count(); ++i) {
                auto& value = regs[elem_regs[i]];
                VirtualReg* store_src = &value;
                if (value.type != elem_type) {
                    if (!is_valid_operation(OP_CAST, value.type, elem_type, return_type))
                        compiler_error(arr->elements[i]->tok, "Cannot store %s as %s in an array\n", value_type_to_str(value.type), value_type_to_str(elem_type));
                    if (!cast_is_free(value.type, elem_type)) {
                        auto& cast_reg = allocate_reg(regs);
                        cast_reg.type = elem_type;
                        ops.push(Instruction{.type = OP_CAST, .location = arr->tok, .binop = {OP_CAST, value.index, value.index}, .reg_index = cast_reg.index});
                        store_src = &cast_reg;
                    }
                }
                ops.push(Instruction{.type = OP_STORE_PTR, .location = arr->tok, .binop = {OP_STORE_PTR, base.index, store_src->index}, .reg_index = store_src->index, .int_val = i * elem_size, .byte_size = (u8)elem_size});
            }
            return_type = TYPE_ARRAY;
            return base;
        } break;

        case Expr_Index: {
            auto* index_expr = dynamic_cast<IndexExpr*>(expr);
            ASSERT_NOT_NULL(index_expr);
            ValueType base_type = TYPE_NOP;
            auto base = translate_to_instruction(ops, regs, local_vars, index_expr->base, base_type);
            if (base_type != TYPE_ARRAY)
                compiler_error(index_expr->base->tok, "Cannot index a non-array value\n");
            ValueType idx_type = TYPE_NOP;
            auto idx = translate_to_instruction(ops, regs, local_vars, index_expr->index, idx_type);
            if (idx_type == TYPE_VOID || (!is_numeric_type(idx_type) && idx_type != TYPE_BOOL))
                compiler_error(index_expr->index->tok, "Array index must be a numeric value\n");
            ValueType elem = TYPE_NOP; s64 len = -1;
            array_metadata(index_expr->base, local_vars, elem, len);
            if (elem == TYPE_NOP)
                compiler_error(index_expr->base->tok, "Cannot determine the element type of this array\n");
            const u32 elem_size = type_size(elem);
            if (idx.is_comp_time) {
                // Fold a compile-time index into the byte displacement.
                auto& res = allocate_reg(regs);
                res.type = elem;
                ops.push(Instruction{.type = OP_LOAD_PTR, .location = index_expr->tok, .binop = {OP_LOAD_PTR, base.index, 0}, .reg_index = res.index, .int_val = (usize)(idx.int_val * (s64)elem_size), .byte_size = (u8)elem_size});
                return_type = elem;
                return res;
            }
            // Runtime index: compute base + i * elem_size, then load.
            auto& addr = allocate_reg(regs);
            addr.type = TYPE_PTR;
            if (elem_size == 1) {
                ops.push(Instruction{.type = OP_PLUS, .location = index_expr->tok, .binop = {OP_PLUS, base.index, idx.index}, .reg_index = addr.index});
            } else {
                auto& size_const = make_const(regs, TYPE_I64, (s64)elem_size);
                auto& scaled = allocate_reg(regs);
                scaled.type = TYPE_I64;
                ops.push(Instruction{.type = OP_MULT, .location = index_expr->tok, .binop = {OP_MULT, idx.index, size_const.index}, .reg_index = scaled.index});
                ops.push(Instruction{.type = OP_PLUS, .location = index_expr->tok, .binop = {OP_PLUS, base.index, scaled.index}, .reg_index = addr.index});
            }
            auto& res = allocate_reg(regs);
            res.type = elem;
            ops.push(Instruction{.type = OP_LOAD_PTR, .location = index_expr->tok, .binop = {OP_LOAD_PTR, addr.index, 0}, .reg_index = res.index, .byte_size = (u8)elem_size});
            return_type = elem;
            return res;
        } break;

        case Expr_IndexAssign: {
            auto* assign = dynamic_cast<IndexAssignExpr*>(expr);
            ASSERT_NOT_NULL(assign);
            ValueType base_type = TYPE_NOP;
            auto base = translate_to_instruction(ops, regs, local_vars, assign->base, base_type);
            if (base_type != TYPE_ARRAY)
                compiler_error(assign->base->tok, "Cannot index a non-array value\n");
            ValueType idx_type = TYPE_NOP;
            auto idx = translate_to_instruction(ops, regs, local_vars, assign->index, idx_type);
            if (idx_type == TYPE_VOID || (!is_numeric_type(idx_type) && idx_type != TYPE_BOOL))
                compiler_error(assign->index->tok, "Array index must be a numeric value\n");
            ValueType elem = TYPE_NOP; s64 len = -1;
            array_metadata(assign->base, local_vars, elem, len);
            if (elem == TYPE_NOP)
                compiler_error(assign->base->tok, "Cannot determine the element type of this array\n");
            const u32 elem_size = type_size(elem);
            ValueType rhs_type = TYPE_NOP;
            auto rhs = translate_to_instruction(ops, regs, local_vars, assign->rhs, rhs_type);
            if (rhs_type == TYPE_VOID)
                compiler_error(assign->rhs->tok, "Cannot assign a void value\n");
            VirtualReg* store_src = &rhs;
            if (rhs_type != elem) {
                if (!is_valid_operation(OP_CAST, rhs_type, elem, return_type))
                    compiler_error(assign->rhs->tok, "Cannot assign %s to a %s array element\n", value_type_to_str(rhs_type), value_type_to_str(elem));
                if (!cast_is_free(rhs_type, elem)) {
                    auto& cast_reg = allocate_reg(regs);
                    cast_reg.type = elem;
                    ops.push(Instruction{.type = OP_CAST, .location = assign->tok, .binop = {OP_CAST, rhs.index, rhs.index}, .reg_index = cast_reg.index});
                    store_src = &cast_reg;
                }
            }
            if (idx.is_comp_time) {
                ops.push(Instruction{.type = OP_STORE_PTR, .location = assign->tok, .binop = {OP_STORE_PTR, base.index, store_src->index}, .reg_index = store_src->index, .int_val = (usize)(idx.int_val * (s64)elem_size), .byte_size = (u8)elem_size});
            } else {
                auto& addr = allocate_reg(regs);
                addr.type = TYPE_PTR;
                if (elem_size == 1) {
                    ops.push(Instruction{.type = OP_PLUS, .location = assign->tok, .binop = {OP_PLUS, base.index, idx.index}, .reg_index = addr.index});
                } else {
                    auto& size_const = make_const(regs, TYPE_I64, (s64)elem_size);
                    auto& scaled = allocate_reg(regs);
                    scaled.type = TYPE_I64;
                    ops.push(Instruction{.type = OP_MULT, .location = assign->tok, .binop = {OP_MULT, idx.index, size_const.index}, .reg_index = scaled.index});
                    ops.push(Instruction{.type = OP_PLUS, .location = assign->tok, .binop = {OP_PLUS, base.index, scaled.index}, .reg_index = addr.index});
                }
                ops.push(Instruction{.type = OP_STORE_PTR, .location = assign->tok, .binop = {OP_STORE_PTR, addr.index, store_src->index}, .reg_index = store_src->index, .byte_size = (u8)elem_size});
            }
            return_type = TYPE_VOID;
            return {};
        } break;

        case Expr_Module:
        case Expr_ImportModule: {
            // Declarative; handled during parsing, nothing to translate.
        } break;

        default: {
            TODO(expr_type_to_str(expr->type));
            compiler_error(expr->tok, "Unknown type of expression %s\n", expr_type_to_str(expr->type));
        }
    }
    return {};
}

void print_help(const char* exe) {
    log("USAGE: %s [options] file...\n", exe);
    log("Options:\n");
    log("-h"); log("\t\tPrint this message\n");
    log("-o"); log("\t\tSet output file(Default is: 'example')\n");
    log("--platform=<name>"); log("\tCodegen target: Windows/Linux/MacOS/BSD/Android (default: host system)\n");
}


