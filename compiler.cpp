#define SL_IMPLEMENTATION
#include "deps/Sl.hpp"
#include "deps/raylib/include/raylib.h"
#include "deps/raylib/include/raymath.h"

#include <string>
#include <vector>
#include <cstdio>

using namespace Sl;

#ifdef _WIN32
#define NOGDI   // avoid windows.h GDI "Rectangle" clashing with raylib's Rectangle struct
#define NOUSER  // avoid windows.h USER "DrawText" clashing with raylib's DrawText
#endif
#define EZBUILD_IMPLEMENTATION
#include "ezbuild.hpp"
#include "compiler.hpp"

static void reset_parser_globals() {
    for (auto& f : g_functions) { f.ops.cleanup(); f.regs.cleanup(); }
    g_functions.set_count(0);
    g_strings.set_count(0);
    g_vars.set_count(0);
    g_labels.set_count(0);
    g_loop_stack.set_count(0);
    g_structs.set_count(0);
    for (auto& m : g_modules) m.imports.cleanup();
    g_modules.set_count(0);
    g_current_module_name = "";
    g_globals_size = 0;
    g_functions.push(DeclaredFunction{"__entry"});
}

#ifndef COMPILER_CLI
// ================================================================
// Visual Block Editor for Nul Language
// ================================================================

#define PORT_R     7.0f
#define HEADER_H   34.0f
#define PORT_SP    32.0f
#define PORT_TOP   (HEADER_H + PORT_SP)
#define MIN_W      140.0f
#define PAD        10.0f
#define TXT_SZ     20
#define FIELD_H    30.0f

enum VBlockType {
    VB_PRINT = 1, VB_NUMBER, VB_STRING, VB_BOOL,
    VB_VARDECL, VB_IF, VB_FOR, VB_FUNCCALL, VB_RETURN,
    VB_MAIN, VB_VARREF, VB_MATH, VB_FUNC
};

struct VBlockDef {
    std::string label;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    Color color;
    bool has_text;
    std::string def_text;
};

static const VBlockDef DEFS[] = {
    {"???",    {}, {}, GRAY,                false, ""},
    {"Print",  {"val"}, {},                  {60,120,200,255}, false, ""},
    {"Number", {}, {"out"},                  {50,170,80,255},  true,  "0"},
    {"String", {}, {"out"},                  {170,100,200,255}, true,  "sss"},
    {"Bool",   {}, {"out"},                  {200,160,40,255}, false, "true"},
    {"Var",    {"value"}, {"ref"},           {180,110,50,255}, true,  "x"},
    {"If",     {"cond"}, {"T","F"},          {200,60,60,255},  false, ""},
    {"For",    {"a","b"}, {"body"},          {60,140,200,255}, false, ""},
    {"Call",   {"arg"}, {"r"},               {140,60,180,255}, true,  "func"},
    {"Return", {"val"}, {},                  {200,60,120,255}, false, ""},
    {"Main",   {}, {}, {75,75,95,255},       false, ""},
    {"Ref",    {}, {"out"},                  {150,90,170,255}, false, ""},
    {"Math",   {"a","b"}, {"out"},           {80,200,120,255}, false, ""},
    {"Func",   {}, {}, {90,90,120,255},      true,  "func"},
};

// Port kinds
enum {
    PORT_DATA_IN = 0,
    PORT_DATA_OUT,
    PORT_FLOW_IN,
    PORT_FLOW_OUT,
};

// Data types (VT_ANY is compatible with everything)
enum VarType {
    VT_ANY = 0,
    VT_NUM,
    VT_STR,
    VT_BOOL,
};

static const char* type_name(int t) {
    if (t == VT_STR) return "string";
    if (t == VT_BOOL) return "bool";
    return "number";
}

// One incoming data source for a block's input port
struct VBlock;
struct VDataSrc {
    VBlock* src;
    int out_port;
    int in_port;
};

struct VBlock {
    int type;
    Vector2 pos;
    float w, h;
    std::string text;
    std::string choice;
    int vtype;             // declared type for VARDECL
    bool editing;
    VBlock* next;              // flow connection (logic/sequence)
    Array<VDataSrc> data_in;   // value connections feeding this block's inputs
};

struct ClipBlock {
    int type;
    Vector2 pos;
    std::string text;
    std::string choice;
    int vtype;
};

struct ClipLink { int a, b, ap, bp; };

struct Clipboard {
    std::vector<ClipBlock> blocks;
    std::vector<ClipLink> data;
    std::vector<ClipLink> flow;
};

struct GraphBlock {
    int type;
    Vector2 pos;
    float w, h;
    std::string text;
    std::string choice;
    int vtype;
    int next;
    std::vector<ClipLink> data;
};

struct GraphState {
    std::vector<GraphBlock> blocks;
    int entry;
};

struct VEditor {
    Array<VBlock*> blocks;
    VBlock* entry;
    VBlock* sel, * hov;
    Array<VBlock*> sel_list;
    bool dragging;
    Vector2 drag_origin;
    Array<Vector2> drag_start_pos;
    bool box_drag;
    Vector2 box_start;
    bool moving;
    Vector2 r_down;
    bool connecting;
    VBlock* c_blk;
    int c_kind;
    int c_port;
    Vector2 c_mpos;
    VBlock* edit_blk;
    Camera2D cam;
    Clipboard clip;
    std::vector<GraphState> undo;
    bool undo_active;
};

static VEditor ed;

static void capture_undo() {
    if (ed.undo_active) return;
    ed.undo_active = true;
    GraphState s;
    s.entry = -1;
    for (int i = 0; i < (int)ed.blocks.count(); i++) {
        auto* b = ed.blocks[i];
        GraphBlock g;
        g.type = b->type;
        g.pos = b->pos;
        g.w = b->w;
        g.h = b->h;
        g.text = b->text;
        g.choice = b->choice;
        g.vtype = b->vtype;
        g.next = -1;
        for (int j = 0; j < (int)ed.blocks.count(); j++)
            if (ed.blocks[j] == b->next) { g.next = j; break; }
        for (auto& in : b->data_in) {
            for (int j = 0; j < (int)ed.blocks.count(); j++)
                if (ed.blocks[j] == in.src) {
                    g.data.push_back(ClipLink{j, i, in.out_port, in.in_port});
                    break;
                }
        }
        if (ed.entry == b) s.entry = i;
        s.blocks.push_back(g);
    }
    if (ed.undo.size() >= 100) ed.undo.erase(ed.undo.begin());
    ed.undo.push_back(std::move(s));
}

