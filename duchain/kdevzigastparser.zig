// Copyright 2023 Jairus Martin <frmdstryr@protonmail.com>
// LGPL
const std = @import("std");
const Ast = std.zig.Ast;
const assert = std.debug.assert;

var gpa = std.heap.GeneralPurposeAllocator(.{}){};

const ZAstLocation = extern struct {
    line: u32 = 0,
    column: u32 = 0,
};

const ZAstRange = extern struct {
    start: ZAstLocation = .{},
    end: ZAstLocation = .{}
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
    // Uses
    Call,
    ContainerInit,
    VarAccess,
    FieldAccess,
    ArrayAccess,
    PtrAccess, // Deref

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
    range: ZAstRange,
    message: [*c]const u8,

    pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
        const msg_len = strlen(self.message);
        if (msg_len > 0) {
            allocator.free(self.message[0..msg_len+1]);
        }
        allocator.destroy(self);
    }

};

const ZNode = extern struct {
    const Self = @This();
    ast: ?*Ast,
    index: Ast.TokenIndex,

    pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
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

pub fn dumpAstFlat(ast: *Ast, stream: anytype) !void {
    var i: usize = 0;
    var indent: usize = 0;
    while (i < ast.nodes.len) : (i += 1) {
        const tag = ast.nodes.items(.tag)[i];
        const data = ast.nodes.items(.data)[i];
        const main_token = ast.nodes.items(.main_token)[i];
        try stream.writeByteNTimes(' ', indent);
        try stream.print("{s}", .{@tagName(tag)});
        try stream.writeAll("{\n");
        {
            indent += 2;
            try stream.writeByteNTimes(' ', indent);
            try stream.print("i={}, data=({},{}) slice={s}\n", .{i, data.lhs, data.rhs, ast.tokenSlice(main_token)});
            try stream.writeByteNTimes(' ', indent);
            indent -= 2;
        }
        try stream.writeByteNTimes(' ', indent);
        try stream.writeAll("}\n");
    }
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
    } else {
//         std.log.debug("Source ----------------\n{s}\n------------------------\n", .{source});
//         var stdout = std.io.getStdOut().writer();
//         dumpAstFlat(ast, stdout) catch |err| {
//             std.log.debug("zig: failed to dump ast {}", .{err});
//         };
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
            .range=ZAstRange{
                .start=ZAstLocation{
                    .line=@intCast(loc.line),
                    .column=@intCast(loc.column),
                },
                .end=ZAstLocation{
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

// export fn ast_node_from_ast(ptr: ?*Ast, index: u32) ?*ZNode {
//     if (ptr) |ast| {
//         const allocator = gpa.allocator();
//         const node = allocator.create(ZNode) catch {
//             std.log.warn("zig: ast_node_from_ast alloc failed", .{});
//             return null;
//         };
//         node.* = ZNode{
//             .ast=ast,
//             .index=index,
//         };
//         return node;
//     }
//     return null;
// }

export fn destroy_ast(ptr: ?*Ast) void {
    // std.log.debug("zig: destroy_ast {}", .{@intFromPtr(ptr)});
    if (ptr) |tree| {
        tree.deinit(gpa.allocator());
    }
}

// export fn destroy_node(ptr: ?*ZNode) void {
//     if (ptr) |node| {
//         node.deinit(gpa.allocator());
//     }
// }

export fn destroy_error(ptr: ?*ZError) void {
    // std.log.debug("zig: destroy_error {}", .{@intFromPtr(ptr)});
    if (ptr) |err| {
        err.deinit(gpa.allocator());
    }
}

export fn ast_node_spelling_range(node: ZNode) ZAstRange {
    if (node.ast == null) {
        return ZAstRange{};
    }
    const ast = node.ast.?;
    if (findNodeIdentifier(ast, node.index)) |ident_token| {
        const loc = ast.tokenLocation(0, ident_token);
        const name = ast.tokenSlice(ident_token);
        // std.log.warn("zig: ast_node_spelling_range {} {}", .{node.index, r});
        //return r;
        return ZAstRange{
            .start=ZAstLocation{
                .line=@intCast(loc.line),
                .column=@intCast(loc.column),
            },
            .end=ZAstLocation{
                .line=@intCast(loc.line),
                .column=@intCast(loc.column + name.len),
            },
        };

    }
    return ZAstRange{};
}

export fn ast_node_extent(node: ZNode) ZAstRange {
    if (node.ast == null) {
        std.log.warn("zig: ast_node_extent ast null", .{});
        return ZAstRange{};
    }
    const ast = node.ast.?;
    if (node.index >= ast.nodes.len) {
        std.log.warn("zig: ast_node_extent index out of range {}", .{node.index});
        return ZAstRange{};
    }
    // TODO: What is the correct start_offset?
    const first_token = ast.firstToken(node.index);
    const start_loc = ast.tokenLocation(0, first_token);
    const last_token = ast.lastToken(node.index);
    const end_loc = ast.tokenLocation(0, last_token);

    return ZAstRange{
        .start=ZAstLocation{
            .line=@intCast(start_loc.line),
            .column=@intCast(start_loc.column),
        },
        .end=ZAstLocation{
            .line=@intCast(end_loc.line),
            .column=@intCast(end_loc.column),
        },
    };
}



pub fn kindFromAstNode(ast: *Ast, index: Ast.TokenIndex) ?NodeKind {
    if (index >= ast.nodes.len) {
        return null;
    }
    return switch (ast.nodes.items(.tag)[index]) {
        .root => .Module,
        .fn_decl => .FunctionDecl,
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
        .struct_init_comma => .ContainerInit,
        .builtin_call,
        .builtin_call_comma,
        .call_one, .call_one_comma,
        .call, .call_comma,
        .async_call, .async_call_comma => .Call,

        .assign,
        .assign_destructure => .VarAccess,

        .deref => .PtrAccess,

        .unwrap_optional,
        .error_value,
        .field_access => .FieldAccess,
        .array_access => .ArrayAccess,

        // TODO
        else => .Unknown,
    };
}

export fn ast_node_kind(node: ZNode) NodeKind {
    if (node.ast) |ast| {
        if (kindFromAstNode(ast, node.index)) |kind| {
            // std.log.debug("zig: ast_node_kind {} is {s}", .{node.index, @tagName(kind)});
            return kind;
        }
    }
    return .Unknown;
}

// Lookup the token index that has the name/identifier for the given node
fn findNodeIdentifier(ast: *Ast, index: Ast.TokenIndex) ?Ast.TokenIndex {
    if (index >= ast.nodes.len) {
        return null;
    }

    const tag = ast.nodes.items(.tag)[index];
    return switch (tag) {
        // fn_decl lhs is a fn_proto
        .fn_decl => findNodeIdentifier(ast, ast.nodes.items(.data)[index].lhs),

        // Token after main_token is the name for all these
        .fn_proto_simple,
        .fn_proto_multi,
        .fn_proto_one,
        .fn_proto,
        .global_var_decl,
        .local_var_decl,
        .simple_var_decl,
        .aligned_var_decl => ast.nodes.items(.main_token)[index] + 1,

        // Field
        .container_field,
        .container_field_init,
        .container_field_align => ast.nodes.items(.main_token)[index],
        else => null,
    };
}

fn indexOfNodeWithTag(ast: Ast, start_token: Ast.TokenIndex, tag: Ast.Node.Tag) ?Ast.TokenIndex {
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

fn isTagContainerDecl(tag: Ast.Node.Tag) bool {
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

fn isTagVarDecl(tag: Ast.Node.Tag) bool {
    return switch (tag) {
        .simple_var_decl,
        .local_var_decl,
        .aligned_var_decl,
        .global_var_decl => true,
        else => false
    };
}

fn isNodeConstVarDecl(ast: *Ast, index: Ast.TokenIndex) bool {
    if (index < ast.nodes.len and isTagVarDecl(ast.nodes.items(.tag)[index])) {
        const main_token = ast.nodes.items(.main_token)[index];
        return ast.tokens.items(.tag)[main_token] == .keyword_const;
    }
    return false;
}

fn testNodeIdent(source: [:0]const u8, tag: Ast.Node.Tag, expected: ?[]const u8) !void {
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

}

fn testSpellingRange(source: [:0]const u8, tag: Ast.Node.Tag, expected: ZAstRange) !void {
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
    const range = ast_node_spelling_range(ZNode{.ast=&ast, .index=index});
    try std.testing.expectEqual(expected, range);
}

test "spelling-range" {
    try testSpellingRange("pub fn foo() void {}", .fn_decl, .{
        .start=.{.line=0, .column=7}, .end=.{.line=0, .column=10}});
    try testSpellingRange("const Foo = struct {\n pub fn foo() void {}\n};", .fn_decl, .{
        .start=.{.line=1, .column=8}, .end=.{.line=1, .column=11}});
    try testSpellingRange("pub fn foo() void {\n var x = 1;\n}", .simple_var_decl, .{
        .start=.{.line=1, .column=5}, .end=.{.line=1, .column=6}});
}

// Caller must free with destroy_string
export fn ast_node_new_spelling_name(node: ZNode) ?[*]const u8 {
    if (node.ast == null) {
        return null;
    }
    const ast = node.ast.?;
    if (node.index >= ast.nodes.len) {
        std.log.warn("zig: token name index={} is out of range", .{node.index});
        return null;
    }
    // const tag = ast.nodes.items(.tag)[node.index];
    if (findNodeIdentifier(ast, node.index)) |ident_token| {
        const name = ast.tokenSlice(ident_token);
        // std.log.debug("zig: token name {} {s} = '{s}' ({})", .{node.index, @tagName(tag), name, name.len});
        const copy = gpa.allocator().dupeZ(u8, name) catch {
            return null;
        };
        return copy.ptr;
    }
    // std.log.warn("zig: token name index={} tag={s} is unknown", .{node.index, @tagName(tag)});
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


const CallbackFn = *const fn(node: ZNode, parent: ZNode, data: ?*anyopaque) callconv(.C) VisitResult;

// Return whether to continue or not
export fn ast_visit(node: ZNode, callback: CallbackFn , data: ?*anyopaque) void {
    if (node.ast == null) {
        std.log.warn("zig: ast_visit node.ast is null", .{});
        return;
    }
    const ast = node.ast.?;
    if (node.index >= ast.nodes.len) {
        std.log.warn("zig: ast_visit index {} is out of range", .{node.index});
        return;
    }
    const tag = ast.nodes.items(.tag)[node.index];
    const d = ast.nodes.items(.data)[node.index];
    const parent = ZNode{.ast=ast, .index=node.index};
    // std.log.debug("zig: ast_visit {} {s}", .{node.index, @tagName(tag)});
    switch (tag) {
        .root => for (ast.rootDecls()) |decl_index| {
            // std.log.warn("zig: ast_visit root_decl index={}", .{decl_index});
            const child = ZNode{.ast=ast, .index=decl_index};
            switch (callback(child, parent, data)) {
                .Break => return,
                .Continue => continue,
                .Recurse => ast_visit(child, callback, data),
            }
        },
        // Recurse to both lhs and rhs, neither are optional
        .@"orelse",
        .for_range,
        .if_simple,
        .grouped_expression,
        .fn_decl => {
            const lhs = ZNode{.ast=ast, .index=d.lhs};
            switch (callback(lhs, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => ast_visit(lhs, callback, data),
            }
            const rhs = ZNode{.ast=ast, .index=d.rhs};
            switch (callback(rhs, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => ast_visit(rhs, callback, data),
            }
        },
        // Only walk data lhs
        .@"try",
        .@"await",
        .@"comptime",
        .@"nosuspend",
        .@"resume",
        .@"continue",
        .enum_literal,
        .optional_type => {
            // TODO: Args?
            const child = ZNode{.ast=ast, .index=d.rhs};
            switch (callback(child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => ast_visit(child, callback, data),
            }
        },

        // Only walk data rhs
        .@"defer",
        .fn_proto => {
            // TODO: Args?
            const child = ZNode{.ast=ast, .index=d.rhs};
            switch (callback(child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => ast_visit(child, callback, data),
            }
        },

        // For all of these walk lhs or rhs of the node's data
        .assign,
        .simple_var_decl,
        .aligned_var_decl,
        .container_decl_two,
        .container_decl_two_trailing,
        .container_field_init,
        .container_field_align,
        .builtin_call_two,
        .builtin_call_two_comma,
        .@"errdefer",
        .@"catch",
        .@"break",
        .fn_proto_simple,
        .block_two,
        .block_two_semicolon => {
            if (d.lhs != 0) {
                const child = ZNode{.ast=ast, .index=d.lhs};
                switch (callback(child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(child, callback, data),
                }
            }
            if (d.rhs != 0) {
                const child = ZNode{.ast=ast, .index=d.rhs};
                switch (callback(child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => ast_visit(child, callback, data),
                }
            }
        },
        // For these walk all sub list nodes in extra data
        .builtin_call,
        .builtin_call_comma,
        .container_decl,
        .container_decl_trailing,
        .block,
        .block_semicolon => {
            for (ast.extra_data[d.lhs..d.rhs]) |decl_index| {
                // std.log.warn("zig: ast_visit root_decl index={}", .{decl_index});
                const child = ZNode{.ast=ast, .index=decl_index};
                switch (callback(child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => ast_visit(child, callback, data),
                }
            }
        },
//         .@"return" => {
//             const child = ZNode{.ast=ast, .index=d.lhs};
//             switch (callback(parent, child, data)) {
//                 .Break, .Continue => {},
//                 .Recurse => ast_visit(child, callback, data),
//             }
//         },
//         .@"break",

        // Leaf nodes
        .char_literal,
        .number_literal,
        .identifier => {},
        else => {
           // std.log.debug("zig: ast_visit unhandled {s}", .{@tagName(tag)});
        }
    }
    return;
}
