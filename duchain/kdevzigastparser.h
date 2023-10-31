/*
 * Copyright 2017  Emma Gospodinova <emma.gospodinova@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ZIGAST_H
#define ZIGAST_H

extern "C" {

enum NodeTag {
    NodeTag_root,
    /// `usingnamespace lhs;`. rhs unused. main_token is `usingnamespace`.
    NodeTag_usingnamespace,
    /// lhs is test name token (must be string literal or identifier), if any.
    /// rhs is the body node.
    NodeTag_test_decl,
    /// lhs is the index into extra_data.
    /// rhs is the initialization expression, if any.
    /// main_token is `var` or `const`.
    NodeTag_global_var_decl,
    /// `var a: x align(y) = rhs`
    /// lhs is the index into extra_data.
    /// main_token is `var` or `const`.
    NodeTag_local_var_decl,
    /// `var a: lhs = rhs`. lhs and rhs may be unused.
    /// Can be local or global.
    /// main_token is `var` or `const`.
    NodeTag_simple_var_decl,
    /// `var a align(lhs) = rhs`. lhs and rhs may be unused.
    /// Can be local or global.
    /// main_token is `var` or `const`.
    NodeTag_aligned_var_decl,
    /// lhs is the identifier token payload if any,
    /// rhs is the deferred expression.
    NodeTag_errdefer,
    /// lhs is unused.
    /// rhs is the deferred expression.
    NodeTag_defer,
    /// lhs catch rhs
    /// lhs catch |err| rhs
    /// main_token is the `catch` keyword.
    /// payload is determined by looking at the next token after the `catch` keyword.
    NodeTag_catch,
    /// `lhs.a`. main_token is the dot. rhs is the identifier token index.
    NodeTag_field_access,
    /// `lhs.?`. main_token is the dot. rhs is the `?` token index.
    NodeTag_unwrap_optional,
    /// `lhs == rhs`. main_token is op.
    NodeTag_equal_equal,
    /// `lhs != rhs`. main_token is op.
    NodeTag_bang_equal,
    /// `lhs < rhs`. main_token is op.
    NodeTag_less_than,
    /// `lhs > rhs`. main_token is op.
    NodeTag_greater_than,
    /// `lhs <= rhs`. main_token is op.
    NodeTag_less_or_equal,
    /// `lhs >= rhs`. main_token is op.
    NodeTag_greater_or_equal,
    /// `lhs *= rhs`. main_token is op.
    NodeTag_assign_mul,
    /// `lhs /= rhs`. main_token is op.
    NodeTag_assign_div,
    /// `lhs %= rhs`. main_token is op.
    NodeTag_assign_mod,
    /// `lhs += rhs`. main_token is op.
    NodeTag_assign_add,
    /// `lhs -= rhs`. main_token is op.
    NodeTag_assign_sub,
    /// `lhs <<= rhs`. main_token is op.
    NodeTag_assign_shl,
    /// `lhs <<|= rhs`. main_token is op.
    NodeTag_assign_shl_sat,
    /// `lhs >>= rhs`. main_token is op.
    NodeTag_assign_shr,
    /// `lhs &= rhs`. main_token is op.
    NodeTag_assign_bit_and,
    /// `lhs ^= rhs`. main_token is op.
    NodeTag_assign_bit_xor,
    /// `lhs |= rhs`. main_token is op.
    NodeTag_assign_bit_or,
    /// `lhs *%= rhs`. main_token is op.
    NodeTag_assign_mul_wrap,
    /// `lhs +%= rhs`. main_token is op.
    NodeTag_assign_add_wrap,
    /// `lhs -%= rhs`. main_token is op.
    NodeTag_assign_sub_wrap,
    /// `lhs *|= rhs`. main_token is op.
    NodeTag_assign_mul_sat,
    /// `lhs +|= rhs`. main_token is op.
    NodeTag_assign_add_sat,
    /// `lhs -|= rhs`. main_token is op.
    NodeTag_assign_sub_sat,
    /// `lhs = rhs`. main_token is op.
    NodeTag_assign,
    /// `a, b, ... = rhs`. main_token is op. lhs is index into `extra_data`
    /// of an lhs elem count followed by an array of that many `Node.Index`,
    /// with each node having one of the following types:
    /// * `global_var_decl`
    /// * `local_var_decl`
    /// * `simple_var_decl`
    /// * `aligned_var_decl`
    /// * Any expression node
    /// The first 3 types correspond to a `var` or `const` lhs node (note
    /// that their `rhs` is always 0). An expression node corresponds to a
    /// standard assignment LHS (which must be evaluated as an lvalue).
    /// There may be a preceding `comptime` token, which does not create a
    /// corresponding `comptime` node so must be manually detected.
    NodeTag_assign_destructure,
    /// `lhs || rhs`. main_token is the `||`.
    NodeTag_merge_error_sets,
    /// `lhs * rhs`. main_token is the `*`.
    NodeTag_mul,
    /// `lhs / rhs`. main_token is the `/`.
    NodeTag_div,
    /// `lhs % rhs`. main_token is the `%`.
    NodeTag_mod,
    /// `lhs ** rhs`. main_token is the `**`.
    NodeTag_array_mult,
    /// `lhs *% rhs`. main_token is the `*%`.
    NodeTag_mul_wrap,
    /// `lhs *| rhs`. main_token is the `*|`.
    NodeTag_mul_sat,
    /// `lhs + rhs`. main_token is the `+`.
    NodeTag_add,
    /// `lhs - rhs`. main_token is the `-`.
    NodeTag_sub,
    /// `lhs ++ rhs`. main_token is the `++`.
    NodeTag_array_cat,
    /// `lhs +% rhs`. main_token is the `+%`.
    NodeTag_add_wrap,
    /// `lhs -% rhs`. main_token is the `-%`.
    NodeTag_sub_wrap,
    /// `lhs +| rhs`. main_token is the `+|`.
    NodeTag_add_sat,
    /// `lhs -| rhs`. main_token is the `-|`.
    NodeTag_sub_sat,
    /// `lhs << rhs`. main_token is the `<<`.
    NodeTag_shl,
    /// `lhs <<| rhs`. main_token is the `<<|`.
    NodeTag_shl_sat,
    /// `lhs >> rhs`. main_token is the `>>`.
    NodeTag_shr,
    /// `lhs & rhs`. main_token is the `&`.
    NodeTag_bit_and,
    /// `lhs ^ rhs`. main_token is the `^`.
    NodeTag_bit_xor,
    /// `lhs | rhs`. main_token is the `|`.
    NodeTag_bit_or,
    /// `lhs orelse rhs`. main_token is the `orelse`.
    NodeTag_orelse,
    /// `lhs and rhs`. main_token is the `and`.
    NodeTag_bool_and,
    /// `lhs or rhs`. main_token is the `or`.
    NodeTag_bool_or,
    /// `op lhs`. rhs unused. main_token is op.
    NodeTag_bool_not,
    /// `op lhs`. rhs unused. main_token is op.
    NodeTag_negation,
    /// `op lhs`. rhs unused. main_token is op.
    NodeTag_bit_not,
    /// `op lhs`. rhs unused. main_token is op.
    NodeTag_negation_wrap,
    /// `op lhs`. rhs unused. main_token is op.
    NodeTag_address_of,
    /// `op lhs`. rhs unused. main_token is op.
    NodeTag_try,
    /// `op lhs`. rhs unused. main_token is op.
    NodeTag_await,
    /// `?lhs`. rhs unused. main_token is the `?`.
    NodeTag_optional_type,
    /// `[lhs]rhs`.
    NodeTag_array_type,
    /// `[lhs:a]b`. `ArrayTypeSentinel[rhs]`.
    NodeTag_array_type_sentinel,
    /// `[*]align(lhs) rhs`. lhs can be omitted.
    /// `*align(lhs) rhs`. lhs can be omitted.
    /// `[]rhs`.
    /// main_token is the asterisk if a pointer or the lbracket if a slice
    /// main_token might be a ** token, which is shared with a parent/child
    /// pointer type and may require special handling.
    NodeTag_ptr_type_aligned,
    /// `[*:lhs]rhs`. lhs can be omitted.
    /// `*rhs`.
    /// `[:lhs]rhs`.
    /// main_token is the asterisk if a pointer or the lbracket if a slice
    /// main_token might be a ** token, which is shared with a parent/child
    /// pointer type and may require special handling.
    NodeTag_ptr_type_sentinel,
    /// lhs is index into ptr_type. rhs is the element type expression.
    /// main_token is the asterisk if a pointer or the lbracket if a slice
    /// main_token might be a ** token, which is shared with a parent/child
    /// pointer type and may require special handling.
    NodeTag_ptr_type,
    /// lhs is index into ptr_type_bit_range. rhs is the element type expression.
    /// main_token is the asterisk if a pointer or the lbracket if a slice
    /// main_token might be a ** token, which is shared with a parent/child
    /// pointer type and may require special handling.
    NodeTag_ptr_type_bit_range,
    /// `lhs[rhs..]`
    /// main_token is the lbracket.
    NodeTag_slice_open,
    /// `lhs[b..c]`. rhs is index into Slice
    /// main_token is the lbracket.
    NodeTag_slice,
    /// `lhs[b..c :d]`. rhs is index into SliceSentinel. Slice end "c" can be omitted.
    /// main_token is the lbracket.
    NodeTag_slice_sentinel,
    /// `lhs.*`. rhs is unused.
    NodeTag_deref,
    /// `lhs[rhs]`.
    NodeTag_array_access,
    /// `lhs{rhs}`. rhs can be omitted.
    NodeTag_array_init_one,
    /// `lhs{rhs,}`. rhs can *not* be omitted
    NodeTag_array_init_one_comma,
    /// `.{lhs, rhs}`. lhs and rhs can be omitted.
    NodeTag_array_init_dot_two,
    /// Same as `array_init_dot_two` except there is known to be a trailing comma
    /// before the final rbrace.
    NodeTag_array_init_dot_two_comma,
    /// `.{a, b}`. `sub_list[lhs..rhs]`.
    NodeTag_array_init_dot,
    /// Same as `array_init_dot` except there is known to be a trailing comma
    /// before the final rbrace.
    NodeTag_array_init_dot_comma,
    /// `lhs{a, b}`. `sub_range_list[rhs]`. lhs can be omitted which means `.{a, b}`.
    NodeTag_array_init,
    /// Same as `array_init` except there is known to be a trailing comma
    /// before the final rbrace.
    NodeTag_array_init_comma,
    /// `lhs{.a = rhs}`. rhs can be omitted making it empty.
    /// main_token is the lbrace.
    NodeTag_struct_init_one,
    /// `lhs{.a = rhs,}`. rhs can *not* be omitted.
    /// main_token is the lbrace.
    NodeTag_struct_init_one_comma,
    /// `.{.a = lhs, .b = rhs}`. lhs and rhs can be omitted.
    /// main_token is the lbrace.
    /// No trailing comma before the rbrace.
    NodeTag_struct_init_dot_two,
    /// Same as `struct_init_dot_two` except there is known to be a trailing comma
    /// before the final rbrace.
    NodeTag_struct_init_dot_two_comma,
    /// `.{.a = b, .c = d}`. `sub_list[lhs..rhs]`.
    /// main_token is the lbrace.
    NodeTag_struct_init_dot,
    /// Same as `struct_init_dot` except there is known to be a trailing comma
    /// before the final rbrace.
    NodeTag_struct_init_dot_comma,
    /// `lhs{.a = b, .c = d}`. `sub_range_list[rhs]`.
    /// lhs can be omitted which means `.{.a = b, .c = d}`.
    /// main_token is the lbrace.
    NodeTag_struct_init,
    /// Same as `struct_init` except there is known to be a trailing comma
    /// before the final rbrace.
    NodeTag_struct_init_comma,
    /// `lhs(rhs)`. rhs can be omitted.
    /// main_token is the lparen.
    NodeTag_call_one,
    /// `lhs(rhs,)`. rhs can be omitted.
    /// main_token is the lparen.
    NodeTag_call_one_comma,
    /// `async lhs(rhs)`. rhs can be omitted.
    NodeTag_async_call_one,
    /// `async lhs(rhs,)`.
    NodeTag_async_call_one_comma,
    /// `lhs(a, b, c)`. `SubRange[rhs]`.
    /// main_token is the `(`.
    NodeTag_call,
    /// `lhs(a, b, c,)`. `SubRange[rhs]`.
    /// main_token is the `(`.
    NodeTag_call_comma,
    /// `async lhs(a, b, c)`. `SubRange[rhs]`.
    /// main_token is the `(`.
    NodeTag_async_call,
    /// `async lhs(a, b, c,)`. `SubRange[rhs]`.
    /// main_token is the `(`.
    NodeTag_async_call_comma,
    /// `switch(lhs) {}`. `SubRange[rhs]`.
    NodeTag_switch,
    /// Same as switch except there is known to be a trailing comma
    /// before the final rbrace
    NodeTag_switch_comma,
    /// `lhs => rhs`. If lhs is omitted it means `else`.
    /// main_token is the `=>`
    NodeTag_switch_case_one,
    /// Same ast `switch_case_one` but the case is inline
    NodeTag_switch_case_inline_one,
    /// `a, b, c => rhs`. `SubRange[lhs]`.
    /// main_token is the `=>`
    NodeTag_switch_case,
    /// Same ast `switch_case` but the case is inline
    NodeTag_switch_case_inline,
    /// `lhs...rhs`.
    NodeTag_switch_range,
    /// `while (lhs) rhs`.
    /// `while (lhs) |x| rhs`.
    NodeTag_while_simple,
    /// `while (lhs) : (a) b`. `WhileCont[rhs]`.
    /// `while (lhs) : (a) b`. `WhileCont[rhs]`.
    NodeTag_while_cont,
    /// `while (lhs) : (a) b else c`. `While[rhs]`.
    /// `while (lhs) |x| : (a) b else c`. `While[rhs]`.
    /// `while (lhs) |x| : (a) b else |y| c`. `While[rhs]`.
    /// The cont expression part `: (a)` may be omitted.
    NodeTag_while,
    /// `for (lhs) rhs`.
    NodeTag_for_simple,
    /// `for (lhs[0..inputs]) lhs[inputs + 1] else lhs[inputs + 2]`. `For[rhs]`.
    NodeTag_for,
    /// `lhs..rhs`. rhs can be omitted.
    NodeTag_for_range,
    /// `if (lhs) rhs`.
    /// `if (lhs) |a| rhs`.
    NodeTag_if_simple,
    /// `if (lhs) a else b`. `If[rhs]`.
    /// `if (lhs) |x| a else b`. `If[rhs]`.
    /// `if (lhs) |x| a else |y| b`. `If[rhs]`.
    NodeTag_if,
    /// `suspend lhs`. lhs can be omitted. rhs is unused.
    NodeTag_suspend,
    /// `resume lhs`. rhs is unused.
    NodeTag_resume,
    /// `continue`. lhs is token index of label if any. rhs is unused.
    NodeTag_continue,
    /// `break :lhs rhs`
    /// both lhs and rhs may be omitted.
    NodeTag_break,
    /// `return lhs`. lhs can be omitted. rhs is unused.
    NodeTag_return,
    /// `fn (a: lhs) rhs`. lhs can be omitted.
    /// anytype and ... parameters are omitted from the AST tree.
    /// main_token is the `fn` keyword.
    /// extern function declarations use this tag.
    NodeTag_fn_proto_simple,
    /// `fn (a: b, c: d) rhs`. `sub_range_list[lhs]`.
    /// anytype and ... parameters are omitted from the AST tree.
    /// main_token is the `fn` keyword.
    /// extern function declarations use this tag.
    NodeTag_fn_proto_multi,
    /// `fn (a: b) rhs addrspace(e) linksection(f) callconv(g)`. `FnProtoOne[lhs]`.
    /// zero or one parameters.
    /// anytype and ... parameters are omitted from the AST tree.
    /// main_token is the `fn` keyword.
    /// extern function declarations use this tag.
    NodeTag_fn_proto_one,
    /// `fn (a: b, c: d) rhs addrspace(e) linksection(f) callconv(g)`. `FnProto[lhs]`.
    /// anytype and ... parameters are omitted from the AST tree.
    /// main_token is the `fn` keyword.
    /// extern function declarations use this tag.
    NodeTag_fn_proto,
    /// lhs is the fn_proto.
    /// rhs is the function body block.
    /// Note that extern function declarations use the fn_proto tags rather
    /// than this one.
    NodeTag_fn_decl,
    /// `anyframe->rhs`. main_token is `anyframe`. `lhs` is arrow token index.
    NodeTag_anyframe_type,
    /// Both lhs and rhs unused.
    NodeTag_anyframe_literal,
    /// Both lhs and rhs unused.
    NodeTag_char_literal,
    /// Both lhs and rhs unused.
    NodeTag_number_literal,
    /// Both lhs and rhs unused.
    NodeTag_unreachable_literal,
    /// Both lhs and rhs unused.
    /// Most identifiers will not have explicit AST nodes, however for expressions
    /// which could be one of many different kinds of AST nodes, there will be an
    /// identifier AST node for it.
    NodeTag_identifier,
    /// lhs is the dot token index, rhs unused, main_token is the identifier.
    NodeTag_enum_literal,
    /// main_token is the string literal token
    /// Both lhs and rhs unused.
    NodeTag_string_literal,
    /// main_token is the first token index (redundant with lhs)
    /// lhs is the first token index; rhs is the last token index.
    /// Could be a series of multiline_string_literal_line tokens, or a single
    /// string_literal token.
    NodeTag_multiline_string_literal,
    /// `(lhs)`. main_token is the `(`; rhs is the token index of the `)`.
    NodeTag_grouped_expression,
    /// `@a(lhs, rhs)`. lhs and rhs may be omitted.
    /// main_token is the builtin token.
    NodeTag_builtin_call_two,
    /// Same as builtin_call_two but there is known to be a trailing comma before the rparen.
    NodeTag_builtin_call_two_comma,
    /// `@a(b, c)`. `sub_list[lhs..rhs]`.
    /// main_token is the builtin token.
    NodeTag_builtin_call,
    /// Same as builtin_call but there is known to be a trailing comma before the rparen.
    NodeTag_builtin_call_comma,
    /// `error{a, b}`.
    /// rhs is the rbrace, lhs is unused.
    NodeTag_error_set_decl,
    /// `struct {}`, `union {}`, `opaque {}`, `enum {}`. `extra_data[lhs..rhs]`.
    /// main_token is `struct`, `union`, `opaque`, `enum` keyword.
    NodeTag_container_decl,
    /// Same as ContainerDecl but there is known to be a trailing comma
    /// or semicolon before the rbrace.
    NodeTag_container_decl_trailing,
    /// `struct {lhs, rhs}`, `union {lhs, rhs}`, `opaque {lhs, rhs}`, `enum {lhs, rhs}`.
    /// lhs or rhs can be omitted.
    /// main_token is `struct`, `union`, `opaque`, `enum` keyword.
    NodeTag_container_decl_two,
    /// Same as ContainerDeclTwo except there is known to be a trailing comma
    /// or semicolon before the rbrace.
    NodeTag_container_decl_two_trailing,
    /// `struct(lhs)` / `union(lhs)` / `enum(lhs)`. `SubRange[rhs]`.
    NodeTag_container_decl_arg,
    /// Same as container_decl_arg but there is known to be a trailing
    /// comma or semicolon before the rbrace.
    NodeTag_container_decl_arg_trailing,
    /// `union(enum) {}`. `sub_list[lhs..rhs]`.
    /// Note that tagged unions with explicitly provided enums are represented
    /// by `container_decl_arg`.
    NodeTag_tagged_union,
    /// Same as tagged_union but there is known to be a trailing comma
    /// or semicolon before the rbrace.
    NodeTag_tagged_union_trailing,
    /// `union(enum) {lhs, rhs}`. lhs or rhs may be omitted.
    /// Note that tagged unions with explicitly provided enums are represented
    /// by `container_decl_arg`.
    NodeTag_tagged_union_two,
    /// Same as tagged_union_two but there is known to be a trailing comma
    /// or semicolon before the rbrace.
    NodeTag_tagged_union_two_trailing,
    /// `union(enum(lhs)) {}`. `SubRange[rhs]`.
    NodeTag_tagged_union_enum_tag,
    /// Same as tagged_union_enum_tag but there is known to be a trailing comma
    /// or semicolon before the rbrace.
    NodeTag_tagged_union_enum_tag_trailing,
    /// `a: lhs = rhs,`. lhs and rhs can be omitted.
    /// main_token is the field name identifier.
    /// lastToken() does not include the possible trailing comma.
    NodeTag_container_field_init,
    /// `a: lhs align(rhs),`. rhs can be omitted.
    /// main_token is the field name identifier.
    /// lastToken() does not include the possible trailing comma.
    NodeTag_container_field_align,
    /// `a: lhs align(c) = d,`. `container_field_list[rhs]`.
    /// main_token is the field name identifier.
    /// lastToken() does not include the possible trailing comma.
    NodeTag_container_field,
    /// `comptime lhs`. rhs unused.
    NodeTag_comptime,
    /// `nosuspend lhs`. rhs unused.
    NodeTag_nosuspend,
    /// `{lhs rhs}`. rhs or lhs can be omitted.
    /// main_token points at the lbrace.
    NodeTag_block_two,
    /// Same as block_two but there is known to be a semicolon before the rbrace.
    NodeTag_block_two_semicolon,
    /// `{}`. `sub_list[lhs..rhs]`.
    /// main_token points at the lbrace.
    NodeTag_block,
    /// Same as block but there is known to be a semicolon before the rbrace.
    NodeTag_block_semicolon,
    /// `asm(lhs)`. rhs is the token index of the rparen.
    NodeTag_asm_simple,
    /// `asm(lhs, a)`. `Asm[rhs]`.
    NodeTag_asm,
    /// `[a] "b" (c)`. lhs is 0, rhs is token index of the rparen.
    /// `[a] "b" (-> lhs)`. rhs is token index of the rparen.
    /// main_token is `a`.
    NodeTag_asm_output,
    /// `[a] "b" (lhs)`. rhs is token index of the rparen.
    /// main_token is `a`.
    NodeTag_asm_input,
    /// `error.a`. lhs is token index of `.`. rhs is token index of `a`.
    NodeTag_error_value,
    /// `lhs!rhs`. main_token is the `!`.
    NodeTag_error_union,
    NodeTag_invalid,
};

// Must be kept in sync with NodeKind enum in kdevzigastparser.zig
enum NodeKind
{
    Unknown = 0,
    Module, // Root
    ContainerDecl, // struct, union
    EnumDecl,
    TemplateDecl, // fn that returns type
    FieldDecl, // container_field
    FunctionDecl,
    ParamDecl,
    VarDecl,
    BlockDecl,
    ErrorDecl,
    AliasDecl, // Import
    TestDecl,
    // Uses
    Call,
    ContainerInit,
    VarAccess,
    FieldAccess,
    ArrayAccess,
    PtrAccess, // Deref
    Literal,
    Ident,
    If,
    For,
    While,
    Switch,
    Defer,
    Catch
};

enum VisitResult
{
    Break = 0,
    Continue = 1,
    Recurse = 2
};

enum CaptureType
{
    Payload = 0,
    Error
};


struct ZAst;

struct SourceLocation
{
    uint32_t line;
    uint32_t column;
};

struct SourceRange
{
    SourceLocation start;
    SourceLocation end;

    bool isEmpty() {
        return start.line == 0 && start.column == 0 &&
               end.line == 0 && end.column == 0;
    }
};

struct NodeData
{
    uint32_t lhs;
    uint32_t rhs;
};


struct ZError
{
    int severity;
    SourceRange range;
    const char* message;
};

typedef uint32_t NodeIndex;
typedef uint32_t TokenIndex;
typedef VisitResult (*VisitorCallbackFn)(ZAst* tree, NodeIndex node, NodeIndex parent, void *data);


struct ArrayTypeSentinel
{
    NodeIndex sentinel;
    NodeIndex elem_type;
};

ZAst *parse_ast(const char *name, const char *source);
uint32_t ast_error_count(ZAst *tree);
void destroy_ast(ZAst *tree);

ZError *ast_error_at(ZAst* tree, uint32_t index);
void destroy_error(ZError *err);

NodeKind ast_node_kind(ZAst *tree, NodeIndex node);
NodeTag ast_node_tag(ZAst* tree, NodeIndex node);
NodeData ast_node_data(ZAst *tree, NodeIndex node);
ArrayTypeSentinel ast_array_type_sentinel(ZAst *tree, NodeIndex node);
NodeIndex ast_visit_one_child(ZAst *tree, NodeIndex node);
NodeIndex ast_var_type(ZAst *tree, NodeIndex node);
NodeIndex ast_var_value(ZAst *tree, NodeIndex node);
NodeIndex ast_fn_return_type(ZAst *tree, NodeIndex node);
bool ast_fn_returns_inferred_error(ZAst *tree, NodeIndex node);
uint32_t ast_fn_param_count(ZAst *tree, NodeIndex node);
NodeIndex ast_fn_param_at(ZAst *tree, NodeIndex node, uint32_t i);


TokenIndex ast_node_name_token(ZAst *tree, NodeIndex node);
TokenIndex ast_node_main_token(ZAst *tree, NodeIndex node);
TokenIndex ast_node_capture_token(ZAst *tree, NodeIndex node, CaptureType capture);
TokenIndex ast_fn_param_token(ZAst *tree, NodeIndex node, uint32_t i);


// NOTE: The caller must free with destroy_string
const char *ast_token_slice(ZAst *tree, TokenIndex token);
void destroy_string(const char *str);

// NOTE: Lines are 0 indexed so the first line is 0 not 1
SourceRange ast_token_range(ZAst *tree, TokenIndex token);
SourceRange ast_node_range(ZAst *tree, NodeIndex node);


void ast_visit(ZAst *tree, NodeIndex node, VisitorCallbackFn callback, void *data);

bool is_zig_builtin_fn_name(const char *name);

}

#endif // ZIGAST_H
