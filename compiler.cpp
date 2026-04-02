#define EZBUILD_IMPLEMENTATION
#include "ezbuild.hpp"

using namespace Sl;

enum TokenType {
    Tok_Ident,
    Tok_IntLit,
    Tok_StrLit,
    Tok_BoolLit,
    Tok_Cast,
    Tok_Type,
    Tok_Keyword,
    Tok_Return,
    Tok_Function,
    Tok_Struct,
    Tok_If,
    Tok_Else,
    Tok_For,
    Tok_Oper,
    Tok_LParen,
    Tok_RParen,
    Tok_LArrow,
    Tok_RArrow,
    Tok_Equals,
    Tok_DEquals,
    Tok_DDot,
    Tok_DDotInclusive,
    Tok_DColon,
    Tok_SemiColon,
    Tok_Comma,
    Tok_Plus,
    Tok_Minus,
    Tok_Mult,
    Tok_Div,
    Tok_Mod,
    Tok_Dot,
    Tok_LBracket,
    Tok_RBracket,
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
    Expr_Function,
    Expr_For,
    Expr_Block,
    Expr_Call,
    Expr_Struct,
    Expr_StructInit,
    Expr_MemberCall,
    Expr_Return,
    Expr_If,
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
    TYPE_PTR,
    TYPE_STR,
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
    OP_LESS,
    OP_CAST,
    OP_GREATER,
    OP_GREATER_EQUALS,
    OP_JMP,
    OP_JMP_IF,
    OP_STORE,
    OP_LOAD,
    OP_CALL,
    OP_RET,
};

struct Token;
struct Lexer;
struct DeclaredString;
struct Register;
struct Expression;
Token eof_token();
void compiler_error(Token location, const char* const format, ...);

StrView out_path = SV_LIT("output");
StrView src_path = SV_LIT("");
const char* src_content = "";

struct Token {
    enum TokenType type = Tok_Eof;
    StrView val = "Eof";
};

struct Field {
    StrView name = "Eof";
    u32 size = 8;
};

struct Keyword {
    StrView val = "Eof";
};

struct IntVal
{
    s64 val;
};
struct PtrVal
{
    void* val;
};
struct StrVal
{
    s64 index;
};
struct BoolVal
{
    bool val;
};
struct Address
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
};

struct Operation
{
    InstructionType type;
    usize lhs_index;
    usize rhs_index;
};
struct Argument
{
    Expression* expr;
    usize reg_index;
    u32 size;
};
struct FunctionCall
{
    Token name;
    Array<Argument> args;
    bool is_void = false;
};
struct Assignment
{
    Variable var;
};
struct Return
{
    Variable var;
    usize size_of_regs;
};
struct Instruction
{
    enum InstructionType type;
    InstructionDataType data_type;
    Token location;
    union {
        struct Address addr;
        struct Assignment assign;
        struct Return ret;
        struct FunctionCall call;
        struct Operation op;
    };
    usize reg_index;
    bool is_visited;
};

struct StackValue
{
    enum ValueType type;
    union {
        s64 int_val;
        void* ptr_val;
        const char* str_val;
        bool bool_val;
    };
};

struct Register
{
    usize index;
    u32 offset;
    u32 size;
    bool is_arg;
    bool is_comp_time;
    bool is_visited;
    ValueType type;
    union {
        s64   int_val;
        void* ptr_val;
        s64   str_val;
        bool  bool_val;
    };
};

const char* value_type_to_str(ValueType type) {
    switch(type) {
        case TYPE_NOP:   return "nop";
        case TYPE_VOID:  return "void";
        case TYPE_I8:    return "i8";
        case TYPE_I16:   return "i16";
        case TYPE_I32:   return "i32";
        case TYPE_I64:   return "i64";
        case TYPE_PTR:   return "Ptr";
        case TYPE_STR:   return "string";
        case TYPE_BOOL:  return "bool";
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
        case OP_LESS: return "LESS";
        case OP_CAST: return "CAST";
        case OP_GREATER: return "GREATER";
        case OP_GREATER_EQUALS: return "GREATER_EQUALS";
        case OP_JMP: return "JMP";
        case OP_JMP_IF: return "JMP_IF";
        case OP_LABEL: return "LABEL";
        case OP_STORE: return "STORE";
        case OP_LOAD: return "LOAD";
        case OP_CALL: return "CALL";
        case OP_RET: return "RET";

        default: UNREACHABLE("inst_type_to_str");
    }
    return "";
}

bool is_numeric_type(ValueType type)
{
    return type >= TYPE_I8 && type <= TYPE_I64;
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
        case Tok_LArrow: return OP_GREATER;
        case Tok_RArrow: return OP_LESS;
        case Tok_Cast: return OP_CAST;
        default: return OP_NOP;
    }
}

bool is_valid_operation(InstructionType operation_type, ValueType lhs, ValueType rhs, ValueType& result_type);


struct StructExpr;

struct InitilizedStruct {
    char* data_ptr;
    StructExpr* expr_ptr;
};

const char* tok_type_to_str(enum TokenType type) {
    switch(type) {
        case Tok_Ident: return "Ident";
        case Tok_IntLit: return "IntLit";
        case Tok_StrLit: return "StrLit";
        case Tok_BoolLit: return "BoolLit";
        case Tok_Keyword: return "Keyword";
        case Tok_Return: return "Return";
        case Tok_Function: return "Function";
        case Tok_Struct: return "Struct";
        case Tok_Cast: return "Cast";
        case Tok_Type: return "Type";
        case Tok_If: return "If";
        case Tok_Else: return "Else";
        case Tok_For: return "For";
        case Tok_Oper: return "Oper";
        case Tok_LParen: return "LParen";
        case Tok_RParen: return "RParen";
        case Tok_LArrow: return "LArrow";
        case Tok_RArrow: return "RArrow";
        case Tok_Equals: return "Equals";
        case Tok_DEquals: return "DEquals";
        case Tok_SemiColon: return "SemiColon";
        case Tok_DColon: return "DColon";
        case Tok_DDot: return "DDot";
        case Tok_DDotInclusive: return "DDotIncl";
        case Tok_Comma: return "Comma";
        case Tok_Dot: return "Dot";
        case Tok_LBracket: return "LBracket";
        case Tok_RBracket: return "RBracket";
        case Tok_Plus: return "Plus";
        case Tok_Minus: return "Minus";
        case Tok_Mult: return "Mult";
        case Tok_Div: return "Div";
        case Tok_Mod: return "Mod";

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
        case Expr_Block: return "Block";
        case Expr_Call: return "Call";
        case Expr_For: return "For";
        case Expr_Return: return "Return";
        case Expr_Struct: return "Struct";
        case Expr_StructInit: return "StructInit";
        case Expr_MemberCall: return "MemberCall";
        case Expr_If: return "If";

        default: UNREACHABLE("expr_type_to_str");
    }
}

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
};

struct PrintExpr : Expression
{
    Expression* rhs = nullptr;
};

struct AssignmentExpr : Expression
{
    Token field_name = {Tok_StrLit, ""};
    Token oper = eof_token();
    bool is_local = false;
    Expression* rhs = nullptr;
};

struct BlockExpr : Expression
{
    Array<Expression*> exprs;
};

struct FunctionExpr : Expression
{
    Array<Expression*> args;
    BlockExpr* block = nullptr;
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
};

struct StructExpr : Expression
{
    Array<Field> fields;
    usize total_size = 0;
};

struct StructInitExpr : Expression
{
};

struct MemberCallExpr : Expression
{
    Token field = eof_token();
};

struct ReturnExpr : Expression
{
    Expression* rhs = nullptr;
};

struct CallExpr : Expression
{
    Array<Argument> args;
};

CallExpr* new_call_expr(Token tok)
{
    auto* expr = new CallExpr();
    expr->tok = tok;
    expr->type = Expr_Call;
    return expr;
}

StructExpr* new_struct_expr(Token name)
{
    auto* expr = new StructExpr();
    expr->type = Expr_Struct;
    expr->tok = name;
    return expr;
}

StructInitExpr* new_struct_init_expr(Token tok)
{
    auto* expr = new StructInitExpr();
    expr->type = Expr_StructInit;
    expr->tok = tok;
    return expr;
}

MemberCallExpr* new_member_call(Token struct_tok, Token field)
{
    auto* expr = new MemberCallExpr();
    expr->type = Expr_MemberCall;
    expr->tok = struct_tok;
    expr->field = field;
    return expr;
}

BinaryExpr* new_binary_expr(Token tok, Expression* lhs = nullptr, Expression* rhs = nullptr)
{
    auto* expr = new BinaryExpr();
    expr->type = Expr_Binary;
    expr->tok = tok;
    expr->lhs = lhs;
    expr->rhs = rhs;
    return expr;
}

PrintExpr* new_print_expr(Token tok, Expression* rhs = nullptr)
{
    auto* expr = new PrintExpr();
    expr->type = Expr_Print;
    expr->tok = tok;
    expr->rhs = rhs;
    return expr;
}

AssignmentExpr* new_assignment_expr(Token tok, Expression* rhs, bool is_local, Token oper, Token field_name = eof_token())
{
    auto* expr = new AssignmentExpr();
    expr->type = Expr_Assignment;
    expr->tok = tok;
    expr->oper = oper;
    expr->field_name = field_name;
    expr->rhs = rhs;
    expr->is_local = is_local;
    return expr;
}

ReturnExpr* new_return_expr(Token tok, Expression* rhs)
{
    auto* expr = new ReturnExpr();
    expr->type = Expr_Return;
    expr->tok = tok;
    expr->rhs = rhs;
    return expr;
}

BlockExpr* new_block_expr(Token tok)
{
    auto* expr = new BlockExpr();
    expr->type = Expr_Block;
    expr->tok = tok;
    return expr;
}