static void do_undo() {
    if (ed.undo.empty()) return;
    GraphState s = std::move(ed.undo.back());
    ed.undo.pop_back();
    for (auto* b : ed.blocks) {
        b->data_in.cleanup();
        delete b;
    }
    ed.blocks.set_count(0);
    std::vector<VBlock*> created(s.blocks.size());
    for (size_t i = 0; i < s.blocks.size(); i++) {
        auto& g = s.blocks[i];
        VBlock* b = new VBlock{};
        b->type = g.type;
        b->pos = g.pos;
        b->w = g.w;
        b->h = g.h;
        b->text = g.text;
        b->choice = g.choice;
        b->vtype = g.vtype;
        b->next = nullptr;
        ed.blocks.push(b);
        created[i] = b;
    }
    for (size_t i = 0; i < s.blocks.size(); i++) {
        auto& g = s.blocks[i];
        if (g.next >= 0 && g.next < (int)created.size()) created[i]->next = created[g.next];
        for (auto& l : g.data)
            created[i]->data_in.push(VDataSrc{created[l.a], l.ap, l.bp});
    }
    ed.entry = (s.entry >= 0 && s.entry < (int)created.size()) ? created[s.entry] : nullptr;
    ed.sel_list.set_count(0);
    ed.sel = nullptr;
    ed.hov = nullptr;
    ed.edit_blk = nullptr;
    ed.c_blk = nullptr;
    ed.connecting = false;
    ed.dragging = false;
    ed.box_drag = false;
    ed.undo_active = false;
}

static float bheight(const VBlock& b) {
    if (b.type == VB_MAIN) return 96.0f;
    auto& d = DEFS[b.type];
    int mx = d.inputs.size() > d.outputs.size() ? (int)d.inputs.size() : (int)d.outputs.size();
    float h = (mx > 0) ? PORT_TOP + (mx - 1) * PORT_SP + PORT_SP / 2.0f + PAD
                       : HEADER_H + PAD;
    if (d.has_text) h += FIELD_H + 4;
    if (b.type == VB_VARDECL) h += FIELD_H + 4;
    if (b.type == VB_VARREF) h += FIELD_H + 4;
    if (b.type == VB_BOOL || b.type == VB_MATH) h += FIELD_H + 4;
    return h;
}

static float bwidth(const VBlock& b) {
    if (b.type == VB_MAIN) return 220.0f;
    auto& d = DEFS[b.type];
    float w = MIN_W;
    float lw = (float)MeasureText(d.label.c_str(), TXT_SZ + 2) + 20;
    if (lw > w) w = lw;
    for (auto& name : d.inputs) {
        float nw = (float)MeasureText(name.c_str(), 11) + PORT_R * 2 + 16;
        if (nw > w) w = nw;
    }
    for (auto& name : d.outputs) {
        float nw = (float)MeasureText(name.c_str(), 11) + PORT_R * 2 + 16;
        if (nw > w) w = nw;
    }
    if (d.has_text) {
        const char* measure = b.text.c_str();
        if (b.type == VB_VARREF) measure = b.choice.c_str();
        float tw = (float)MeasureText(measure, TXT_SZ) + PAD * 2 + 8;
        if (tw > w) w = tw;
    }
    return w;
}

static Vector2 ppos(const VBlock& b, int pi, bool out) {
    float cy = b.pos.y + PORT_TOP + pi * PORT_SP;
    if (out) return {b.pos.x + b.w, cy};
    return {b.pos.x, cy};
}

static Vector2 fppos(const VBlock& b, bool out) {
    if (out) return {b.pos.x + b.w, b.pos.y + HEADER_H + PORT_R};
    return {b.pos.x, b.pos.y + HEADER_H + PORT_R};
}

static Vector2 port_pos(const VBlock& b, int kind, int idx) {
    if (kind == PORT_FLOW_IN)  return fppos(b, false);
    if (kind == PORT_FLOW_OUT) return fppos(b, true);
    return ppos(b, idx, kind == PORT_DATA_OUT);
}

static VBlock* find_port(Vector2 m, int& kind, int& idx) {
    for (int i = (int)ed.blocks.count() - 1; i >= 0; i--) {
        auto* b = ed.blocks[i];
        auto& d = DEFS[b->type];
        for (int j = 0; j < (int)d.inputs.size(); j++) {
            if (Vector2Distance(m, ppos(*b, j, false)) < PORT_R + 4) {
                kind = PORT_DATA_IN; idx = j; return b;
            }
        }
        for (int j = 0; j < (int)d.outputs.size(); j++) {
            if (Vector2Distance(m, ppos(*b, j, true)) < PORT_R + 4) {
                kind = PORT_DATA_OUT; idx = j; return b;
            }
        }
        if (Vector2Distance(m, fppos(*b, false)) < PORT_R + 4) {
            kind = PORT_FLOW_IN; idx = 0; return b;
        }
        if (Vector2Distance(m, fppos(*b, true)) < PORT_R + 4) {
            kind = PORT_FLOW_OUT; idx = 0; return b;
        }
    }
    return nullptr;
}

static VBlock* find_block(Vector2 m) {
    for (int i = (int)ed.blocks.count() - 1; i >= 0; i--) {
        auto* b = ed.blocks[i];
        if (m.x >= b->pos.x && m.x <= b->pos.x + b->w &&
            m.y >= b->pos.y && m.y <= b->pos.y + b->h)
            return b;
    }
    return nullptr;
}

static Rectangle text_rect(const VBlock& b) {
    float fy = b.pos.y + b.h - FIELD_H - PAD / 2.0f;
    if (b.type == VB_VARDECL) {
        float f2 = b.pos.y + b.h - 2 * FIELD_H - PAD - 4.0f;
        return {b.pos.x + PAD, f2, b.w - 2 * PAD, FIELD_H};
    }
    return {b.pos.x + PAD, fy, b.w - 2 * PAD, FIELD_H};
}

static Rectangle var_type_rect(const VBlock& b) {
    float fy = b.pos.y + b.h - FIELD_H - PAD / 2.0f;
    return {b.pos.x + PAD, fy, b.w - 2 * PAD, FIELD_H};
}

static Rectangle dropdown_rect(const VBlock& b) {
    if (b.type == VB_VARDECL) return var_type_rect(b);
    return text_rect(b);
}

static void start_edit(VBlock* b) {
    if (!DEFS[b->type].has_text) return;
    capture_undo();
    while (GetCharPressed() > 0) {}
    b->editing = true;
    ed.edit_blk = b;
}

static void cycle_type(VBlock* b) {
    capture_undo();
    if (b->vtype == VT_NUM) b->vtype = VT_STR;
    else if (b->vtype == VT_STR) b->vtype = VT_BOOL;
    else b->vtype = VT_NUM;
}

static void cycle_varref(VBlock* b) {
    capture_undo();
    std::vector<std::string> names;
    for (auto* p : ed.blocks)
        if (p->type == VB_VARDECL) names.push_back(p->text);
    if (names.empty()) { b->choice.clear(); return; }
    int idx = -1;
    for (int i = 0; i < (int)names.size(); i++)
        if (names[i] == b->choice) { idx = i; break; }
    b->choice = names[(idx + 1) % (int)names.size()];
}

static const char* MATH_OPS[] = {"+", "-", "*", "/"};

static void cycle_bool(VBlock* b) {
    capture_undo();
    b->text = (b->text == "false") ? "true" : "false";
}

static void cycle_math(VBlock* b) {
    capture_undo();
    int idx = 0;
    for (int i = 0; i < 4; i++)
        if (b->choice == MATH_OPS[i]) { idx = i; break; }
    b->choice = MATH_OPS[(idx + 1) % 4];
}

