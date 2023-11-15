// Copyright 2023 Jairus Martin <frmdstryr@protonmail.com>
// LGPL
const std = @import("std");
const Ast = std.zig.Ast;
const assert = std.debug.assert;

const Index = Ast.Node.Index;
const Tag = Ast.Node.Tag;
// const VisitResult = Ast.VisitResult;
const INVALID_TOKEN = std.math.maxInt(Index);

var gpa = std.heap.GeneralPurposeAllocator(.{}){};

const SourceLocation = extern struct {
    line: u32 = 0,
    column: u32 = 0,
};

const SourceRange = extern struct {
    start: SourceLocation = .{},
    end: SourceLocation = .{}
};

const Severity = enum(c_int) { NoSeverity = 0, Error = 1, Warning = 2, Hint = 4 };

const VisitResult = enum(u8) {
    Break = 0,
    Continue,
    Recurse,
};

const CaptureType = enum (u8) {
    Payload = 0,
    Error
};

// TODO: Just use import C ?
const NodeKind = enum(u8) {
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
    Ident,
    If,
    For,
    While,
    Switch,
    Defer,
    Catch,
    Usingnamespace,
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
            allocator.free(self.message[0 .. msg_len + 1]);
        }
        allocator.destroy(self);
    }
};

fn printAstError(ast: *Ast, filename: []const u8, source: []const u8) !void {
    const stderr = std.io.getStdErr().writer();
    for (ast.errors) |parse_error| {
        const loc = ast.tokenLocation(0, parse_error.token);
        try stderr.print("{s}:{d}:{d}: error: ", .{ filename, loc.line + 1, loc.column + 1 });
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

pub fn dumpAstFlat(ast: *const Ast, stream: anytype) !void {
    var i: usize = 0;
    const tags = ast.nodes.items(.tag);
    const node_data = ast.nodes.items(.data);
    const main_tokens = ast.nodes.items(.main_token);
    try stream.writeAll(TABLE_HEADER);
    while (i < ast.nodes.len) : (i += 1) {
        const tag = tags[i];
        const data = node_data[i];
        const main_token = main_tokens[i];
        try stream.print(TABLE_ROW_FORMAT, .{ i, @tagName(tag), data.lhs, data.rhs, ast.tokenSlice(main_token) });
    }
    try stream.writeAll("\n");
}

pub fn dumpAstNodes(ast: *const Ast, nodes: []Index, stream: anytype) !void {
    const tags = ast.nodes.items(.tag);
    const node_data = ast.nodes.items(.data);
    const main_tokens = ast.nodes.items(.main_token);
    try stream.writeAll(TABLE_HEADER);
    for (nodes) |i| {
        const tag = tags[i];
        const data = node_data[i];
        const main_token = main_tokens[i];
        try stream.print(TABLE_ROW_FORMAT, .{ i, @tagName(tag), data.lhs, data.rhs, ast.tokenSlice(main_token) });
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
    const source: [:0]const u8 = source_ptr[0..source_len :0];

    std.log.info("zig: parsing filename '{s}'...", .{name});

    const allocator = gpa.allocator();
    const ast = allocator.create(Ast) catch |err| {
        std.log.warn("zig: parsing {s} error: {}", .{ name, err });
        return null;
    };
    ast.* = Ast.parse(allocator, source, .zig) catch |err| {
        std.log.warn("zig: parsing {s} error: {}", .{ name, err });
        return null;
    };

    if (ast.errors.len > 0) {
        printAstError(ast, name, source) catch |err| {
            std.log.warn("zig: failed to print trace {}", .{err});
        };
    }
//     else if (true) {//@import("builtin").is_test) {
//         var stdout = std.io.getStdOut().writer();
//         stdout.print("Source ----------------\n{s}\n------------------------\n", .{source}) catch {};
//         //var stdout = std.io.getStdOut().writer();
//         dumpAstFlat(ast, stdout) catch |err| {
//             std.log.debug("zig: failed to dump ast {}", .{err});
//         };
//     }
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
        result.* = ZError{ .severity = if (err.is_note) .Hint else .Error, .range = SourceRange{
            .start = SourceLocation{
                .line = @intCast(loc.line),
                .column = @intCast(loc.column),
            },
            .end = SourceLocation{
                .line = @intCast(loc.line),
                .column = @intCast(loc.column),
            },
        }, .message = msg };
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

export fn ast_node_capture_token(ptr: ?*Ast, index: Index, capture: CaptureType) Index {
    if (ptr) |ast| {
        if (index >= ast.nodes.len) {
            return INVALID_TOKEN;
        }
        if (ast.fullIf(index)) |r| {
            return switch (capture) {
                .Payload => r.payload_token orelse INVALID_TOKEN,
                .Error => r.error_token orelse INVALID_TOKEN,
            };
        }
        if (ast.fullWhile(index)) |r| {
            return switch (capture) {
                .Payload => r.payload_token orelse INVALID_TOKEN,
                .Error => r.error_token orelse INVALID_TOKEN,
            };
        }

        if (ast.fullFor(index)) |r| {
            return switch (capture) {
                .Payload => r.payload_token,
                .Error => INVALID_TOKEN,
            };
        }

        const tag = ast.nodes.items(.tag)[index];
        if (tag == .@"errdefer") {
            return switch (capture) {
                .Payload => ast.nodes.items(.data)[index].lhs,
                .Error => INVALID_TOKEN,
            };
        }
        if (tag == .@"catch") {
            // If token after catch is a | then uses token after that
            const main_token = ast.nodes.items(.main_token)[index];
            if (ast.tokens.items(.tag)[main_token+1] == .pipe) {
                return main_token + 2;
            }
        }
    }
    return INVALID_TOKEN;
}


fn testCaptureName(source: [:0]const u8, tag: Tag, capture: CaptureType, expected: ?[]const u8) !void {
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
    const tok = ast_node_capture_token(&ast, index, capture);
    if (expected) |str| {
        try std.testing.expectEqualSlices(u8, str, ast.tokenSlice(tok));
    } else {
        try std.testing.expect(tok == INVALID_TOKEN);
    }
}

test "capture-name" {
    try testCaptureName("test { if (a) |b| { _ = b; }}", .if_simple, .Payload, "b");
    try testCaptureName("test { if (a) |b| { _ = b; }}", .if_simple, .Error, null);
    try testCaptureName("test { if (a) |b| { _ = b; } else |c| {}}", .@"if", .Error, "c");
    try testCaptureName("test { while (a) |b| { _ = b; }}", .while_simple, .Payload, "b");
    try testCaptureName("test { for (a) |b| { _ = b; }}", .for_simple, .Payload, "b");
    try testCaptureName("test { for (&a) |*b| { _ = b; }}", .for_simple, .Payload, "*");
    try testCaptureName("test { try foo() catch |err| {}; }", .@"catch", .Payload, "err");
    try testCaptureName("test { errdefer |err| {} }", .@"errdefer", .Payload, "err");
}

export fn ast_node_range(ptr: ?*Ast, index: Index) SourceRange {
    if (ptr == null) {
        std.log.warn("zig: ast_node_range ast null", .{});
        return SourceRange{};
    }
    const ast = ptr.?;
    if (index >= ast.nodes.len) {
        std.log.warn("zig: ast_node_range index out of range {}", .{index});
        return SourceRange{};
    }
    // TODO: What is the correct start_offset?
    const first_token = ast.firstToken(index);
    const start_loc = ast.tokenLocation(0, first_token);
    const last_token = ast.lastToken(index);
    const end_loc = ast.tokenLocation(0, last_token);

    return SourceRange{
        .start = SourceLocation{
            .line = @intCast(start_loc.line),
            .column = @intCast(start_loc.column),
        },
        .end = SourceLocation{
            .line = @intCast(end_loc.line),
            .column = @intCast(end_loc.column),
        },
    };
}

// Assumes token is in the valid range
fn isTokenSliceEql(ast: *Ast, token: Index, value: []const u8) bool {
    return std.mem.eql(u8, ast.tokenSlice(token), value);
}

export fn ast_node_kind(ptr: ?*Ast, index: Index) NodeKind {
    if (ptr == null) {
        return .Unknown;
    }
    const ast = ptr.?;
    if (index >= ast.nodes.len) {
        return .Unknown;
    }
    const main_tokens = ast.nodes.items(.main_token);
    return switch (ast.nodes.items(.tag)[index]) {
        .root => .Module,
        .fn_decl => .FunctionDecl,
        .test_decl => .TestDecl,
        .simple_var_decl, .local_var_decl, .aligned_var_decl, .global_var_decl => .VarDecl,
        .container_decl, .container_decl_trailing, .container_decl_two, .container_decl_two_trailing, .container_decl_arg, .container_decl_arg_trailing =>
            if (isTokenSliceEql(ast, main_tokens[index], "enum"))
                .EnumDecl
            else
                .ContainerDecl,
        .block, .block_semicolon, .block_two, .block_two_semicolon, .@"comptime" => .BlockDecl,
        .container_field_init, .container_field_align, .container_field => .FieldDecl,
        .error_set_decl => .ErrorDecl,
        // TODO: Param? Import? Detect if template
        .struct_init, .struct_init_comma, .struct_init_dot, .struct_init_dot_comma, .struct_init_dot_two, .struct_init_dot_two_comma, .struct_init_one, .struct_init_one_comma => .ContainerInit,

        .builtin_call, .builtin_call_comma, .builtin_call_two, .builtin_call_two_comma, .call, .call_comma, .call_one, .call_one_comma, .async_call, .async_call_comma, .async_call_one, .async_call_one_comma => .Call,

        .equal_equal, .bang_equal, .less_than, .greater_than, .less_or_equal, .greater_or_equal, .mul, .div, .mod, .mul_wrap, .mul_sat, .add, .sub, .add_wrap, .sub_wrap, .add_sat, .sub_sat, .shl, .shl_sat, .shr, .bit_and, .bit_xor, .bit_or, .bool_and, .bool_or, .assign_mul, .assign_div, .assign_mod, .assign_add, .assign_sub, .assign_shl, .assign_shl_sat, .assign_shr, .assign_bit_and, .assign_bit_xor, .assign_bit_or, .assign_mul_wrap, .assign_add_wrap, .assign_sub_wrap, .assign_mul_sat, .assign_add_sat, .assign_sub_sat, .assign, .assign_destructure => .VarAccess,

        .deref => .PtrAccess,

        .unwrap_optional, .error_value, .field_access => .FieldAccess,
        .array_mult, .array_cat, .array_access => .ArrayAccess,

        .string_literal, .multiline_string_literal, .number_literal, .char_literal => .Literal,

        .identifier => .Ident,

        .if_simple, .@"if" => .If,
        .for_range, .for_simple, .@"for" => .For,
        .while_cont, .while_simple, .@"while" => .While,
        .switch_comma, .@"switch" => .Switch,
        .@"defer", .@"errdefer" => .Defer,
        .@"catch" => .Catch,
        .@"usingnamespace" => .Usingnamespace,
        // TODO
        else => .Unknown,
    };
}

export fn ast_node_tag(ptr: ?*Ast, index: Index) Ast.Node.Tag {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            const tag = ast.nodes.items(.tag)[index];
            // std.log.debug("zig: ast_node_tag {} is {s}", .{ index, @tagName(tag) });
            return tag;
        }
    }
    return .root; // Invalid unless index == 0
}

// Caller must free with destroy_string
export fn ast_token_slice(ptr: ?*Ast, token: Index) ?[*]const u8 {
    if (ptr) |ast| {
        if (token < ast.tokens.len) {
            const name = ast.tokenSlice(token);
            const copy = gpa.allocator().dupeZ(u8, name) catch {
                return null;
            };
            return copy.ptr;
        }
    }
    return null;
}

fn findNodeComment(ast: *const Ast, node: Index) ?[]const u8 {
    const first_token = ast.firstToken(node);
    const loc = ast.tokenLocation(0, first_token);
    // std.log.warn("node start: {}", .{loc});
    const comment_end = loc.line_start -| 1;
    var comment_start: usize = comment_end;

    while (comment_start > 3) {
        // Find previous next line
        const remaining = ast.source[0..comment_start];
        // std.log.warn("remaining: '{s}'", .{remaining});
        const previous_line_start = std.mem.lastIndexOfScalar(u8, remaining, '\n') orelse 0;
        // std.log.warn("previous_line_start: '{}'", .{previous_line_start});

        const comment = std.mem.trimLeft(u8, ast.source[previous_line_start..comment_end], " \t\n");
        if (std.mem.startsWith(u8, comment, "//")) {
            comment_start = previous_line_start;
        } else {
            break;
        }
    }
    const comment = std.mem.trim(u8, ast.source[comment_start..comment_end], " \t\r\n");
    if (comment.len < 3 or comment.len > 1000) {
        return null;
    }
    return comment;
}

fn testNodeComment(source: [:0]const u8, tag: Tag, expected: ?[]const u8) !void {
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

    const n = indexOfNodeWithTag(ast, 0, tag);
    if (n == null) {
        try dumpAstFlat(&ast, stdout);
        try std.testing.expect(n != null);
    }
    const r = findNodeComment(&ast, n.?);
    if (expected) |value| {
        try std.testing.expect(r != null);
        try std.testing.expectEqualSlices(u8, value, r.?);
    } else {
        try std.testing.expect(r == null);
    }
}

test "node-comment" {
    try testNodeComment(
        \\// Test 1
        \\pub fn foo() void {}
        , .fn_decl, "// Test 1");
    try testNodeComment(
        \\
        \\// Test 2
        \\pub fn foo() void {}
        , .fn_decl, "// Test 2");
    try testNodeComment(
        \\
        \\  // Test 3
        \\pub fn foo() void {}
        , .fn_decl, "// Test 3");
    try testNodeComment(
        \\
        \\// Line 1
        \\// Line 2
        \\pub fn foo() void {}
        , .fn_decl, "// Line 1\n// Line 2");
    try testNodeComment(
        \\
        \\// Comment not on fn
        \\var x = 1;
        \\pub fn foo() void {}
        , .fn_decl, null);
    try testNodeComment(
        \\// Comment not on fn at start
        \\var x = 1;
        \\
        \\pub fn foo() void {}
        , .fn_decl, null);
    try testNodeComment(
        \\const Foo = struct {
        \\   // Comment in struct
        \\   pub fn foo() void {}
        \\};
        , .fn_decl, "// Comment in struct");
    try testNodeComment(
        \\const Foo = struct {
        \\   // Comment in struct
        \\   pub fn foo() void {}
        \\};
        , .container_decl_two, null);
    try testNodeComment(
        \\// Struct comment
        \\const Foo = struct {
        \\   // Comment in struct
        \\   pub fn foo() void {}
        \\};
        , .container_decl_two, "// Struct comment");
    try testNodeComment(
        \\const Foo = struct {
        \\   /// This field is a u8
        \\   field: u8 = 0,
        \\};
        , .container_field_init, "/// This field is a u8");
}

// Caller must free with destroy_string
export fn ast_node_comment(ptr: ?*Ast, node: Index) ?[*]const u8 {
    if (ptr) |ast| {
        if (node < ast.nodes.len) {
            if (findNodeComment(ast, node)) |comment| {
                const copy = gpa.allocator().dupeZ(u8, comment) catch {
                    return null;
                };
                return copy.ptr;
            }
        }
    }
    return null;
}

export fn ast_token_range(ptr: ?*Ast, token: Index) SourceRange {
    if (ptr) |ast| {
        if (token < ast.tokens.len) {
            const loc = ast.tokenLocation(0, token);
            const name = ast.tokenSlice(token);
            return SourceRange{
                .start = SourceLocation{
                    .line = @intCast(loc.line),
                    .column = @intCast(loc.column),
                },
                .end = SourceLocation{
                    .line = @intCast(loc.line),
                    .column = @intCast(loc.column + name.len),
                },
            };
        }
    }
    return SourceRange{};
}

export fn ast_var_is_const(ptr: ?*Ast, index: Index) bool {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            const tag = ast.nodes.items(.tag)[index];
            return switch (tag) {
                .simple_var_decl, .aligned_var_decl, .global_var_decl, .local_var_decl => isTokenSliceEql(ast, ast.nodes.items(.main_token)[index], "const"),
                else => false,
            };
        }
    }
    return false;
}