FunctionExpr* new_function_expr(Token tok, Array<Expression*>& args, BlockExpr* block)
{
    if (tok.val == "__entry") {
        compiler_error(tok, "__entry function is reserved and cannot be defined, noob\n");
    }
    auto* expr = new FunctionExpr();
    expr->type = Expr_Function;
    expr->tok = tok;
    expr->args.set_data(args.data());
    expr->args.set_capacity(args.capacity());
    expr->args.set_count(args.count());
    expr->block = block;
    return expr;
}

ForExpr* new_for_expr(Token tok, Expression* left_cond, Expression* right_cond, BlockExpr* block, bool is_inclusive)
{
    auto* expr = new ForExpr();
    expr->type = Expr_For;
    expr->tok = tok;
    expr->left_cond = left_cond;
    expr->right_cond = right_cond;
    expr->block = block;
    expr->is_inclusive = is_inclusive;
    return expr;
}

IfExpr* new_if_expr(Token tok, Expression* condition, BlockExpr* if_block, BlockExpr* else_block)
{
    auto* expr = new IfExpr();
    expr->type = Expr_If;
    expr->tok = tok;
    expr->condition = condition;
    expr->if_block = if_block;
    expr->else_block = else_block;
    return expr;
}

Token eof_token() { return {Tok_Eof, {"Eof", 3}}; }

Expression* parse_primary(Lexer& lexer);
void print_expr(Expression* expr, int parent_prec);
Expression* parse_expression(Lexer& lexer, Expression* lhs = nullptr, int min_prec = 0);
Expression* parse_ident(Lexer& lexer);
Expression* parse_keyword(Lexer& lexer);
void add_function_or_report_if_exit(FunctionExpr* expr);
void add_struct_or_report_if_exit(StructExpr* expr);
void add_string(BinaryExpr* expr);
DeclaredString* get_string(StrView name, usize* index = nullptr);
// Variable* find_variable(Token variable, Array<Variable>& vars, Array<Variable>& outer_vars);

void print_token(Token val) {
    log("Token: '" SV_FORMAT "' (%s)\n", SV_ARG(val.val), tok_type_to_str(val.type));
}

Keyword keyword_array[] = {
    {"true"},
    {"false"},
    {"print"},
    {"i8"},
    {"i16"},
    {"i32"},
    {"i64"},
    {"bool"},
    {"string"},
};

struct Operator {
    StrView val;
    enum TokenType type;
    int precedence = -1;
    bool contains(char op) const {
        return val.data[0] == op;
    }
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
    {"as", Tok_Cast,    1},
    {"<",  Tok_LArrow,  1},
    {">",  Tok_RArrow,  1},
    {"==", Tok_DEquals, 1},
    {"+",  Tok_Plus,    1},
    {"-",  Tok_Minus,   1},
    {"*",  Tok_Mult,    2},
    {"/",  Tok_Div,     2},
    {"%",  Tok_Mod,     2},
};

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

void add_field_or_report_if_exist(StructExpr* expr, Token tok_field)
{
    ASSERT_NOT_NULL(expr);
    for (auto& field : expr->fields) {
        if (field.name == tok_field.val) {
            compiler_error(tok_field, "Duplicate member '" SV_FORMAT "'\n", SV_ARG(tok_field.val));
        }
    }
    expr->fields.push(Field{tok_field.val});
}