static int block_out_type(const VBlock* b, int out_port) {
    (void)out_port;
    switch (b->type) {
        case VB_NUMBER: return VT_NUM;
        case VB_STRING: return VT_STR;
        case VB_BOOL:   return VT_BOOL;
        case VB_MATH:   return VT_NUM;
        case VB_VARDECL: return b->vtype;
        case VB_VARREF:
            for (auto* p : ed.blocks)
                if (p->type == VB_VARDECL && p->text == b->choice) return p->vtype;
            return VT_ANY;
        default: return VT_ANY;   // If T/F, For body, Call r, Main
    }
}

static int block_in_type(const VBlock* b, int in_port) {
    (void)in_port;
    switch (b->type) {
        case VB_VARDECL: return b->vtype;
        case VB_IF:      return VT_BOOL;
        case VB_FOR:     return VT_NUM;
        case VB_MATH:    return VT_NUM;
        default:         return VT_ANY;   // Print.val, Call, Return
    }
}

static bool types_compatible(int src, int dst) {
    if (src == VT_ANY || dst == VT_ANY) return true;
    return src == dst;
}

// ================================================================
// Nul source generation: walk blocks from Main, emit Nul code
// ================================================================

static std::string indent_str(int depth) {
    return std::string((size_t)depth * 4, ' ');
}

static std::string nul_escape(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\";
        else if (c == '\n') r += "\n";
        else r += c;
    }
    return r;
}

static VBlock* data_src(VBlock* b, int in_port) {
    for (auto& s : b->data_in)
        if (s.in_port == in_port) return s.src;
    return nullptr;
}

static std::string expr_of(VBlock* b, int in_port) {
    VBlock* src = data_src(b, in_port);
    if (!src) return "";
    switch (src->type) {
        case VB_NUMBER:  return src->text.empty() ? "0" : src->text;
        case VB_STRING:  return "\"" + nul_escape(src->text) + "\"";
        case VB_BOOL:    return (src->text == "false") ? "false" : "true";
        case VB_VARREF:  return src->choice.empty() ? "0" : src->choice;
        case VB_VARDECL: return src->text.empty() ? "0" : src->text;
        case VB_MATH: {
            std::string a = expr_of(src, 0);
            std::string c = expr_of(src, 1);
            if (a.empty()) a = "0";
            if (c.empty()) c = "0";
            return "(" + a + " " + (src->choice.empty() ? "+" : src->choice) + " " + c + ")";
        }
        case VB_FUNCCALL: {
            std::string a = expr_of(src, 0);
            std::string name = src->text.empty() ? "func" : src->text;
            return name + "(" + (a.empty() ? "0" : a) + ")";
        }
        default:         return "0";
    }
}

static std::string var_default(VBlock* b) {
    if (b->vtype == VT_STR) return "\"\"";
    if (b->vtype == VT_BOOL) return "false";
    return "0";
}

static int g_loop_id = 0;
static std::string gen_chain(VBlock* b, int depth);

static std::string gen_statement(VBlock* b, int depth) {
    std::string ind = indent_str(depth);
    std::string out;
    switch (b->type) {
        case VB_PRINT: {
            std::string e = expr_of(b, 0);
            if (!e.empty()) out += ind + "print(" + e + ");\n";
        } break;
        case VB_VARDECL: {
            std::string name = b->text.empty() ? "x" : b->text;
            std::string e = expr_of(b, 0);
            if (e.empty()) e = var_default(b);
            out += ind + name + " := " + e + ";\n";
        } break;
        case VB_IF: {
            std::string e = expr_of(b, 0);
            if (e.empty()) e = "false";
            out += ind + "if (" + e + ") {\n";
            out += gen_chain(b->next, depth + 1);
            out += ind + "}\n";
        } break;
        case VB_FOR: {
            std::string a = expr_of(b, 0);
            std::string c = expr_of(b, 1);
            if (a.empty()) a = "0";
            if (c.empty()) c = "0";
            std::string lv = "__i" + std::to_string(g_loop_id++);
            out += ind + "for " + lv + " := " + a + " .. " + c + " {\n";
            out += gen_chain(b->next, depth + 1);
            out += ind + "}\n";
        } break;
        case VB_RETURN: {
            std::string e = expr_of(b, 0);
            out += ind + (e.empty() ? "return;\n" : "return " + e + ";\n");
        } break;
        case VB_FUNCCALL: {
            bool consumed = false;
            for (auto* p : ed.blocks) {
                if (p == b) continue;
                for (auto& s : p->data_in)
                    if (s.src == b) { consumed = true; break; }
                if (consumed) break;
            }
            if (!consumed) {
                std::string a = expr_of(b, 0);
                std::string name = b->text.empty() ? "func" : b->text;
                out += ind + name + "(" + (a.empty() ? "0" : a) + ");\n";
            }
        } break;
        default: break;  // Number/String/Bool/VarRef/Call/Main/Func are expressions or flow roots, not statements
    }
    return out;
}

static std::string gen_chain(VBlock* b, int depth) {
    std::string out;
    int guard = 0;
    while (b && guard++ < 1000) {
        out += gen_statement(b, depth);
        b = b->next;
    }
    return out;
}

static std::string gen_nul_source() {
    g_loop_id = 0;
    std::string out;
    bool has_main = false;
    for (auto* b : ed.blocks)
        if (b->type == VB_MAIN) has_main = true;
    if (!has_main) out += "fn main()\n{\n}\n\n";
    for (auto* b : ed.blocks) {
        if (b->type != VB_MAIN && b->type != VB_FUNC) continue;
        std::string name = b->type == VB_MAIN ? "main" : (b->text.empty() ? "func" : b->text);
        out += "fn " + name + "()\n{\n";
        out += gen_chain(b->next, 1);
        VBlock* last = b->next;
        int guard = 0;
        while (last && last->next && guard++ < 1000)
            last = last->next;
        if (!last || last->type != VB_RETURN)
            out += "    return;\n";
        out += "}\n\n";
    }
    return out;
}

// ================================================================
// Compile the graph to test.asm via the Nul parser (compiler.hpp)
// ================================================================

static bool write_file_text(const char* path, const std::string& s) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(s.data(), 1, s.size(), f);
    fclose(f);
    return true;
}

static std::string g_status = "";

static void compile_graph_to_asm() {
    std::string nul = gen_nul_source();
    if (!write_file_text("test.nul", nul)) {
        g_status = "Failed to write test.nul";
        return;
    }

    out_path = SV_LIT("test");
    src_path = SV_LIT("test.nul");
    reset_parser_globals();

    Lexer lexer(SV_LIT(""));
    lexer._source.append(nul.c_str());
    lexer._source.append_null(false);
    src_content = lexer._source.data();

    if (!lexer.tokenize()) { g_status = "Lexer failed"; return; }

    Array<Expression*> exprs;
    if (!parse(lexer, exprs) || exprs.count() == 0) { g_status = "Parse failed"; return; }

    Array<Instruction> global_ops{};
    Array<Variable> vars{};
    Array<VirtualReg> regs{};
    for (auto& expr : exprs) {
        ValueType rt = TYPE_NOP;
        translate_to_instruction(global_ops, regs, vars, expr, rt);
    }

    if (compile_program(global_ops, regs))
        g_status = "Compiled -> test.asm";
    else
        g_status = "Compile failed";
}

