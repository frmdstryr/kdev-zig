// Copyright 2023 Jairus Martin <frmdstryr@protonmail.com>
// LGPL
const std = @import("std");
const Ast = std.zig.Ast;
const assert = std.debug.assert;

const TokenIndex = Ast.TokenIndex;
const Tag = Ast.Node.Tag;

var gpa = std.heap.GeneralPurposeAllocator(.{}){};

const SourceLocation = extern struct {
    line: u32 = 0,
    column: u32 = 0,
};

const SourceRange = extern struct {
    start: SourceLocation = .{},
    end: SourceLocation = .{}
};

const Severity = enum(c_int) {
    NoSeverity = 0,
    Error = 1,
    Warning = 2,
    Hint = 4
};

const VisitResult = enum(c_int) {
    Break = 0,
    Continue = 1,
    Recurse = 2,
};

// TODO: Just use import C ?
const NodeKind = enum(c_int) {
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
    TestDecl,
    // Uses
    Call,
    ContainerInit,
    VarAccess,
    FieldAccess,
    ArrayAccess,
    PtrAccess, // Deref
    Literal,
    Ident
};

// std.mem.len does not check for null
fn strlen(value: [*c]const u8) usize {
    if (value == null) {
        return 0;
    }
    return std.mem.len(value);
}

const ZError = extern struct {
    const Self = @This();
    severity: Severity,
    range: SourceRange,
    message: [*c]const u8,

    pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
        const msg_len = strlen(self.message);
        if (msg_len > 0) {
            allocator.free(self.message[0..msg_len+1]);
        }
        allocator.destroy(self);
    }

};

fn printAstError(ast: *Ast, filename: []const u8, source: []const u8) !void {
    const stderr = std.io.getStdErr().writer();
    for (ast.errors) |parse_error| {
        const loc = ast.tokenLocation(0, parse_error.token);
        try stderr.print("{s}:{d}:{d}: error: ", .{filename, loc.line + 1, loc.column + 1 });
        try ast.renderError(parse_error, stderr);
        try stderr.print("\n{s}\n", .{source[loc.line_start..loc.line_end]});
        {
            var i: usize = 0;
            while (i < loc.column) : (i += 1) {
                try stderr.writeAll(" ");
            }
            try stderr.writeAll("^");
        }
        try stderr.writeAll("\n");
    }
}

const TABLE_HEADER =
\\|  #  | Tag                       |   Data    |    Main token   |
\\|-----|---------------------------|-----------|-----------------|
\\
;
const TABLE_ROW_FORMAT = "|{: >5}| {s: <25} | {: >4},{: >4} | {s: <15} |\n";

pub fn dumpAstFlat(ast: *Ast, stream: anytype) !void {
    var i: usize = 0;
    const tags = ast.nodes.items(.tag);
    const node_data = ast.nodes.items(.data);
    const main_tokens = ast.nodes.items(.main_token);
    try stream.writeAll(TABLE_HEADER);
    while (i < ast.nodes.len) : (i += 1) {
        const tag = tags[i];
        const data = node_data[i];
        const main_token = main_tokens[i];
        try stream.print(TABLE_ROW_FORMAT, .{
            i, @tagName(tag), data.lhs, data.rhs, ast.tokenSlice(main_token)
        });
    }
    try stream.writeAll("\n");
}

pub fn dumpAstNodes(ast: *Ast, nodes: []TokenIndex, stream: anytype) !void {
    const tags = ast.nodes.items(.tag);
    const node_data = ast.nodes.items(.data);
    const main_tokens = ast.nodes.items(.main_token);
    try stream.writeAll(TABLE_HEADER);
    for (nodes) |i| {
        const tag = tags[i];
        const data = node_data[i];
        const main_token = main_tokens[i];
        try stream.print(TABLE_ROW_FORMAT, .{
            i, @tagName(tag), data.lhs, data.rhs, ast.tokenSlice(main_token)
        });
    }
    try stream.writeAll("\n");
}

export fn parse_ast(name_ptr: [*c]const u8, source_ptr: [*c]const u8) ?*Ast {
    const name_len = strlen(name_ptr);
    const source_len = strlen(source_ptr);

    if (name_len == 0 or source_len == 0) {
        std.log.warn("zig: name or source is empty", .{});
        return null;
    }

    const name = name_ptr[0..name_len];
    const source: [:0]const u8 = source_ptr[0..source_len:0];

    std.log.info("zig: parsing filename '{s}'...", .{name});


    const allocator = gpa.allocator();
    const ast = allocator.create(Ast) catch |err| {
        std.log.warn("zig: parsing {s} error: {}", .{name, err});
        return null;
    };
    ast.* = Ast.parse(allocator, source, .zig) catch |err| {
        std.log.warn("zig: parsing {s} error: {}", .{name, err});
        return null;
    };

    if (ast.errors.len > 0) {
        printAstError(ast, name, source) catch |err| {
            std.log.warn("zig: failed to print trace {}", .{err});
        };
    } else if (@import("builtin").is_test) {
        std.log.debug("Source ----------------\n{s}\n------------------------\n", .{source});
        var stdout = std.io.getStdOut().writer();
        dumpAstFlat(ast, stdout) catch |err| {
            std.log.debug("zig: failed to dump ast {}", .{err});
        };
    }
    return ast;
}

export fn ast_error_count(ptr: ?*Ast) u32 {
    // std.log.warn("zig: ast_error_count", .{});
    if (ptr) |ast| {
        return @intCast(ast.errors.len);
    }
    return 0;
}

