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

enum AstType {
    /// sub_list[lhs...rhs]
    AstType_root,
    /// `usingnamespace lhs;`. rhs unused. main_token is `usingnamespace`.
    AstType_usingnamespace,
    /// lhs is test name token (must be string literal or identifier), if any.
    /// rhs is the body node.
    AstType_test_decl,
    /// lhs is the index into extra_data.
    /// rhs is the initialization expression, if any.
    /// main_token is `var` or `const`.
    AstType_global_var_decl,
    /// `var a: x align(y) = rhs`
    /// lhs is the index into extra_data.
    /// main_token is `var` or `const`.
    AstType_local_var_decl,
    /// `var a: lhs = rhs`. lhs and rhs may be unused.
    /// Can be local or global.
    /// main_token is `var` or `const`.
    AstType_simple_var_decl,
    /// `var a align(lhs) = rhs`. lhs and rhs may be unused.
    /// Can be local or global.
    /// main_token is `var` or `const`.
    AstType_aligned_var_decl,
    /// lhs is the identifier token payload if any,
    /// rhs is the deferred expression.
    AstType_errdefer,
    /// lhs is unused.
    /// rhs is the deferred expression.
    AstType_defer,
    /// lhs catch rhs
    /// lhs catch |err| rhs
    /// main_token is the `catch` keyword.
    /// payload is determined by looking at the next token after the `catch` keyword.
    AstType_catch,
    /// `lhs.a`. main_token is the dot. rhs is the identifier token index.
    AstType_field_access,
    /// `lhs.?`. main_token is the dot. rhs is the `?` token index.
    AstType_unwrap_optional,
    /// `lhs == rhs`. main_token is op.
    AstType_equal_equal,
    /// `lhs != rhs`. main_token is op.
    AstType_bang_equal,
    /// `lhs < rhs`. main_token is op.
    AstType_less_than,
    /// `lhs > rhs`. main_token is op.
    AstType_greater_than,
    /// `lhs <= rhs`. main_token is op.
    AstType_less_or_equal,
    /// `lhs >= rhs`. main_token is op.
    AstType_greater_or_equal,
    /// `lhs *= rhs`. main_token is op.
    AstType_assign_mul,
    /// `lhs /= rhs`. main_token is op.
    AstType_assign_div,
    /// `lhs *= rhs`. main_token is op.
    AstType_assign_mod,
    /// `lhs += rhs`. main_token is op.
    AstType_assign_add,
    /// `lhs -= rhs`. main_token is op.
    AstType_assign_sub,
    /// `lhs <<= rhs`. main_token is op.
    AstType_assign_shl,
    /// `lhs <<|= rhs`. main_token is op.
    AstType_assign_shl_sat,
    /// `lhs >>= rhs`. main_token is op.
    AstType_assign_shr,
    /// `lhs &= rhs`. main_token is op.
    AstType_assign_bit_and,
    /// `lhs ^= rhs`. main_token is op.
    AstType_assign_bit_xor,
    /// `lhs |= rhs`. main_token is op.
    AstType_assign_bit_or,
    /// `lhs *%= rhs`. main_token is op.
    AstType_assign_mul_wrap,
    /// `lhs +%= rhs`. main_token is op.
    AstType_assign_add_wrap,
    /// `lhs -%= rhs`. main_token is op.
    AstType_assign_sub_wrap,
    /// `lhs *|= rhs`. main_token is op.
    AstType_assign_mul_sat,
    /// `lhs +|= rhs`. main_token is op.
    AstType_assign_add_sat,
    /// `lhs -|= rhs`. main_token is op.
    AstType_assign_sub_sat,
    /// `lhs = rhs`. main_token is op.
    AstType_assign,
    /// `lhs || rhs`. main_token is the `||`.
    AstType_merge_error_sets,
    /// `lhs * rhs`. main_token is the `*`.
    AstType_mul,
    /// `lhs / rhs`. main_token is the `/`.
    AstType_div,
    /// `lhs % rhs`. main_token is the `%`.
    AstType_mod,
    /// `lhs ** rhs`. main_token is the `**`.
    AstType_array_mult,
    /// `lhs *% rhs`. main_token is the `*%`.
    AstType_mul_wrap,
    /// `lhs *| rhs`. main_token is the `*|`.
    AstType_mul_sat,
    /// `lhs + rhs`. main_token is the `+`.
    AstType_add,
    /// `lhs - rhs`. main_token is the `-`.
    AstType_sub,
    /// `lhs ++ rhs`. main_token is the `++`.
    AstType_array_cat,
    /// `lhs +% rhs`. main_token is the `+%`.
    AstType_add_wrap,
    /// `lhs -% rhs`. main_token is the `-%`.
    AstType_sub_wrap,
    /// `lhs +| rhs`. main_token is the `+|`.
    AstType_add_sat,
    /// `lhs -| rhs`. main_token is the `-|`.
    AstType_sub_sat,
    /// `lhs << rhs`. main_token is the `<<`.
    AstType_shl,
    /// `lhs <<| rhs`. main_token is the `<<|`.
    AstType_shl_sat,
    /// `lhs >> rhs`. main_token is the `>>`.
    AstType_shr,
    /// `lhs & rhs`. main_token is the `&`.
    AstType_bit_and,
    /// `lhs ^ rhs`. main_token is the `^`.
    AstType_bit_xor,
    /// `lhs | rhs`. main_token is the `|`.
    AstType_bit_or,
    /// `lhs orelse rhs`. main_token is the `orelse`.
    AstType_orelse,
    /// `lhs and rhs`. main_token is the `and`.
    AstType_bool_and,
    /// `lhs or rhs`. main_token is the `or`.
    AstType_bool_or,
    /// `op lhs`. rhs unused. main_token is op.
    AstType_bool_not,
    /// `op lhs`. rhs unused. main_token is op.
    AstType_negation,
    /// `op lhs`. rhs unused. main_token is op.
    AstType_bit_not,
    /// `op lhs`. rhs unused. main_token is op.
    AstType_negation_wrap,
    /// `op lhs`. rhs unused. main_token is op.
    AstType_address_of,
    /// `op lhs`. rhs unused. main_token is op.
    AstType_try,
    /// `op lhs`. rhs unused. main_token is op.
    AstType_await,
    /// `?lhs`. rhs unused. main_token is the `?`.
    AstType_optional_type,
    /// `[lhs]rhs`.
    AstType_array_type,
    /// `[lhs:a]b`. `ArrayTypeSentinel[rhs]`.
    AstType_array_type_sentinel,
    /// `[*]align(lhs) rhs`. lhs can be omitted.
    /// `*align(lhs) rhs`. lhs can be omitted.
    /// `[]rhs`.
    /// main_token is the asterisk if a pointer or the lbracket if a slice
    /// main_token might be a ** token, which is shared with a parent/child
    /// pointer type and may require special handling.
    AstType_ptr_type_aligned,
    /// `[*:lhs]rhs`. lhs can be omitted.
    /// `*rhs`.
    /// `[:lhs]rhs`.
    /// main_token is the asterisk if a pointer or the lbracket if a slice
    /// main_token might be a ** token, which is shared with a parent/child
    /// pointer type and may require special handling.
    AstType_ptr_type_sentinel,
    /// lhs is index into ptr_type. rhs is the element type expression.
    /// main_token is the asterisk if a pointer or the lbracket if a slice
    /// main_token might be a ** token, which is shared with a parent/child
    /// pointer type and may require special handling.
    AstType_ptr_type,
    /// lhs is index into ptr_type_bit_range. rhs is the element type expression.
    /// main_token is the asterisk if a pointer or the lbracket if a slice
    /// main_token might be a ** token, which is shared with a parent/child
    /// pointer type and may require special handling.
    AstType_ptr_type_bit_range,
    /// `lhs[rhs..]`
    /// main_token is the lbracket.
    AstType_slice_open,
    /// `lhs[b..c]`. rhs is index into Slice
    /// main_token is the lbracket.
    AstType_slice,
    /// `lhs[b..c :d]`. rhs is index into SliceSentinel
    /// main_token is the lbracket.
    AstType_slice_sentinel,
    /// `lhs.*`. rhs is unused.
    AstType_deref,
    /// `lhs[rhs]`.
    AstType_array_access,
    /// `lhs{rhs}`. rhs can be omitted.
    AstType_array_init_one,
    /// `lhs{rhs,}`. rhs can *not* be omitted
    AstType_array_init_one_comma,
    /// `.{lhs, rhs}`. lhs and rhs can be omitted.
    AstType_array_init_dot_two,
    /// Same as `array_init_dot_two` except there is known to be a trailing comma
    /// before the final rbrace.
    AstType_array_init_dot_two_comma,
    /// `.{a, b}`. `sub_list[lhs..rhs]`.
    AstType_array_init_dot,
    /// Same as `array_init_dot` except there is known to be a trailing comma
    /// before the final rbrace.
    AstType_array_init_dot_comma,
    /// `lhs{a, b}`. `sub_range_list[rhs]`. lhs can be omitted which means `.{a, b}`.
    AstType_array_init,
    /// Same as `array_init` except there is known to be a trailing comma
    /// before the final rbrace.
    AstType_array_init_comma,
    /// `lhs{.a = rhs}`. rhs can be omitted making it empty.
    /// main_token is the lbrace.
    AstType_struct_init_one,
    /// `lhs{.a = rhs,}`. rhs can *not* be omitted.
    /// main_token is the lbrace.
    AstType_struct_init_one_comma,
    /// `.{.a = lhs, .b = rhs}`. lhs and rhs can be omitted.
    /// main_token is the lbrace.
    /// No trailing comma before the rbrace.
    AstType_struct_init_dot_two,
    /// Same as `struct_init_dot_two` except there is known to be a trailing comma
    /// before the final rbrace.
    AstType_struct_init_dot_two_comma,
    /// `.{.a = b, .c = d}`. `sub_list[lhs..rhs]`.
    /// main_token is the lbrace.
    AstType_struct_init_dot,
    /// Same as `struct_init_dot` except there is known to be a trailing comma
    /// before the final rbrace.
    AstType_struct_init_dot_comma,
    /// `lhs{.a = b, .c = d}`. `sub_range_list[rhs]`.
    /// lhs can be omitted which means `.{.a = b, .c = d}`.
    /// main_token is the lbrace.
    AstType_struct_init,
    /// Same as `struct_init` except there is known to be a trailing comma
    /// before the final rbrace.
    AstType_struct_init_comma,
    /// `lhs(rhs)`. rhs can be omitted.
    /// main_token is the lparen.
    AstType_call_one,
    /// `lhs(rhs,)`. rhs can be omitted.
    /// main_token is the lparen.
    AstType_call_one_comma,
    /// `async lhs(rhs)`. rhs can be omitted.
    AstType_async_call_one,
    /// `async lhs(rhs,)`.
    AstType_async_call_one_comma,
    /// `lhs(a, b, c)`. `SubRange[rhs]`.
    /// main_token is the `(`.
    AstType_call,
    /// `lhs(a, b, c,)`. `SubRange[rhs]`.
    /// main_token is the `(`.
    AstType_call_comma,
    /// `async lhs(a, b, c)`. `SubRange[rhs]`.
    /// main_token is the `(`.
    AstType_async_call,
    /// `async lhs(a, b, c,)`. `SubRange[rhs]`.
    /// main_token is the `(`.
    AstType_async_call_comma,
    /// `switch(lhs) {}`. `SubRange[rhs]`.
    AstType_switch,
    /// Same as switch except there is known to be a trailing comma
    /// before the final rbrace
    AstType_switch_comma,
    /// `lhs => rhs`. If lhs is omitted it means `else`.
    /// main_token is the `=>`
    AstType_switch_case_one,
    /// `a, b, c => rhs`. `SubRange[lhs]`.
    /// main_token is the `=>`
    AstType_switch_case,
    /// `lhs...rhs`.
    AstType_switch_range,
    /// `while (lhs) rhs`.
    /// `while (lhs) |x| rhs`.
    AstType_while_simple,
    /// `while (lhs) : (a) b`. `WhileCont[rhs]`.
    /// `while (lhs) : (a) b`. `WhileCont[rhs]`.
    AstType_while_cont,
    /// `while (lhs) : (a) b else c`. `While[rhs]`.
    /// `while (lhs) |x| : (a) b else c`. `While[rhs]`.
    /// `while (lhs) |x| : (a) b else |y| c`. `While[rhs]`.
    AstType_while,
    /// `for (lhs) rhs`.
    AstType_for_simple,
    /// `for (lhs) a else b`. `if_list[rhs]`.
    AstType_for,
    /// `if (lhs) rhs`.
    /// `if (lhs) |a| rhs`.
    AstType_if_simple,
    /// `if (lhs) a else b`. `If[rhs]`.
    /// `if (lhs) |x| a else b`. `If[rhs]`.
    /// `if (lhs) |x| a else |y| b`. `If[rhs]`.
    AstType_if,
    /// `suspend lhs`. lhs can be omitted. rhs is unused.
    AstType_suspend,
    /// `resume lhs`. rhs is unused.
    AstType_resume,
    /// `continue`. lhs is token index of label if any. rhs is unused.
    AstType_continue,
    /// `break :lhs rhs`
    /// both lhs and rhs may be omitted.
    AstType_break,
    /// `return lhs`. lhs can be omitted. rhs is unused.
    AstType_return,
    /// `fn(a: lhs) rhs`. lhs can be omitted.
    /// anytype and ... parameters are omitted from the AST tree.
    /// main_token is the `fn` keyword.
    /// extern function declarations use this tag.
    AstType_fn_proto_simple,
    /// `fn(a: b, c: d) rhs`. `sub_range_list[lhs]`.
    /// anytype and ... parameters are omitted from the AST tree.
    /// main_token is the `fn` keyword.
    /// extern function declarations use this tag.
    AstType_fn_proto_multi,
    /// `fn(a: b) rhs addrspace(e) linksection(f) callconv(g)`. `FnProtoOne[lhs]`.
    /// zero or one parameters.
    /// anytype and ... parameters are omitted from the AST tree.
    /// main_token is the `fn` keyword.
    /// extern function declarations use this tag.
    AstType_fn_proto_one,
    /// `fn(a: b, c: d) rhs addrspace(e) linksection(f) callconv(g)`. `FnProto[lhs]`.
    /// anytype and ... parameters are omitted from the AST tree.
    /// main_token is the `fn` keyword.
    /// extern function declarations use this tag.
    AstType_fn_proto,
    /// lhs is the fn_proto.
    /// rhs is the function body block.
    /// Note that extern function declarations use the fn_proto tags rather
    /// than this one.
    AstType_fn_decl,
    /// `anyframe->rhs`. main_token is `anyframe`. `lhs` is arrow token index.
    AstType_anyframe_type,
    /// Both lhs and rhs unused.
    AstType_anyframe_literal,
    /// Both lhs and rhs unused.
    AstType_char_literal,
    /// Both lhs and rhs unused.
    AstType_integer_literal,
    /// Both lhs and rhs unused.
    AstType_float_literal,
    /// Both lhs and rhs unused.
    AstType_unreachable_literal,
    /// Both lhs and rhs unused.
    /// Most identifiers will not have explicit AST nodes, however for expressions
    /// which could be one of many different kinds of AST nodes, there will be an
    /// identifier AST node for it.
    AstType_identifier,
    /// lhs is the dot token index, rhs unused, main_token is the identifier.
    AstType_enum_literal,
    /// main_token is the string literal token
    /// Both lhs and rhs unused.
    AstType_string_literal,
    /// main_token is the first token index (redundant with lhs)
    /// lhs is the first token index; rhs is the last token index.
    /// Could be a series of multiline_string_literal_line tokens, or a single
    /// string_literal token.
    AstType_multiline_string_literal,
    /// `(lhs)`. main_token is the `(`; rhs is the token index of the `)`.
    AstType_grouped_expression,
    /// `@a(lhs, rhs)`. lhs and rhs may be omitted.
    /// main_token is the builtin token.
    AstType_builtin_call_two,
    /// Same as builtin_call_two but there is known to be a trailing comma before the rparen.
    AstType_builtin_call_two_comma,
    /// `@a(b, c)`. `sub_list[lhs..rhs]`.
    /// main_token is the builtin token.
    AstType_builtin_call,
    /// Same as builtin_call but there is known to be a trailing comma before the rparen.
    AstType_builtin_call_comma,
    /// `error{a, b}`.
    /// rhs is the rbrace, lhs is unused.
    AstType_error_set_decl,
    /// `struct {}`, `union {}`, `opaque {}`, `enum {}`. `extra_data[lhs..rhs]`.
    /// main_token is `struct`, `union`, `opaque`, `enum` keyword.
    AstType_container_decl,
    /// Same as ContainerDecl but there is known to be a trailing comma
    /// or semicolon before the rbrace.
    AstType_container_decl_trailing,
    /// `struct {lhs, rhs}`, `union {lhs, rhs}`, `opaque {lhs, rhs}`, `enum {lhs, rhs}`.
    /// lhs or rhs can be omitted.
    /// main_token is `struct`, `union`, `opaque`, `enum` keyword.
    AstType_container_decl_two,
    /// Same as ContainerDeclTwo except there is known to be a trailing comma
    /// or semicolon before the rbrace.
    AstType_container_decl_two_trailing,
    /// `union(lhs)` / `enum(lhs)`. `SubRange[rhs]`.
    AstType_container_decl_arg,
    /// Same as container_decl_arg but there is known to be a trailing
    /// comma or semicolon before the rbrace.
    AstType_container_decl_arg_trailing,
    /// `union(enum) {}`. `sub_list[lhs..rhs]`.
    /// Note that tagged unions with explicitly provided enums are represented
    /// by `container_decl_arg`.
    AstType_tagged_union,
    /// Same as tagged_union but there is known to be a trailing comma
    /// or semicolon before the rbrace.
    AstType_tagged_union_trailing,
    /// `union(enum) {lhs, rhs}`. lhs or rhs may be omitted.
    /// Note that tagged unions with explicitly provided enums are represented
    /// by `container_decl_arg`.
    AstType_tagged_union_two,
    /// Same as tagged_union_two but there is known to be a trailing comma
    /// or semicolon before the rbrace.
    AstType_tagged_union_two_trailing,
    /// `union(enum(lhs)) {}`. `SubRange[rhs]`.
    AstType_tagged_union_enum_tag,
    /// Same as tagged_union_enum_tag but there is known to be a trailing comma
    /// or semicolon before the rbrace.
    AstType_tagged_union_enum_tag_trailing,
    /// `a: lhs = rhs,`. lhs and rhs can be omitted.
    /// main_token is the field name identifier.
    /// lastToken() does not include the possible trailing comma.
    AstType_container_field_init,
    /// `a: lhs align(rhs),`. rhs can be omitted.
    /// main_token is the field name identifier.
    /// lastToken() does not include the possible trailing comma.
    AstType_container_field_align,
    /// `a: lhs align(c) = d,`. `container_field_list[rhs]`.
    /// main_token is the field name identifier.
    /// lastToken() does not include the possible trailing comma.
    AstType_container_field,
    /// `comptime lhs`. rhs unused.
    AstType_comptime,
    /// `nosuspend lhs`. rhs unused.
    AstType_nosuspend,
    /// `{lhs rhs}`. rhs or lhs can be omitted.
    /// main_token points at the lbrace.
    AstType_block_two,
    /// Same as block_two but there is known to be a semicolon before the rbrace.
    AstType_block_two_semicolon,
    /// `{}`. `sub_list[lhs..rhs]`.
    /// main_token points at the lbrace.
    AstType_block,
    /// Same as block but there is known to be a semicolon before the rbrace.
    AstType_block_semicolon,
    /// `asm(lhs)`. rhs is the token index of the rparen.
    AstType_asm_simple,
    /// `asm(lhs, a)`. `Asm[rhs]`.
    AstType_asm,
    /// `[a] "b" (c)`. lhs is 0, rhs is token index of the rparen.
    /// `[a] "b" (-> lhs)`. rhs is token index of the rparen.
    /// main_token is `a`.
    AstType_asm_output,
    /// `[a] "b" (lhs)`. rhs is token index of the rparen.
    /// main_token is `a`.
    AstType_asm_input,
    /// `error.a`. lhs is token index of `.`. rhs is token index of `a`.
    AstType_error_value,
    /// `lhs!rhs`. main_token is the `!`.
    AstType_error_union,