// Return var / field type or 0
export fn ast_var_type(ptr: ?*Ast, index: Index) Index {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            const tag = ast.nodes.items(.tag)[index];
            return switch (tag) {
                .simple_var_decl => ast.simpleVarDecl(index).ast.type_node,
                .aligned_var_decl => ast.alignedVarDecl(index).ast.type_node,
                .global_var_decl => ast.globalVarDecl(index).ast.type_node,
                .local_var_decl => ast.localVarDecl(index).ast.type_node,
                // All are data.lhs
                .container_field, .container_field_align, .container_field_init => ast.nodes.items(.data)[index].lhs,
                else => 0,
            };
        }
    }
    return 0;
}

export fn ast_var_value(ptr: ?*Ast, index: Index) Index {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            const tag = ast.nodes.items(.tag)[index];
            return switch (tag) {
                .simple_var_decl,
                .aligned_var_decl,
                .global_var_decl,
                .local_var_decl,
                .container_field_init, => ast.nodes.items(.data)[index].rhs,
                // TODO:
                // .container_field,
                // .container_field_align,
                else => 0,
            };
        }
    }
    return 0;
}


export fn ast_fn_returns_inferred_error(ptr: ?*Ast, index: Index) bool {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            var buf: [1]Ast.Node.Index = undefined;
            if (ast.fullFnProto(&buf, index)) |proto| {
                // Zig doesn't use error union if error type is omitted
                const token_tags = ast.tokens.items(.tag);
                const maybe_bang = ast.firstToken(proto.ast.return_type) - 1;
                return token_tags[maybe_bang] == .bang;
            }
        }
    }
    return false;
}