export fn ast_error_at(ptr: ?*Ast, index: u32) ?*ZError {
    // std.log.warn("zig: ast_error_at {}", .{index});
    if (ptr) |ast| {
        if (index >= ast.errors.len) {
            return null;
        }
        const allocator = gpa.allocator();
        const result = allocator.create(ZError) catch {
            std.log.warn("zig: ast_error_at alloc failed", .{});
            return null;
        };
        // TODO: Expanding buffer?
        var buf: [4096]u8 = undefined;
        var fbs = std.io.fixedBufferStream(&buf);

        const err = ast.errors[index];
        const loc = ast.tokenLocation(0, err.token);
        ast.renderError(err, fbs.writer()) catch {
            std.log.warn("zig: ast_error_at alloc error message too large", .{});
            allocator.destroy(result);
            return null;
        };
        const msg = allocator.dupeZ(u8, fbs.getWritten()) catch {
            std.log.warn("zig: ast_error_at message alloc failed", .{});
            allocator.destroy(result);
            return null;
        };
        result.* = ZError{
            .severity=if (err.is_note) .Hint else .Error,
            .range=SourceRange{
                .start=SourceLocation{
                    .line=@intCast(loc.line),
                    .column=@intCast(loc.column),
                },
                .end=SourceLocation{
                    .line=@intCast(loc.line),
                    .column=@intCast(loc.column),
                },
            },
            .message=msg
        };
        return result;
    }
    return null;
}

export fn destroy_ast(ptr: ?*Ast) void {
    // std.log.debug("zig: destroy_ast {}", .{@intFromPtr(ptr)});
    if (ptr) |tree| {
        tree.deinit(gpa.allocator());
    }
}

export fn destroy_error(ptr: ?*ZError) void {
    // std.log.debug("zig: destroy_error {}", .{@intFromPtr(ptr)});
    if (ptr) |err| {
        err.deinit(gpa.allocator());
    }
}

export fn ast_node_spelling_range(ptr: ?*Ast, index: TokenIndex) SourceRange {
    if (ptr == null) {
        return SourceRange{};
    }
    const ast = ptr.?;
    if (findNodeIdentifier(ast, index)) |ident_token| {
        const loc = ast.tokenLocation(0, ident_token);
        const name = ast.tokenSlice(ident_token);
        // std.log.warn("zig: ast_node_spelling_range {} {s}", .{index, name});
        //return r;
        return SourceRange{
            .start=SourceLocation{
                .line=@intCast(loc.line),
                .column=@intCast(loc.column),
            },
            .end=SourceLocation{
                .line=@intCast(loc.line),
                .column=@intCast(loc.column + name.len),
            },
        };
    }
//     std.log.warn("zig: ast_node_spelling_range {} unknown {}", .{
//         index, ast.nodes.items(.tag)[index]
//     });
    return SourceRange{};
}

export fn ast_node_extent(ptr: ?*Ast, index: TokenIndex) SourceRange {
    if (ptr == null) {
        std.log.warn("zig: ast_node_extent ast null", .{});
        return SourceRange{};
    }
    const ast = ptr.?;
    if (index >= ast.nodes.len) {
        std.log.warn("zig: ast_node_extent index out of range {}", .{index});
        return SourceRange{};
    }
    // TODO: What is the correct start_offset?
    const first_token = ast.firstToken(index);
    const start_loc = ast.tokenLocation(0, first_token);
    const last_token = ast.lastToken(index);
    const end_loc = ast.tokenLocation(0, last_token);

    return SourceRange{
        .start=SourceLocation{
            .line=@intCast(start_loc.line),
            .column=@intCast(start_loc.column),
        },
        .end=SourceLocation{
            .line=@intCast(end_loc.line),
            .column=@intCast(end_loc.column),
        },
    };
}



pub fn kindFromAstNode(ast: *Ast, index: TokenIndex) ?NodeKind {
    if (index >= ast.nodes.len) {
        return null;
    }
    return switch (ast.nodes.items(.tag)[index]) {
        .root => .Module,
        .fn_decl => .FunctionDecl,
        .test_decl => .TestDecl,
        .simple_var_decl,
        .local_var_decl,
        .aligned_var_decl,
        .global_var_decl => .VarDecl,
        .container_decl,
        .container_decl_trailing,
        .container_decl_two,
        .container_decl_two_trailing,
        .container_decl_arg,
        .container_decl_arg_trailing => .ContainerDecl,

        .block,
        .block_semicolon,
        .block_two,
        .block_two_semicolon => .BlockDecl,
        .container_field_init,
        .container_field_align,
        .container_field => .FieldDecl,
        .error_set_decl => .ErrorDecl,
        // TODO: Param? Import? Detect if template
        .struct_init,
        .struct_init_comma,
        .struct_init_dot,
        .struct_init_dot_comma,
        .struct_init_dot_two,
        .struct_init_dot_two_comma,
        .struct_init_one,
        .struct_init_one_comma => .ContainerInit,
        .builtin_call,
        .builtin_call_comma,

        .call,
        .call_comma,
        .call_one,
        .call_one_comma,
        .async_call,
        .async_call_comma,
        .async_call_one,
        .async_call_one_comma => .Call,

        .assign,
        .assign_destructure => .VarAccess,

        .deref => .PtrAccess,

        .unwrap_optional,
        .error_value,
        .field_access => .FieldAccess,
        .array_access => .ArrayAccess,

        .string_literal,
        .multiline_string_literal,
        .number_literal,
        .char_literal => .Literal,

        .identifier => .Ident,

        // TODO
        else => .Unknown,
    };
}

export fn ast_node_kind(ptr: ?*Ast, index: TokenIndex) NodeKind {
    if (ptr) |ast| {
        if (kindFromAstNode(ast, index)) |kind| {
            // std.log.debug("zig: ast_node_kind {} is {s}", .{index, @tagName(kind)});
            return kind;
        }
    }
    return .Unknown;
}

// Lookup the token index that has the name/identifier for the given node
fn findNodeIdentifier(ast: *Ast, index: TokenIndex) ?TokenIndex {
    if (index >= ast.nodes.len) {
        return null;
    }
    const tag = ast.nodes.items(.tag)[index];
    // std.log.warn("zig: findNodeIdentifier {} {s}", .{index, @tagName(tag)});
    return switch (tag) {
        // Data lhs is the identifier
        .call,
        .call_comma,
        .call_one,
        .call_one_comma,
        .async_call,
        .async_call_comma,
        .async_call_one,
        .async_call_one_comma,
        .struct_init_one,
        .struct_init_one_comma,
        .struct_init,
        .struct_init_comma,
        .fn_decl => findNodeIdentifier(ast, ast.nodes.items(.data)[index].lhs),

        // Data rhs is the identifier
        .field_access => ast.nodes.items(.data)[index].rhs,

        // Token after main_token is the name for all these
        .fn_proto_simple,
        .fn_proto_multi,
        .fn_proto_one,
        .fn_proto,
        .global_var_decl,
        .local_var_decl,
        .simple_var_decl,
        .aligned_var_decl => ast.nodes.items(.main_token)[index] + 1,

        // Main token is identifier
        .identifier,
        .string_literal,
        .container_field,
        .container_field_init,
        .container_field_align => ast.nodes.items(.main_token)[index],
        .test_decl =>
            if (ast.nodes.items(.data)[index].lhs == 0)
                null else ast.nodes.items(.data)[index].lhs,
        else => null,
    };
}