static bool is_selected(VBlock* b) {
    for (auto* p : ed.sel_list)
        if (p == b) return true;
    return false;
}

static void clear_selection() {
    ed.sel_list.set_count(0);
    ed.sel = nullptr;
}

static void add_selection(VBlock* b) {
    ed.sel_list.push(b);
    ed.sel = b;
}

static void remove_selection(VBlock* b) {
    for (int i = (int)ed.sel_list.count() - 1; i >= 0; i--)
        if (ed.sel_list[i] == b) { ed.sel_list.remove_unordered(i); break; }
    if (ed.sel == b) ed.sel = ed.sel_list.count() ? ed.sel_list[0] : nullptr;
}

static int sel_index(VBlock* b) {
    for (int i = 0; i < (int)ed.sel_list.count(); i++)
        if (ed.sel_list[i] == b) return i;
    return -1;
}

static void copy_selection() {
    ed.clip.blocks.clear();
    ed.clip.data.clear();
    ed.clip.flow.clear();
    for (auto* b : ed.sel_list)
        ed.clip.blocks.push_back(ClipBlock{b->type, b->pos, b->text, b->choice, b->vtype});
    for (auto* b : ed.sel_list) {
        int fi = sel_index(b);
        if (fi < 0) continue;
        for (auto& s : b->data_in) {
            int si = sel_index(s.src);
            if (si >= 0) ed.clip.data.push_back(ClipLink{si, fi, s.out_port, s.in_port});
        }
        if (b->next) {
            int ti = sel_index(b->next);
            if (ti >= 0) ed.clip.flow.push_back(ClipLink{fi, ti, 0, 0});
        }
    }
}

static void paste_clipboard() {
    size_t n = ed.clip.blocks.size();
    if (n == 0) return;
    capture_undo();
    float minx = ed.clip.blocks[0].pos.x, miny = ed.clip.blocks[0].pos.y;
    for (auto& c : ed.clip.blocks) {
        if (c.pos.x < minx) minx = c.pos.x;
        if (c.pos.y < miny) miny = c.pos.y;
    }
    Vector2 base = GetScreenToWorld2D(GetMousePosition(), ed.cam);
    std::vector<VBlock*> created(n);
    clear_selection();
    for (size_t i = 0; i < n; i++) {
        auto& c = ed.clip.blocks[i];
        VBlock* nb = new VBlock{};
        nb->type = c.type;
        nb->text = c.text;
        nb->choice = c.choice;
        nb->vtype = c.vtype;
        nb->pos = {c.pos.x - minx + base.x, c.pos.y - miny + base.y};
        nb->w = bwidth(*nb);
        nb->h = bheight(*nb);
        ed.blocks.push(nb);
        if (!ed.entry) ed.entry = nb;
        created[i] = nb;
        add_selection(nb);
    }
    for (auto& l : ed.clip.data)
        created[l.b]->data_in.push(VDataSrc{created[l.a], l.ap, l.bp});
    for (auto& l : ed.clip.flow)
        created[l.a]->next = created[l.b];
}

static void del_block(VBlock* b);

static void delete_selected() {
    capture_undo();
    std::vector<VBlock*> to_del;
    for (auto* p : ed.sel_list) to_del.push_back(p);
    if (to_del.empty() && ed.sel) to_del.push_back(ed.sel);
    for (auto* b : to_del) del_block(b);
    clear_selection();
}

static void del_block(VBlock* b) {
    if (!b || b == ed.entry) return;
    for (auto* p : ed.blocks)
        if (p != b && p->next == b) p->next = nullptr;
    for (auto* p : ed.blocks) {
        if (p == b) continue;
        for (int i = (int)p->data_in.count() - 1; i >= 0; i--)
            if (p->data_in[i].src == b) p->data_in.remove_unordered(i);
    }
    if (ed.entry == b) ed.entry = nullptr;
    if (ed.sel == b) ed.sel = nullptr;
    for (int i = (int)ed.sel_list.count() - 1; i >= 0; i--)
        if (ed.sel_list[i] == b) ed.sel_list.remove_unordered(i);
    if (ed.hov == b) ed.hov = nullptr;
    if (ed.edit_blk == b) ed.edit_blk = nullptr;
    if (ed.c_blk == b) { ed.connecting = false; ed.c_blk = nullptr; }
    b->data_in.cleanup();
    for (int i = 0; i < (int)ed.blocks.count(); i++) {
        if (ed.blocks[i] == b) { ed.blocks.remove_unordered(i); break; }
    }
    for (auto* p : ed.blocks) {
        if (p->type != VB_VARREF) continue;
        bool found = false;
        for (auto* q : ed.blocks)
            if (q->type == VB_VARDECL && q->text == p->choice) { found = true; break; }
        if (!found) p->choice.clear();
    }
    delete b;
}

static void delete_conn(VBlock* b, int kind, int idx) {
    if (kind == PORT_FLOW_OUT) {
        b->next = nullptr;
    } else if (kind == PORT_FLOW_IN) {
        for (auto* p : ed.blocks)
            if (p != b && p->next == b) p->next = nullptr;
    } else if (kind == PORT_DATA_IN) {
        for (int i = (int)b->data_in.count() - 1; i >= 0; i--)
            if (b->data_in[i].in_port == idx) b->data_in.remove_unordered(i);
    } else { // PORT_DATA_OUT
        for (auto* p : ed.blocks) {
            if (p == b) continue;
            for (int i = (int)p->data_in.count() - 1; i >= 0; i--)
                if (p->data_in[i].src == b && p->data_in[i].out_port == idx)
                    p->data_in.remove_unordered(i);
        }
    }
}

static int block_count() { return (int)ed.blocks.count(); }

static void bezier_line(Vector2 a, Vector2 b, Color color) {
    float dx = fabsf(b.x - a.x);
    float t = fmaxf(50.0f, dx * 0.4f);
    Vector2 prev = a;
    for (int i = 1; i <= 16; i++) {
        float s = (float)i / 16;
        float s2 = s * s, s3 = s2 * s;
        float m = 1 - s, m2 = m * m, m3 = m2 * m;
        Vector2 p = {
            m3*a.x + 3*m2*s*(a.x+t) + 3*m*s2*(b.x-t) + s3*b.x,
            m3*a.y + 3*m2*s*a.y + 3*m*s2*b.y + s3*b.y
        };
        DrawLineV(prev, p, color);
        prev = p;
    }
}