struct Lexer
{
public:
    LocalArray<Token> _tokens;
    StrBuilder _source;
    Lexer(StrView source) {
        read_entire_file(source, _source);
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
                    if (ch == '"' || ch == '\n') {
                        skip_char();
                        break;
                    }
                    if (ch == 0) {
                        auto tok = Token{Tok_StrLit, source_view.sub_view(mark, get_index())};
                        compiler_error(tok, "Unclosed string litteral '" SV_FORMAT "' \n", SV_ARG(tok.val));
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
                else if (ident_view == "fn")
                    type = Tok_Function;
                else if (ident_view == "if")
                    type = Tok_If;
                else if (ident_view == "else")
                    type = Tok_Else;
                else if (ident_view == "for")
                    type = Tok_For;
                else if (ident_view == "struct")
                    type = Tok_Struct;
                else if (ident_view == "as")
                    type = Tok_Cast;
                _tokens.push(Token{type, ident_view});
            } else if (is_digit(ch)) {
                while (is_digit(ch)) {
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
                _tokens.push(Token{Tok_IntLit, source_view.sub_view(mark, get_index())});
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
                    _tokens.push(Token{Tok_Equals, source_view.sub_view(mark, end_mark)});
                    continue;
                }
                if (ch == '=' && next == '=') {
                    skip_char();
                    end_mark = get_index();
                    _tokens.push(Token{Tok_DEquals, source_view.sub_view(mark, end_mark)});
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
            } else if (ch == '\n' || ch == '\r' || ch == ' ') {
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

void check_assignemnt(Token variable, Token oper, bool is_already_exist, bool check_assignment = true)
{
    if (is_already_exist && oper.val == ":=")
        compiler_error(variable, "Variable '" SV_FORMAT "' already created, if you want to reassign it: use operator '='\n", SV_ARG(variable.val));
    if (check_assignment && !is_already_exist && oper.val == "=")
        compiler_error(variable, "Assignment to undeclared variable '" SV_FORMAT "', if you want to create variable: use operator ':='\n", SV_ARG(variable.val));
}

void compiler_error(Token location, const char* const format, ...)
{
    va_list args;
    va_start(args, format);
        ptrdiff_t loc = location.val.data - src_content;
        StrView view(src_content, loc+1);
        auto line = 0;
        auto offset = 0;
        while(view.size > 0) {
            ++line;
            auto last_line = view.chop_left_by_delimeter("\n");
            if (view.size == 0) {
                offset = last_line.size;
                break;
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

struct Instruction;
struct DeclaredFunction
{
    StrView name = "wtf";
    FunctionExpr* expr = nullptr;
    Array<Instruction> ops{};
    Array<Register> regs{};
    u32 vars_size = 0;
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

Array<DeclaredFunction> g_functions;
Array<DeclaredStruct> g_structs;
Array<DeclaredString> g_strings;
Array<Variable> g_vars;
Array<Label> g_labels;

Instruction allocate_label()
{
    g_labels.push(Label());
    return Instruction {
        .type = OP_LABEL,
        .addr = {(s64)g_labels.count()-1},
        .is_visited = true,
    };
}

void set_label(Instruction label, Array<Instruction>& ops)
{
    ASSERT_TRUE(label.type == OP_LABEL);
    g_labels[label.addr.ip].ip = ops.count() - 1;
}

Variable* get_variable(Token variable, bool is_only_local, Array<Variable>* local_vars = nullptr)
{
    if (local_vars) {
        for (auto& var : *local_vars) {
            if (var.name == variable.val) {
                return &var;
            }
        }
    }
    if (!is_only_local) {
        for (auto& var : g_vars) {
            if (var.name == variable.val) {
                return &var;
            }
        }
    }
    return nullptr;
}

Variable* get_and_add_variable(Token variable, bool is_local, ValueType type, usize reg_index, Array<Variable>* local_vars = nullptr)
{
    if (local_vars) {
        for (auto& var : *local_vars) {
            if (var.name == variable.val) {
                return &var;
            }
        }
    }
    if (!is_local) {
        for (auto& var : g_vars) {
            if (var.name == variable.val) {
                return &var;
            }
        }
    }
    if (is_local) {
        ASSERT_NOT_NULL(local_vars);
        local_vars->push(Variable{variable.val, type, reg_index, true});
        return &local_vars->last();
    } else {
        g_vars.push(Variable{variable.val, type, reg_index, false});
        return &g_vars.last();
    }
}

Expression* parse_binary_expr(Lexer& lexer) {
    Token tok = lexer.peak();
    if (tok.type == Tok_RParen) {
        compiler_error(tok, "Unmatched ')'\n");
    }
    if (tok.type == Tok_LParen) {
        lexer.skip();
        Expression* inner = parse_expression(lexer);
        Token close = lexer.next();
        if (close.val != ")") {
            compiler_error(close, "Expected ')', got '" SV_FORMAT "'\n", SV_ARG(close.val));
        }
        return inner;
    }
    if (tok.type == Tok_Ident) {
        return parse_ident(lexer);
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
        compiler_error(tok, "Expected { at the start of the block, but got: '" SV_FORMAT "'\n", SV_ARG(tok.val));
    }
    compiler_error(tok, "Expected start of the block, but got: '" SV_FORMAT "'\n", SV_ARG(tok.val));
    exit(1);
}
Expression* parse_keyword(Lexer& lexer) {
    Token tok = lexer.next();
    if (tok.type == Tok_Return) {
        return new_return_expr(tok, parse_expression(lexer));
    }

    if (tok.type == Tok_If) {
        auto* cond = parse_expression(lexer);
        auto next = lexer.peak();
        if (next.type == Tok_LBracket) {
            BlockExpr* if_block = parse_block(lexer);
            BlockExpr* else_block = nullptr;
            ASSERT_NOT_NULL(if_block);
            next = lexer.peak();
            if (next.type == Tok_Else) {
                lexer.next();
                else_block = parse_block(lexer);
                ASSERT_NOT_NULL(if_block);
            }
            return new_if_expr(cond->tok, cond, if_block, else_block);
        } else {
            compiler_error(next, "Expected block expression '{' after if condition\n");
        }
    }

    if (tok.type == Tok_For) {
        auto* left_cond = parse_expression(lexer);
        ASSERT_NOT_NULL(left_cond);
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
                compiler_error(bracket, "Expected block expression '{' after for expression\n");
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
    if (tok.val == "i8") {
        return new_binary_expr(Token{Tok_Type, tok.val});
    }
    if (tok.val == "i16") {
        return new_binary_expr(Token{Tok_Type, tok.val});
    }
    if (tok.val == "i32") {
        return new_binary_expr(Token{Tok_Type, tok.val});
    }
    if (tok.val == "i64") {
        return new_binary_expr(Token{Tok_Type, tok.val});
    }
    if (tok.val == "bool") {
        return new_binary_expr(Token{Tok_Type, tok.val});
    }
    if (tok.val == "string") {
        return new_binary_expr(Token{Tok_Type, tok.val});
    }
    compiler_error(tok, "Unknown keyword: '" SV_FORMAT "'\n", SV_ARG(tok.val));
    exit(1);
}

AssignmentExpr* parse_assignment(Token variable, bool is_local, Lexer& lexer, Token field_name = eof_token())
{
    auto tok = lexer.next();
    if (tok.type == Tok_Equals) {
        auto* inner = parse_expression(lexer);
        skip_semicolon_if_exist(lexer);

        return new_assignment_expr(variable, inner, is_local, tok, field_name);
    }
    compiler_error(tok, "Expected assignment expression, but got '" SV_FORMAT "'\n", SV_ARG(tok.val));
    return nullptr;
}

Expression* parse_ident(Lexer& lexer) {
    Token tok = lexer.next();
    if (tok.type != Tok_Ident) {
        compiler_error(tok, "Expected Identifier, but got '" SV_FORMAT "'\n", SV_ARG(tok.val));
    }

    auto next = lexer.peak();
    if (next.type == Tok_Equals) {
        auto* is_already_exist = get_variable(tok, false);
        check_assignemnt(tok, next, is_already_exist != nullptr, false);
        bool is_local = true;
        if (is_already_exist)
            is_local = is_already_exist->is_local;
        return parse_assignment(tok, is_local, lexer);
    }
    if (next.type == Tok_LParen) {
        lexer.next();
        auto* call = new_call_expr(tok);
        auto close = lexer.peak();
        if (close.type == Tok_RParen) {
            lexer.next();
            return call;
        }
        Expression* inner = parse_expression(lexer);
        call->args.push(Argument{inner});
        close = lexer.next();
        while (close.type == Tok_Comma) {
            inner = parse_expression(lexer);
            if (!inner) break;
            call->args.push(Argument{inner});
            close = lexer.next();
        }
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
    if (next.type == Tok_Dot) {
        lexer.skip();
        auto field = lexer.next();
        if (field.type != Tok_Ident) {
           compiler_error(field, "Expected member field, but got: '" SV_FORMAT "'\n", SV_ARG(field.val));
        }
        if (lexer.peak().type == Tok_Equals) {
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
    auto* struct_expr = new_struct_expr(struct_name);

    tok = lexer.peak();
    if (tok.type == Tok_LBracket) {
        lexer.skip();
        if (lexer.peak().type == Tok_RBracket) {
            lexer.skip();
            skip_semicolon_if_exist(lexer);
            return struct_expr;
        }
        auto field = lexer.next();
        if (field.type != Tok_Ident) {
            compiler_error(field, "Expected member name in function declaration, but got: '" SV_FORMAT "'\n", SV_ARG(field.val));
        }
        bool isPreviosWasField = true;
        add_field_or_report_if_exist(struct_expr, field);
        Token next_tok;
        do {
            next_tok = lexer.peak();
            if (next_tok.type == Tok_RBracket) {
            end:
                lexer.skip();
                skip_semicolon_if_exist(lexer);
                struct_expr->total_size = sizeof(double) * struct_expr->fields.count();
                return struct_expr;
            }
            if (isPreviosWasField && next_tok.type != Tok_Comma) {
                compiler_error(next_tok, "Expected ',' after member in function declaration, but got: '" SV_FORMAT "'\n", SV_ARG(next_tok.val));
            }
            if (next_tok.type == Tok_Comma) {
                lexer.skip();
                next_tok = lexer.peak();
                isPreviosWasField = false;
            }
            if (lexer.peak().type == Tok_RBracket)
                goto end;
            field = lexer.next();
            if (field.type != Tok_Ident)
                break;
            add_field_or_report_if_exist(struct_expr, field);
            isPreviosWasField = true;
        } while (next_tok.type != Tok_Eof);

        compiler_error(next_tok, "Expected } at the end of the function declaration, but got: '" SV_FORMAT "'\n", SV_ARG(next_tok.val));
    } else {
        compiler_error(tok, "Expected { at the start of the function declaration, but got: '" SV_FORMAT "'\n", SV_ARG(tok.val));
    }
    return struct_expr;
}

FunctionExpr* parse_function(Lexer& lexer) {
    auto decl = lexer.next();
    if (decl.type != Tok_Function) {
        compiler_error(decl, "Expected function declaration, got '" SV_FORMAT "'\n", SV_ARG(decl.val));
    }
    auto name = lexer.next();
    if (name.type != Tok_Ident) {
        compiler_error(name, "Expected function name, got '" SV_FORMAT "'\n", SV_ARG(name.val));
    }
    Array<Expression*> args;
    auto args_tok = lexer.next();
    if (args_tok.type == Tok_LParen) {
        Token close = lexer.peak();
        if (close.type != Tok_RParen) {
            do {
                Expression* inner = parse_expression(lexer);
                if (!inner) break;
                args.push(inner);
                close = lexer.next();
            } while (close.type == Tok_Comma);
            if (close.type != Tok_RParen) {
                compiler_error(close, "Expected ')' after '(', got '" SV_FORMAT "'\n", SV_ARG(close.val));
            }
        } else {
            lexer.next();
        }
    }
    return new_function_expr(name, args, parse_block(lexer));
}

Expression* parse_primary(Lexer& lexer) {
    auto next_tok = lexer.peak();
    if (next_tok.type == Tok_Struct) {
        auto* struc = parse_struct(lexer);
        ASSERT_NOT_NULL(struc);
        add_struct_or_report_if_exit(struc);
        return struc;
    } else if (next_tok.type == Tok_Function) {
        auto* fun = parse_function(lexer);
        ASSERT_NOT_NULL(fun);
        add_function_or_report_if_exit(fun);
        return fun;
    } else if (next_tok.type == Tok_Ident) {
        lexer.skip();
        auto oper = lexer.peak();
        bool is_already_exist = get_variable(next_tok, false) != nullptr;
        check_assignemnt(next_tok, oper, is_already_exist);
        auto* assign = parse_assignment(next_tok, false, lexer);
        ASSERT_NOT_NULL(assign);
        // get_and_add_variable(assign->tok, false, 8);
        return assign;
    } else {
        compiler_error(next_tok, "Expected function, global variable, or struct declaration at the top block, but got '" SV_FORMAT "'\n", SV_ARG(next_tok.val));
    }
    return nullptr;
}

Expression* parse_expression(Lexer& lexer, Expression* lhs, int min_prec) {
    if (!lhs) {
        lhs = parse_binary_expr(lexer);
        if (!lhs) return nullptr;
    }

    while (!lexer._tokens.is_empty()) {
       Token op = lexer.peak();
       auto op_index = is_operator(op);
       if (op_index == 0 || prec(op_index) < min_prec) break;

       lexer.next();
       Expression* rhs = parse_expression(lexer, nullptr, prec(op_index) + 1);
       if (!rhs) {
           compiler_error(op, "Expected expression after operator: '" SV_FORMAT "'\n", SV_ARG(op.val));
       }
       lhs = new_binary_expr(op, lhs, rhs);
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

void add_string(BinaryExpr* expr)
{
    auto name = expr->tok.val;
    if (get_string(name) == nullptr) {
        u64 offset = 0;
        for (auto& str : g_strings) {
            offset += str.name.size + 1;
        }
        g_strings.push(DeclaredString{name, expr, offset});
    }
}

void add_struct_or_report_if_exit(StructExpr* expr)
{
    for (auto& struc : g_structs) {
        if (struc.name == expr->tok.val) {
            compiler_error(expr->tok, "Function '" SV_FORMAT "' already declared\n", SV_ARG(struc.name));
        }
    }
    g_structs.push(DeclaredStruct{expr->tok.val, expr});
}

void add_function_or_report_if_exit(FunctionExpr* expr)
{
    for (auto& fun : g_functions) {
        if (fun.name == expr->tok.val) {
            compiler_error(expr->tok, "Function '" SV_FORMAT "' already declared\n", SV_ARG(fun.name));
        }
    }
    g_functions.push(DeclaredFunction{expr->tok.val, expr});
}

bool parse(Lexer& lexer, Array<Expression*>& exprs) {
    Expression* expr = nullptr;
    do {
        expr = parse_primary(lexer);
        if (!expr) return false;
        exprs.push(expr);
    } while (!lexer._tokens.is_empty() && expr);

    return true;
}

Array<Instruction> ops;

bool is_valid_operation(InstructionType operation_type, ValueType lhs, ValueType rhs, ValueType& result_type)
{
    switch(operation_type) {
        case OP_MINUS:
        case OP_PLUS: {
            if (!is_numeric_type(lhs) && !is_numeric_type(rhs))
                return false;
            else if ((lhs == TYPE_PTR || lhs == TYPE_STR) && !is_numeric_type(rhs))
                return false;
            else {
                result_type = MAX(lhs, rhs);
                return true;
            }
        } break;

        case OP_MULT:
        case OP_DIVIDE:
        case OP_MOD: {
            if (!is_numeric_type(lhs) && !is_numeric_type(rhs))
                return false;
            result_type = MAX(lhs, rhs);
            return true;
        }

        case OP_GREATER_EQUALS:
        case OP_GREATER:
        case OP_LESS:
        case OP_EQUALS: {
            if (!is_numeric_type(lhs) && !is_numeric_type(rhs))
                return false;
            result_type = TYPE_BOOL;
            return true;
        } break;

        case OP_CAST: {
            switch (rhs) {
                case TYPE_I8:
                case TYPE_I16:
                case TYPE_I32:
                case TYPE_I64: {
                    if (!is_numeric_type(lhs) && lhs != TYPE_STR && lhs != TYPE_BOOL)
                        return false;
                } break;

                case TYPE_STR: {
                    if (!is_numeric_type(lhs) && lhs != TYPE_STR)
                        return false;
                } break;

                case TYPE_BOOL: {
                    if (!is_numeric_type(lhs) && lhs != TYPE_BOOL)
                        return false;
                } break;

                default: {
                    TODO("OP_CAST");
                } break;
            }
            result_type = rhs;
            return true;
        }

        default: TODO(inst_type_to_str(operation_type));
    }
    return false;
}

Register& allocate_reg(Array<Register>& regs, u32 size, bool is_arg = false)
{
    u32 offset = 8;
    u32 offset_arg = 8;
    for (auto& reg : regs) {
        if (reg.is_arg)
            offset_arg += reg.size;
        else
            offset += reg.size;
    }
    if (is_arg)
        regs.push(Register{regs.count(), offset_arg, size, is_arg});
    else
        regs.push(Register{regs.count(), offset, size});
    return regs.last();
}

void update_all_offsets(Array<Register>& regs)
{
    u32 offset = 0;
    u32 offset_arg = 8;
    for (auto& reg : regs) {
        if (!reg.is_visited) continue;
        if (reg.is_arg) {
            offset_arg += reg.size;
            reg.offset = offset_arg;
        } else {
            offset += reg.size;
            reg.offset = offset;
        }
    }
}

usize regs_total_size(Array<Register>& regs)
{
    u32 offset = 8;
    for (auto& reg : regs) {
        if (reg.is_visited)
            offset += reg.size;
    }
    return offset;
}

bool compile_ops(StrBuilder& builder, Array<Instruction>& ops, Array<Register>& regs, usize rsp, bool is_main = false)
{
    usize ip = 0;
    while(ip++ < ops.count()) {
        auto current_inst = ops.get(ip - 1);
        if (!current_inst.is_visited)
            continue;
        // builder.append(".inst_") << ip << ':' << '\n';
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

            case OP_PUSH_STR: {
                auto& reg = regs[current_inst.reg_index];
                auto& str = g_strings[reg.str_val];
                if (!reg.is_visited) continue;
                builder.append("\tlea rax, [rbp - ") << reg.offset << ']' << '\n';
                builder.append("\tmov rbx, __strings + ") << str.offset << '\n';
                builder.append("\tmov [rax], rbx\n");
            } break;

            case OP_EQUALS: {
                auto& lhs = regs[current_inst.op.lhs_index];
                auto& rhs = regs[current_inst.op.rhs_index];
                auto& res = regs[current_inst.reg_index];

                builder.append("\txor rcx, rcx\n");
                if (rhs.is_comp_time)
                    builder.append("\tmov rax, ") << rhs.int_val << '\n';
                else
                    builder.append("\tmov rax, [rbp - ") << rhs.offset << ']' << '\n';
                if (lhs.is_comp_time)
                    builder.append("\tmov rbx, ") << lhs.int_val << '\n';
                else
                    builder.append("\tmov rbx, [rbp - ") << lhs.offset << ']' << '\n';
                builder.append("\tcmp rax, rbx\n");
                builder.append("\tsete cl\n");
                (builder.append("\tmov [rbp - ") << res.offset).append("], rcx\n");
            } break;

            case OP_LESS: {
                auto& lhs = regs[current_inst.op.lhs_index];
                auto& rhs = regs[current_inst.op.rhs_index];
                auto& res = regs[current_inst.reg_index];

                if (rhs.is_comp_time)
                    builder.append("\tmov rax, ") << rhs.int_val << '\n';
                else
                    builder.append("\tmov rax, [rbp - ") << rhs.offset << ']' << '\n';
                if (lhs.is_comp_time)
                    builder.append("\tmov rbx, ") << lhs.int_val << '\n';
                else
                    builder.append("\tmov rbx, [rbp - ") << lhs.offset << ']' << '\n';
                builder.append("\tcmp rax, rbx\n");
                builder.append("\tsetl al\n");
                builder.append("\tmovzx rax, al\n");
                (builder.append("\tmov [rbp - ") << res.offset).append("], rax\n");
            } break;

            case OP_GREATER_EQUALS:
            case OP_GREATER: {
                auto& lhs = regs[current_inst.op.lhs_index];
                auto& rhs = regs[current_inst.op.rhs_index];
                auto& res = regs[current_inst.reg_index];

                if (rhs.is_comp_time)
                    builder.append("\tmov rax, ") << rhs.int_val << '\n';
                else
                    builder.append("\tmov rax, [rbp - ") << rhs.offset << ']' << '\n';
                // if (lhs.is_comp_time)
                    // builder.append("\tmov rbx, ") << lhs.int_val << '\n';
                // else
                    builder.append("\tmov rbx, [rbp - ") << lhs.offset << ']' << '\n';
                builder.append("\tcmp rax, rbx\n");
                if (current_inst.type == OP_GREATER)
                    builder.append("\tsetg al\n");
                else
                    builder.append("\tsetge al\n");
                builder.append("\tmovzx rax, al\n");
                (builder.append("\tmov [rbp - ") << res.offset).append("], rax\n");
            } break;

            case OP_PRINT: {
                const char* func;
                auto& reg = regs[current_inst.reg_index];
                u32 offset = reg.offset;
                if (current_inst.assign.var.type == TYPE_STR) func = "__print_str";
                else if (current_inst.assign.var.type == TYPE_BOOL) func = "__print_bool";
                else if (current_inst.assign.var.type == TYPE_PTR) TODO("TYPE_PTR");
                else func = "__print_num";

                if (get_system() == FlagsSystem::WINDOWS)
                    builder.append("\tmov rcx, ");
                else
                    builder.append("\tmov rdi, ");
                if (reg.is_comp_time)
                    builder << reg.int_val << '\n';
                else {
                    char sign = reg.is_arg ? '+' : '-';
                    builder.append("[rbp ") << sign << offset << ']' << '\n';
                }
                builder.append("\tcall ").append(func) << '\n';
            } break;

            case OP_STORE: {
                ASSERT(current_inst.data_type == TYPE_VARIABLE, "Invalid data type\n");
                auto& var = current_inst.assign.var;
                auto& rhs = regs[current_inst.reg_index];
                auto& reg = regs[var.reg_index];
                if (!rhs.is_visited) continue;

                builder.append("\tmov rbx, QWORD [rbp - ") << rhs.offset << ']' << '\n';
                const char* size_word_reg = "rbx";

                auto offset = reg.offset;
                if (var.is_local) {
                    char sign = var.is_argument ? '+' : '-';
                    // if (var.is_argument) offset += 8;
                    builder.append("\tlea rax, [rbp ") << sign << offset << ']' << '\n';
                } else {
                    builder.append("\tmov rax, __globals + ") << offset << '\n';
                }
                builder.append("\tmov [rax], ").append(size_word_reg) << '\n';
            } break;

            case OP_LOAD: {
                ASSERT(current_inst.data_type == TYPE_VARIABLE, "Invalid data type\n");
                auto& var = current_inst.assign.var;
                auto& reg = regs[current_inst.reg_index];
                auto& rhs = regs[var.reg_index];
                if (!rhs.is_visited) continue;

                char sign = var.is_argument ? '+' : '-';
                auto offset = rhs.offset;
                if (var.is_local) {
                    if (var.is_argument) offset += 8;
                    builder.append("\tmov rax, [rbp ") << sign << offset << ']' << '\n';
                } else {
                    builder.append("\tmov rax, [__globals + ") << offset << ']' << '\n';
                }
                builder.append("\tmov [rbp - ") << reg.offset << ']';
                builder.append(", rax\n");
            } break;

            case OP_INC: {
                auto& lhs = regs[current_inst.op.lhs_index];

                builder.append("\tinc qword [rbp - ") << lhs.offset << ']' << '\n';
            } break;

            case OP_JMP: {
                ASSERT(current_inst.data_type == TYPE_ADDRESS, "Invalid data type\n");
                builder.append("\tjmp .lab_") << current_inst.addr.ip << '\n';
            } break;

            case OP_JMP_IF: {
                ASSERT(current_inst.data_type == TYPE_ADDRESS, "Invalid data type\n");
                auto& cond = regs[current_inst.reg_index];

                builder.append("\tmov rax, [rbp - ") << cond.offset << ']' << '\n';
                builder.append("\ttest rax, rax\n");
                builder.append("\tjnz .lab_") << current_inst.addr.ip << '\n';
            } break;

            case OP_LABEL: {
                builder.append(".lab_") << current_inst.addr.ip << ':' << '\n';
            } break;

            case OP_RET: {
                // auto& res = regs[current_inst.ret.var.reg_index];
                auto& res = regs[current_inst.reg_index];
                if (res.is_comp_time)
                    builder.append("\tmov rax, ") << res.int_val << '\n';
                else
                    builder.append("\tmov rax, [rbp - ") << res.offset << ']' << '\n';
                // if (current_inst.ret.size_of_regs > 0)
                   	// builder.append("\tadd rsp, ") << current_inst.ret.size_of_regs << '\n';
                builder.append("\tmov rsp, rbp\n");
                builder.append("\tpop rbp\n");
                builder.append("\tret\n");
            } break;

            case OP_CALL: {
                ASSERT(current_inst.data_type == TYPE_CALL, "Invalid data type\n");
                auto current_arg_offset = 0;
                for (auto& arg : current_inst.call.args) {
                    auto& res = regs[arg.reg_index];
                    builder.append("\tmov rax, [rbp - ") << res.offset << ']' << '\n';
                    builder.append("\tmov [rsp+") << current_arg_offset << ']';
                        builder.append(", rax\n");
                    current_arg_offset += arg.size;
                }
                builder.append("\tcall ").append(current_inst.call.name.val) << '\n';
                // builder.append("\tadd rsp, ") << current_inst.call.args_count * 8 << '\n';

                auto& res = regs[current_inst.reg_index];
                if (!current_inst.call.is_void) {
                    builder.append("\tmov [rbp - ") << res.offset << ']';
                    builder.append(", rax\n");
                }
            } break;

            case OP_PLUS: {
                // this wont work is register is argument xd
                auto& lhs = regs[current_inst.op.lhs_index];
                auto& rhs = regs[current_inst.op.rhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;
                if (res.is_comp_time) {
                    (builder.append("\tmov QWORD [rbp - ") << res.offset).append("], ") << res.int_val << '\n';
                    break;
                }

                if (lhs.is_comp_time)
                    builder.append("\tmov rax, ") << lhs.int_val << '\n';
                else
                    builder.append("\tmov rax, [rbp - ") << lhs.offset << ']' << '\n';
                if (rhs.is_comp_time)
                    builder.append("\tmov rbx, ") << rhs.int_val << '\n';
                else
                    builder.append("\tmov rbx, [rbp - ") << rhs.offset << ']' << '\n';
                builder.append("\tadd rax, rbx\n");
                (builder.append("\tmov [rbp - ") << res.offset).append("], rax\n");
            } break;

            case OP_MINUS: {
                auto& lhs = regs[current_inst.op.lhs_index];
                auto& rhs = regs[current_inst.op.rhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;
                if (res.is_comp_time) {
                    (builder.append("\tmov QWORD [rbp - ") << res.offset).append("], ") << res.int_val << '\n';
                    break;
                }

                if (lhs.is_comp_time)
                    builder.append("\tmov rax, ") << lhs.int_val << '\n';
                else
                    builder.append("\tmov rax, [rbp - ") << lhs.offset << ']' << '\n';
                if (rhs.is_comp_time)
                    builder.append("\tmov rbx, ") << rhs.int_val << '\n';
                else
                    builder.append("\tmov rbx, [rbp - ") << rhs.offset << ']' << '\n';

                builder.append("\tsub rax, rbx\n");
                (builder.append("\tmov [rbp - ") << res.offset).append("], rax\n");
            } break;

            case OP_MOD:
            case OP_DIVIDE: {
                auto& lhs = regs[current_inst.op.lhs_index];
                auto& rhs = regs[current_inst.op.rhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;
                if (res.is_comp_time) {
                    (builder.append("\tmov QWORD [rbp - ") << res.offset).append("], ") << res.int_val << '\n';
                    break;
                }

                builder.append("\txor rdx, rdx\n");
                if (lhs.is_comp_time)
                    builder.append("\tmov rax, ") << lhs.int_val << '\n';
                else
                    builder.append("\tmov rax, [rbp - ") << lhs.offset << ']' << '\n';
                if (rhs.is_comp_time)
                    builder.append("\tmov rcx, ") << rhs.int_val << '\n';
                else
                    builder.append("\tmov rcx, [rbp - ") << rhs.offset << ']' << '\n';
                builder.append("\tdiv rcx\n");
                if (current_inst.type == OP_MOD)
                    (builder.append("\tmov [rbp - ") << res.offset).append("], rdx\n");
                else
                    (builder.append("\tmov [rbp - ") << res.offset).append("], rax\n");
            } break;

            case OP_MULT: {
                auto& lhs = regs[current_inst.op.lhs_index];
                auto& rhs = regs[current_inst.op.rhs_index];
                auto& res = regs[current_inst.reg_index];
                if (!res.is_visited) continue;
                if (res.is_comp_time) {
                    (builder.append("\tmov QWORD [rbp - ") << res.offset).append("], ") << res.int_val << '\n';
                    break;
                }

                builder.append("\txor rdx, rdx\n");
                if (lhs.is_comp_time)
                    builder.append("\tmov rax, ") << lhs.int_val << '\n';
                else
                    builder.append("\tmov rax, [rbp - ") << lhs.offset << ']' << '\n';
                if (rhs.is_comp_time)
                    builder.append("\tmov rcx, ") << rhs.int_val << '\n';
                else
                    builder.append("\tmov rcx, [rbp - ") << rhs.offset << ']' << '\n';
                builder.append("\tmul rcx\n");
                (builder.append("\tmov [rbp - ") << res.offset).append("], rax\n");
            } break;

            case OP_NOP: UNREACHABLE("compile_ops");
            default: TODO(inst_type_to_str(current_inst.type));
        }
    }
    return true;
}

bool compile_function(StrBuilder& builder, DeclaredFunction func, Array<Instruction>& global_ops)
{
    builder.append(func.name) << ':' << '\n';
	const bool is_main = func.name == "main";
	const bool is_entry = func.name == "__entry";
   	builder.append("\tpush rbp\n");
   	builder.append("\tmov rbp, rsp\n");
    if (func.regs.count() > 0) {
        usize registers_size = func.regs.last().offset + func.regs.last().size;
       	builder.append("\tsub rsp, ") << registers_size << '\n';
    }
	bool contains_return = false;
	for (auto op : func.ops) {
	    if (op.type == OP_RET) {
            contains_return = true;
            break;
		}
	}
	if (!contains_return) {
    	if (is_main) {
            auto& ret_reg = allocate_reg(func.regs, 8);
            ret_reg.is_comp_time = true;
            ret_reg.is_visited = true;
            ret_reg.int_val = 0;
            func.ops.push(Instruction{.type = OP_RET, .location = func.expr->tok, .reg_index= ret_reg.index, .is_visited = true,});
        } else {
            if (!is_entry)
                compiler_error(func.expr->tok, "Function '" SV_FORMAT "' does not return\n", SV_ARG(func.name));
        }
	}
	if (is_entry) {
        auto reg = allocate_reg(func.regs, 8);
    	global_ops.push(Instruction{.type = OP_CALL, .data_type = TYPE_CALL, .call = {Token{Tok_StrLit, "main"}, 0}, .reg_index = reg.index, .is_visited = true});
        if (get_system() != FlagsSystem::WINDOWS)
           	global_ops.push(Instruction{.type = OP_CALL, .data_type = TYPE_CALL, .call = {Token{Tok_StrLit, "__exit"}, 0}, .reg_index = reg.index, .is_visited = true});
        else
           	global_ops.push(Instruction{.type = OP_RET, .is_visited = true});
    	compile_ops(builder, global_ops, func.regs, func.vars_size, false);
	} else {
	    compile_ops(builder, func.ops, func.regs, func.vars_size, is_main);
	}

	return true;
}

static const char hex_chars[] = "0123456789ABCDEF";
void append_hex(StrBuilder& builder, StrView str, u32& append_nulls) {
    usize iter = 0;
    while (iter < str.size) {
        char c = str.data[iter];
        builder.append("0x", 2);
        if (c == '\\' && iter + 1 < str.size && str.data[iter + 1] == 'n') {
            c = '\n';
            ++iter;
            ++append_nulls;
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

            case OP_JMP: {
                usize target_ip = g_labels[inst.addr.ip].ip;
                if (!visited[target_ip]) {
                    worklist.push(target_ip);
                }
                if (ip + 1 < ops.count() && !visited[ip + 1]) {
                    worklist.push(ip + 1);
                }
                continue;
            }

            case OP_JMP_IF: {
                usize target_ip = g_labels[inst.addr.ip].ip;

                if (!visited[target_ip]) {
                    worklist.push(target_ip);
                }
                if (ip + 1 < ops.count() && !visited[ip + 1]) {
                    worklist.push(ip + 1);
                }
                regs[inst.reg_index].is_visited = true;
                continue;
            }

            case OP_PUSH_STR:
            case OP_PRINT:
            case OP_RET:
            case OP_PUSH_BOOL:
            case OP_PUSH_I64:
            case OP_PUSH_I32:
            case OP_PUSH_I16:
            case OP_PUSH_I8: {
                auto& reg = regs[inst.reg_index];
                if (!reg.is_comp_time)
                    reg.is_visited = true;
            } break;

            case OP_LESS:
            case OP_PLUS:
            case OP_MINUS:
            case OP_MOD:
            case OP_DIVIDE:
            case OP_MULT: {
                auto& lhs = regs[inst.op.lhs_index];
                auto& rhs = regs[inst.op.rhs_index];
                auto& res = regs[inst.reg_index];
                if (!lhs.is_comp_time)
                    lhs.is_visited = true;
                if (!rhs.is_comp_time)
                    rhs.is_visited = true;
            } break;

            case OP_EQUALS:
            case OP_GREATER_EQUALS:
            case OP_GREATER: {
                auto& lhs = regs[inst.op.lhs_index];
                auto& rhs = regs[inst.op.rhs_index];
                auto& res = regs[inst.reg_index];
                // if (!lhs.is_comp_time)
                    lhs.is_visited = true;
                if (!rhs.is_comp_time)
                    rhs.is_visited = true;
            } break;

            case OP_STORE:
            case OP_LOAD: {
                auto& reg = regs[inst.reg_index];
                auto& rhs = regs[inst.assign.var.reg_index];
                if (!rhs.is_comp_time)
                    rhs.is_visited = true;
                reg.is_visited = true;
            } break;

            case OP_INC: {
                auto& res = regs[inst.reg_index];
                res.is_visited = true;
                res.is_comp_time = false;
            } break;

            case OP_CALL: {
                for (usize i = 0; i < inst.call.args.count(); ++i) {
                    auto& call_arg = inst.call.args[i];
                    auto& arg = regs[call_arg.reg_index];
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

void print_ast_function(DeclaredFunction& func)
{
    if (func.name == "__entry") return;

    log(SV_FORMAT ":\n", SV_ARG(func.name));
    Array<Register>& regs = func.regs;
    for (usize ip = 0; ip < func.ops.count(); ++ip) {
        auto& current_inst = func.ops[ip];
        if (!current_inst.is_visited) continue;

        auto& reg = regs[current_inst.reg_index];
        switch(current_inst.type)
            {
                case OP_PUSH_I64:
                case OP_PUSH_I32:
                case OP_PUSH_I16:
                case OP_PUSH_I8: {
                    if (!reg.is_visited) continue;
                    log("  [%zu] = %lld\n", reg.index, reg.int_val);
                } break;

                case OP_PUSH_BOOL: {
                    log("  [%zu] = %s\n", reg.index, reg.bool_val ? "true" : "false");
                } break;

                case OP_PUSH_STR: {
                    log("  [%zu] = \"" SV_FORMAT "\"\n", reg.index, SV_ARG(g_strings[reg.str_val].name));
                } break;

                case OP_EQUALS: {
                    log("  [%zu] = [%zu] == [%zu]\n", reg.index, current_inst.op.rhs_index, current_inst.op.lhs_index);
                } break;

                case OP_LESS: {
                    log("  [%zu] = [%zu] < [%zu]\n", reg.index, current_inst.op.rhs_index, current_inst.op.lhs_index);
                } break;

                case OP_GREATER_EQUALS:
                case OP_GREATER: {
                    if (current_inst.type == OP_GREATER)
                        log("  [%zu] = [%zu] > [%zu]\n", reg.index, current_inst.op.rhs_index, current_inst.op.lhs_index);
                    else
                        log("  [%zu] = [%zu] >= [%zu]\n", reg.index, current_inst.op.rhs_index, current_inst.op.lhs_index);
                } break;

                case OP_PRINT: {
                    const char* func;
                    if (current_inst.assign.var.type == TYPE_STR) func = "__print_str";
                    else if (current_inst.assign.var.type == TYPE_BOOL) func = "__print_bool";
                    else if (current_inst.assign.var.type == TYPE_PTR) TODO("TYPE_PTR");
                    else func = "__print_num";

                    if (reg.is_comp_time)
                        log("  %s(%lld)\n", func, reg.int_val);
                    else
                        log("  %s([%zu])\n", func, current_inst.reg_index);
                } break;

                case OP_STORE: {
                    ASSERT(current_inst.data_type == TYPE_VARIABLE, "Invalid data type\n");
                    auto& var = current_inst.assign.var;
                    if (!regs[var.reg_index].is_visited) continue;
                    if (reg.is_comp_time) {
                        log("  [%zu] = %lld\n", var.reg_index, reg.int_val);
                        break;
                    }
                    log("  [%zu] = [%zu]\n", var.reg_index, current_inst.reg_index);
                } break;

                case OP_LOAD: {
                    ASSERT(current_inst.data_type == TYPE_VARIABLE, "Invalid data type\n");
                    auto& var = current_inst.assign.var;
                    if (!regs[var.reg_index].is_visited) continue;
                    if (reg.is_comp_time)
                        log("  [%zu] = %lld\n", current_inst.reg_index, reg.int_val);
                    else
                        log("  [%zu] = [%zu]\n", current_inst.reg_index, var.reg_index);
                } break;

                case OP_INC: {
                    log("  inc [%zu]\n", current_inst.op.lhs_index);
                } break;

                case OP_JMP: {
                    ASSERT(current_inst.data_type == TYPE_ADDRESS, "Invalid data type\n");
                    log("  jmp %lld\n", current_inst.addr.ip);
                } break;

                case OP_JMP_IF: {
                    ASSERT(current_inst.data_type == TYPE_ADDRESS, "Invalid data type\n");
                    log("  if [%zu] jmp %lld\n", current_inst.reg_index, current_inst.addr.ip);
                } break;

                case OP_RET: {
                    if (reg.is_comp_time)
                        log("  return %lld\n", reg.int_val);
                    else
                        log("  return [%zu]\n", current_inst.reg_index);
                } break;

                case OP_CALL: {
                    ASSERT(current_inst.data_type == TYPE_CALL, "Invalid data type\n");
                    auto current_arg_offset = 0;
                    if (!current_inst.call.is_void) {
                        log("  [%zu] = ", current_inst.reg_index);
                    }
                    log(SV_FORMAT "(", SV_ARG(current_inst.call.name.val));

                    for (usize i = 0; i < current_inst.call.args.count(); ++i) {
                        auto& arg = current_inst.call.args[i];
                        log("  [%zu]", arg.reg_index);
                        if (i < current_inst.call.args.count() - 1)
                            log(", ");
                    }
                    log(")\n");
                } break;

                case OP_PLUS: {
                    if (!reg.is_visited) continue;
                    if (reg.is_comp_time)
                        log("  [%zu] = %lld\n", current_inst.reg_index, reg.int_val);
                    else
                        log("  [%zu] = [%zu] + [%zu]\n", current_inst.reg_index, current_inst.op.rhs_index, current_inst.op.lhs_index);
                } break;

                case OP_MINUS: {
                    if (!reg.is_visited) continue;
                    auto& reg = regs[current_inst.reg_index];
                    if (reg.is_comp_time)
                        log("  [%zu] = %lld\n", current_inst.reg_index, reg.int_val);
                    else
                        log("  [%zu] = [%zu] - [%zu]\n", current_inst.reg_index, current_inst.op.rhs_index, current_inst.op.lhs_index);
                } break;

                case OP_MOD:
                case OP_DIVIDE: {
                    if (!reg.is_visited) continue;
                    if (reg.is_comp_time)
                        log("  [%zu] = %lld\n", current_inst.reg_index, reg.int_val);
                    else {
                        if (current_inst.type == OP_MOD)
                            log("  [%zu] = [%zu] %% [%zu]\n", current_inst.reg_index, current_inst.op.rhs_index, current_inst.op.lhs_index);
                        else
                            log("  [%zu] = [%zu] / [%zu]\n", current_inst.reg_index, current_inst.op.rhs_index, current_inst.op.lhs_index);
                    }
                } break;

                case OP_MULT: {
                    if (!reg.is_visited) continue;
                    if (reg.is_comp_time)
                        log("  [%zu] = %lld\n", current_inst.reg_index, reg.int_val);
                    else
                        log("  [%zu] = [%zu] * [%zu]\n", current_inst.reg_index, current_inst.op.rhs_index, current_inst.op.lhs_index);
                } break;

                case OP_LABEL: {
                    log("%zu:\n", current_inst.addr.ip);
                } break;

                case OP_NOP: UNREACHABLE("eval");
                default: TODO(inst_type_to_str(current_inst.type));
        }
    }
}

void add_std_library(StrBuilder& builder)
{
#ifdef _WIN32
    builder.append("extern GetStdHandle, WriteConsoleA\n");
    builder.append("__print_num:\n");
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
	builder.append("\tcall GetStdHandle\n");
	builder.append("\tmov	ecx, esi\n");
	builder.append("\tnot	ecx\n");
	builder.append("\tlea	r8d, [rdi + rcx]\n");
	builder.append("\tadd	r8d, 32\n");
	builder.append("\tmov	qword [rsp + 32], 0\n");
	builder.append("\tlea	r9, [rsp + 44]\n");
	builder.append("\tmov	rcx, rax\n");
	builder.append("\tmov	rdx, rsi\n");
	builder.append("\tcall WriteConsoleA\n");
	builder.append("\tnop\n");
	builder.append("\tadd	rsp, 88\n");
	builder.append("\tret\n");
    builder.append("__print_str:\n");
    builder.append("\tpush	rsi\n");
    builder.append("\tpush	rdi\n");
    builder.append("\tsub	rsp, 56\n");
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
    builder.append("\tcall	GetStdHandle\n");
    builder.append("\tmov	qword [rsp + 32], 0\n");
    builder.append("\tlea	r9, [rsp + 52]\n");
    builder.append("\tmov	rcx, rax\n");
    builder.append("\tmov	rdx, rsi\n");
    builder.append("\tmov	r8d, edi\n");
    builder.append("\tcall	WriteConsoleA\n");
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
    builder.append("\tcall	GetStdHandle\n");
    builder.append("\ttest	bl, bl\n");
    builder.append("\tje	.LBB2_2\n");
    builder.append("\tmov	qword [rsp + 32], 0\n");
    builder.append("\tmov	rdx, __strings\n");
    builder.append("\tlea	r9, [rsp + 44]\n");
    builder.append("\tmov	rcx, rax\n");
    builder.append("\tmov	r8d, 5\n");
    builder.append("\tjmp	.LBB2_3\n");
    builder.append(".LBB2_2:\n");
    builder.append("\tmov	qword [rsp + 32], 0\n");
    builder.append("\tmov	rdx, __strings + 6\n");
    builder.append("\tlea	r9, [rsp + 44]\n");
    builder.append("\tmov	rcx, rax\n");
    builder.append("\tmov	r8d, 6\n");
    builder.append(".LBB2_3:\n");
    builder.append("\tcall	WriteConsoleA\n");
    builder.append("\tnop\n");
    builder.append("\tadd	rsp, 48\n");
    builder.append("\tpop	rbx\n");
    builder.append("\tret\n");
#else
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
    builder.append("__print_str:\n");
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
    builder.append("__print_bool:\n");
    builder.append("\ttest	edi, edi\n");
    builder.append("\tje	.LBB2_2\n");
    builder.append("\tmov	rsi, __strings\n");
    builder.append("\tmov	eax, 1\n");
    builder.append("\tmov	edx, 5\n");
    builder.append("\tmov	edi, 1\n");
    builder.append("\tsyscall\n");
    builder.append("\tret\n");
    builder.append(".LBB2_2:\n");
    builder.append("\tmov	rsi, __strings + 6\n");
    builder.append("\tmov	eax, 1\n");
    builder.append("\tmov	edx, 6\n");
    builder.append("\tmov	edi, 1\n");
    builder.append("\tsyscall\n");
    builder.append("\tret\n");
    builder.append("__exit:\n");
   	builder.append("\tmov	rdi, rax\n");
   	builder.append("\tmov	rax, 60\n");
    builder.append("\tsyscall\n");
#endif
}

bool compile_program(Array<Instruction>& global_ops)
{
    StrBuilder builder{};
    builder.append("BITS 64\n");
    builder.append("section .text\n");
    builder.append("extern main, __entry\n");
    add_std_library(builder);

    for (auto& func : g_functions) {
        dead_code(func);
        update_all_offsets(func.regs);
        compile_function(builder, func, global_ops);
        if (0) print_ast_function(func);
    }

    if (g_strings.count() != 0) {
        builder.append("\nsection .data\n");
        builder.append("\t__strings: db ");
        for (auto str : g_strings) {
            u32 append_nulls = 0;
            append_hex(builder, str.name, append_nulls);
            for (u32 i = 0; i < append_nulls; ++i)
                builder.append("0,");
            builder.append("0, ");
        }
        builder.append('\n');
    }
    builder.append("\nsection .bss\n");
    builder.append("\t__globals: resb 99999\n"); // TODO: make this value dependent on size of all global vars
    StrBuilder out_asm_b{}; out_asm_b.append(out_path).append(".asm").append_null(false);
    StrBuilder out_obj_b{}; out_obj_b.append(out_path).append(".obj").append_null(false);
    auto out_asm = out_asm_b.to_string_view(true);
    auto out_obj = out_obj_b.to_string_view(true);
    if (!write_to_file(out_asm, builder.data(), builder.count())) return false;

    Cmd cmd{};
    cmd.push("nasm", "-o");
    cmd.append(out_obj).append(' ');
    if (get_system() == FlagsSystem::WINDOWS)
        cmd.push("-f", "win64");
    else
        cmd.push("-f", "elf64");
    cmd.append(out_asm);
    if (!cmd.execute().wait()) return false;
    if (get_system() == FlagsSystem::WINDOWS) {
        cmd.push("link");
        cmd.append(out_obj).append(' ');
        cmd.push("kernel32.lib", "/ENTRY:__entry", "/NOLOGO");
        if (!cmd.execute().wait()) return false;
        cmd.push(out_path);
    }
    else {
        cmd.push("ld");
        cmd.append(out_obj).append(' ');
        cmd.push("--entry", "__entry", "-o");
        cmd.append(out_path);
        if (!cmd.execute().wait()) return false;
        cmd.append("./").append(out_path);
    }
    return cmd.execute().wait();
}

u32 type_size(ValueType type)
{
    switch (type) {
        case TYPE_VOID: return 0;
        case TYPE_BOOL: return 8;
        case TYPE_I8:   return 1;
        case TYPE_I16:  return 2;
        case TYPE_I32:  return 4;
        case TYPE_I64:  return 8;
        case TYPE_PTR:  return 8;
        case TYPE_STR:  return 8;
        default:
            UNREACHABLE("type_size");
    }
}

void eval_binary(InstructionType instruction_type, ValueType type, Register& lhs, Register& rhs, Register& res)
{
    switch(type) {
        case TYPE_I64:
        {
            res.is_comp_time = true;
            auto lhs_val = lhs.int_val;
            auto rhs_val = rhs.int_val;
            switch(instruction_type) {
                case OP_PLUS: {
                    res.int_val = lhs_val + rhs_val;
                } break;

                case OP_MINUS: {
                    res.int_val = lhs_val - rhs_val;
                } break;

                case OP_MULT: {
                    res.int_val = lhs_val * rhs_val;
                } break;

                case OP_DIVIDE: {
                    res.int_val = lhs_val / rhs_val;
                } break;

                case OP_MOD: {
                    res.int_val = lhs_val % rhs_val;
                } break;

                default: UNREACHABLE("eval_binary");
            }
        } break;

        case TYPE_BOOL: break;

        default: TODO(value_type_to_str(type));
    }
}

Register translate_to_instruction(Array<Instruction>& ops, Array<Register>& regs, Array<Variable>& local_vars, Expression* expr, ValueType& return_type)
{
    if (!expr) return {};
    switch(expr->type)
    {
        case Expr_Binary: {
            auto* bin_expr = dynamic_cast<BinaryExpr*>(expr);
            ASSERT_NOT_NULL(bin_expr);
            ValueType lhs_type = TYPE_NOP;
            ValueType rhs_type = TYPE_NOP;
            auto lhs_reg = translate_to_instruction(ops, regs, local_vars, bin_expr->lhs, lhs_type);
            auto rhs_reg = translate_to_instruction(ops, regs, local_vars, bin_expr->rhs, rhs_type);
            // u32 _index = regs.count() - 1;
            auto& operation = bin_expr->tok.type;
            if (operation == Tok_IntLit) {
                StrBuilder val{get_global_allocator()};
                val.append(bin_expr->tok.val).append_null(false);
                return_type = TYPE_I64;

                auto& reg = allocate_reg(regs, type_size(return_type));
                reg.is_comp_time = true;
                reg.type = return_type;
                reg.int_val = atoll(val.data());
                ops.push(Instruction{.type = OP_PUSH_I64, .location = bin_expr->tok, .reg_index = reg.index});
                return reg;
            }
            else if (operation == Tok_BoolLit) {
                return_type = TYPE_BOOL;

                auto& reg = allocate_reg(regs, type_size(return_type));
                reg.is_comp_time = true;
                reg.type = return_type;
                reg.bool_val = bin_expr->tok.val == "true" ? true : false;
                ops.push(Instruction{.type = OP_PUSH_BOOL, .location = bin_expr->tok, .reg_index = reg.index});
                return reg;
            }
            else if (operation == Tok_StrLit) {
                usize index = 0;
                auto* str = get_string(bin_expr->tok.val, &index);
                if (!str) TODO("report error");
                return_type = TYPE_STR;

                auto& reg = allocate_reg(regs, type_size(return_type));
                reg.type = return_type;
                reg.str_val = index;
                ops.push(Instruction{.type = OP_PUSH_STR, .location = bin_expr->tok, .reg_index = reg.index});
                return reg;
            }
            else if (operation == Tok_Type) {
                if (bin_expr->tok.val == "i8") return_type = TYPE_I8;
                else if (bin_expr->tok.val == "i16") return_type = TYPE_I16;
                else if (bin_expr->tok.val == "i32") return_type = TYPE_I32;
                else if (bin_expr->tok.val == "i64") return_type = TYPE_I64;
                else if (bin_expr->tok.val == "bool") return_type = TYPE_BOOL;
                else if (bin_expr->tok.val == "string") return_type = TYPE_STR;
                else
                    compiler_error(bin_expr->tok, "Unknown cast type '" SV_FORMAT "'\n", SV_ARG(bin_expr->tok.val));
                // ops.push(Instruction{.type = OP_PUSH_TYPE, .data_type = TYPE_LITERAL, .location = bin_expr->tok, .bool_val = {return_type}});
            }
            else if (operation == Tok_Ident) {
                auto* var = get_variable(bin_expr->tok, false, &local_vars);
                if (!var || !var->is_accesible) {
                    compiler_error(bin_expr->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(bin_expr->tok.val));
                }
                return_type = var->type;
                // auto& reg = allocate_reg(regs, type_size(var->type));
                auto& var_reg = regs[var->reg_index];
                // ops.push(Instruction{.type = OP_LOAD, .data_type = TYPE_VARIABLE, .location = bin_expr->tok, .assign = {*var}, .reg_index = reg.index});
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
                    auto& res_reg = allocate_reg(regs, type_size(return_type));
                    if (lhs_reg.is_comp_time && rhs_reg.is_comp_time)
                        eval_binary(instruction_type, return_type, lhs_reg, rhs_reg, res_reg);
                    ops.push(Instruction{.type = instruction_type, .location = bin_expr->tok, .op = {instruction_type, lhs_reg.index, rhs_reg.index}, .reg_index = res_reg.index});
                    return res_reg;
                }
            }
        } break;

        case Expr_Function: {
            auto* fun_expr = dynamic_cast<FunctionExpr*>(expr);
            ASSERT_NOT_NULL(fun_expr);
            Array<Instruction> function_ops{};
            Array<Register> function_regs{};
            Array<Variable> vars{};
            auto index = g_functions.find_first(DeclaredFunction{fun_expr->tok.val, fun_expr});
            if (index != Array<DeclaredFunction>::INVALID_INDEX) {
                s64 args_count = 0;
                for (auto& arg : fun_expr->args) {
                    auto& reg = allocate_reg(function_regs, 8, true);
                    vars.push(Variable{.name = arg->tok.val, .type = TYPE_I64, .reg_index = reg.index, .is_local = true, .is_argument = true});
                }
            } else {
                // should never happened, probably compiler bug if hits
                compiler_error(fun_expr->tok, "Undeclared function '" SV_FORMAT "'\n", SV_ARG(fun_expr->tok.val));
            }
            translate_to_instruction(function_ops, function_regs, vars, fun_expr->block, return_type);
            usize vars_size = 0;
            for (auto var : vars) {
                // if (!var.is_argument)
                    // vars_size += var.total_size;
            }
            g_functions[index].vars_size = vars_size;
            g_functions[index].regs = function_regs;
            g_functions[index].ops = function_ops;
        } break;

        case Expr_Block: {
            auto* block_expr = dynamic_cast<BlockExpr*>(expr);
            ASSERT_NOT_NULL(block_expr);
            auto before_vars = local_vars.count();
            auto before_regs = regs.count();
            for (auto& statement : block_expr->exprs) {
                translate_to_instruction(ops, regs, local_vars, statement, return_type);
            }
            for (; before_vars < local_vars.count(); ++before_vars)
                local_vars[before_vars].is_accesible = false;
        } break;

        case Expr_Return: {
            auto* ret_expr = dynamic_cast<ReturnExpr*>(expr);
            ASSERT_NOT_NULL(ret_expr);
            auto res_reg = translate_to_instruction(ops, regs, local_vars, ret_expr->rhs, return_type);

            Variable var = {ret_expr->tok.val, return_type};
            usize size_of_regs = regs_total_size(regs);
            ops.push(Instruction{.type = OP_RET, .location = ret_expr->tok, .ret = {var, size_of_regs}, .reg_index = res_reg.index});
        } break;

        case Expr_Print: {
            auto* print_expr = dynamic_cast<PrintExpr*>(expr);
            ASSERT_NOT_NULL(print_expr);
            ValueType rhs_type;
            auto rhs = translate_to_instruction(ops, regs, local_vars, print_expr->rhs, rhs_type);

            return_type = TYPE_VOID;
            Variable var = {print_expr->rhs->tok.val, rhs_type};
            ops.push(Instruction{.type = OP_PRINT, .location = print_expr->tok, .assign = {var}, .reg_index = rhs.index});
        } break;

        case Expr_Assignment: {
            auto* assign = dynamic_cast<AssignmentExpr*>(expr);
            ASSERT_NOT_NULL(assign);

            auto* is_already_exist = get_variable(assign->tok, false, &local_vars);
            bool var_is_local = assign->is_local;
            bool already_exist = false;
            if (is_already_exist) {
                var_is_local = is_already_exist->is_local;
                already_exist = true;
            }
            check_assignemnt(assign->tok, assign->oper, already_exist, var_is_local);

            auto rhs = translate_to_instruction(ops, regs, local_vars, assign->rhs, return_type);

            if (is_already_exist) {
                if (!is_valid_operation(OP_CAST, is_already_exist->type, return_type, return_type)) {
                    compiler_error(assign->rhs->tok, "Cannot assign %s to %s variable\n", value_type_to_str(return_type), value_type_to_str(is_already_exist->type));
                }
            }
            if (assign && assign->rhs->type == Expr_StructInit) TODO("StructInit");

            auto& res = allocate_reg(regs, type_size(return_type));
            auto* var = get_and_add_variable(assign->tok, var_is_local, return_type, res.index, &local_vars);
            if (!var || !var->is_accesible) {
                compiler_error(assign->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(assign->tok.val));
            }
            var->type = return_type;
            ops.push(Instruction{.type = OP_STORE, .data_type = TYPE_VARIABLE, .location = assign->tok, .assign = {*var}, .reg_index = rhs.index});
            return_type = TYPE_VOID;
            return res;
        } break;

        case Expr_Call: {
            auto* call_expr = dynamic_cast<CallExpr*>(expr);
            ASSERT_NOT_NULL(call_expr);
            auto index = g_functions.find_first(DeclaredFunction{call_expr->tok.val});
            if (index != Array<DeclaredFunction>::INVALID_INDEX) {
                const u32 expected = g_functions[index].expr->args.count();
                const auto got = call_expr->args.count();
                if (expected != got) {
                    compiler_error(call_expr->tok, "Not enough arguments provided to a function '" SV_FORMAT"', expected %d, got %d\n", SV_ARG(call_expr->tok.val), expected, got);
                }
                for (auto& arg : call_expr->args) {
                    auto before = regs.count();
                    translate_to_instruction(ops, regs, local_vars, arg.expr, return_type);
                    ASSERT_NOT_NULL(before != regs.count());
                    arg.reg_index = regs.count() - 1;
                    arg.size = type_size(TYPE_I64); // TODO: need actuall size
                }
                auto& reg = allocate_reg(regs, type_size(TYPE_I64)); //TODO: need actuall type
                ops.push(Instruction{.type = OP_CALL, .data_type = TYPE_CALL, .location = call_expr->tok, .call = {call_expr->tok, call_expr->args}, .reg_index = reg.index});
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

            auto body_label = allocate_label();
            auto else_label = allocate_label();
            auto end_label = allocate_label();

            ops.push(Instruction{.type = OP_JMP_IF, .data_type = TYPE_ADDRESS, .location=if_expr->condition->tok, .addr = {body_label.addr.ip}, .reg_index = if_reg.index});
            ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .addr = {else_label.addr.ip}});
            set_label(body_label, ops);
            ops.push(body_label);
            translate_to_instruction(ops, regs, local_vars, if_expr->if_block, return_type);
            ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .addr = {end_label.addr.ip}});

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

            auto cond_label = allocate_label();
            auto block_label = allocate_label();
            auto end_label = allocate_label();

            s64 before_everything = ops.count();
            auto left_reg = translate_to_instruction(ops, regs, local_vars, for_expr->left_cond, return_type);

            set_label(cond_label, ops);
            ops.push(cond_label);
            auto right_reg = translate_to_instruction(ops, regs, local_vars, for_expr->right_cond, return_type);
            auto& res = allocate_reg(regs, type_size(TYPE_BOOL));

            auto compare_type = for_expr->is_inclusive ? OP_GREATER_EQUALS : OP_GREATER;
            ops.push(Instruction{.type = compare_type, .op = {compare_type, left_reg.index, right_reg.index}, .reg_index = res.index, .is_visited = true,});
            ops.push(Instruction{.type = OP_JMP_IF, .data_type = TYPE_ADDRESS, .location=for_expr->tok, .addr = {block_label.addr.ip}, .reg_index = res.index, .is_visited = true,});
            ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .addr = {end_label.addr.ip}, .is_visited = true,});

            auto before_block = ops.count();
            set_label(block_label, ops);
            ops.push(block_label);
            translate_to_instruction(ops, regs, local_vars, for_expr->block, return_type);
            if (before_block != ops.count()) {
                ops.push(Instruction{.type = OP_INC, .op = {OP_INC, left_reg.index}, .is_visited = true,});
                ops.push(Instruction{.type = OP_JMP, .data_type = TYPE_ADDRESS, .addr = {cond_label.addr.ip}, .is_visited = true,});
                set_label(end_label, ops);
                ops.push(end_label);
            } else {
                // Reset all operations if block is empty
                ops.set_count(before_everything);
            }
            return_type = TYPE_VOID;
        } break;

        case Expr_StructInit: {
            auto* struct_expr = dynamic_cast<StructInitExpr*>(expr);
            ASSERT_NOT_NULL(struct_expr);
            if (!g_structs.contains(DeclaredStruct{struct_expr->tok.val})) {
                compiler_error(struct_expr->tok, "Use of undeclared struct: '" SV_FORMAT "'\n", SV_ARG(struct_expr->tok.val));
            }
            // ops.push(Instruction{.type = OP_PUSH_I64, .data_type = TYPE_LITERAL, .location = struct_expr->tok, .int_val = {0}});
            return_type = TYPE_VOID;
        } break;

        case Expr_MemberCall: {
            auto* member_expr = dynamic_cast<MemberCallExpr*>(expr);
            ASSERT_NOT_NULL(member_expr);
            auto* var = get_variable(member_expr->tok, false, &local_vars);
            if (!var || !var->is_accesible) {
                compiler_error(member_expr->tok, "Use of undeclared variable: '" SV_FORMAT "'\n", SV_ARG(member_expr->tok.val));
            }
            ASSERT_TRUE(var->is_ptr);
            auto index = g_structs.find_first(DeclaredStruct{var->struct_name});
            bool found_member = false;
            if (!g_structs.is_valid_index(index)) {
                compiler_error(member_expr->tok, "Cannot find Struct with name: '" SV_FORMAT "'\n", SV_ARG(var->name));
            }
            auto new_var = *var;
            new_var.is_ptr = false;
            u32 offset = 0;
            for (auto& field : g_structs[index].expr->fields) {
                if (field.name == member_expr->field.val) {
                    found_member = true;
                    // new_var.offset += offset;
                    // new_var.total_size = field.size;
                    break;
                }
                offset += field.size;
            }
            if (!found_member) {
                compiler_error(member_expr->tok, "Struct '" SV_FORMAT "' doesnt contain '" SV_FORMAT "' field\n", SV_ARG(g_structs[index].name), SV_ARG(member_expr->field.val));
            }
            // ops.push(Instruction{.type = OP_LOAD, .data_type = TYPE_VARIABLE, .location = member_expr->tok, .var = new_var});
        } break;

        case Expr_Struct: {
            // probably doesnt need to exist
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
}

int main(int argc, char** argv)
{
    rebuild_itself((ExecutableOptions{.debug=true,.warnings=FlagsWarning::FATAL}), argc, argv, "ezbuild.hpp");
    //log_set_current(logger_colored);

    bool output_mode = false;
    for (int i = 1; i < argc; ++i) {
        StrView arg = argv[i];
        if (output_mode) {
            if (arg.starts_with("--") || arg.starts_with("-")) {
                log_error("Expected output file, but got: '" SV_FORMAT "'\n", SV_ARG(arg));
                exit(1);
            }
            out_path = arg;
            output_mode = false;
            continue;
        }
        if (arg == "-o") {
            output_mode = true;
        } else if (arg == "EZBUILD_REBUILT") {
            continue;
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            exit(0);
        } else {
            if (src_path.size > 0) {
                log_error("Compiler for now only supports one file compilation, but got [" SV_FORMAT ", " SV_FORMAT "]\n",
                        SV_ARG(src_path), SV_ARG(arg));
                exit(1);
            }
            src_path = arg;
        }
    }
    if (src_path.size == 0) {
        log_error("No source files was provided...\n");
        exit(1);
    }

    Lexer lexer(src_path);
    if (!lexer.tokenize()) return 1;
    g_strings.push(DeclaredString{"true\n", nullptr, 0});
    g_strings.push(DeclaredString{"false\n", nullptr, 6});
    g_functions.push(DeclaredFunction{"__entry"});

    bool display_tokens = false;
    if (display_tokens) {
        lexer._tokens.forEach([](auto& tok){
            print_token(tok);
        });
    }

    Array<Expression*> exprs;
    if (parse(lexer, exprs) && exprs.count() > 0)
    {
        Array<Instruction> ops{};
        Array<Variable> vars{};
        Array<Register> regs{};
        for (auto& expr : exprs) {
            ValueType return_type = TYPE_NOP;
            translate_to_instruction(ops, regs, vars, expr, return_type);
        }
        // eval(ops);
        compile_program(ops);
    }

    return EXIT_SUCCESS;
}