fn indexOfNodeWithTag(ast: Ast, start_token: TokenIndex, tag: Tag) ?TokenIndex {
    if (start_token < ast.nodes.len) {
        const tags = ast.nodes.items(.tag);
        for (start_token..ast.nodes.len) |i| {
            if (tags[i] == tag) {
                return @intCast(i);
            }
        }
    }
    return null;
}

fn isTagContainerDecl(tag: Tag) bool {
    return switch (tag) {
        .container_decl,
        .container_decl_trailing,
        .container_decl_two,
        .container_decl_two_trailing,
        .container_decl_arg,
        .container_decl_arg_trailing => true,
        else => false
    };
}

fn isTagVarDecl(tag: Tag) bool {
    return switch (tag) {
        .simple_var_decl,
        .local_var_decl,
        .aligned_var_decl,
        .global_var_decl => true,
        else => false
    };
}

fn isNodeConstVarDecl(ast: *Ast, index: TokenIndex) bool {
    if (index < ast.nodes.len and isTagVarDecl(ast.nodes.items(.tag)[index])) {
        const main_token = ast.nodes.items(.main_token)[index];
        return ast.tokens.items(.tag)[main_token] == .keyword_const;
    }
    return false;
}

fn testNodeIdent(source: [:0]const u8, tag: Tag, expected: ?[]const u8) !void {
    const allocator = std.testing.allocator;
    var ast = try Ast.parse(allocator, source, .zig);
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&ast, "", source);
    }
    try std.testing.expect(ast.errors.len == 0);
    try dumpAstFlat(&ast, stdout);

    const index = indexOfNodeWithTag(ast, 0, tag).?;
    const r = findNodeIdentifier(&ast, index);
    if (expected) |str| {
        try std.testing.expectEqualSlices(u8, ast.tokenSlice(r.?), str);
        try stdout.print("location={}\n", .{ast.tokenLocation(0, r.?)});
    } else {
        try std.testing.expect(r == null);
    }
}

test "find-node-ident" {
    // Note: It uses the first node with the token found
    try testNodeIdent("pub fn main () void {}", .fn_decl, "main");
    try testNodeIdent("fn main () void {}", .fn_decl, "main");
    try testNodeIdent("inline fn main () void {}", .fn_decl, "main");
    try testNodeIdent("pub fn main(arg: u8) void {}", .fn_decl, "main");
    try testNodeIdent("pub fn main(a: u8, b: bool) void {}", .fn_decl, "main");
    try testNodeIdent("var foo = 1;", .simple_var_decl, "foo");
    try testNodeIdent("const foo = 1;", .simple_var_decl, "foo");
    try testNodeIdent("var foo: u8 = undefined;", .simple_var_decl, "foo");
    try testNodeIdent("const Bar = struct {};", .simple_var_decl, "Bar");
    try testNodeIdent("const Bar = struct {\npub fn foo() void {}\n};", .fn_decl, "foo");
    try testNodeIdent("pub fn main(a: u8) callconv(.C) void {}", .fn_decl, "main");
    try testNodeIdent("pub fn main(a: u8, b: bool) callconv(.C) void {}", .fn_decl, "main");
    try testNodeIdent("const Bar = struct {foo: u8,};", .container_field_init, "foo");
    try testNodeIdent("const Bar = struct {foo: bool align(4),};", .container_field_align, "foo");
    try testNodeIdent("pub fn main() u8 {}", .identifier, "u8");
    try testNodeIdent("pub fn main(a: bool) void {}", .identifier, "bool");
    try testNodeIdent("pub fn main() void {var a: u8 = 0;}", .simple_var_decl, "a");
    try testNodeIdent("const Foo = struct{ a: u8 = 0};\nvar foo = Foo{};", .struct_init_one, "Foo");
    try testNodeIdent("const Foo = struct{ a: u8 = 0};\nvar foo = Foo{.a=0,};", .struct_init_one_comma, "Foo");
    try testNodeIdent("test \"first\" { }", .test_decl, "\"first\"");
    try testNodeIdent("const Foo = struct{ a: u8 = 0};\nvar foo = Foo{.a=0,};", .struct_init_one_comma, "Foo");

    // Calls
    try testNodeIdent("pub fn main() void { foo(); }", .call_one, "foo");
    try testNodeIdent("pub fn main() void { foo(1,); }", .call_one_comma, "foo");
    try testNodeIdent("pub fn main() void { async foo(1); }", .async_call_one, "foo");
    try testNodeIdent("pub fn main() void { async foo(1,); }", .async_call_one_comma, "foo");
    try testNodeIdent("pub fn main() void { foo(1,2); }", .call, "foo");
    try testNodeIdent("pub fn main() void { async foo(1,2); }", .async_call, "foo");
    try testNodeIdent("pub fn main() void { foo(1,2,); }", .call_comma, "foo");
    try testNodeIdent("pub fn main() void { async foo(1,2,); }", .async_call_comma, "foo");
}

fn testSpellingRange(source: [:0]const u8, tag: Tag, expected: SourceRange) !void {
    const allocator = std.testing.allocator;
    var ast = try Ast.parse(allocator, source, .zig);
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&ast, "", source);
    }
    try std.testing.expect(ast.errors.len == 0);
    try dumpAstFlat(&ast, stdout);

    const index = indexOfNodeWithTag(ast, 0, tag).?;
    const range = ast_node_spelling_range(&ast, index);
    std.log.warn("zig: {} range {}", .{index, range});
    try std.testing.expectEqual(expected.start.line, range.start.line);
    try std.testing.expectEqual(expected.start.column, range.start.column);
    try std.testing.expectEqual(expected.end.line, range.end.line);
    try std.testing.expectEqual(expected.end.column, range.end.column);
}