export fn ast_fn_return_type(ptr: ?*Ast, index: Index) Index {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            const data_items = ast.nodes.items(.data);
            const main_tokens = ast.nodes.items(.main_token);
            const tag = ast.nodes.items(.tag)[index];
            return switch (tag) {
                .fn_decl => data_items[data_items[index].lhs].rhs,
                .builtin_call_two, .builtin_call_two_comma => if (isTokenSliceEql(ast, main_tokens[index], "@as"))
                    data_items[index].lhs
                else
                    0,
                else => 0,
            };
        }
    }
    return 0;
}

export fn ast_fn_param_count(ptr: ?*Ast, index: Index) u32 {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            var buffer: [1]Ast.Node.Index = undefined;
            if (ast.fullFnProto(&buffer, index)) |proto| {
                return @intCast(proto.ast.params.len);
            }
        }
    }
    return 0;
}

export fn ast_fn_param_at(ptr: ?*Ast, index: Index, i: u32) Index {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            var buffer: [1]Ast.Node.Index = undefined;
            if (ast.fullFnProto(&buffer, index)) |proto| {
                if (i < proto.ast.params.len) {
                    return proto.ast.params[i];
                }
            }
        }
    }
    return 0;
}

export fn ast_fn_param_token(ptr: ?*Ast, index: Index, i: u32) Index {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            var buffer: [1]Ast.Node.Index = undefined;
            if (ast.fullFnProto(&buffer, index)) |proto| {
                var j: usize = 0;
                var it = proto.iterate(ast);
                while (it.next()) |param| {
                    if (j == i) {
                        return param.name_token orelse INVALID_TOKEN;
                    }
                    j += 1;
                }
            }
        }
    }
    return INVALID_TOKEN;
}

// Note: 0 is a valid token
export fn ast_node_main_token(ptr: ?*Ast, index: Index) Index {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
            return ast.nodes.items(.main_token)[index];
        }
    }
    return INVALID_TOKEN;
}


// Lookup the token index that has the name/identifier for the given node
export fn ast_node_name_token(ptr: ?*Ast, index: Index) Index {
    if (ptr) |ast| {
        if (index < ast.nodes.len) {
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
                .fn_decl => ast_node_name_token(ast, ast.nodes.items(.data)[index].lhs),

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
                .builtin_call,
                .builtin_call_comma,
                .builtin_call_two,
                .builtin_call_two_comma,
                .container_field,
                .container_field_init,
                .container_field_align => ast.nodes.items(.main_token)[index],
                .test_decl => blk: {
                    const v = ast.nodes.items(.data)[index].lhs;
                    break :blk if (v == 0) INVALID_TOKEN else v;
                },
                else => INVALID_TOKEN,
            };
        }
    }
    return INVALID_TOKEN;
}

fn indexOfNodeWithTag(ast: Ast, start_token: Index, tag: Tag) ?Index {
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
        .container_decl, .container_decl_trailing, .container_decl_two, .container_decl_two_trailing, .container_decl_arg, .container_decl_arg_trailing => true,
        else => false,
    };
}

fn isTagVarDecl(tag: Tag) bool {
    return switch (tag) {
        .simple_var_decl, .local_var_decl, .aligned_var_decl, .global_var_decl => true,
        else => false,
    };
}