static void draw_block(VBlock* b) {
    auto& d = DEFS[b->type];
    Rectangle r = {b->pos.x, b->pos.y, b->w, b->h};
    bool sel = is_selected(b);
    Vector2 mp = GetScreenToWorld2D(GetMousePosition(), ed.cam);

    Color bg = d.color;
    bg.a = sel ? 255 : 200;
    DrawRectangleRounded(r, 0.15f, 6, bg);

    Rectangle hr = {b->pos.x, b->pos.y, b->w, HEADER_H};
    Color hc = {(unsigned char)(d.color.r*0.5f), (unsigned char)(d.color.g*0.5f), (unsigned char)(d.color.b*0.5f), 255};
    DrawRectangleRounded(hr, 0.15f, 6, hc);
    DrawRectangle((int)b->pos.x, (int)(b->pos.y + HEADER_H - 6), (int)b->w, 6, hc);

    int tw = MeasureText(d.label.c_str(), TXT_SZ + 4);
    DrawText(d.label.c_str(), (int)(b->pos.x + b->w/2 - tw/2), (int)(b->pos.y + 7), TXT_SZ + 4, WHITE);

    for (int i = 0; i < (int)d.inputs.size(); i++) {
        Vector2 pp = ppos(*b, i, false);
        bool ph = Vector2Distance(mp, pp) < PORT_R + 4;
        DrawCircleV(pp, PORT_R, (Color){40, 40, 45, 255});
        DrawCircleV(pp, PORT_R - 2, ph ? YELLOW : (Color){180, 180, 190, 255});
        auto& name = d.inputs[i];
        if (!name.empty())
            DrawText(name.c_str(), (int)(pp.x + PORT_R + 6), (int)(pp.y - 7), 14, (Color){200, 200, 210, 255});
    }
    for (int i = 0; i < (int)d.outputs.size(); i++) {
        Vector2 pp = ppos(*b, i, true);
        bool ph = Vector2Distance(mp, pp) < PORT_R + 4;
        DrawCircleV(pp, PORT_R, (Color){40, 40, 45, 255});
        DrawCircleV(pp, PORT_R - 2, ph ? YELLOW : d.color);
        auto& name = d.outputs[i];
        if (!name.empty()) {
            int nw = MeasureText(name.c_str(), 14);
            DrawText(name.c_str(), (int)(pp.x - PORT_R - 6 - nw), (int)(pp.y - 7), 14, (Color){200, 200, 210, 255});
        }
    }

    Vector2 fin = fppos(*b, false), fout = fppos(*b, true);
    bool fin_h = Vector2Distance(mp, fin) < PORT_R + 4;
    bool fout_h = Vector2Distance(mp, fout) < PORT_R + 4;
    DrawCircleV(fin, PORT_R, (Color){20, 30, 40, 255});
    DrawCircleV(fin, PORT_R - 2, fin_h ? YELLOW : (Color){90, 190, 210, 255});
    DrawCircleV(fout, PORT_R, (Color){20, 30, 40, 255});
    DrawCircleV(fout, PORT_R - 2, fout_h ? YELLOW : (Color){90, 190, 210, 255});

    if (b->type == VB_VARDECL) {
        Rectangle tr = var_type_rect(*b);
        bool th = CheckCollisionPointRec(mp, tr);
        DrawRectangleRounded(tr, 0.2f, 4, (Color){30, 30, 36, 255});
        DrawRectangleRoundedLines(tr, 0.2f, 4, th ? YELLOW : (Color){100, 100, 110, 255});
        DrawText(type_name(b->vtype), (int)(tr.x + 6), (int)(tr.y + 4), TXT_SZ - 2, WHITE);
        Vector2 c = {tr.x + tr.width - 14, tr.y + tr.height / 2.0f};
        DrawTriangle({c.x - 5, c.y - 3}, {c.x + 5, c.y - 3}, {c.x, c.y + 4}, (Color){180, 180, 190, 255});
        Rectangle nr = text_rect(*b);
        Color fc = b->editing ? (Color){50, 50, 55, 255} : (Color){35, 35, 40, 255};
        DrawRectangleRounded(nr, 0.2f, 4, fc);
        DrawRectangleRoundedLines(nr, 0.2f, 4, (Color){100, 100, 110, 255});
        DrawText(b->text.c_str(), (int)(nr.x + 6), (int)(nr.y + 4), TXT_SZ, WHITE);
        if (b->editing) {
            int cx = (int)(nr.x + 6 + MeasureText(b->text.c_str(), TXT_SZ));
            DrawLine(cx, (int)nr.y + 4, cx, (int)(nr.y + 4 + TXT_SZ), WHITE);
        }
    } else if (b->type == VB_VARREF || b->type == VB_BOOL || b->type == VB_MATH) {
        Rectangle fr = dropdown_rect(*b);
        bool dh = CheckCollisionPointRec(mp, fr);
        DrawRectangleRounded(fr, 0.2f, 4, (Color){40, 40, 48, 255});
        DrawRectangleRoundedLines(fr, 0.2f, 4, dh ? YELLOW : (Color){100, 100, 110, 255});
        const char* cur = b->type == VB_VARREF ? (b->choice.empty() ? "select variable..." : b->choice.c_str())
                        : b->type == VB_MATH ? (b->choice.empty() ? "+" : b->choice.c_str())
                        : b->text.c_str();
        DrawText(cur, (int)(fr.x + 6), (int)(fr.y + 4), TXT_SZ - 2, WHITE);
        Vector2 c = {fr.x + fr.width - 14, fr.y + fr.height / 2.0f};
        DrawTriangle({c.x - 5, c.y - 3}, {c.x + 5, c.y - 3}, {c.x, c.y + 4}, (Color){180, 180, 190, 255});
    } else if (d.has_text) {
        float fy = b->pos.y + b->h - FIELD_H - PAD/2;
        Rectangle fr = {b->pos.x + PAD, fy, b->w - 2*PAD, FIELD_H};
        Color fc = b->editing ? (Color){50, 50, 55, 255} : (Color){35, 35, 40, 255};
        DrawRectangleRounded(fr, 0.2f, 4, fc);
        DrawRectangleRoundedLines(fr, 0.2f, 4, (Color){100, 100, 110, 255});
        DrawText(b->text.c_str(), (int)(fr.x + 6), (int)(fr.y + 4), TXT_SZ, WHITE);
        if (b->editing) {
            int cx = (int)(fr.x + 6 + MeasureText(b->text.c_str(), TXT_SZ));
            DrawLine(cx, (int)fr.y + 4, cx, (int)(fr.y + 4 + TXT_SZ), WHITE);
        }
    }

    if (sel) DrawRectangleRoundedLines(r, 0.15f, 6, WHITE);
}