test "spelling-range" {
    try testSpellingRange("pub fn foo() void {}", .fn_decl, .{
        .start=.{.line=0, .column=7}, .end=.{.line=0, .column=10}});
    try testSpellingRange("const Foo = struct {\n pub fn foo() void {}\n};", .fn_decl, .{
        .start=.{.line=1, .column=8}, .end=.{.line=1, .column=11}});
    try testSpellingRange("pub fn foo() void {\n var x = 1;\n}", .simple_var_decl, .{
        .start=.{.line=1, .column=5}, .end=.{.line=1, .column=6}});
    try testSpellingRange("pub fn foo() void {\n if (true) {\n  var x = 1;}\n}", .simple_var_decl, .{
        .start=.{.line=2, .column=6}, .end=.{.line=2, .column=7}});
    try testSpellingRange("const Foo = struct {a: bool};\nvar foo = Foo{};", .struct_init_one, .{
        .start=.{.line=1, .column=10}, .end=.{.line=1, .column=13}});
    try testSpellingRange("var x = y.foo();", .call_one, .{
        .start=.{.line=0, .column=10}, .end=.{.line=0, .column=13}});
}

// Caller must free with destroy_string
export fn ast_node_new_spelling_name(ptr: ?*Ast, index: TokenIndex) ?[*]const u8 {
    if (ptr == null) {
        return null;
    }
    const ast = ptr.?;
    if (index >= ast.nodes.len) {
        std.log.warn("zig: token name index={} is out of range", .{index});
        return null;
    }
    const tag = ast.nodes.items(.tag)[index];
    if (findNodeIdentifier(ast, index)) |ident_token| {
        const name = if (tag == .test_decl)
            std.mem.trim(u8, ast.tokenSlice(ident_token), "\"")
            else ast.tokenSlice(ident_token);
        // std.log.debug("zig: token name {} {s} = '{s}' ({})", .{index, @tagName(tag), name, name.len});
        const copy = gpa.allocator().dupeZ(u8, name) catch {
            return null;
        };
        return copy.ptr;
    }
    // std.log.warn("zig: token name index={} tag={s} is unknown", .{index, @tagName(tag)});
    return null;
}

export fn destroy_string(str: [*c]const u8) void {
    // std.log.warn("zig: destroy_str {}", .{@intFromPtr(str)});
    const len = strlen(str);
    if (len > 0) {
        const allocator = gpa.allocator();
        allocator.free(str[0..len + 1]);
    }
}


export fn ast_format(
    source_ptr: [*c]const u8,
) ?[*]const u8 {
    if (source_ptr == null) {
        return null;
    }
    const source_len = strlen(source_ptr);
    if (source_len == 0) {
        return null;
    }
    const source: [:0]const u8 = source_ptr[0..source_len:0];
    const allocator = gpa.allocator();
    var ast = Ast.parse(allocator, source, .zig) catch |err| {
        std.log.warn("zig: format error: {}\n", .{err});
        return null;
    };
    defer ast.deinit(allocator);
    const formatted = ast.render(allocator) catch |err| {
        std.log.warn("zig: format error: {}\n", .{err});
        return null;
    };
    return formatted.ptr;
}

fn visitOne(ptr: ?*Ast, node: TokenIndex, parent: TokenIndex, data: ?*anyopaque) callconv(.C) VisitResult {
    _ = ptr;
    _ = parent;
    if (data) |index_ptr| {
        const index: *TokenIndex = @ptrCast(@alignCast(index_ptr));
        index.* = node;
    } else {
        std.log.warn("zig: visit one data is null", .{});
    }
    return .Break;
}

// Visit one child
export fn ast_visit_one_child(ptr: ?*Ast, node: TokenIndex) TokenIndex {
    var result: TokenIndex = 0;
    ast_visit(ptr, node, visitOne, &result);
    return result;
}



fn visitAll(ptr: ?*Ast, child: TokenIndex, parent: TokenIndex, data: ?*anyopaque) callconv(.C) VisitResult {
    _ = ptr;
    _ = parent;
    if (data) |nodes_ptr| {
        const nodes: *std.ArrayList(TokenIndex) = @ptrCast(@alignCast(nodes_ptr));
        const remaining = nodes.unusedCapacitySlice();
        if (remaining.len > 0) {
            remaining[0] = child;
            nodes.items.len += 1;
            return .Recurse;
        }
    }
    return .Break;

}

// Visit all nodes up to the capacity provided in the node list
fn astVisitAll(ptr: ?*Ast, node: TokenIndex, nodes: *std.ArrayList(TokenIndex)) []TokenIndex {
    ast_visit(ptr, node, visitAll, @ptrCast(nodes));
    return nodes.items[0..nodes.items.len];
}

fn testVisitChild(source: [:0]const u8, tag: Tag, expected: Tag) !void {
    const allocator = std.testing.allocator;
    var ast = try Ast.parse(allocator, source, .zig);
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&ast, "", source);
    }
    try std.testing.expect(ast.errors.len == 0);
    try dumpAstFlat(&ast, stdout);

    const index = indexOfNodeWithTag(ast, 0, tag).?;
    const child = ast_visit_one_child(&ast, index);
    const result = ast.nodes.items(.tag)[child];
    try std.testing.expectEqual(expected, result);
}

test "child-node" {
    try testVisitChild("test { try main(); }", .@"try", .call_one);
    try testVisitChild("test { main(); }", .call_one, .identifier);
    try testVisitChild("test { a.main(); }", .call_one, .field_access);
    try testVisitChild("test { a.main(); }", .field_access, .identifier);
}


pub fn assertNodeIndexValid(ptr:?*Ast, child: TokenIndex, parent: TokenIndex) void {
    if (ptr) |ast| {
        if (child == 0 or child > ast.nodes.len) {
            var stderr = std.io.getStdErr().writer();
            dumpAstFlat(ast, stderr) catch {};
            std.log.err("zig: ast_visit child index {} from parent {} is out of range or will create a loop", .{
                child, parent
            });
            assert(false);
        }
    }
}


const CallbackFn = *const fn(ptr: ?*Ast, node: TokenIndex, parent: TokenIndex, data: ?*anyopaque) callconv(.C) VisitResult;