fn isNodeConstVarDecl(ast: *Ast, index: Index) bool {
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
    const tok = ast_node_name_token(&ast, index);
    if (expected) |str| {
        try std.testing.expectEqualSlices(u8, ast.tokenSlice(tok), str);
        try stdout.print("location={}\n", .{ast.tokenLocation(0, tok)});
    } else {
        try std.testing.expect(tok == 0);
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
    try testNodeIdent("pub fn main() u8 { return @min(1,2); }", .builtin_call_two, "@min");
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
    const tok = ast_node_name_token(&ast, index);
    const range = ast_token_range(&ast, tok);
    std.log.warn("zig: {} range {}", .{ index, range });
    try std.testing.expectEqual(expected.start.line, range.start.line);
    try std.testing.expectEqual(expected.start.column, range.start.column);
    try std.testing.expectEqual(expected.end.line, range.end.line);
    try std.testing.expectEqual(expected.end.column, range.end.column);
}

test "spelling-range" {
    try testSpellingRange("pub fn foo() void {}", .fn_decl, .{ .start = .{ .line = 0, .column = 7 }, .end = .{ .line = 0, .column = 10 } });
    try testSpellingRange("const Foo = struct {\n pub fn foo() void {}\n};", .fn_decl, .{ .start = .{ .line = 1, .column = 8 }, .end = .{ .line = 1, .column = 11 } });
    try testSpellingRange("pub fn foo() void {\n var x = 1;\n}", .simple_var_decl, .{ .start = .{ .line = 1, .column = 5 }, .end = .{ .line = 1, .column = 6 } });
    try testSpellingRange("pub fn foo() void {\n if (true) {\n  var x = 1;}\n}", .simple_var_decl, .{ .start = .{ .line = 2, .column = 6 }, .end = .{ .line = 2, .column = 7 } });
    try testSpellingRange("const Foo = struct {a: bool};\nvar foo = Foo{};", .struct_init_one, .{ .start = .{ .line = 1, .column = 10 }, .end = .{ .line = 1, .column = 13 } });
    try testSpellingRange("var x = y.foo();", .call_one, .{ .start = .{ .line = 0, .column = 10 }, .end = .{ .line = 0, .column = 13 } });
    try testSpellingRange("var x = @min(1, 2);", .builtin_call_two, .{ .start = .{ .line = 0, .column = 8 }, .end = .{ .line = 0, .column = 12 } });
}


export fn destroy_string(str: [*c]const u8) void {
    // std.log.warn("zig: destroy_str {}", .{@intFromPtr(str)});
    const len = strlen(str);
    if (len > 0) {
        const allocator = gpa.allocator();
        allocator.free(str[0 .. len + 1]);
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
    const source: [:0]const u8 = source_ptr[0..source_len :0];
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

const NodeData = extern struct {
    lhs: Index = 0,
    rhs: Index = 0,
};

// Visit one child
export fn ast_node_data(ptr: ?*Ast, node: Index) NodeData {
    if (ptr) |ast| {
        if (node < ast.nodes.len) {
            const data = ast.nodes.items(.data)[node];
            return NodeData{.lhs = data.lhs, .rhs = data.rhs};
        }
    }
    return NodeData{};
}

export fn ast_extra_data(ptr: ?*Ast, index: Index) Index {
    if (ptr) |ast| {
        if (index < ast.extra_data.len) {
            return ast.extra_data[index];
        }
    }
    return 0;
}

const ArrayTypeSentinel = extern struct {
    sentinel: Index = 0,
    elem_type: Index = 0,
};

export fn ast_array_type_sentinel(ptr: ?*Ast, node: Index) ArrayTypeSentinel {
    if (ptr) |ast| {
        if (node < ast.nodes.len) {
            const tag = ast.nodes.items(.tag)[node];
            if (tag == .array_type_sentinel) {
                const d = ast.nodes.items(.data)[node];
                const array_data = ast.extraData(d.rhs, Ast.Node.ArrayTypeSentinel);
                return ArrayTypeSentinel{
                    .sentinel = array_data.sentinel,
                    .elem_type = array_data.elem_type
                };
            }
        }
    }
    return ArrayTypeSentinel{};
}

const IfData = extern struct {
    payload_token: Index = INVALID_TOKEN,
    error_token: Index = INVALID_TOKEN,
    cond_expr: Index = 0,
    then_expr: Index = 0,
    else_expr: Index = 0,
};

export fn ast_if_data(ptr: ?*Ast, node: Index) IfData {
    if (ptr) |ast| {
        if (node < ast.nodes.len) {
            if (ast.fullIf(node)) |data| {
                return IfData{
                    .payload_token = data.payload_token orelse INVALID_TOKEN,
                    .error_token = data.error_token orelse INVALID_TOKEN,
                    .cond_expr = data.ast.cond_expr,
                    .then_expr = data.ast.then_expr,
                    .else_expr = data.ast.else_expr,
                };
            }
        }
    }
    return IfData{};
}

const VarDataInfo = packed struct {
    is_pub: bool = false,
    is_const: bool = false,
    is_comptime: bool = false,
    is_extern: bool = false,
    is_threadlocal: bool = false,
    reserved: u3 = 0,
};

const VarData = extern struct {
    lib_name: Index = INVALID_TOKEN,
    type_node: Index = 0,
    align_node: Index = 0,
    addrspace_node: Index = 0,
    section_node: Index = 0,
    init_node: Index = 0,
    info: VarDataInfo = .{},
};

export fn ast_var_data(ptr: ?*Ast, node: Index) VarData {
    if (ptr) |ast| {
        if (node < ast.nodes.len) {
            if (ast.fullVarDecl(node)) |data| {
                return VarData{
                    .lib_name = data.lib_name orelse INVALID_TOKEN,
                    .type_node = data.ast.type_node,
                    .align_node = data.ast.align_node,
                    .addrspace_node = data.ast.addrspace_node,
                    .section_node = data.ast.section_node,
                    .init_node = data.ast.init_node,
                    .info = VarDataInfo{
                        .is_pub = data.visib_token != null,
                        .is_const = isTokenSliceEql(ast, data.ast.mut_token, "const"),
                        .is_extern = data.extern_export_token != null,
                        .is_comptime = data.comptime_token != null,
                        .is_threadlocal = data.threadlocal_token != null,
                    }
                };
            }
        }
    }
    return VarData{};
}


export fn ast_array_init_size(ptr: ?*Ast, node: Index) u32 {
    if (ptr) |ast| {
        if (node < ast.nodes.len) {
            var nodes: [2]Index = undefined;
            if (ast.fullArrayInit(&nodes, node)) |array_data| {
                return @intCast(array_data.ast.elements.len);
            }
        }
    }
    return 0;
}

fn visitOne(ast: *const Ast, node: Index, parent: Index, data: *Index) VisitResult {
    _ = ast;
    _ = parent;
    data.* = node;
    return .Break;
}

// Visit one child
export fn ast_visit_one_child(ptr: ?*Ast, node: Index) Index {
    var result: Index = 0;
    if (ptr) |ast| {
        visit(ast, node, *Index, visitOne, &result);
    }
    return result;
}

fn visitAll(ast: *const Ast, child: Index, parent: Index, nodes: *std.ArrayList(Index)) VisitResult {
    _ = ast;
    _ = parent;
    const remaining = nodes.unusedCapacitySlice();
    if (remaining.len > 0) {
        remaining[0] = child;
        nodes.items.len += 1;
        return .Recurse;
    }
    return .Break;
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

pub fn assertNodeIndexValid(ast: *const Ast, child: Index, parent: Index) void {
    if (child == 0 or child > ast.nodes.len) {
        var stderr = std.io.getStdErr().writer();
        dumpAstFlat(ast, stderr) catch {};
        std.log.err("zig: ast_visit child index {} from parent {} is out of range or will create a loop", .{ child, parent });
        assert(false);
    }
}

const CallbackFn = *const fn (ptr: ?*const Ast, node: Index, parent: Index, data: ?*anyopaque) callconv(.C) VisitResult;

const ExternVisitor = struct {
    const Self = @This();
    delegate: CallbackFn,
    data: ?*anyopaque,

    pub fn callback(ast: *const Ast, node: Index, parent: Index, self: *const ExternVisitor) VisitResult {
        return self.delegate(ast, node, parent, self.data);
    }
};

// Return whether to continue or not
export fn ast_visit(ptr: ?*Ast, parent: Index, callback: CallbackFn, data: ?*anyopaque) void {
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
    if (@import("builtin").is_test) {
        var stdout = std.io.getStdOut().writer();
        stdout.print("zig: ast_visit {s} at {} '{s}'\n", .{ @tagName(tag), parent, ast.tokenSlice(ast.nodes.items(.main_token)[parent]) }) catch {};
    }
    const visitor = ExternVisitor{
        .delegate = callback,
        .data = data,
    };
    visit(ast, parent, *const ExternVisitor, &ExternVisitor.callback, &visitor);
}

pub fn visit(
    ast: *const Ast,
    parent: Index,
    comptime T: type,
    callback: *const fn (ast: *const Ast, node: Index, parent: Index, data: T) VisitResult,
    data: T
) void {
    const tag = ast.nodes.items(.tag)[parent];
    const d = ast.nodes.items(.data)[parent];
    switch (tag) {
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
        .error_set_decl,
        .identifier => {},

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
        .slice_open,
        .array_access,
        .array_init_one_comma,
        .struct_init_one_comma,
        .switch_range,
        .while_simple,
        .error_union,
        => {
            {
                const child = d.lhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
            {
                const child = d.rhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
        },
        // Only walk data lhs
        .asm_simple,
        .asm_output,
        .asm_input,
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
        .grouped_expression => if (d.lhs != 0) {
            const child = d.lhs;
            assertNodeIndexValid(ast, child, parent);
            switch (callback(ast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => visit(ast, child, T, callback, data),
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
                .Recurse => visit(ast, child, T, callback, data),
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
        .array_init_one,
        .array_init_dot_two,
        .array_init_dot_two_comma,
        .tagged_union_two,
        .tagged_union_two_trailing,
        .@"catch",
        .switch_case_one,
        .switch_case_inline_one,
        .fn_proto_simple,
        .fn_decl,
        .block_two,
        .block_two_semicolon,
        => {
            if (d.lhs != 0) {
                const child = d.lhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
            if (d.rhs != 0) {
                const child = d.rhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
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
        .tagged_union,
        .tagged_union_trailing,
        .array_init_dot,
        .array_init_dot_comma,
        .block,
        .block_semicolon => {
            for (ast.extra_data[d.lhs..d.rhs]) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(ast, child, T, callback, data),
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
        .tagged_union_enum_tag,
        .tagged_union_enum_tag_trailing,
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
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
            const field_range = ast.extraData(d.rhs, Ast.Node.SubRange);
            for (ast.extra_data[field_range.start..field_range.end]) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
        },
        // Visit lhs sub range, then rhs
        .switch_case, .switch_case_inline, .fn_proto_multi => {
            const field_range = ast.extraData(d.lhs, Ast.Node.SubRange);
            for (ast.extra_data[field_range.start..field_range.end]) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
            const child = d.rhs;
            assertNodeIndexValid(ast, child, parent);
            switch (callback(ast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => visit(ast, child, T, callback, data),
            }
        },
        .while_cont => {
            const while_data = ast.extraData(d.rhs, Ast.Node.WhileCont);
            inline for (.{ d.lhs, while_data.cont_expr, while_data.then_expr }) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
        },
        .@"while" => {
            const while_data = ast.extraData(d.rhs, Ast.Node.While);
            inline for (.{ d.lhs, while_data.cont_expr, while_data.then_expr, while_data.else_expr }) |child| {
                if (child != 0) { // cont expr part may be omitted
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(ast, child, T, callback, data),
                    }
                }
            }
        },
        .@"if" => {
            const if_data = ast.extraData(d.rhs, Ast.Node.If);
            inline for (.{ d.lhs, if_data.then_expr, if_data.else_expr }) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
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
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }

            // For body
            {
                const child = ast.extra_data[d.lhs + extra.inputs];
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }

            // Else body
            if (extra.has_else) {
                const child = ast.extra_data[d.lhs + extra.inputs + 1];
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
        },
        .fn_proto, .fn_proto_one => |t| {
            var buf: [1]Ast.Node.Index = undefined;
            const fn_proto = if (t == .fn_proto_one)
                ast.fnProtoOne(&buf, parent).ast
            else
                ast.fnProto(parent).ast;
            for (fn_proto.params) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
            inline for (.{ d.rhs, fn_proto.align_expr, fn_proto.addrspace_expr, fn_proto.section_expr, fn_proto.callconv_expr }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(ast, child, T, callback, data),
                    }
                }
            }
        },
        .global_var_decl => {
            const var_data = ast.extraData(d.lhs, Ast.Node.GlobalVarDecl);
            inline for (.{ var_data.type_node, var_data.align_node, var_data.addrspace_node, var_data.section_node, d.rhs }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(ast, child, T, callback, data),
                    }
                }
            }
        },
        .local_var_decl => {
            const var_data = ast.extraData(d.lhs, Ast.Node.LocalVarDecl);
            inline for (.{ var_data.type_node, var_data.align_node, d.rhs }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(ast, child, T, callback, data),
                    }
                }
            }
        },

        .array_type_sentinel => {
            const array_data = ast.extraData(d.rhs, Ast.Node.ArrayTypeSentinel);
            inline for (.{ d.lhs, array_data.sentinel, array_data.elem_type }) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
        },
        .container_field => {
            const field_data = ast.extraData(d.rhs, Ast.Node.ContainerField);
            inline for (.{ d.lhs, field_data.align_expr, field_data.value_expr }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(ast, child, T, callback, data),
                    }
                }
            }
        },
        .slice => {
            const slice_data = ast.extraData(d.rhs, Ast.Node.Slice);
            inline for (.{ d.lhs, slice_data.start, slice_data.end }) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
        },
        .slice_sentinel => {
            const slice_data = ast.extraData(d.rhs, Ast.Node.SliceSentinel);
            inline for (.{ d.lhs, slice_data.start, slice_data.end, slice_data.sentinel }) |child| {
                if (child != 0) { // slice end may be 0
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(ast, child, T, callback, data),
                    }
                }
            }
        },
        .@"asm" => {
            {
                const child = d.lhs;
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
            const asm_data = ast.extraData(d.rhs, Ast.Node.Asm);
            for (ast.extra_data[asm_data.items_start..asm_data.items_end]) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
        },
        .root => for (ast.rootDecls()) |child| {
            assertNodeIndexValid(ast, child, parent);
            switch (callback(ast, child, parent, data)) {
                .Break => return,
                .Continue => continue,
                .Recurse => visit(ast, child, T, callback, data),
            }
        },
        .ptr_type => {
            const ptr_data = ast.extraData(d.lhs, Ast.Node.PtrType);
            inline for (.{ d.rhs, ptr_data.sentinel, ptr_data.align_node, ptr_data.addrspace_node }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(ast, child, T, callback, data),
                    }
                }
            }
        },
        .ptr_type_bit_range => {
            const ptr_data = ast.extraData(d.lhs, Ast.Node.PtrTypeBitRange);
            inline for (.{ d.rhs, ptr_data.sentinel, ptr_data.align_node, ptr_data.addrspace_node, ptr_data.bit_range_start, ptr_data.bit_range_end }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(ast, child, parent);
                    switch (callback(ast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(ast, child, T, callback, data),
                    }
                }
            }
        },
        .assign_destructure => {
            const elem_count = ast.extra_data[d.lhs];
            // var decls (const a, const b, etc..)
            for (ast.extra_data[d.lhs+1..][0..elem_count]) |child| {
                assertNodeIndexValid(ast, child, parent);
                switch (callback(ast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(ast, child, T, callback, data),
                }
            }
            // The value to destructure
            const child = d.rhs;
            assertNodeIndexValid(ast, child, parent);
            switch (callback(ast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => visit(ast, child, T, callback, data),
            }
        },
    }
    return;
}

fn testVisit(source: [:0]const u8, tag: Tag) !void {
    const allocator = std.testing.allocator;
    var ast = try Ast.parse(allocator, source, .zig);
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    // try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&ast, "", source);
        return error.ParseError;
    }
    // try dumpAstFlat(&ast, stdout);

    var nodes = try std.ArrayList(Index).initCapacity(allocator, ast.nodes.len + 1);
    nodes.appendAssumeCapacity(0); // Callback does not call on initial node
    defer nodes.deinit();
    // try stdout.writeAll("Visited order\n");

    visit(&ast, 0, *std.ArrayList(Index), visitAll, &nodes);
    const visited = nodes.items[0..nodes.items.len];

    // try dumpAstNodes(&ast, visited, stdout);
    try std.testing.expectEqual(ast.nodes.len, visited.len);

    // Sort visited nodes and make sure each was hit
    std.mem.sort(Index, visited, {}, std.sort.asc(Index));
    for (visited, 0..) |a, b| {
        try std.testing.expectEqual(@as(Index, @intCast(b)), a);
    }

//     var timer = try std.time.Timer.start();
//     for (0..ast.tokens.len) |i| {
//         _ = ast.tokenLocation(0, @intCast(i));
//     }
//     const t0: f64 = @floatFromInt(timer.read());
//     std.log.warn("Old: {d:6.2}ms", .{std.math.round(t0/std.time.ms_per_s)});
//
//     timer.reset();
//     for (0..ast.tokens.len) |i| {
//         _ = ast.tokenLocation(0, @intCast(i));
//     }
//     const t1: f64 = @floatFromInt(timer.read());
//     std.log.warn("New: {d:6.2}ms", .{std.math.round(t1/std.time.ms_per_s)});
//     const dt = t1-t0;
//     std.log.warn("Diff: delta={d:6.2}ms {d:6.2}% {s}", .{
//         dt/std.time.ms_per_s, 100*(t1-t0)/t0, if (t1 < t0) "faster" else "slower"});
//
//     for (0..ast.tokens.len) |i| {
//         const a = ast.tokenLocation(0, @intCast(i));
//         const b = ast.tokenLocationScan(0, @intCast(i));
//         try std.testing.expectEqual(a, b);
//     }

    // If given, make sure the tag is actually used in the source parsed
    if (indexOfNodeWithTag(ast, 0, tag) == null) {
        std.log.err("Expected tag {} not found in ast\n", .{tag});
        assert(false);
    }
}

test "ast-visit" {
    // Test that the visitor reaches each node
    try testVisit("test { }", .test_decl);
    try testVisit("test { var a = 0; }", .simple_var_decl);
    try testVisit("test { var a: u8 align(4) = 0; }", .local_var_decl);
    try testVisit("export const isr_vector linksection(\"isr_vector\") = [_]ISR{};", .global_var_decl);
    try testVisit("test { var a align(4) = 0; }", .aligned_var_decl);
    try testVisit("test { var a = 0; errdefer {a = 1;} }", .@"errdefer");
    try testVisit("test { errdefer |err| { @panic(@errorName(err));} }", .@"errdefer");
    try testVisit("test { var a = 0; defer { a = 1; } }", .@"defer");
    try testVisit("test { foo() catch {}; }", .@"catch");
    try testVisit("test { foo() catch |err| { @panic(@errorName(err)); }; }", .@"catch");
    try testVisit("const A = {var a: u8 = 0;}; test { A.a = 1; }", .field_access);
    try testVisit("test{ a.? = 0; }", .unwrap_optional);
    try testVisit("test { var a = 0 == 1; }", .equal_equal);
    try testVisit("test { var a = 0 != 1; }", .bang_equal);
    try testVisit("test { var a = 0 < 1; }", .less_than);
    try testVisit("test { var a = 0 <= 1; }", .less_or_equal);
    try testVisit("test { var a = 0 >= 1; }", .greater_or_equal);
    try testVisit("test { var a = 0 > 1; }", .greater_than);
    try testVisit("test { var a = 0; a *= 1; }", .assign_mul);
    try testVisit("test { var a = 0; a /= 1; }", .assign_div);
    try testVisit("test { var a = 0; a %= 1; }", .assign_mod);
    try testVisit("test { var a = 0; a += 1; }", .assign_add);
    try testVisit("test { var a = 0; a -= 1; }", .assign_sub);
    try testVisit("test { var a = 1; a <<= 1; }", .assign_shl);
    try testVisit("test { var a = 1; a <<|= 1; }", .assign_shl_sat);
    try testVisit("test { var a = 2; a >>= 1; }", .assign_shr);
    try testVisit("test { var a = 2; a &= 3; }", .assign_bit_and);
    try testVisit("test { var a = 2; a ^= 1; }", .assign_bit_xor);
    try testVisit("test { var a = 2; a |= 1; }", .assign_bit_or);
    try testVisit("test { var a = 2; a *%= 0xFF; }", .assign_mul_wrap);
    try testVisit("test { var a = 2; a +%= 0xFF; }", .assign_add_wrap);
    try testVisit("test { var a = 2; a -%= 0xFF; }", .assign_sub_wrap);
    try testVisit("test { var a = 2; a = 1; }", .assign);
    try testVisit("test {\n const arr: [3]u32 = .{ 10, 20, 30 };\n const x, const y, const z = arr;}", .assign_destructure);
    try testVisit("const E1 = error{E1}; const E2 = E1 || error{E3};", .merge_error_sets);
    try testVisit("test { var a = 2; a = 2 * a; }", .mul);
    try testVisit("test { var a = 2; a = a / 2; }", .div);
    try testVisit("test { var a = 2; a = a % 2; }", .mod);
    try testVisit("test { var a = [2]u8{1, 2} ** 2; }", .array_mult);
    try testVisit("test { var a: u8 = 2 *% 0xFF;}", .mul_wrap);
    try testVisit("test { var a: u8 = 2 *| 0xFF;}", .mul_sat);
    try testVisit("test { var a: u8 = 2 + 0xF0;}", .add);
    try testVisit("test { var a: u8 = 0xF0 - 0x2;}", .sub);
    try testVisit("test { var a = [2]u8{1, 2} ++ [_]u8{3}; }", .array_cat);
    try testVisit("test { var a: u8 = 2 +% 0xFF;}", .add_wrap);
    try testVisit("test { var a: u8 = 2 -% 0xFF;}", .sub_wrap);
    try testVisit("test { var a: u8 = 2 +| 0xFF;}", .add_sat);
    try testVisit("test { var a: u8 = 2 -| 0xFF;}", .sub_sat);
    try testVisit("test { var a: u8 = 2 << 1;}", .shl);
    try testVisit("test { var a: u8 = 2 <<| 10;}", .shl_sat);
    try testVisit("test { var a: u8 = 2 >> 1;}", .shr);
    try testVisit("test { var a: u8 = 2 & 1;}", .bit_and);
    try testVisit("test { var a: u8 = 2 ^ 1;}", .bit_xor);
    try testVisit("test { var a: u8 = 2 | 1;}", .bit_or);
    try testVisit("test { var a: u8 = null orelse 1;}", .@"orelse");
    try testVisit("test { var a = true and true;}", .bool_and);
    try testVisit("test { var a = true or false;}", .bool_or);
    try testVisit("test { var a = !true; }", .bool_not);
    try testVisit("test { var a = -foo(); }", .negation);
    try testVisit("test { var a = ~0x4; }", .bit_not);
    try testVisit("test { var a = -%foo(); }", .negation_wrap);
    try testVisit("test { var a = 0; var b = &a; }", .address_of);
    try testVisit("test { try foo(); }", .@"try");
    try testVisit("test { await foo(); }", .@"await");
    try testVisit("test { var a: ?bool = null; }", .optional_type);
    try testVisit("test { var a: [2]u8 = undefined; }", .array_type);
    try testVisit("test { var a: [2:0]u8 = undefined; }", .array_type_sentinel);
    try testVisit("test { var a: *align(8) u8 = undefined; }", .ptr_type_aligned);
    try testVisit("test { var a: [*]align(8) u8 = undefined; }", .ptr_type_aligned);
    try testVisit("test { var a: []u8 = undefined; }", .ptr_type_aligned);
    try testVisit("test { var a: [*:0]u8 = undefined; }", .ptr_type_sentinel);
    try testVisit("test { var a: [:0]u8 = undefined; }", .ptr_type_sentinel);
    // try testVisit("test { var a: *u8 = undefined; }", .ptr_type_sentinel); // FIXME: maybe docs incorrect ?
    // try testVisit("test { var a: [*c]u8 = undefined; }", .ptr_type);  // TODO: How
    // try testVisit("test { var a: [*c]u8 = undefined; }", .ptr_type_bit_range);  // TODO: How
    try testVisit("test { var a = [2]u8{1,2}; var b = a[0..]; }", .slice_open);
    try testVisit("test { var a = [2]u8{1,2}; var b = a[0..1]; }", .slice);
    try testVisit("test { var a = [2]u8{1,2}; var b = a[0..100:0]; }", .slice_sentinel);
    try testVisit("test { var a = [2]u8{1,2}; var b = a[0..:0]; }", .slice_sentinel);
    try testVisit("test { var a = 0; var b = &a; var c = b.*; }", .deref);
    try testVisit("test { var a = [_]u8{1}; }", .array_init_one);
    try testVisit("test { var a = [_]u8{1,}; }", .array_init_one_comma);
    try testVisit("test { var a: [2]u8 = .{1,2}; }", .array_init_dot_two);
    try testVisit("test { var a: [2]u8 = .{1,2,}; }", .array_init_dot_two_comma);
    try testVisit("test { var a = [_]u8{1,2,3,4,5}; }", .array_init);
    try testVisit("test { var a = [_]u8{1,2,3,}; }", .array_init_comma);
    try testVisit("const A = struct {a: u8};test { var a = A{.a=0}; }", .struct_init_one);
    try testVisit("const A = struct {a: u8};test { var a = A{.a=0,}; }", .struct_init_one_comma);
    try testVisit("const A = struct {a: u8, b: u8};test { var a: A = .{.a=0,.b=1}; }", .struct_init_dot_two);
    try testVisit("const A = struct {a: u8, b: u8};test { var a: A = .{.a=0,.b=1,}; }", .struct_init_dot_two_comma);
    try testVisit("const A = struct {a: u8, b: u8, c: u8};test { var a: A = .{.a=0,.b=1,.c=2}; }", .struct_init_dot);
    try testVisit("const A = struct {a: u8, b: u8, c: u8};test { var a: A = .{.a=0,.b=1,.c=2,}; }", .struct_init_dot_comma);
    try testVisit("const A = struct {a: u8, b: u8, c: u8};test { var a = A{.a=0,.b=1,.c=2}; }", .struct_init);
    try testVisit("const A = struct {a: u8, b: u8, c: u8};test { var a = A{.a=0,.b=1,.c=2,}; }", .struct_init_comma);
    try testVisit("pub fn main(a: u8) void {}\ntest { main(1); }", .call_one);
    try testVisit("pub fn main(a: u8) void {}\ntest { main(1,); }", .call_one_comma);
    try testVisit("pub fn main(a: u8) void {}\ntest { async main(1); }", .async_call_one);
    try testVisit("pub fn main(a: u8) void {}\ntest { async main(1,); }", .async_call_one_comma);
    try testVisit("pub fn main(a: u8, b: u8, c: u8) void {}\ntest { main(1, 2, 3); }", .call);
    try testVisit("pub fn main(a: u8, b: u8, c: u8) void {}\ntest { main(1, 2, 3, ); }", .call_comma);
    try testVisit("pub fn main(a: u8, b: u8, c: u8) void {}\ntest { async main(1, 2, 3); }", .async_call);
    try testVisit("pub fn main(a: u8, b: u8, c: u8) void {}\ntest { async main(1, 2, 3, ); }", .async_call_comma);
    try testVisit("test { var i: u1 = 0; switch (i) {0=>{}, 1=>{}} }", .@"switch");
    try testVisit("test { var a = \"ab\"; switch (a[0]) {'a'=> {}, else=>{},} }", .switch_comma);
    try testVisit("test { var a = \"ab\"; switch (a[0]) {'a'=> {}, else=>{},} }", .switch_case_one);
    try testVisit("test { var i: u1 = 0; switch (i) {0=>{}, inline else=>{}} }", .switch_case_inline_one);
    try testVisit("test { var i: u1 = 0; switch (i) {0, 1=>{} } }", .switch_case);
    try testVisit("test { var i: u1 = 0; switch (i) {inline 0, 1=>{} } }", .switch_case_inline);
    try testVisit("test { var i: u8 = 0; switch (i) {0...8=>{}, else=>{}} }", .switch_range);
    try testVisit("test { while (true) {} }", .while_simple);
    try testVisit("test {var opt: ?u8 = null; while (opt) |v| { _ = v; } }", .while_simple); // TODO: Where is the x ?
    try testVisit("test {var i = 0; while (i < 10) : (i+=1) {} }", .while_cont);
    try testVisit("test {var i = 0; while (i < 0) : (i+=1) {} else {} }", .@"while");
    try testVisit("test {var i = 0; while (i < 0) {} else {} }", .@"while");
    try testVisit("test {var opt: ?u8 = null; while (opt) |v| : (opt = null) { _ = v; } else {} }", .@"while"); // ???
    try testVisit("test {\n for ([2]u8{1,2}) |i| {\n  print(i);\n }\n}", .for_simple);
    // TODO: For
    try testVisit("test {\n for ([2]u8{1,2}, [2]u8{3,4}) |a, b| {\n  print(a + b);\n }\n}", .@"for");
    try testVisit("test {\n for ([2]u8{1,2}, 0..) |a, i| {\n  print(a + i);\n }\n}", .@"for");
    try testVisit("test {var x = [_]u8{}; for (x)|i| {print(i);} else {print(0);}}", .@"for");
    try testVisit("test {\n for (0..2) |i| {\n  print(i);\n }\n}", .for_range);
    try testVisit("test {\n for (0..0) |i| {\n  print(i); if (i == 2) break;\n }\n}", .for_range);
    try testVisit("test { if (true) { var a = 0; } }", .if_simple);
    try testVisit("test {var x = if (true) 1 else 2; }", .@"if");
    try testVisit("test {var x: ?anyframe = null; suspend x;}", .@"suspend");
    try testVisit("test {var x: ?anyframe = null; resume x.?;}", .@"resume");
    try testVisit("test {var i: usize = 0; outer: while (i < 10) : (i += 1) { while (true) { continue :outer; } }}", .@"continue");
    try testVisit("test {var i: usize = 0; while (i < 10) : (i += 1) { continue; } }", .@"continue");
    try testVisit("test {var i: usize = 0; while (i < 10) : (i += 1) { break; } }", .@"break");
    try testVisit("test {var i: usize = 0; outer: while (i < 10) : (i += 1) { while (true) { break :outer; } }}", .@"break");
    try testVisit("pub fn foo() u8 { return 1; }", .@"return");
    try testVisit("pub fn foo() void { return; }", .@"return");
    try testVisit("pub fn foo(a: u8) u8 { return a; }", .fn_proto_simple);
    try testVisit("pub fn foo(a: u8, b: u8) u8 { return a + b; }", .fn_proto_multi);
    try testVisit("pub fn foo(a: u8) callconv(.C) u8 { return a; }", .fn_proto_one);
    try testVisit("pub fn foo(a: u8, b: u8) callconv(.C) u8 { return a + b; }", .fn_proto);
    try testVisit("pub fn foo(a: u8, b: u8) callconv(.C) u8 { return a + b; }", .fn_decl);
    try testVisit("test {var f: anyframe = undefined; anyframe->foo;}", .anyframe_type);
    try testVisit("test {var f: anyframe = undefined;}", .anyframe_literal);
    try testVisit("test {var f: u8 = 'c';}", .char_literal);
    try testVisit("test {var f: u8 = 0;}", .number_literal);
    try testVisit("test {if (false) {unreachable;}}", .unreachable_literal);
    try testVisit("test {var f: u8 = 0;}", .identifier);
    try testVisit("const A = enum {a, b, c}; test {var x: A = .a;}", .enum_literal);
    try testVisit("test {var x = \"abcd\";}", .string_literal);
    try testVisit("test {var x = \\\\aba\n;}", .multiline_string_literal);
    try testVisit("test {var x = (1 + 1);}", .grouped_expression);
    try testVisit("test {var x = @min(1, 2);}", .builtin_call_two);
    try testVisit("test {var x = @min(1, 2,);}", .builtin_call_two_comma);
    try testVisit("test {var x = @min(1, 2, 3);}", .builtin_call);
    try testVisit("test {var x = @min(1, 2, 3,);}", .builtin_call_comma);
    try testVisit("const E = error{a, b};", .error_set_decl);
    try testVisit("const A = struct {a: u8, b: u8, c: u8};", .container_decl);
    try testVisit("const A = struct {a: u8, b: u8, c: u8,};", .container_decl_trailing);
    try testVisit("const A = struct {a: u8, b: u8};", .container_decl_two);
    try testVisit("const A = struct {a: u8, b: u8, };", .container_decl_two_trailing);
    try testVisit("const A = struct(u16) {a: u8, b: u8};", .container_decl_arg);
    try testVisit("const A = struct(u16) {a: u8, b: u8,};", .container_decl_arg_trailing);

    try testVisit("const V = union(enum) {int: i32, boolean: bool, none};", .tagged_union);
    try testVisit("const V = union(enum) {int: i32, boolean: bool, none,};", .tagged_union_trailing);
    try testVisit("const V = union(enum) {int: i32, boolean: bool};", .tagged_union_two);
    try testVisit("const V = union(enum) {int: i32, boolean: bool,};", .tagged_union_two_trailing);
    try testVisit("const V = union(enum(u8)) {int: i32, boolean: bool};", .tagged_union_enum_tag);
    try testVisit("const V = union(enum(u8)) {int: i32, boolean: bool,};", .tagged_union_enum_tag_trailing);

    try testVisit("const A = struct {a: u8 = 0};", .container_field_init);
    try testVisit("const A = struct {a: u8 align(4)};", .container_field_align);
    try testVisit("const A = struct {a: u8 align(4) = 0};", .container_field);
    try testVisit("pub fn foo() u8 { return 1; } test { const x = comptime foo(); }", .@"comptime");
    try testVisit("pub fn foo() u8 { return 1; } test { const x = nosuspend foo(); }", .@"nosuspend");
    try testVisit("test {}", .block_two);
    try testVisit("test {var a = 1;}", .block_two_semicolon);
    try testVisit("test {if (1) {} if (2) {} if (3) {} }", .block);
    try testVisit("test {var a = 1; var b = 2; var c = 3;}", .block_semicolon);
    try testVisit("test { asm(\"nop\"); }", .asm_simple);
    const asm_source =
        \\pub fn syscall0(number: SYS) usize {
        \\  return asm volatile ("svc #0"
        \\    : [ret] "={x0}" (-> usize),
        \\    : [number] "{x8}" (@intFromEnum(number)),
        \\    : "memory", "cc"
        \\  );
        \\}
    ;
    try testVisit(asm_source, .@"asm");
    try testVisit(asm_source, .asm_input);
    try testVisit(asm_source, .asm_output);

    try testVisit("const e = error.EndOfStream;", .error_value);
    try testVisit("const e = error{a} ! error{b};", .error_union);
}

test "all-visit" {
    // Walk entire zig lib source tree
    const allocator = std.testing.allocator;
    const zig_lib = "/home/jrm/projects/zig/";
    var dir = try std.fs.cwd().openIterableDir(zig_lib, .{});
    defer dir.close();
    var walker = try dir.walk(allocator);
    defer walker.deinit();
    const buffer_size = 20*1000*1024; // 20MB
    while (try walker.next()) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.path, ".zig")) {
            std.log.warn("{s}", .{entry.path});
            const file = try entry.dir.openFile(entry.basename, .{});
            const source = try file.readToEndAllocOptions(
                allocator, buffer_size, null, 4, 0
            );
            defer allocator.free(source);
            defer file.close();
            testVisit(source, .root) catch |err| switch (err) {
                error.ParseError => {}, // Skip
                else => return err,
            };
        }
    }
}