static void render() {
    ClearBackground((Color){25, 25, 30, 255});

    BeginMode2D(ed.cam);

    auto line_color = (Color){35, 35, 40, 255};
    Vector2 tl = GetScreenToWorld2D({0, 0}, ed.cam);
    Vector2 br = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, ed.cam);
    for (float x = floorf(tl.x / 40.0f) * 40.0f; x < br.x; x += 40)
        DrawLine(x, tl.y, x, br.y, line_color);
    for (float y = floorf(tl.y / 40.0f) * 40.0f; y < br.y; y += 40)
        DrawLine(tl.x, y, br.x, y, line_color);

    auto flow_color = (Color){150, 130, 230, 220};
    auto data_color = (Color){220, 200, 160, 200};

    for (auto* b : ed.blocks) {
        if (b->next)
            bezier_line(fppos(*b, true), fppos(*b->next, false), flow_color);
        for (auto& s : b->data_in)
            bezier_line(ppos(*s.src, s.out_port, true), ppos(*b, s.in_port, false), data_color);
    }

    for (auto* b : ed.blocks)
        draw_block(b);

    if (ed.connecting && ed.c_blk) {
        Vector2 sp = port_pos(*ed.c_blk, ed.c_kind, ed.c_port);
        Color col = (ed.c_kind == PORT_FLOW_IN || ed.c_kind == PORT_FLOW_OUT) ? flow_color : data_color;
        if (ed.c_kind == PORT_DATA_OUT || ed.c_kind == PORT_DATA_IN) {
            int k2, i2;
            VBlock* hb = find_port(ed.c_mpos, k2, i2);
            if (hb && hb != ed.c_blk) {
                bool ok = (ed.c_kind == PORT_DATA_OUT && k2 == PORT_DATA_IN)
                    ? types_compatible(block_out_type(ed.c_blk, ed.c_port), block_in_type(hb, i2))
                    : (ed.c_kind == PORT_DATA_IN && k2 == PORT_DATA_OUT)
                    ? types_compatible(block_out_type(hb, i2), block_in_type(ed.c_blk, ed.c_port))
                    : true;
                if (!ok) col = RED;
            }
        }
        bezier_line(sp, ed.c_mpos, col);
        DrawCircleV(ed.c_mpos, 5, (Color){255, 255, 100, 200});
    }

    if (ed.box_drag) {
        Vector2 b2 = GetScreenToWorld2D(GetMousePosition(), ed.cam);
        Rectangle box = {fminf(ed.box_start.x, b2.x), fminf(ed.box_start.y, b2.y),
                         fabsf(b2.x - ed.box_start.x), fabsf(b2.y - ed.box_start.y)};
        DrawRectangle(box.x, box.y, box.width, box.height, (Color){120, 200, 255, 25});
        DrawRectangleLinesEx(box, 1.5f, (Color){120, 200, 255, 220});
    }

    EndMode2D();

    DrawText("[1-9] Blocks  [0] Var Ref  [M] Math  [F] Func  [R] Return  [K] Compile  [Ctrl+Z/C/V] Undo/Copy/Paste  [Shift+Click] Multi  [DEL] Delete  [E] Edit  [Scroll] Zoom",
             10, GetScreenHeight() - 30, 18, (Color){120, 120, 130, 255});
    DrawText(TextFormat("Blocks: %d  Zoom: %.0f%%", block_count(), ed.cam.zoom * 100),
             10, GetScreenHeight() - 54, 18, (Color){100, 100, 110, 255});
    if (!g_status.empty()) {
        DrawText(g_status.c_str(), 10, GetScreenHeight() - 80, 18, GREEN);
    }
}

Vector2 last_mp = {};

static VBlock* create_block(int type, Vector2 pos) {
    capture_undo();
    VBlock* b = new VBlock{};
    b->type = type;
    b->text = DEFS[type].def_text;
    if (type == VB_VARDECL) b->vtype = VT_NUM;
    if (type == VB_MATH) b->choice = "+";
    b->w = bwidth(*b);
    b->h = bheight(*b);
    b->pos = {pos.x - b->w / 2.0f, pos.y - HEADER_H / 2.0f};
    ed.blocks.push(b);
    if (!ed.entry) ed.entry = b;
    clear_selection();
    add_selection(b);
    return b;
}