// Return whether to continue or not
export fn ast_visit(ptr: ?*Ast, parent: TokenIndex, callback: CallbackFn , data: ?*anyopaque) void {
    if (ptr == null) {
        std.log.warn("zig: ast_visit ast is null", .{});
        return;
    }
    const ast = ptr.?;
    if (parent > ast.nodes.len) {
        std.log.warn("zig: ast_visit node index {} is out of range", .{parent});
        return;
    }
    const tag = ast.nodes.items(.tag)[parent];
    const d = ast.nodes.items(.data)[parent];
    std.log.debug("zig: ast_visit {s} at {} '{s}'", .{
        @tagName(tag),
        parent,
        ast.tokenSlice(ast.nodes.items(.main_token)[parent])
    });
    switch (tag) {
        .root => for (ast.rootDecls()) |child| {
            // std.log.warn("zig: ast_visit root_decl index={}", .{decl_index});
            assertNodeIndexValid(ast, child, parent);
            switch (callback(ast, child, parent, data)) {
                .Break => return,
                .Continue => continue,
                .Recurse => ast_visit(ast, child, callback, data),
            }
        },
        // Recurse to both lhs and rhs, neither are optional
        .for_simple,
        .if_simple,
        .equal_equal,
        .bang_equal,
        .less_than,
        .greater_than,
        .less_or_equal,
        .greater_or_equal,
        .assign_mul,
        .assign_div,
        .assign_mod,
        .assign_add,
        .assign_sub,
        .assign_shl,
        .assign_shl_sat,
        .assign_shr,
        .assign_bit_and,
        .assign_bit_xor,
        .assign_bit_or,
        .assign_mul_wrap,
        .assign_add_wrap,
        .assign_sub_wrap,
        .assign_mul_sat,
        .assign_add_sat,
        .assign_sub_sat,
        // TODO: .assign_destructure
        .assign,
        .merge_error_sets,
        .mul,
        .div,
        .mod,
        .array_mult,
        .mul_wrap,
        .mul_sat,
        .add,
        .sub,
        .array_cat,
        .add_wrap,
        .sub_wrap,
        .add_sat,
        .sub_sat,
        .shl,
        .shl_sat,
        .shr,
        .bit_and,
        .bit_xor,
        .bit_or,
        .@"orelse",
        .bool_and,
        .bool_or,
        .array_type,
        // TODO array_type_sentinel
        .slice_open,
        // TODO .slice_sentinel, ???
        .array_access,
        .array_init_one_comma,
        .struct_init_one_comma,
        .switch_range,
        .while_simple,
        .error_union => {
            {
                const child = d.lhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
            {
                const child = d.rhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
        },
        // Only walk data lhs
        .bool_not,
        .negation,
        .bit_not,
        .negation_wrap,
        .address_of,
        .@"try",
        .@"await",
        .optional_type,
        .deref,
        .@"comptime",
        .@"nosuspend",
        .@"resume",
        .@"return",
        .@"suspend",
        .@"usingnamespace",
        .field_access,
        .unwrap_optional,
        .grouped_expression => if (d.lhs != 0 ) {
            const child = d.lhs;
            assertNodeIndexValid(ast, child, parent);
            switch (callback(ast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => ast_visit(ast, child, callback, data),
            }
        },

        // Only walk data rhs
        .@"defer",
        .@"errdefer",
        .@"break",
        .anyframe_type,
        .test_decl => if (d.rhs != 0) {
            const child = d.rhs;
            assertNodeIndexValid(ast, child, parent);
            switch (callback(ast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => ast_visit(ast, child, callback, data),
            }
        },

        // For all of these walk lhs and/or rhs of the node's data
        .for_range, // rhs is optional
        .struct_init_one,
        .struct_init_dot_two,
        .struct_init_dot_two_comma,
        .simple_var_decl,
        .aligned_var_decl,
        .container_decl_two,
        .container_decl_two_trailing,
        .container_field_init,
        .container_field_align,
        .call_one,
        .call_one_comma,
        .async_call_one,
        .async_call_one_comma,
        .builtin_call_two,
        .builtin_call_two_comma,
        .ptr_type_aligned,
        .ptr_type_sentinel,
        // TODO ptr_type
        .array_init_one,
        .array_init_dot_two,
        .array_init_dot_two_comma,
        .@"catch",
        .switch_case_one,
        .switch_case_inline_one,
        .fn_proto_simple,
        .fn_decl,
        .block_two,
        .block_two_semicolon => {
            if (d.lhs != 0) {
                const child = d.lhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
            if (d.rhs != 0) {
                const child = d.rhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
        },
        // For these walk all sub list nodes in extra data
        .struct_init_dot,
        .struct_init_dot_comma,
        .builtin_call,
        .builtin_call_comma,
        .container_decl,
        .container_decl_trailing,
        .array_init_dot,
        .array_init_dot_comma,
        .block,
        .block_semicolon => {
            for (ast.extra_data[d.lhs..d.rhs]) |child| {
                // std.log.warn("zig: ast_visit root_decl index={}", .{decl_index});
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
        },

        // Special nodes

        // Visit lhs and rhs as sub range
        .call,
        .call_comma,
        .async_call,
        .async_call_comma,
        .container_decl_arg,
        .container_decl_arg_trailing,
        .@"switch",
        .switch_comma,
        .array_init,
        .array_init_comma,
        .struct_init,
        .struct_init_comma => {
            {
                const child = d.lhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
            const field_range = ast.extraData(d.rhs, Ast.Node.SubRange);
            for (ast.extra_data[field_range.start..field_range.end]) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }

        },
        // Visit lhs sub range, then rhs
        .switch_case,
        .switch_case_inline,
        .fn_proto_multi => {
            const field_range = ast.extraData(d.lhs, Ast.Node.SubRange);
            for (ast.extra_data[field_range.start..field_range.end]) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
            const child = d.rhs;
            assertNodeIndexValid(ast, child, parent);
            switch (callback(ast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => ast_visit(ast, child, callback, data),
            }
        },
        .while_cont => {
            const while_data = ast.extraData(d.rhs, Ast.Node.WhileCont);
            inline for (.{d.lhs, while_data.cont_expr, while_data.then_expr}) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
        },
        .@"while" => {
            const while_data = ast.extraData(d.rhs, Ast.Node.While);
            inline for (.{d.lhs, while_data.cont_expr, while_data.then_expr, while_data.else_expr}) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
        },


        .@"if" => {
            const if_data = ast.extraData(d.rhs, Ast.Node.If);
            inline for (.{d.lhs, if_data.then_expr, if_data.else_expr}) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
        },

        .@"for" => {
            // See std.zig.Ast.forFull
            const extra = @as(Ast.Node.For, @bitCast(d.rhs));
            for (ast.extra_data[d.lhs..][0..extra.inputs]) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }

            // For body
            {
                const child = ast.extra_data[d.lhs+extra.inputs];
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }

            // Else body
            if (extra.has_else) {
                const child = ast.extra_data[d.lhs+extra.inputs+1];
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }

        },
        .fn_proto,
        .fn_proto_one => |t| {
            var buf: [1]Ast.Node.Index = undefined;
            const fn_proto = if (t == .fn_proto_one)
                ast.fnProtoOne(&buf, parent).ast
                else ast.fnProto(parent).ast;
            for (fn_proto.params) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
            inline for (.{d.rhs, fn_proto.align_expr, fn_proto.addrspace_expr, fn_proto.section_expr, fn_proto.callconv_expr}) |child| {
                if (child != 0 ) {
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => ast_visit(ast, child, callback, data),
                    }
                }
            }
        },

        .local_var_decl => {
            const var_data = ast.extraData(d.lhs, Ast.Node.LocalVarDecl);
            inline for (.{var_data.type_node, var_data.align_node, d.rhs}) |child| {
                if (child != 0 ) {
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => ast_visit(ast, child, callback, data),
                    }
                }
            }
        },

        .array_type_sentinel => {
            const array_data = ast.extraData(d.rhs, Ast.Node.ArrayTypeSentinel);
            inline for (.{d.lhs, array_data.sentinel, array_data.elem_type}) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }

        },

        .slice => {
            const slice_data = ast.extraData(d.rhs, Ast.Node.Slice);
            inline for (.{d.lhs, slice_data.start, slice_data.end}) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }
        },

        .slice_sentinel => {
            const slice_data = ast.extraData(d.rhs, Ast.Node.SliceSentinel);
            inline for (.{d.lhs, slice_data.start, slice_data.end, slice_data.sentinel}) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(ast, child, callback, data),
                }
            }

        },

        // Leaf nodes have no children
        .@"continue",
        .string_literal,
        .multiline_string_literal,
        .char_literal,
        .number_literal,
        .enum_literal,
        .anyframe_literal,
        .unreachable_literal,
        .error_value,
        .identifier => {},
        else => {
            if (@import("builtin").is_test) {
                std.log.debug("zig: ast_visit unhandled {s}", .{@tagName(tag)});
            }
        }
    }
    return;
}


fn testVisitTree(source: [:0]const u8, expectedTag: ?Tag) !void {
    const allocator = std.testing.allocator;
    var ast = try Ast.parse(allocator, source, .zig);
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&ast, "", source);
    }
    try std.testing.expect(ast.errors.len == 0);
    try dumpAstFlat(&ast, stdout);

    var nodes = try std.ArrayList(TokenIndex).initCapacity(allocator, ast.nodes.len+1);
    nodes.appendAssumeCapacity(0); // Callback does not call on initial node
    defer nodes.deinit();
    try stdout.writeAll("Visited order\n");
    const visited = astVisitAll(&ast, 0, &nodes);
    try dumpAstNodes(&ast, visited, stdout);
    try std.testing.expectEqual(ast.nodes.len, visited.len);

    // Sort visited nodes and make sure each was hit
    std.mem.sort(TokenIndex, visited, {}, std.sort.asc(TokenIndex));
    for (visited, 0..) |a, b| {
        try std.testing.expectEqual(@as(TokenIndex, @intCast(b)), a);
    }

    // If given, make sure the tag is actually used in the source parsed
    if (expectedTag) |tag| {
        if (indexOfNodeWithTag(ast, 0, tag) == null) {
            std.log.err("Expected tag {} not found in ast\n", .{tag});
            assert(false);
        }
    }
}