const BUILTIN_FN_NAMES = [_][:0]const u8{
    "@addrSpaceCast",
    "@addWithOverflow",
    "@alignCast",
    "@alignOf",
    "@as",
    "@atomicLoad",
    "@atomicRmw",
    "@atomicStore",
    "@bitCast",
    "@bitOffsetOf",
    "@bitSizeOf",
    "@breakpoint",
    "@mulAdd",
    "@byteSwap",
    "@bitReverse",
    "@offsetOf",
    "@call",
    "@cDefine",
    "@cImport",
    "@cInclude",
    "@clz",
    "@cmpxchgStrong",
    "@cmpxchgWeak",
    "@compileError",
    "@compileLog",
    "@constCast",
    "@ctz",
    "@cUndef",
    "@cVaArg",
    "@cVaCopy",
    "@cVaEnd",
    "@cVaStart",
    "@divExact",
    "@divFloor",
    "@divTrunc",
    "@embedFile",
    "@enumFromInt",
    "@errorFromInt",
    "@errorName",
    "@errorReturnTrace",
    "@errorCast",
    "@export",
    "@extern",
    "@fence",
    "@field",
    "@fieldParentPtr",
    "@floatCast",
    "@floatFromInt",
    "@frameAddress",
    "@hasDecl",
    "@hasField",
    "@import",
    "@inComptime",
    "@intCast",
    "@intFromBool",
    "@intFromEnum",
    "@intFromError",
    "@intFromFloat",
    "@intFromPtr",
    "@max",
    "@memcpy",
    "@memset",
    "@min",
    "@wasmMemorySize",
    "@wasmMemoryGrow",
    "@mod",
    "@mulWithOverflow",
    "@panic",
    "@popCount",
    "@prefetch",
    "@ptrCast",
    "@ptrFromInt",
    "@rem",
    "@returnAddress",
    "@select",
    "@setAlignStack",
    "@setCold",
    "@setEvalBranchQuota",
    "@setFloatMode",
    "@setRuntimeSafety",
    "@shlExact",
    "@shlWithOverflow",
    "@shrExact",
    "@shuffle",
    "@sizeOf",
    "@splat",
    "@reduce",
    "@src",
    "@sqrt",
    "@sin",
    "@cos",
    "@tan",
    "@exp",
    "@exp2",
    "@log",
    "@log2",
    "@log10",
    "@abs",
    "@floor",
    "@ceil",
    "@trunc",
    "@round",
    "@subWithOverflow",
    "@tagName",
    "@This",
    "@trap",
    "@truncate",
    "@Type",
    "@typeInfo",
    "@typeName",
    "@TypeOf",
    "@unionInit",
    "@Vector",
    "@volatileCast",
    "@workGroupId",
    "@workGroupSize",
    "@workItemId"
};

export fn is_zig_builtin_fn_name(ptr: [*c]const u8) bool {
    const len = strlen(ptr);
    if (len > 0) {
        // TODO: Pull from BuiltinFn.zig
        const name = ptr[0..len];
        for (BUILTIN_FN_NAMES) |n| {
            if (std.mem.eql(u8, name, n)) {
                return true;
            }
        }
    }
    return false;
}

test "is_zig_builtin_fn_name" {
    try std.testing.expectEqual(true, is_zig_builtin_fn_name("@min"));
    try std.testing.expectEqual(false, is_zig_builtin_fn_name("@foo"));
    try std.testing.expectEqual(false, is_zig_builtin_fn_name(""));
}