static void input() {
    ed.undo_active = false;

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        ed.moving = true;
        ed.r_down = GetMousePosition();
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
        ed.moving = false;
        if (!ed.edit_blk && Vector2Distance(GetMousePosition(), ed.r_down) < 6.0f) {
            int k, pi;
            Vector2 wmp = GetScreenToWorld2D(GetMousePosition(), ed.cam);
            VBlock* b = find_port(wmp, k, pi);
            if (b) {
                capture_undo();
                delete_conn(b, k, pi);
            }
        }
    }

    Vector2 mp = GetScreenToWorld2D(GetMousePosition(), ed.cam);

    if (ed.moving) {
        auto mouse_delta = mp - last_mp;
        ed.cam.offset.x += mouse_delta.x * ed.cam.zoom;
        ed.cam.offset.y += mouse_delta.y * ed.cam.zoom;
        mp = GetScreenToWorld2D(GetMousePosition(), ed.cam);
    }
    
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), ed.cam);
        ed.cam.zoom *= (1.0f + wheel * 0.15f);
        if (ed.cam.zoom < 0.1f) ed.cam.zoom = 0.1f;
        if (ed.cam.zoom > 5.0f) ed.cam.zoom = 5.0f;
        Vector2 mouseScreen = GetWorldToScreen2D(mouseWorld, ed.cam);
        ed.cam.offset.x += GetMousePosition().x - mouseScreen.x;
        ed.cam.offset.y += GetMousePosition().y - mouseScreen.y;
        mp = GetScreenToWorld2D(GetMousePosition(), ed.cam);
    }
    
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && ed.box_drag) {
        Rectangle box = {fminf(ed.box_start.x, mp.x), fminf(ed.box_start.y, mp.y),
                         fabsf(mp.x - ed.box_start.x), fabsf(mp.y - ed.box_start.y)};
        if (box.width > 4.0f || box.height > 4.0f) {
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            if (!shift) clear_selection();
            for (auto* b : ed.blocks) {
                Rectangle r = {b->pos.x, b->pos.y, b->w, b->h};
                if (CheckCollisionRecs(box, r)) add_selection(b);
            }
        }
        ed.box_drag = false;
    }

    ed.hov = find_block(mp);

    last_mp = mp;
    if (ed.edit_blk) {
        auto* b = ed.edit_blk;
        if (!b->editing) { ed.edit_blk = nullptr; return; }
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key < 127) b->text.push_back((char)key);
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !b->text.empty()) b->text.pop_back();
        b->w = bwidth(*b);
        b->h = bheight(*b);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
            b->editing = false; ed.edit_blk = nullptr;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Rectangle r = {b->pos.x, b->pos.y, b->w, b->h};
            if (!CheckCollisionPointRec(mp, r)) { b->editing = false; ed.edit_blk = nullptr; }
        }
        return;
    }

    if (ed.connecting) {
        ed.c_mpos = mp;
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            int k2, i2;
            VBlock* hb = find_port(mp, k2, i2);
            if (hb && ed.c_blk && hb != ed.c_blk) {
                if (ed.c_kind == PORT_FLOW_OUT && k2 == PORT_FLOW_IN) {
                    for (auto* p : ed.blocks)
                        if (p != ed.c_blk && p->next == hb) p->next = nullptr;
                    ed.c_blk->next = hb;
                } else if (ed.c_kind == PORT_FLOW_IN && k2 == PORT_FLOW_OUT) {
                    for (auto* p : ed.blocks)
                        if (p != ed.c_blk && p->next == ed.c_blk) p->next = nullptr;
                    hb->next = ed.c_blk;
                } else if (ed.c_kind == PORT_DATA_OUT && k2 == PORT_DATA_IN) {
                    if (types_compatible(block_out_type(ed.c_blk, ed.c_port), block_in_type(hb, i2))) {
                        for (int q = (int)hb->data_in.count() - 1; q >= 0; q--)
                            if (hb->data_in[q].in_port == i2) hb->data_in.remove_unordered(q);
                        hb->data_in.push(VDataSrc{ed.c_blk, ed.c_port, i2});
                    }
                } else if (ed.c_kind == PORT_DATA_IN && k2 == PORT_DATA_OUT) {
                    if (types_compatible(block_out_type(hb, i2), block_in_type(ed.c_blk, ed.c_port))) {
                        for (int q = (int)ed.c_blk->data_in.count() - 1; q >= 0; q--)
                            if (ed.c_blk->data_in[q].in_port == ed.c_port) ed.c_blk->data_in.remove_unordered(q);
                        ed.c_blk->data_in.push(VDataSrc{hb, i2, ed.c_port});
                    }
                }
            }
            ed.connecting = false;
            ed.c_blk = nullptr;
        }
        return;
    }

    if (ed.dragging) {
        Vector2 delta = {mp.x - ed.drag_origin.x, mp.y - ed.drag_origin.y};
        for (int i = 0; i < (int)ed.sel_list.count(); i++)
            if (ed.sel_list[i])
                ed.sel_list[i]->pos = {ed.drag_start_pos[i].x + delta.x, ed.drag_start_pos[i].y + delta.y};
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) ed.dragging = false;
        return;
    }

    if (IsKeyPressed(KEY_ZERO)) {
        create_block(VB_VARREF, mp);
        return;
    }
    if (IsKeyPressed(KEY_M)) {
        create_block(VB_MATH, mp);
        return;
    }
    if (IsKeyPressed(KEY_F)) {
        create_block(VB_FUNC, mp);
        return;
    }
    if (IsKeyPressed(KEY_R)) {
        create_block(VB_RETURN, mp);
        return;
    }
    for (int i = 1; i <= 9; i++) {
        if (IsKeyPressed(KEY_ONE + i - 1)) {
            create_block(i == 9 ? VB_MATH : i, mp);
            return;
        }
    }
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (ctrl && IsKeyPressed(KEY_Z)) { do_undo(); return; }
    if (ctrl && IsKeyPressed(KEY_C)) { copy_selection(); return; }
    if (ctrl && IsKeyPressed(KEY_V)) { paste_clipboard(); return; }
    if ((IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) && (ed.sel_list.count() || ed.sel)) {
        delete_selected();
        return;
    }
    if (IsKeyPressed(KEY_E) && ed.sel) {
        start_edit(ed.sel);
        return;
    }
    if (IsKeyPressed(KEY_T) && ed.sel) {
        if (ed.sel->type == VB_BOOL) {
            capture_undo();
            ed.sel->text = (ed.sel->text == "true") ? "false" : "true";
        }
        return;
    }
    if (IsKeyPressed(KEY_K)) {
        compile_graph_to_asm();
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) { clear_selection(); return; }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int k, pi;
        VBlock* hb = find_port(mp, k, pi);
        if (hb) {
            capture_undo();
            ed.connecting = true;
            ed.c_blk = hb; ed.c_kind = k; ed.c_port = pi;
            ed.c_mpos = mp;
            return;
        }
        VBlock* hit = find_block(mp);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (hit) {
            ed.sel = hit;
            if (shift) {
                if (is_selected(hit)) remove_selection(hit);
                else add_selection(hit);
                return;
            }
            if (!is_selected(hit)) { clear_selection(); add_selection(hit); }
            if (hit->type == VB_VARREF && CheckCollisionPointRec(mp, dropdown_rect(*hit))) {
                cycle_varref(hit);
                return;
            }
            if ((hit->type == VB_BOOL || hit->type == VB_MATH) && CheckCollisionPointRec(mp, dropdown_rect(*hit))) {
                if (hit->type == VB_BOOL) cycle_bool(hit);
                else cycle_math(hit);
                return;
            }
            if (hit->type == VB_VARDECL && CheckCollisionPointRec(mp, var_type_rect(*hit))) {
                cycle_type(hit);
                return;
            }
            if (DEFS[hit->type].has_text && CheckCollisionPointRec(mp, text_rect(*hit))) {
                start_edit(hit);
                return;
            }
            capture_undo();
            ed.dragging = true;
            ed.drag_origin = mp;
            ed.drag_start_pos.set_count(0);
            for (auto* p : ed.sel_list) ed.drag_start_pos.push(p->pos);
            return;
        }
        if (!shift) clear_selection();
        ed.box_drag = true;
        ed.box_start = mp;
    }
}

int main(int argc, char** argv)
{
    InitWindow(1280, 720, "Nul Visual Editor");
    SetTargetFPS(240);

    ed.entry = nullptr;
    ed.sel = nullptr;
    ed.hov = nullptr;
    ed.edit_blk = nullptr;
    ed.cam = {};
    ed.cam.offset = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    ed.cam.target = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    ed.cam.zoom = 1.0f;

    VBlock* m = new VBlock{};
    m->type = VB_MAIN;
    m->w = bwidth(*m);
    m->h = bheight(*m);
    m->pos = {150, 300};
    ed.blocks.push(m);
    ed.entry = m;

    VBlock* b1 = new VBlock{};
    b1->type = VB_STRING;
    b1->text = "Hello world!\\n";
    b1->w = bwidth(*b1);
    b1->h = bheight(*b1);
    b1->pos = {400, 300};
    ed.blocks.push(b1);

    VBlock* b2 = new VBlock{};
    b2->type = VB_PRINT;
    b2->w = bwidth(*b2);
    b2->h = bheight(*b2);
    b2->pos = {650, 300};
    ed.blocks.push(b2);

    m->next = b1;
    b1->next = b2;
    b2->data_in.push(VDataSrc{b1, 0, 0});

    while (!WindowShouldClose()) {
        input();
        BeginDrawing();
        render();
        EndDrawing();
    }

    for (auto* b : ed.blocks) { b->data_in.cleanup(); delete b; }
    return 0;
}







#endif // !COMPILER_CLI

#ifdef COMPILER_CLI
// Command-line compiler: `compiler_cli [options] file.nul|folder`
// A path to a folder compiles every `.nul` file inside it into ONE executable;
// a path to a `.nul` file compiles just that file. The output name comes from
// `-o name`, or in folder mode from the first (alphabetically) `.nul` file.
struct Sources
{
    StrView src = "";
    bool parsed = false;
};

static StrView file_basename(StrView path) {
    StrView name = path;
    for (usize i = 0; i < path.size; ++i) {
        if (path.data[i] == '/' || path.data[i] == '\\')
            name = StrView(path.data + i + 1, path.size - i - 1);
    }
    return name;
}