test "ast-visit" {
    // Test that the visitor reaches each node
    try testVisitTree("test { }", .test_decl);
    try testVisitTree("test { var a = 0; }", .simple_var_decl);
    try testVisitTree("test { var a: u8 align(4) = 0; }", .local_var_decl);
    //try testVisitTree("var a: u8 align(4) = 0;", .global_var_decl); ???
    try testVisitTree("test { var a align(4) = 0; }", .aligned_var_decl);
    try testVisitTree("test { var a = 0; errdefer {a = 1;} }", .@"errdefer");
    try testVisitTree("test { errdefer |err| { @panic(@errorName(err));} }", .@"errdefer");
    try testVisitTree("test { var a = 0; defer { a = 1; } }", .@"defer");
    try testVisitTree("test { foo() catch {}; }", .@"catch");
    try testVisitTree("test { foo() catch |err| { @panic(@errorName(err)); }; }", .@"catch");
    try testVisitTree("const A = {var a: u8 = 0;}; test { A.a = 1; }", .field_access);
    try testVisitTree("test{ a.? = 0; }", .unwrap_optional);
    try testVisitTree("test { var a = 0 == 1; }", .equal_equal);
    try testVisitTree("test { var a = 0 != 1; }", .bang_equal);
    try testVisitTree("test { var a = 0 < 1; }", .less_than);
    try testVisitTree("test { var a = 0 <= 1; }", .less_or_equal);
    try testVisitTree("test { var a = 0 >= 1; }", .greater_or_equal);
    try testVisitTree("test { var a = 0 > 1; }", .greater_than);
    try testVisitTree("test { var a = 0; a *= 1; }", .assign_mul);
    try testVisitTree("test { var a = 0; a /= 1; }", .assign_div);
    try testVisitTree("test { var a = 0; a %= 1; }", .assign_mod);
    try testVisitTree("test { var a = 0; a += 1; }", .assign_add);
    try testVisitTree("test { var a = 0; a -= 1; }", .assign_sub);
    try testVisitTree("test { var a = 1; a <<= 1; }", .assign_shl);
    try testVisitTree("test { var a = 1; a <<|= 1; }", .assign_shl_sat);
    try testVisitTree("test { var a = 2; a >>= 1; }", .assign_shr);
    try testVisitTree("test { var a = 2; a &= 3; }", .assign_bit_and);
    try testVisitTree("test { var a = 2; a ^= 1; }", .assign_bit_xor);
    try testVisitTree("test { var a = 2; a |= 1; }", .assign_bit_or);
    try testVisitTree("test { var a = 2; a *%= 0xFF; }", .assign_mul_wrap);
    try testVisitTree("test { var a = 2; a +%= 0xFF; }", .assign_add_wrap);
    try testVisitTree("test { var a = 2; a -%= 0xFF; }", .assign_sub_wrap);
    try testVisitTree("test { var a = 2; a = 1; }", .assign);
    // TODO: assign_destructure
    try testVisitTree("const E1 = error{E1}; const E2 = E1 || error{E3};", .merge_error_sets);
    try testVisitTree("test { var a = 2; a = 2 * a; }", .mul);
    try testVisitTree("test { var a = 2; a = a / 2; }", .div);
    try testVisitTree("test { var a = 2; a = a % 2; }", .mod);
    try testVisitTree("test { var a = [2]u8{1, 2} ** 2; }", .array_mult);
    try testVisitTree("test { var a: u8 = 2 *% 0xFF;}", .mul_wrap);
    try testVisitTree("test { var a: u8 = 2 *| 0xFF;}", .mul_sat);
    try testVisitTree("test { var a: u8 = 2 + 0xF0;}", .add);
    try testVisitTree("test { var a: u8 = 0xF0 - 0x2;}", .sub);
    try testVisitTree("test { var a = [2]u8{1, 2} ++ [_]u8{3}; }", .array_cat);
    try testVisitTree("test { var a: u8 = 2 +% 0xFF;}", .add_wrap);
    try testVisitTree("test { var a: u8 = 2 -% 0xFF;}", .sub_wrap);
    try testVisitTree("test { var a: u8 = 2 +| 0xFF;}", .add_sat);
    try testVisitTree("test { var a: u8 = 2 -| 0xFF;}", .sub_sat);
    try testVisitTree("test { var a: u8 = 2 << 1;}", .shl);
    try testVisitTree("test { var a: u8 = 2 <<| 10;}", .shl_sat);
    try testVisitTree("test { var a: u8 = 2 >> 1;}", .shr);
    try testVisitTree("test { var a: u8 = 2 & 1;}", .bit_and);
    try testVisitTree("test { var a: u8 = 2 ^ 1;}", .bit_xor);
    try testVisitTree("test { var a: u8 = 2 | 1;}", .bit_or);
    try testVisitTree("test { var a: u8 = null orelse 1;}", .@"orelse");
    try testVisitTree("test { var a = true and true;}", .bool_and);
    try testVisitTree("test { var a = true or false;}", .bool_or);
    try testVisitTree("test { var a = !true; }", .bool_not);
    try testVisitTree("test { var a = -foo(); }", .negation);
    try testVisitTree("test { var a = ~0x4; }", .bit_not);
    try testVisitTree("test { var a = -%foo(); }", .negation_wrap);
    try testVisitTree("test { var a = 0; var b = &a; }", .address_of);
    try testVisitTree("test { try foo(); }", .@"try");
    try testVisitTree("test { await foo(); }", .@"await");
    try testVisitTree("test { var a: ?bool = null; }", .optional_type);
    try testVisitTree("test { var a: [2]u8 = undefined; }", .array_type);
    try testVisitTree("test { var a: [2:0]u8 = undefined; }", .array_type_sentinel);
    try testVisitTree("test { var a: *align(8) u8 = undefined; }", .ptr_type_aligned);
    try testVisitTree("test { var a: [*]align(8) u8 = undefined; }", .ptr_type_aligned);
    try testVisitTree("test { var a: []u8 = undefined; }", .ptr_type_aligned);
    try testVisitTree("test { var a: [*:0]u8 = undefined; }", .ptr_type_sentinel);
    try testVisitTree("test { var a: [:0]u8 = undefined; }", .ptr_type_sentinel);
    // try testVisitTree("test { var a: *u8 = undefined; }", .ptr_type_sentinel); // FIXME: maybe docs incorrect ?
    // try testVisitTree("test { var a: [*c]u8 = undefined; }", .ptr_type);  // TODO: How
    // try testVisitTree("test { var a: [*c]u8 = undefined; }", .ptr_type_bit_range);  // TODO: How
    try testVisitTree("test { var a = [2]u8{1,2}; var b = a[0..]; }", .slice_open);
    try testVisitTree("test { var a = [2]u8{1,2}; var b = a[0..1]; }", .slice);
    try testVisitTree("test { var a = [2]u8{1,2}; var b = a[0..1:0]; }", .slice_sentinel);
    try testVisitTree("test { var a = 0; var b = &a; var c = b.*; }", .deref);
    try testVisitTree("test { var a = [_]u8{1}; }", .array_init_one);
    try testVisitTree("test { var a = [_]u8{1,}; }", .array_init_one_comma);
    try testVisitTree("test { var a: [2]u8 = .{1,2}; }", .array_init_dot_two);
    try testVisitTree("test { var a: [2]u8 = .{1,2,}; }", .array_init_dot_two_comma);
    try testVisitTree("test { var a = [_]u8{1,2,3,4,5}; }", .array_init);
    try testVisitTree("test { var a = [_]u8{1,2,3,}; }", .array_init_comma);
    try testVisitTree("const A = struct {a: u8};test { var a = A{.a=0}; }", .struct_init_one);
    try testVisitTree("const A = struct {a: u8};test { var a = A{.a=0,}; }", .struct_init_one_comma);
    try testVisitTree("const A = struct {a: u8, b: u8};test { var a: A = .{.a=0,.b=1}; }", .struct_init_dot_two);
    try testVisitTree("const A = struct {a: u8, b: u8};test { var a: A = .{.a=0,.b=1,}; }", .struct_init_dot_two_comma);
    try testVisitTree("const A = struct {a: u8, b: u8, c: u8};test { var a: A = .{.a=0,.b=1,.c=2}; }", .struct_init_dot);
    try testVisitTree("const A = struct {a: u8, b: u8, c: u8};test { var a: A = .{.a=0,.b=1,.c=2,}; }", .struct_init_dot_comma);
    try testVisitTree("const A = struct {a: u8, b: u8, c: u8};test { var a = A{.a=0,.b=1,.c=2}; }", .struct_init);
    try testVisitTree("const A = struct {a: u8, b: u8, c: u8};test { var a = A{.a=0,.b=1,.c=2,}; }", .struct_init_comma);
    try testVisitTree("pub fn main(a: u8) void {}\ntest { main(1); }", .call_one);
    try testVisitTree("pub fn main(a: u8) void {}\ntest { main(1,); }", .call_one_comma);
    try testVisitTree("pub fn main(a: u8) void {}\ntest { async main(1); }", .async_call_one);
    try testVisitTree("pub fn main(a: u8) void {}\ntest { async main(1,); }", .async_call_one_comma);
    try testVisitTree("pub fn main(a: u8, b: u8, c: u8) void {}\ntest { main(1, 2, 3); }", .call);
    try testVisitTree("pub fn main(a: u8, b: u8, c: u8) void {}\ntest { main(1, 2, 3, ); }", .call_comma);
    try testVisitTree("pub fn main(a: u8, b: u8, c: u8) void {}\ntest { async main(1, 2, 3); }", .async_call);
    try testVisitTree("pub fn main(a: u8, b: u8, c: u8) void {}\ntest { async main(1, 2, 3, ); }", .async_call_comma);
    try testVisitTree("test { var i: u1 = 0; switch (i) {0=>{}, 1=>{}} }", .@"switch");
    try testVisitTree("test { var a = \"ab\"; switch (a[0]) {'a'=> {}, else=>{},} }", .switch_comma);
    try testVisitTree("test { var a = \"ab\"; switch (a[0]) {'a'=> {}, else=>{},} }", .switch_case_one);
    try testVisitTree("test { var i: u1 = 0; switch (i) {0=>{}, inline else=>{}} }", .switch_case_inline_one);
    try testVisitTree("test { var i: u1 = 0; switch (i) {0, 1=>{} } }", .switch_case);
    try testVisitTree("test { var i: u1 = 0; switch (i) {inline 0, 1=>{} } }", .switch_case_inline);
    try testVisitTree("test { var i: u8 = 0; switch (i) {0...8=>{}, else=>{}} }", .switch_range);
    try testVisitTree("test { while (true) {} }", .while_simple);
    try testVisitTree("test {var opt: ?u8 = null; while (opt) |v| { _ = v; } }", .while_simple); // TODO: Where is the x ?
    try testVisitTree("test {var i = 0; while (i < 10) : (i+=1) {} }", .while_cont);
    try testVisitTree("test {var i = 0; while (i < 0) : (i+=1) {} else {} }", .@"while");
    try testVisitTree("test {var opt: ?u8 = null; while (opt) |v| : (opt = null) { _ = v; } else {} }", .@"while"); // ???
    try testVisitTree("test {\n for ([2]u8{1,2}) |i| {\n  print(i);\n }\n}", .for_simple);
    // TODO: For
    try testVisitTree("test {\n for ([2]u8{1,2}, [2]u8{3,4}) |a, b| {\n  print(a + b);\n }\n}", .@"for");
    try testVisitTree("test {\n for ([2]u8{1,2}, 0..) |a, i| {\n  print(a + i);\n }\n}", .@"for");
    try testVisitTree("test {var x = [_]u8{}; for (x)|i| {print(i);} else {print(0);}}", .@"for");
    try testVisitTree("test {\n for (0..2) |i| {\n  print(i);\n }\n}", .for_range);
    try testVisitTree("test {\n for (0..0) |i| {\n  print(i); if (i == 2) break;\n }\n}", .for_range);
    try testVisitTree("test { if (true) { var a = 0; } }", .if_simple);
    try testVisitTree("test {var x = if (true) 1 else 2; }", .@"if");
    try testVisitTree("test {var x: ?anyframe = null; suspend x;}", .@"suspend");
    try testVisitTree("test {var x: ?anyframe = null; resume x.?;}", .@"resume");
    try testVisitTree("test {var i: usize = 0; outer: while (i < 10) : (i += 1) { while (true) { continue :outer; } }}", .@"continue");
    try testVisitTree("test {var i: usize = 0; while (i < 10) : (i += 1) { continue; } }", .@"continue");
    try testVisitTree("test {var i: usize = 0; while (i < 10) : (i += 1) { break; } }", .@"break");
    try testVisitTree("test {var i: usize = 0; outer: while (i < 10) : (i += 1) { while (true) { break :outer; } }}", .@"break");
    try testVisitTree("pub fn foo() u8 { return 1; }", .@"return");
    try testVisitTree("pub fn foo() void { return; }", .@"return");
    try testVisitTree("pub fn foo(a: u8) u8 { return a; }", .fn_proto_simple);
    try testVisitTree("pub fn foo(a: u8, b: u8) u8 { return a + b; }", .fn_proto_multi);
    try testVisitTree("pub fn foo(a: u8) callconv(.C) u8 { return a; }", .fn_proto_one);
    try testVisitTree("pub fn foo(a: u8, b: u8) callconv(.C) u8 { return a + b; }", .fn_proto);
    try testVisitTree("pub fn foo(a: u8, b: u8) callconv(.C) u8 { return a + b; }", .fn_decl);
    try testVisitTree("test {var f: anyframe = undefined; anyframe->foo;}", .anyframe_type);
    try testVisitTree("test {var f: anyframe = undefined;}", .anyframe_literal);
    try testVisitTree("test {var f: u8 = 'c';}", .char_literal);
    try testVisitTree("test {var f: u8 = 0;}", .number_literal);
    try testVisitTree("test {if (false) {unreachable;}}", .unreachable_literal);
    try testVisitTree("test {var f: u8 = 0;}", .identifier);
    try testVisitTree("const A = enum {a, b, c}; test {var x: A = .a;}", .enum_literal);
    try testVisitTree("test {var x = \"abcd\";}", .string_literal);
}