    AstType_invalid,
};

struct NodeData {
    uint32_t lhs;
    uint32_t rhs;
};

struct AstNode {
    uint32_t node_index;
    uint32_t token_index;
    AstType type;
    NodeData data;
};

// Must be kept in sync with NodeKind enum in kdevzigastparser.zig
enum ZNodeKind
{
    Unknown = 0,
    Module, // Root
    ContainerDecl, // struct, union
    EnumDecl,
    TemplateDecl, // fn that returns type
    FieldDecl, // container_field
    FunctionDecl,
    ParmDecl,
    VarDecl,
    BlockDecl,
    ErrorDecl,
    AliasDecl, // Import
    // Uses
    Call,
    ContainerInit,
    VarAccess,
    FieldAccess,
    ArrayAccess,
    PtrAccess // Deref
};

enum ZVisitResult
{
    Break = 0,
    Continue = 1,
    Recurse = 2
};

struct ZAst;
struct ZNode
{
    ZAst* ast;
    uint32_t index;
};

struct ZAstLocation
{
    uint32_t line;
    uint32_t column;
};

struct ZAstRange
{
    ZAstLocation start;
    ZAstLocation end;

    bool isEmpty() {
        return start.line == 0 && start.column == 0 &&
               end.line == 0 && end.column == 0;
    }
};

struct ZError
{
    int severity;
    ZAstRange range;
    const char* message;
};

typedef ZVisitResult (*CallbackFn)(ZNode node, ZNode parent, void *data);


ZAst *parse_ast(const char *name, const char *source);
uint32_t ast_error_count(ZAst *tree);
void destroy_ast(ZAst *tree);

ZError *ast_error_at(ZAst* tree, uint32_t index);
void destroy_error(ZError *err);

ZNodeKind ast_node_kind(ZNode node);

const char *ast_node_new_spelling_name(ZNode node);
// NOTE: Lines are 0 indexed so the first line is 0 not 1
ZAstRange ast_node_spelling_range(ZNode node);
ZAstRange ast_node_extent(ZNode node);

void destroy_string(const char *str);

void ast_visit(ZNode node, CallbackFn callback, void *data);

}

#endif // ZIGAST_H