static int compile_nul_files(Array<Sources>& sources, StrView out_file) {
    reset_parser_globals();

    // Default the output name to the first source file without its extension.
    if (out_file.size == 0) {
        StrView base = sources[0].src;
        while (base.size > 0) {
            if (base.data[base.size - 1] == '.') { base.chop_right(1); break; }
            base.chop_right(1);
        }
        StrBuilder out_b{get_global_allocator()};
        out_b.append(base).append_null(false);
        out_path = out_b.to_string_view(true);
    } else {
        StrBuilder out_b{get_global_allocator()};
        out_b.append(out_file).append_null(false);
        out_path = out_b.to_string_view(true);
    }

    // Pass 1: expand includes, lex and parse every file. Function declarations
    // land in g_functions here, so a file that *uses* a function may come
    // before the file that *defines* it. The Lexer's source buffer backs every
    // token (and every string literal), so the lexers stay alive until after
    // compile_program() has emitted the binary.
    Array<Lexer*> lexers{};
    Array<Expression*> all_exprs{};
    // Per-file module and expression range: pass 2 translates each file's
    // statements under the file's module context (functions establish their own
    // inside translate_function_body), and imports are validated once every
    // module declaration is known.
    Array<StrView> file_modules{};
    Array<usize> file_expr_start{};
    // Per-file error-attribution context: translation (pass 2) runs after all
    // files are parsed, so src_path/src_content must be restored per file or
    // errors would name whichever file was parsed last.
    Array<StrView> file_paths{};
    Array<const char*> file_contents{};
    for (auto& src : sources) {
        log_info("Compiling " SV_FORMAT "\n", SV_ARG(src.src));
        if (src.parsed) continue;

        StrBuilder expanded{};
        Array<StrView> include_chain{};
        Array<DefineMacro> macros{};
        if (!preprocess_includes(expanded, src.src.data, include_chain, macros)) {
            expanded.cleanup();
            include_chain.cleanup();
            macros.cleanup();
            return 1;
        }

        auto* lexer = new Lexer(SV_LIT(""));
        lexer->_source.append(expanded.data(), expanded.count());
        lexer->_source.append_null(false);
        src_content = lexer->_source.data();
        expanded.cleanup();
        include_chain.cleanup();
        macros.cleanup();

        {
            // compiler_error() requires a null-terminated src_path for reporting.
            StrBuilder src_b{get_global_allocator()};
            src_b.append(src.src).append_null(false);
            src_path = src_b.to_string_view(true);
        }
        file_paths.push(src_path);
        file_contents.push(src_content);

        if (!lexer->tokenize()) { delete lexer; return 1; }
        if (lexer->_tokens.is_empty()) { delete lexer; continue; }
        file_expr_start.push(all_exprs.count());
        if (!parse(*lexer, all_exprs)) { delete lexer; return 1; }
        lexers.push(lexer);
        // parse() leaves g_current_module_name set to the file's module ("" if
        // the file declares none).
        file_modules.push(g_current_module_name);
    }
    if (all_exprs.count() == 0) {
        log_error("No code to compile\n");
        return 1;
    }

    // Every `import` must name a module declared by one of the compiled files.
    for (auto& m : g_modules)
        for (auto& imp : m.imports)
            if (!find_module(imp))
                compiler_error(Token{Tok_Ident, imp},
                    "Module '" SV_FORMAT "' is not declared in any compiled file; import it or add a file that declares it\n",
                    SV_ARG(imp));

    // Pass 2: translate every expression; all declarations already exist, so
    // top-level statements from any file can call functions from any file.
    Array<Instruction> ops{};
    Array<VirtualReg> regs{};
    Array<Variable> vars{};
    usize file_idx = 0;
    for (usize i = 0; i < all_exprs.count(); ++i) {
        // Enter the next file's module context once its expression range starts.
        while (file_idx + 1 < file_expr_start.count() && i >= file_expr_start[file_idx + 1])
            ++file_idx;
        g_current_module_name = file_modules[file_idx];
        src_path = file_paths[file_idx];
        src_content = file_contents[file_idx];
        ValueType return_type = TYPE_NOP;
        translate_to_instruction(ops, regs, vars, all_exprs[i], return_type);
    }
    g_current_module_name = "";

    g_run_compiled = true;
    bool ok = compile_program(ops, regs);

    for (auto* lexer : lexers) delete lexer;
    if (!ok) {
        log_error("Compilation failed\n");
        return 1;
    }
    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    bool output_mode = false;
    bool output_set = false;
    for (int i = 1; i < argc; ++i) {
        StrView arg = argv[i];
        if (output_mode) {
            if (arg.starts_with("--") || arg.starts_with("-")) {
                log_error("Expected output file, but got: '" SV_FORMAT "'\n", SV_ARG(arg));
                exit(1);
            }
            out_path = arg;
            output_mode = false;
            output_set = true;
            continue;
        }
        if (arg == "-o") {
            output_mode = true;
        } else if (arg.starts_with("--platform=")) {
            StrView value = arg;
            value.data += sizeof("--platform=");
            value.size -= sizeof("--platform=");
            g_target_platform = parse_platform(value);
            if (g_target_platform == FlagsSystem::UNKNOWN) {
                log_error("Unknown platform '" SV_FORMAT "', expected Windows/Linux/MacOS/BSD/Android\n",
                        SV_ARG(value));
                exit(1);
            }
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            exit(0);
        } else {
            if (src_path.size > 0) {
                log_error("Expected a single .nul file or a folder, but got [" SV_FORMAT ", " SV_FORMAT "]\n",
                        SV_ARG(src_path), SV_ARG(arg));
                exit(1);
            }
            src_path = arg;
        }
    }
    if (src_path.size == 0) {
        log_error("No source files was provided...\n");
        exit(0);
    }

    // A path that names a folder compiles every `.nul` file inside it into one
    // executable; the output is named after the first `.nul` file (or `-o`).
    Array<FileEntry> files{};
    bool is_folder = false;
    {
        ScopedLogger mute(logger_muted);
        is_folder = read_folder(src_path, files);
    }
    if (is_folder) {
        // The global allocator never frees, so these views stay valid after the
        // loop; compile_nul_files() overwrites the global src_path per file, so
        // keep the folder argument in a stable local.
        StrView folder_path = src_path;
        Array<Sources> sources{};
        for (usize i = 0; i < files.count(); ++i) {
            const FileEntry& f = files[i];
            if (f.type != FileType::NORMAL) continue;
            if (!f.name.ends_with(SV_LIT(".nul"))) continue;

            StrBuilder full{get_global_allocator()};
            full.append(folder_path);
            if (!folder_path.ends_with("/") && !folder_path.ends_with("\\")) full.append("/");
            full.append(f.name).append_null(false);
            sources.push(Sources{full.to_string_view(true)});
        }
        if (sources.count() == 0) {
            log_error("No .nul files found in '" SV_FORMAT "'\n", SV_ARG(src_path));
            return 1;
        }
        // Sort by file name (lexically, ignoring the folder prefix) so the
        // "first file" - and thus the default output name - is stable.
        for (usize i = 1; i < sources.count(); ++i) {
            StrView key = sources[i].src;
            StrView key_name = file_basename(key);
            usize j = i;
            while (j > 0) {
                StrView prev_name = file_basename(sources[j - 1].src);
                usize n = MIN(key_name.size, prev_name.size);
                int cmp = memcmp(prev_name.data, key_name.data, n);
                if (cmp == 0) cmp = (int)(prev_name.size - key_name.size);
                if (cmp <= 0) break;
                sources[j] = sources[j - 1];
                --j;
            }
            sources[j].src = key;
        }
        return compile_nul_files(sources, output_set ? out_path : StrView(""));
    }

    // Single file mode: the output name is the source without its extension.
    Array<Sources> single{};
    single.push(Sources{src_path});
    return compile_nul_files(single, output_set ? out_path : StrView(""));
}
#endif // COMPILER_CLI