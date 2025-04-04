// Copyright 2023 Jairus Martin <frmdstryr@protonmail.com>
// LGPL
const std = @import("std");
const Ast = std.zig.Ast;
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;

const TokenIndex = Ast.TokenIndex;
const NodeIndex = Ast.Node.Index;
const Tag = Ast.Node.Tag;
// const VisitResult = Ast.VisitResult;
const INVALID_TOKEN = std.math.maxInt(TokenIndex);

var gpa = std.heap.GeneralPurposeAllocator(.{}){};

// kdev-zig makes heavy use of fastTokenLocation which is really slow
// so use a custom version
const ZAst = struct {
    const Self = @This();
    ast: Ast,
    line_offsets: []u32,

    pub fn parse(allocator: Allocator, source: [:0]const u8) !ZAst {
        var line_offsets = std.ArrayListUnmanaged(u32){};
        defer line_offsets.deinit(allocator);

        const estimated_line_count = source.len / 80;
        try line_offsets.ensureTotalCapacity(allocator, estimated_line_count);
        {

            var offset: u32 = 0;
            while (std.mem.indexOfScalarPos(u8, source, offset, '\n')) |i| {
                try line_offsets.append(allocator, @intCast(i));
                offset = @as(u32, @intCast(i)) + 1;
            }
            // If the file does not end with a newline, add a final entry
            if (offset < source.len) {
                try line_offsets.append(allocator, @intCast(source.len));
            }
        }
        return ZAst{
            .ast = try Ast.parse(allocator, source, .zig),
            .line_offsets = try line_offsets.toOwnedSlice(allocator),
        };
    }

    pub fn fastTokenLocation(self: ZAst, token_index: TokenIndex) Ast.Location {
        var loc = Ast.Location{
            .line = 0,
            .column = 0,
            .line_start = 0,
            .line_end = self.ast.source.len,
        };
        const token_start = self.ast.tokens.items(.start)[token_index];
        for (self.line_offsets) |i| {
            if (i >= token_start) {
                loc.column = token_start - loc.line_start;
                loc.line_end = i;
                break; // Went past
            }
            loc.line += 1;
            loc.line_start = i + 1;
        }
        return loc;
    }


    pub fn deinit(self: *ZAst, allocator: Allocator) void {
        self.ast.deinit(allocator);
        allocator.free(self.line_offsets);
        self.* = undefined;
    }

};

const SourceLocation = extern struct {
    line: u32 = 0,
    column: u32 = 0,
};

const SourceRange = extern struct {
    start: SourceLocation = .{},
    end: SourceLocation = .{}
};

const SourceSlice = extern struct {
    data: ?[*]const u8 = null,
    len: u32 = 0,
};

const Severity = enum(u32) { NoSeverity = 0, Error = 1, Warning = 2, Hint = 4 };

const VisitResult = enum(u32) {
    Break = 0,
    Continue,
    Recurse,
};

const CaptureType = enum(u32) {
    Payload = 0,
    Error
};

// TODO: Just use import C ?
const NodeKind = enum(u32) {
    Unknown = 0,
    Module, // Root
    ContainerDecl, // struct, union
    EnumDecl,
    UnionDecl,
    FieldDecl, // container_field
    FunctionDecl,
    ParamDecl,
    VarDecl,
    BlockDecl,
    ErrorDecl,
    TestDecl,
    // Uses
    Call,
    If,
    For,
    While,
    Switch,
    Defer,
    Catch,
    Usingnamespace,
    FnProto
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

fn printAstError(zast: *ZAst, filename: []const u8, source: []const u8) !void {
    const stderr = std.io.getStdErr().writer();
    for (zast.ast.errors) |parse_error| {
        const loc = zast.fastTokenLocation(parse_error.token);
        try stderr.print("{s}:{d}:{d}: error: ", .{ filename, loc.line + 1, loc.column + 1 });
        try zast.ast.renderError(parse_error, stderr);
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

pub fn dumpAstFlat(ast: Ast, stream: anytype) !void {
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

pub fn dumpAstNodes(ast: Ast, nodes: []NodeIndex, stream: anytype) !void {
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

const CompletionResultType = enum(u32) {
    Unknown = 0,
    Field,
    BuiltinCall,
    Identifier,
};

const ZCompletion = extern struct {
    const Self = @This();
    result_type: CompletionResultType = .Unknown,
    name: [*c]const u8 = null,

    pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
        const name_len = strlen(self.name);
        if (name_len > 0) {
            allocator.free(self.name[0 .. name_len + 1]);
        }
        allocator.destroy(self);
    }
};

export fn complete_expr(text_ptr: [*c]const u8, text_following_ptr: [*c]const u8) ?*ZCompletion {
    const text_len = strlen(text_ptr);
    const following_len = strlen(text_following_ptr);
    if (text_len == 0) {
        return null;
    }
    const text: [:0]const u8 = text_ptr[0..text_len :0];
    const following: [:0]const u8 = text_following_ptr[0..following_len:0];
    std.log.info("zig: complete: {s} {s}", .{text, following});

    const line_start = std.mem.lastIndexOfScalar(u8, text, '\n') orelse 0;
    const last_line = text[line_start..];


    const allocator = gpa.allocator();
    var completion = ZCompletion{};
    var zast = ZAst.parse(allocator, last_line) catch {
        return null;
    };
    defer zast.deinit(allocator);
    const stdout = std.io.getStdOut().writer();
    dumpAstFlat(zast.ast, stdout) catch {};
    //const decls = ast.rootDecls();

    if (std.mem.endsWith(u8, last_line, ".")) {
        completion.result_type = .Field;
    }
    else if (std.mem.endsWith(u8, last_line, "@\"")) {
        completion.result_type = .Identifier;
    }
    else if (std.mem.endsWith(u8, last_line, "@")) {
        completion.result_type = .BuiltinCall;
    }


    if (zast.ast.nodes.len > 0) {
        const i: u32 = @intCast(zast.ast.nodes.len-1);
        const tags = zast.ast.nodes.items(.tag);
        const node_data = zast.ast.nodes.items(.data);
        const main_tokens = zast.ast.nodes.items(.main_token);
        const tag: Tag = tags[i];
        switch (tag) {
            .identifier => {
                const tok = main_tokens[i];
                completion.name = allocator.dupeZ(u8, zast.ast.tokenSlice(tok)) catch {
                    return null;
                };
            },
            .field_access => {
                const d = node_data[i];
                if (d.lhs == 0) {
                    return null;
                }
                if (tags[d.lhs] == .identifier) {
                    const start_tok = main_tokens[d.lhs];
                    const start_loc = zast.fastTokenLocation(start_tok);
                    const start = start_loc.line_start + start_loc.column;
                    const end_loc = zast.fastTokenLocation(d.rhs);
                    const attr = zast.ast.tokenSlice(d.rhs);
                    const end = end_loc.line_start + end_loc.column + attr.len;
                    if (end > start) {
                        const name = zast.ast.source[start..end];
                        completion.name = allocator.dupeZ(u8, name) catch {
                            return null;
                        };
                    }
                }
                return null;
            },
            .container_field_init => {
                const tok = main_tokens[i];
                const d = node_data[i];
                var name = zast.ast.tokenSlice(tok);
                if (d.lhs != 0) {
                    if (tags[d.lhs] == .field_access) {
                        const start = zast.fastTokenLocation(tok);
                        const end = zast.fastTokenLocation(d.rhs);
                        if (end.line_end - 1 > start.line_start) {
                            name = zast.ast.source[start.line_start..end.line_end - 1];
                        }
                    }
                }
                completion.name = allocator.dupeZ(u8, name) catch {
                    return null;
                };
            },
            else => {},
        }
    }

    const result: *ZCompletion = allocator.create(ZCompletion) catch {
        return null;
    };
    result.* = completion;
    return result;
}

export fn ast_tag_by_name(tag_name: [*c]const u8) u32 {
    if (tag_name) |n| {
        const name = std.mem.span(n);
        inline for(std.meta.fields(std.zig.Ast.Node.Tag)) |f| {
            if (std.mem.eql(u8, name, f.name)) {
                return f.value;
            }
        }
    }
    return std.math.maxInt(u32);
}

export fn parse_ast(name_ptr: [*c]const u8, source_ptr: [*c]const u8, print_ast: bool) ?*ZAst {
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
    const zast: *ZAst = allocator.create(ZAst) catch |err| {
        std.log.warn("zig: parsing {s} error: {}", .{ name, err });
        return null;
    };
    zast.*= ZAst.parse(allocator, source) catch |err| {
        std.log.warn("zig: parsing {s} error: {}", .{ name, err });
        allocator.destroy(zast);
        return null;
    };

    if (zast.ast.errors.len > 0) {
        printAstError(zast, name, source) catch |err| {
            std.log.warn("zig: failed to print trace {}", .{err});
        };
    }
    else if (print_ast) {
        var stdout = std.io.getStdOut().writer();
        stdout.print("Source ----------------\n{s}\n------------------------\n", .{source}) catch {};
        dumpAstFlat(zast.ast, stdout) catch |err| {
            std.log.debug("zig: failed to dump ast {}", .{err});
        };
    }
    return zast;
}

export fn ast_error_count(ptr: ?*ZAst) u32 {
    // std.log.warn("zig: ast_error_count", .{});
    if (ptr) |zast| {
        return @intCast(zast.ast.errors.len);
    }
    return 0;
}

export fn ast_error_at(ptr: ?*ZAst, index: u32) ?*ZError {
    // std.log.warn("zig: ast_error_at {}", .{index});
    if (ptr) |zast| {
        if (index >= zast.ast.errors.len) {
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

        const err = zast.ast.errors[index];
        const loc = zast.fastTokenLocation(err.token);
        zast.ast.renderError(err, fbs.writer()) catch {
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

export fn destroy_ast(ptr: ?*ZAst) void {
    // std.log.debug("zig: destroy_ast {}", .{@intFromPtr(ptr)});
    if (ptr) |zast| {
        zast.deinit(gpa.allocator());
    }
}

export fn destroy_error(ptr: ?*ZError) void {
    // std.log.debug("zig: destroy_error {}", .{@intFromPtr(ptr)});
    if (ptr) |zerr| {
        zerr.deinit(gpa.allocator());
    }
}

export fn destroy_completion(ptr: ?*ZCompletion) void {
    // std.log.debug("zig: destroy_completion {}", .{@intFromPtr(ptr)});
    if (ptr) |zcompletion| {
        zcompletion.deinit(gpa.allocator());
    }
}

export fn ast_node_block_label_token(ptr: ?*ZAst, index: NodeIndex) TokenIndex {
    if (ptr) |zast| {
        if (index >= zast.ast.nodes.len) {
            return INVALID_TOKEN;
        }

        const tag = zast.ast.nodes.items(.tag)[index];
        switch (tag) {
            .block_two,
            .block_two_semicolon,
            .block,
            .block_semicolon => {
                const lbrace = zast.ast.nodes.items(.main_token)[index];
                const token_tags =  zast.ast.tokens.items(.tag);
                if (token_tags[lbrace-1] == .colon and token_tags[lbrace-2] == .identifier) {
                    return lbrace - 2;
                }
            },
            else => {}
        }
    }
    return INVALID_TOKEN;
}

export fn ast_node_capture_token(ptr: ?*ZAst, index: NodeIndex, capture: CaptureType) TokenIndex {
    if (ptr) |zast| {
        if (index >= zast.ast.nodes.len) {
            return INVALID_TOKEN;
        }
        if (zast.ast.fullIf(index)) |r| {
            return switch (capture) {
                .Payload => r.payload_token orelse INVALID_TOKEN,
                .Error => r.error_token orelse INVALID_TOKEN,
            };
        }
        if (zast.ast.fullWhile(index)) |r| {
            return switch (capture) {
                .Payload => r.payload_token orelse INVALID_TOKEN,
                .Error => r.error_token orelse INVALID_TOKEN,
            };
        }

        if (zast.ast.fullFor(index)) |r| {
            return switch (capture) {
                .Payload => r.payload_token,
                .Error => INVALID_TOKEN,
            };
        }

        const tag = zast.ast.nodes.items(.tag)[index];
        if (tag == .@"errdefer") {
            return switch (capture) {
                .Payload => zast.ast.nodes.items(.data)[index].lhs,
                .Error => INVALID_TOKEN,
            };
        }
        if (tag == .@"catch") {
            // If token after catch is a | then uses token after that
            const main_token = zast.ast.nodes.items(.main_token)[index];
            if (zast.ast.tokens.items(.tag)[main_token+1] == .pipe) {
                return main_token + 2;
            }
        }
    }
    return INVALID_TOKEN;
}


fn testCaptureName(source: [:0]const u8, tag: Tag, capture: CaptureType, expected: ?[]const u8) !void {
    const allocator = std.testing.allocator;
    var ast = try ZAst.parse(allocator, source);
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (ast.ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&ast, "", source);
    }
    try std.testing.expect(ast.ast.errors.len == 0);
    try dumpAstFlat(ast.ast, stdout);

    const index = indexOfNodeWithTag(ast.ast, 0, tag).?;
    const tok = ast_node_capture_token(&ast, index, capture);
    if (expected) |str| {
        try std.testing.expectEqualSlices(u8, str, ast.ast.tokenSlice(tok));
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

export fn ast_node_range(ptr: ?*ZAst, index: NodeIndex) SourceRange {
    if (ptr == null) {
        std.log.warn("zig: ast_node_range ast null", .{});
        return SourceRange{};
    }
    const zast = ptr.?;
    if (index >= zast.ast.nodes.len) {
        std.log.warn("zig: ast_node_range index out of range {}", .{index});
        return SourceRange{};
    }
    // TODO: What is the correct start_offset?
    const first_token = zast.ast.firstToken(index);
    const start_loc = zast.fastTokenLocation(first_token);
    const last_token = zast.ast.lastToken(index);
    const end_loc = zast.fastTokenLocation(last_token);

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
fn isTokenSliceEql(ast: Ast, token: TokenIndex, value: []const u8) bool {
    return std.mem.eql(u8, ast.tokenSlice(token), value);
}

// Node kind is only needed for contexts
export fn ast_node_kind(ptr: ?*ZAst, index: NodeIndex) u32 {
    if (ptr == null) {
        return @intFromEnum(NodeKind.Unknown);
    }
    const zast = ptr.?;
    if (index >= zast.ast.nodes.len) {
        return @intFromEnum(NodeKind.Unknown);
    }
    const main_token = zast.ast.nodes.items(.main_token)[index];
    const tag: Tag = zast.ast.nodes.items(.tag)[index];
    const kind: NodeKind = switch (tag) {
        .fn_decl => .FunctionDecl,
        .fn_proto,
        .fn_proto_multi,
        .fn_proto_one,
        .fn_proto_simple => .FnProto,
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
        .container_decl_arg_trailing => switch (zast.ast.tokens.items(.tag)[main_token]) {
            .keyword_enum => .EnumDecl,
            .keyword_union => .UnionDecl,
            else => .ContainerDecl
        },
        .block,
        .block_semicolon,
        .block_two,
        .block_two_semicolon,
        .@"comptime" => .BlockDecl,
        .container_field_init,
        .container_field_align,
        .container_field => .FieldDecl,
        .error_set_decl => .ErrorDecl,
        .builtin_call,
        .builtin_call_comma,
        .builtin_call_two,
        .builtin_call_two_comma,
        .call,
        .call_comma,
        .call_one,
        .call_one_comma,
        .async_call,
        .async_call_comma,
        .async_call_one,
        .async_call_one_comma => .Call,
        .tagged_union,
        .tagged_union_trailing,
        .tagged_union_two,
        .tagged_union_two_trailing,
        .tagged_union_enum_tag,
        .tagged_union_enum_tag_trailing => .UnionDecl,
        //.struct_init, .struct_init_comma, .struct_init_dot, .struct_init_dot_comma, .struct_init_dot_two, .struct_init_dot_two_comma, .struct_init_one, .struct_init_one_comma => .ContainerInit,
        //.equal_equal, .bang_equal, .less_than, .greater_than, .less_or_equal, .greater_or_equal, .mul, .div, .mod, .mul_wrap, .mul_sat, .add, .sub, .add_wrap, .sub_wrap, .add_sat, .sub_sat, .shl, .shl_sat, .shr, .bit_and, .bit_xor, .bit_or, .bool_and, .bool_or, .assign_mul, .assign_div, .assign_mod, .assign_add, .assign_sub, .assign_shl, .assign_shl_sat, .assign_shr, .assign_bit_and, .assign_bit_xor, .assign_bit_or, .assign_mul_wrap, .assign_add_wrap, .assign_sub_wrap, .assign_mul_sat, .assign_add_sat, .assign_sub_sat, .assign, .assign_destructure => .VarAccess,
        //.deref => .PtrAccess,
        //.unwrap_optional, .error_value, .field_access => .FieldAccess,
        //.array_mult, .array_cat, .array_access => .ArrayAccess,
        //.string_literal, .multiline_string_literal, .number_literal, .char_literal => .Literal,
        // .identifier => .Ident,

        .if_simple, .@"if" => .If,
        .for_range, .for_simple, .@"for" => .For,
        .while_cont, .while_simple, .@"while" => .While,
        .switch_comma, .@"switch" => .Switch,
        .@"defer", .@"errdefer" => .Defer,
        .@"catch" => .Catch,
        .@"usingnamespace" => .Usingnamespace,
        // TODO
        .root => .Module,
        else => .Unknown,
    };
    return @intFromEnum(kind);
}

fn testNodeKind(source: [:0]const u8, index: NodeIndex, expected: NodeKind) !void {
    const allocator = std.testing.allocator;
    var ast = try ZAst.parse(allocator, source);
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (ast.ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&ast, "", source);
    }
    try std.testing.expect(ast.ast.errors.len == 0);
    const r = ast_node_kind(&ast, index);
    try std.testing.expect(r == expected);
}

test "node-kind" {
    try testNodeKind(
        \\pub fn foo() void {}
       , 0, .Module);
}


export fn ast_node_tag(ptr: ?*ZAst, index: NodeIndex) u32 {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            const tag = zast.ast.nodes.items(.tag)[index];
            // std.log.debug("zig: ast_node_tag {} is {s}", .{ index, @tagName(tag) });
            return @intFromEnum(tag);
        }
    }
    return @intFromEnum(Ast.Node.Tag.root); // Invalid unless index == 0
}

export fn ast_token_slice(ptr: ?*ZAst, token: TokenIndex) SourceSlice {
    if (ptr) |zast| {
        if (token < zast.ast.tokens.len) {
            const name = zast.ast.tokenSlice(token);
            return SourceSlice{.data=name.ptr, .len=@intCast(name.len)};
        }
    }
    return SourceSlice{};
}

fn findNodeComment(zast: *const ZAst, node: NodeIndex) ?[]const u8 {
    const first_token = zast.ast.firstToken(node);
    const loc = zast.fastTokenLocation(first_token);
    // std.log.warn("node start: {}", .{loc});
    const comment_end = loc.line_start -| 1;
    var comment_start: usize = comment_end;

    while (comment_start > 3) {
        // Find previous next line
        const remaining = zast.ast.source[0..comment_start];
        // std.log.warn("remaining: '{s}'", .{remaining});
        const previous_line_start = std.mem.lastIndexOfScalar(u8, remaining, '\n') orelse 0;
        // std.log.warn("previous_line_start: '{}'", .{previous_line_start});

        const comment = std.mem.trimLeft(u8, zast.ast.source[previous_line_start..comment_end], " \t\n");
        if (std.mem.startsWith(u8, comment, "//")) {
            comment_start = previous_line_start;
        } else {
            break;
        }
    }
    const comment = std.mem.trim(u8, zast.ast.source[comment_start..comment_end], " \t\r\n");
    if (comment.len < 3 or comment.len > 5000) {
        return null;
    }
    return comment;
}

fn testNodeComment(source: [:0]const u8, tag: Tag, expected: ?[]const u8) !void {
    const allocator = std.testing.allocator;
    var ast = try ZAst.parse(allocator, source);
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (ast.ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&ast, "", source);
    }
    try std.testing.expect(ast.ast.errors.len == 0);

    const n = indexOfNodeWithTag(ast.ast, 0, tag);
    if (n == null) {
        try dumpAstFlat(ast.ast, stdout);
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

export fn ast_node_comment(ptr: ?*ZAst, node: NodeIndex) SourceSlice {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            if (findNodeComment(zast, node)) |comment| {
                return SourceSlice{.data=comment.ptr, .len=@intCast(comment.len)};
            }
        }
    }
    return SourceSlice{};
}

export fn ast_token_range(ptr: ?*ZAst, token: TokenIndex) SourceRange {
    if (ptr) |zast| {
        if (token < zast.ast.tokens.len) {
            const loc = zast.fastTokenLocation(token);
            const name = zast.ast.tokenSlice(token);
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

export fn ast_var_is_const(ptr: ?*ZAst, index: NodeIndex) bool {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            const tag = zast.ast.nodes.items(.tag)[index];
            return switch (tag) {
                .simple_var_decl, .aligned_var_decl, .global_var_decl, .local_var_decl => isTokenSliceEql(zast.ast, zast.ast.nodes.items(.main_token)[index], "const"),
                else => false,
            };
        }
    }
    return false;
}

// Return var / field type or 0
export fn ast_var_type(ptr: ?*ZAst, index: NodeIndex) NodeIndex {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            if (zast.ast.fullVarDecl(index)) |var_data| {
                return var_data.ast.type_node;
            }
            if (zast.ast.fullContainerField(index)) |field_data| {
                return field_data.ast.type_expr;
            }
        }
    }
    return 0;
}

export fn ast_var_value(ptr: ?*ZAst, index: NodeIndex) NodeIndex {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            if (zast.ast.fullVarDecl(index)) |var_data| {
                return var_data.ast.init_node;
            }
            if (zast.ast.fullContainerField(index)) |field_data| {
                return field_data.ast.value_expr;
            }
        }
    }
    return 0;
}


export fn ast_fn_returns_inferred_error(ptr: ?*ZAst, index: NodeIndex) bool {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            var buf: [1]Ast.Node.Index = undefined;
            if (zast.ast.fullFnProto(&buf, index)) |proto| {
                // Zig doesn't use error union if error type is omitted
                const token_tags = zast.ast.tokens.items(.tag);
                const maybe_bang = zast.ast.firstToken(proto.ast.return_type) - 1;
                return token_tags[maybe_bang] == .bang;
            }
        }
    }
    return false;
}

export fn ast_fn_name(ptr: ?*ZAst, index: NodeIndex) TokenIndex {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            var buf: [1]Ast.Node.Index = undefined;
            if (zast.ast.fullFnProto(&buf, index)) |proto| {
                if (proto.name_token) |tok| {
                    return tok;
                }
            }
        }
    }
    return INVALID_TOKEN;
}

export fn ast_fn_return_type(ptr: ?*ZAst, index: NodeIndex) NodeIndex {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            var buf: [1]Ast.Node.Index = undefined;
            if (zast.ast.fullFnProto(&buf, index)) |proto| {
                return proto.ast.return_type;
            }
        }
    }
    return 0;
}

export fn ast_call_arg_count(ptr: ?*ZAst, index: NodeIndex) u32 {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            var buffer: [1]Ast.Node.Index = undefined;
            if (zast.ast.fullCall(&buffer, index)) |call| {
                return @intCast(call.ast.params.len);
            }
        }
    }
    return 0;
}

export fn ast_call_arg_at(ptr: ?*ZAst, index: NodeIndex, i: u32) NodeIndex {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            var buffer: [1]Ast.Node.Index = undefined;
            if (zast.ast.fullCall(&buffer, index)) |call| {
                if (i < call.ast.params.len) {
                    return call.ast.params[i];
                }
            }
        }
    }
    return 0;
}


export fn ast_fn_param_count(ptr: ?*ZAst, index: NodeIndex) u32 {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            var buffer: [1]Ast.Node.Index = undefined;
            if (zast.ast.fullFnProto(&buffer, index)) |proto| {
                var i: u32 = 0;
                var iter = proto.iterate(&zast.ast);
                while (iter.next()) |_| {
                    i += 1;
                }
                return i;
            }
        }
    }
    return 0;
}

const ParamDataInfo = packed struct {
    is_comptime: bool = false,
    is_noalias: bool = false,
    is_anytype: bool = false,
    is_vararg: bool = false,
    reserved: u4 = 0,
};

const ParamData = extern struct {
    name_token: TokenIndex = INVALID_TOKEN,
    type_expr: NodeIndex = 0,
    info: ParamDataInfo = .{},
};

export fn ast_fn_param_at(ptr: ?*ZAst, index: NodeIndex, i: u32) ParamData {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            var buffer: [1]Ast.Node.Index = undefined;
            if (zast.ast.fullFnProto(&buffer, index)) |proto| {
                var iter = proto.iterate(&zast.ast);
                var j: u32 = 0;
                while (iter.next()) |param| {
                    if (j == i) {
                        return ParamData{
                            .name_token = param.name_token orelse INVALID_TOKEN,
                            .type_expr = param.type_expr,
                            .info = ParamDataInfo{
                                .is_comptime = if (param.comptime_noalias) |tok| isTokenSliceEql(zast.ast, tok, "comptime") else false,
                                .is_noalias = if (param.comptime_noalias) |tok| isTokenSliceEql(zast.ast, tok, "noalias") else false,
                                .is_anytype = if (param.anytype_ellipsis3) |tok| isTokenSliceEql(zast.ast, tok, "anytype") else false,
                                .is_vararg = if (param.anytype_ellipsis3) |tok| isTokenSliceEql(zast.ast, tok, "...") else false,
                            },
                        };
                    }
                    j += 1;
                }
            }
        }
    }
    return ParamData{};
}

export fn ast_struct_init_field_count(ptr: ?*ZAst, index: NodeIndex) u32 {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            var buffer: [2]Ast.Node.Index = undefined;
            if (zast.ast.fullStructInit(&buffer, index)) |struct_data| {
                return @intCast(struct_data.ast.fields.len);
            }
        }
    }
    return 0;
}


const FieldInitData = extern struct {
    name_token: TokenIndex = INVALID_TOKEN,
    value_expr: NodeIndex = 0,
};

export fn ast_struct_init_field_at(ptr: ?*ZAst, index: NodeIndex, i: u32) FieldInitData {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            var buffer: [2]Ast.Node.Index = undefined;
            if (zast.ast.fullStructInit(&buffer, index)) |struct_data| {
                if (i < struct_data.ast.fields.len) {
                    const f = struct_data.ast.fields[i];
                    return FieldInitData{
                        // TODO: Is this valid for every case???
                        .name_token=zast.ast.firstToken(f) -| 2,
                        .value_expr=f
                    };
                }
            }
        }
    }
    return FieldInitData{};
}

// Note: 0 is a valid token
export fn ast_node_main_token(ptr: ?*ZAst, index: NodeIndex) TokenIndex {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            return zast.ast.nodes.items(.main_token)[index];
        }
    }
    return INVALID_TOKEN;
}


// Lookup the token index that has the name/identifier for the given node
export fn ast_node_name_token(ptr: ?*ZAst, index: NodeIndex) TokenIndex {
    if (ptr) |zast| {
        if (index < zast.ast.nodes.len) {
            const tag: Tag = zast.ast.nodes.items(.tag)[index];
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
                .fn_decl => ast_node_name_token(zast, zast.ast.nodes.items(.data)[index].lhs),

                // Data rhs is the identifier
                .field_access => zast.ast.nodes.items(.data)[index].rhs,

                // Token after main_token is the name for all these
                .fn_proto_simple,
                .fn_proto_multi,
                .fn_proto_one,
                .fn_proto,
                .global_var_decl,
                .local_var_decl,
                .simple_var_decl,
                .aligned_var_decl => zast.ast.nodes.items(.main_token)[index] + 1,

                // Main token is identifier
                .identifier,
                .string_literal,
                .enum_literal,
                .builtin_call,
                .builtin_call_comma,
                .builtin_call_two,
                .builtin_call_two_comma,
                .container_field,
                .container_field_init,
                .container_field_align => zast.ast.nodes.items(.main_token)[index],
                .test_decl => blk: {
                    const v = zast.ast.nodes.items(.data)[index].lhs;
                    break :blk if (v == 0) INVALID_TOKEN else v;
                },
                else => INVALID_TOKEN,
            };
        }
    }
    return INVALID_TOKEN;
}

fn indexOfNodeWithTag(ast: Ast, start_token: TokenIndex, tag: Tag) ?NodeIndex {
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

fn isNodeConstVarDecl(ast: *Ast, index: NodeIndex) bool {
    if (index < ast.nodes.len and isTagVarDecl(ast.nodes.items(.tag)[index])) {
        const main_token = ast.nodes.items(.main_token)[index];
        return ast.tokens.items(.tag)[main_token] == .keyword_const;
    }
    return false;
}

fn testNodeIdent(source: [:0]const u8, tag: Tag, expected: ?[]const u8) !void {
    const allocator = std.testing.allocator;
    var zast = try ZAst.parse(allocator, source);
    defer zast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (zast.ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&zast, "", source);
    }
    try std.testing.expect(zast.ast.errors.len == 0);
    try dumpAstFlat(zast.ast, stdout);

    const index = indexOfNodeWithTag(zast.ast, 0, tag).?;
    const tok = ast_node_name_token(&zast, index);
    if (expected) |str| {
        try std.testing.expectEqualSlices(u8, zast.ast.tokenSlice(tok), str);
        try stdout.print("location={}\n", .{zast.fastTokenLocation(tok)});
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
    var zast = try ZAst.parse(allocator, source);
    defer zast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (zast.ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&zast, "", source);
    }
    try std.testing.expect(zast.ast.errors.len == 0);
    try dumpAstFlat(zast.ast, stdout);

    const index = indexOfNodeWithTag(zast.ast, 0, tag).?;
    const tok = ast_node_name_token(&zast, index);
    const range = ast_token_range(&zast, tok);
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
    lhs: NodeIndex = 0,
    rhs: NodeIndex = 0,
};

// Visit one child
export fn ast_node_data(ptr: ?*ZAst, node: NodeIndex) NodeData {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            const data: Ast.Node.Data = zast.ast.nodes.items(.data)[node];
            return NodeData{.lhs = data.lhs, .rhs = data.rhs};
        }
    }
    return NodeData{};
}

export fn ast_extra_data(ptr: ?*ZAst, index: NodeIndex) u32 {
    if (ptr) |zast| {
        if (index < zast.ast.extra_data.len) {
            return zast.ast.extra_data[index];
        }
    }
    return 0;
}

const ArrayTypeSentinel = extern struct {
    sentinel: NodeIndex = 0,
    elem_type: NodeIndex = 0,
};

export fn ast_array_type_sentinel(ptr: ?*ZAst, node: NodeIndex) ArrayTypeSentinel {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            const tag: Tag = zast.ast.nodes.items(.tag)[node];
            if (tag == .array_type_sentinel) {
                const d: Ast.Node.Data = zast.ast.nodes.items(.data)[node];
                const array_data = zast.ast.extraData(d.rhs, Ast.Node.ArrayTypeSentinel);
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
    payload_token: TokenIndex = INVALID_TOKEN,
    error_token: TokenIndex = INVALID_TOKEN,
    cond_expr: NodeIndex = 0,
    then_expr: NodeIndex = 0,
    else_expr: NodeIndex = 0,
};

export fn ast_if_data(ptr: ?*ZAst, node: NodeIndex) IfData {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            if (zast.ast.fullIf(node)) |data| {
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
    lib_name: TokenIndex = INVALID_TOKEN,
    type_node: NodeIndex = 0,
    align_node: NodeIndex = 0,
    addrspace_node: NodeIndex = 0,
    section_node: NodeIndex = 0,
    init_node: NodeIndex = 0,
    info: VarDataInfo = .{},
};

export fn ast_var_data(ptr: ?*ZAst, node: NodeIndex) VarData {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            if (zast.ast.fullVarDecl(node)) |data| {
                return VarData{
                    .lib_name = data.lib_name orelse INVALID_TOKEN,
                    .type_node = data.ast.type_node,
                    .align_node = data.ast.align_node,
                    .addrspace_node = data.ast.addrspace_node,
                    .section_node = data.ast.section_node,
                    .init_node = data.ast.init_node,
                    .info = VarDataInfo{
                        .is_pub = data.visib_token != null,
                        .is_const = isTokenSliceEql(zast.ast, data.ast.mut_token, "const"),
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

const PtrTypeInfo = packed struct {
    is_nullable: bool = false,
    is_const: bool = false,
    is_volatile: bool = false,
    reserved: u5 = 0,
};

const PtrTypeData = extern struct {
    main_token: TokenIndex = INVALID_TOKEN,
    align_node: NodeIndex = 0,
    addrspace_node: NodeIndex= 0,
    sentinel: NodeIndex = 0,
    bit_range_start: NodeIndex = 0,
    bit_range_end: NodeIndex = 0,
    child_type: NodeIndex = 0,
    info: PtrTypeInfo = .{},
};

export fn ast_ptr_type_data(ptr: ?*ZAst, node: NodeIndex) PtrTypeData {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            if (zast.ast.fullPtrType(node)) |ptr_data| {
                return PtrTypeData{
                    .main_token = ptr_data.ast.main_token,
                    .align_node = ptr_data.ast.align_node,
                    .sentinel = ptr_data.ast.sentinel,
                    .bit_range_start = ptr_data.ast.bit_range_start,
                    .bit_range_end = ptr_data.ast.bit_range_end,
                    .child_type = ptr_data.ast.child_type,
                    .info = PtrTypeInfo{
                        .is_nullable = ptr_data.allowzero_token != null,
                        .is_const = ptr_data.const_token != null,
                        .is_volatile = ptr_data.volatile_token != null,
                    },
                };
            }
        }
    }
    return PtrTypeData{};
}


export fn ast_array_init_item_size(ptr: ?*ZAst, node: NodeIndex) u32 {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            var nodes: [2]Ast.Node.Index = undefined;
            if (zast.ast.fullArrayInit(&nodes, node)) |array_data| {
                return @intCast(array_data.ast.elements.len);
            }
        }
    }
    return 0;
}

export fn ast_array_init_item_at(ptr: ?*ZAst, node: NodeIndex, i: u32) NodeIndex {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            var nodes: [2]Ast.Node.Index = undefined;
            if (zast.ast.fullArrayInit(&nodes, node)) |array_data| {
                if (i < array_data.ast.elements.len) {
                    return array_data.ast.elements[i];
                }
            }
        }
    }
    return 0;
}

const SubRange = extern struct {
    start: NodeIndex = 0,
    end: NodeIndex = 0
};

export fn ast_sub_range(ptr: ?*ZAst, node: NodeIndex) SubRange {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            const tag = zast.ast.nodes.items(.tag)[node];
            const d = zast.ast.nodes.items(.data)[node];
            switch (tag) {
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
                    const field_range = zast.ast.extraData(d.rhs, Ast.Node.SubRange);
                    return SubRange{
                        .start=field_range.start,
                        .end=field_range.end,
                    };
                },
                .switch_case, .switch_case_inline, .fn_proto_multi => {
                    const field_range = zast.ast.extraData(d.lhs, Ast.Node.SubRange);
                    return SubRange{
                        .start=field_range.start,
                        .end=field_range.end,
                    };
                },
                else => {
                    // std.log.warn("sub range accessed on invalid node {} tag: {}", .{node, tag});
                },
            }
        }
    }
    return SubRange{};
}

export fn ast_switch_case_size(ptr: ?*ZAst, node: NodeIndex) u32 {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            if (zast.ast.fullSwitchCase(node)) |switch_case| {
                return @intCast(switch_case.ast.values.len);
            }
        }
    }
    return 0;
}

export fn ast_switch_case_item_at(ptr: ?*ZAst, node: NodeIndex, i: u32) NodeIndex {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            if (zast.ast.fullSwitchCase(node)) |switch_case| {
                if (i < switch_case.ast.values.len) {
                    return switch_case.ast.values[i];
                }
            }
        }
    }
    return 0;
}

export fn ast_for_input_count(ptr: ?*ZAst, node: NodeIndex) u32 {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            if (zast.ast.fullFor(node)) |for_data| {
                return @intCast(for_data.ast.inputs.len);
            }
        }
    }
    return 0;
}

export fn ast_for_input_at(ptr: ?*ZAst, node: NodeIndex, i: u32) NodeIndex {
    if (ptr) |zast| {
        if (node < zast.ast.nodes.len) {
            if (zast.ast.fullFor(node)) |for_data| {
                if (i < for_data.ast.inputs.len) {
                    return for_data.ast.inputs[i];
                }
            }
        }
    }
    return 0;
}

fn visitOne(zast: *const ZAst, node: NodeIndex, parent: NodeIndex, data: *NodeIndex) VisitResult {
    _ = zast;
    _ = parent;
    data.* = node;
    return .Break;
}

// Visit one child
export fn ast_visit_one_child(ptr: ?*ZAst, node: NodeIndex) NodeIndex {
    var result: NodeIndex = 0;
    if (ptr) |zast| {
        visit(zast, node, *NodeIndex, visitOne, &result);
    }
    return result;
}

fn visitAll(zast: *const ZAst, child: NodeIndex, parent: NodeIndex, nodes: *std.ArrayList(NodeIndex)) VisitResult {
    _ = zast;
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
    var zast = try ZAst.parse(allocator, source);
    defer zast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (zast.ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&zast, "", source);
    }
    try std.testing.expect(zast.ast.errors.len == 0);
    try dumpAstFlat(zast.ast, stdout);

    const index = indexOfNodeWithTag(zast.ast, 0, tag).?;
    const child = ast_visit_one_child(&zast, index);
    const result = zast.ast.nodes.items(.tag)[child];
    try std.testing.expectEqual(expected, result);
}

test "child-node" {
    try testVisitChild("test { try main(); }", .@"try", .call_one);
    try testVisitChild("test { main(); }", .call_one, .identifier);
    try testVisitChild("test { a.main(); }", .call_one, .field_access);
    try testVisitChild("test { a.main(); }", .field_access, .identifier);
}

pub fn assertNodeIndexValid(ast: Ast, child: NodeIndex, parent: NodeIndex) void {
    if (child == 0 or child > ast.nodes.len) {
        const stderr = std.io.getStdErr().writer();
        dumpAstFlat(ast, stderr) catch {};
        std.log.err("zig: ast_visit child index {} from parent {} is out of range or will create a loop", .{ child, parent });
        assert(false);
    }
}

const CallbackFn = *const fn (ptr: ?*const ZAst, node: NodeIndex, parent: NodeIndex, data: ?*anyopaque) callconv(.C) VisitResult;

const ExternVisitor = struct {
    const Self = @This();
    delegate: CallbackFn,
    data: ?*anyopaque,

    pub fn callback(ast: *const ZAst, node: NodeIndex, parent: NodeIndex, self: *const ExternVisitor) VisitResult {
        return self.delegate(ast, node, parent, self.data);
    }
};

// Return whether to continue or not
export fn ast_visit(ptr: ?*ZAst, parent: NodeIndex, callback: CallbackFn, data: ?*anyopaque) void {
    if (ptr == null) {
        std.log.warn("zig: ast_visit ast is null", .{});
        return;
    }
    const zast = ptr.?;
    if (parent > zast.ast.nodes.len) {
        std.log.warn("zig: ast_visit node index {} is out of range", .{parent});
        return;
    }
    const tag = zast.ast.nodes.items(.tag)[parent];
    if (@import("builtin").is_test) {
        var stdout = std.io.getStdOut().writer();
        stdout.print("zig: ast_visit {s} at {} '{s}'\n", .{ @tagName(tag), parent, zast.ast.tokenSlice(zast.ast.nodes.items(.main_token)[parent]) }) catch {};
    }
    const visitor = ExternVisitor{
        .delegate = callback,
        .data = data,
    };
    visit(zast, parent, *const ExternVisitor, &ExternVisitor.callback, &visitor);
}

pub fn visit(
    zast: *const ZAst,
    parent: NodeIndex,
    comptime T: type,
    callback: *const fn (zast: *const ZAst, node: NodeIndex, parent: NodeIndex, data: T) VisitResult,
    data: T
) void {
    const tag: Tag = zast.ast.nodes.items(.tag)[parent];
    const d: Ast.Node.Data = zast.ast.nodes.items(.data)[parent];
    switch (tag) {
        // Leaf nodes have no children
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
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
            {
                const child = d.rhs;
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
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
            assertNodeIndexValid(zast.ast, child, parent);
            switch (callback(zast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => visit(zast, child, T, callback, data),
            }
        },

        // Only walk data rhs
        .@"defer",
        .@"errdefer",
        .@"break",
        .@"continue",
        .anyframe_type,
        .test_decl => if (d.rhs != 0) {
            const child = d.rhs;
            assertNodeIndexValid(zast.ast, child, parent);
            switch (callback(zast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => visit(zast, child, T, callback, data),
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
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
            if (d.rhs != 0) {
                const child = d.rhs;
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
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
            for (zast.ast.extra_data[d.lhs..d.rhs]) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(zast, child, T, callback, data),
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
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
            const field_range = zast.ast.extraData(d.rhs, Ast.Node.SubRange);
            for (zast.ast.extra_data[field_range.start..field_range.end]) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
        },
        // Visit lhs sub range, then rhs
        .switch_case, .switch_case_inline, .fn_proto_multi => {
            const field_range = zast.ast.extraData(d.lhs, Ast.Node.SubRange);
            for (zast.ast.extra_data[field_range.start..field_range.end]) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
            const child = d.rhs;
            assertNodeIndexValid(zast.ast, child, parent);
            switch (callback(zast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => visit(zast, child, T, callback, data),
            }
        },
        .while_cont => {
            const while_data = zast.ast.extraData(d.rhs, Ast.Node.WhileCont);
            inline for (.{ d.lhs, while_data.cont_expr, while_data.then_expr }) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
        },
        .@"while" => {
            const while_data = zast.ast.extraData(d.rhs, Ast.Node.While);
            inline for (.{ d.lhs, while_data.cont_expr, while_data.then_expr, while_data.else_expr }) |child| {
                if (child != 0) { // cont expr part may be omitted
                    assertNodeIndexValid(zast.ast, child, parent);
                    switch (callback(zast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(zast, child, T, callback, data),
                    }
                }
            }
        },
        .@"if" => {
            const if_data = zast.ast.extraData(d.rhs, Ast.Node.If);
            inline for (.{ d.lhs, if_data.then_expr, if_data.else_expr }) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
        },

        .@"for" => {
            // See std.zig.Ast.forFull
            const extra = @as(Ast.Node.For, @bitCast(d.rhs));
            for (zast.ast.extra_data[d.lhs..][0..extra.inputs]) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }

            // For body
            {
                const child = zast.ast.extra_data[d.lhs + extra.inputs];
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }

            // Else body
            if (extra.has_else) {
                const child = zast.ast.extra_data[d.lhs + extra.inputs + 1];
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
        },
        .fn_proto, .fn_proto_one => |t| {
            var buf: [1]Ast.Node.Index = undefined;
            const fn_proto = if (t == .fn_proto_one)
                zast.ast.fnProtoOne(&buf, parent).ast
            else
                zast.ast.fnProto(parent).ast;
            for (fn_proto.params) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
            inline for (.{ d.rhs, fn_proto.align_expr, fn_proto.addrspace_expr, fn_proto.section_expr, fn_proto.callconv_expr }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(zast.ast, child, parent);
                    switch (callback(zast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(zast, child, T, callback, data),
                    }
                }
            }
        },
        .global_var_decl => {
            const var_data = zast.ast.extraData(d.lhs, Ast.Node.GlobalVarDecl);
            inline for (.{ var_data.type_node, var_data.align_node, var_data.addrspace_node, var_data.section_node, d.rhs }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(zast.ast, child, parent);
                    switch (callback(zast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(zast, child, T, callback, data),
                    }
                }
            }
        },
        .local_var_decl => {
            const var_data = zast.ast.extraData(d.lhs, Ast.Node.LocalVarDecl);
            inline for (.{ var_data.type_node, var_data.align_node, d.rhs }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(zast.ast, child, parent);
                    switch (callback(zast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(zast, child, T, callback, data),
                    }
                }
            }
        },

        .array_type_sentinel => {
            const array_data = zast.ast.extraData(d.rhs, Ast.Node.ArrayTypeSentinel);
            inline for (.{ d.lhs, array_data.sentinel, array_data.elem_type }) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
        },
        .container_field => {
            const field_data = zast.ast.extraData(d.rhs, Ast.Node.ContainerField);
            inline for (.{ d.lhs, field_data.align_expr, field_data.value_expr }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(zast.ast, child, parent);
                    switch (callback(zast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(zast, child, T, callback, data),
                    }
                }
            }
        },
        .slice => {
            const slice_data = zast.ast.extraData(d.rhs, Ast.Node.Slice);
            inline for (.{ d.lhs, slice_data.start, slice_data.end }) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
        },
        .slice_sentinel => {
            const slice_data = zast.ast.extraData(d.rhs, Ast.Node.SliceSentinel);
            inline for (.{ d.lhs, slice_data.start, slice_data.end, slice_data.sentinel }) |child| {
                if (child != 0) { // slice end may be 0
                    assertNodeIndexValid(zast.ast, child, parent);
                    switch (callback(zast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(zast, child, T, callback, data),
                    }
                }
            }
        },
        .@"asm" => {
            {
                const child = d.lhs;
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => {},
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
            const asm_data = zast.ast.extraData(d.rhs, Ast.Node.Asm);
            for (zast.ast.extra_data[asm_data.items_start..asm_data.items_end]) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
        },
        .root => for (zast.ast.rootDecls()) |child| {
            assertNodeIndexValid(zast.ast, child, parent);
            switch (callback(zast, child, parent, data)) {
                .Break => return,
                .Continue => continue,
                .Recurse => visit(zast, child, T, callback, data),
            }
        },
        .ptr_type => {
            const ptr_data = zast.ast.extraData(d.lhs, Ast.Node.PtrType);
            inline for (.{ d.rhs, ptr_data.sentinel, ptr_data.align_node, ptr_data.addrspace_node }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(zast.ast, child, parent);
                    switch (callback(zast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(zast, child, T, callback, data),
                    }
                }
            }
        },
        .ptr_type_bit_range => {
            const ptr_data = zast.ast.extraData(d.lhs, Ast.Node.PtrTypeBitRange);
            inline for (.{ d.rhs, ptr_data.sentinel, ptr_data.align_node, ptr_data.addrspace_node, ptr_data.bit_range_start, ptr_data.bit_range_end }) |child| {
                if (child != 0) {
                    assertNodeIndexValid(zast.ast, child, parent);
                    switch (callback(zast, child, parent, data)) {
                        .Break => return,
                        .Continue => {},
                        .Recurse => visit(zast, child, T, callback, data),
                    }
                }
            }
        },
        .assign_destructure => {
            const elem_count = zast.ast.extra_data[d.lhs];
            // var decls (const a, const b, etc..)
            for (zast.ast.extra_data[d.lhs+1..][0..elem_count]) |child| {
                assertNodeIndexValid(zast.ast, child, parent);
                switch (callback(zast, child, parent, data)) {
                    .Break => return,
                    .Continue => continue,
                    .Recurse => visit(zast, child, T, callback, data),
                }
            }
            // The value to destructure
            const child = d.rhs;
            assertNodeIndexValid(zast.ast, child, parent);
            switch (callback(zast, child, parent, data)) {
                .Break => return,
                .Continue => {},
                .Recurse => visit(zast, child, T, callback, data),
            }
        },
    }
    return;
}

fn testVisit(source: [:0]const u8, tag: Tag) !void {
    const allocator = std.testing.allocator;
    var zast = try ZAst.parse(allocator, source);
    defer zast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    // try stdout.print("-----------\n{s}\n--------\n", .{source});
    if (zast.ast.errors.len > 0) {
        try stdout.writeAll("Parse error:\n");
        try printAstError(&zast, "", source);
        return error.ParseError;
    }
    // try dumpAstFlat(&ast, stdout);

    var nodes = try std.ArrayList(NodeIndex).initCapacity(allocator, zast.ast.nodes.len + 1);
    nodes.appendAssumeCapacity(0); // Callback does not call on initial node
    defer nodes.deinit();
    // try stdout.writeAll("Visited order\n");

    visit(&zast, 0, *std.ArrayList(NodeIndex), visitAll, &nodes);
    const visited = nodes.items[0..nodes.items.len];

    // try dumpAstNodes(&ast, visited, stdout);
    try std.testing.expectEqual(zast.ast.nodes.len, visited.len);

    // Sort visited nodes and make sure each was hit
    std.mem.sort(NodeIndex, visited, {}, std.sort.asc(NodeIndex));
    for (visited, 0..) |a, b| {
        try std.testing.expectEqual(@as(NodeIndex, @intCast(b)), a);
    }

//     var timer = try std.time.Timer.start();
//     for (0..ast.tokens.len) |i| {
//         _ = ast.fastTokenLocation(@intCast(i));
//     }
//     const t0: f64 = @floatFromInt(timer.read());
//     std.log.warn("Old: {d:6.2}ms", .{std.math.round(t0/std.time.ms_per_s)});
//
//     timer.reset();
//     for (0..ast.tokens.len) |i| {
//         _ = ast.fastTokenLocation(@intCast(i));
//     }
//     const t1: f64 = @floatFromInt(timer.read());
//     std.log.warn("New: {d:6.2}ms", .{std.math.round(t1/std.time.ms_per_s)});
//     const dt = t1-t0;
//     std.log.warn("Diff: delta={d:6.2}ms {d:6.2}% {s}", .{
//         dt/std.time.ms_per_s, 100*(t1-t0)/t0, if (t1 < t0) "faster" else "slower"});
//
//     for (0..ast.tokens.len) |i| {
//         const a = ast.fastTokenLocation(@intCast(i));
//         const b = ast.fastTokenLocationScan(0, @intCast(i));
//         try std.testing.expectEqual(a, b);
//     }

    // If given, make sure the tag is actually used in the source parsed
    if (indexOfNodeWithTag(zast.ast, 0, tag) == null) {
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
    try testVisit("const V = union(enum(u32)) {int: i32, boolean: bool};", .tagged_union_enum_tag);
    try testVisit("const V = union(enum(u32)) {int: i32, boolean: bool,};", .tagged_union_enum_tag_trailing);

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
//
// test "all-visit" {
//     // Walk entire zig lib source tree
//     const allocator = std.testing.allocator;
//     const zig_lib = "/home/jrm/projects/zig/";
//     var dir = try std.fs.cwd().openDir(zig_lib, .{.iterate=true});
//     defer dir.close();
//     var walker = try dir.walk(allocator);
//     defer walker.deinit();
//     const buffer_size = 20*1000*1024; // 20MB
//     while (try walker.next()) |entry| {
//         if (entry.kind == .file and std.mem.endsWith(u8, entry.path, ".zig")) {
//             std.log.warn("{s}", .{entry.path});
//             const file = try entry.dir.openFile(entry.basename, .{});
//             const source = try file.readToEndAllocOptions(
//                 allocator, buffer_size, null, 4, 0
//             );
//             defer allocator.free(source);
//             defer file.close();
//             testVisit(source, .root) catch |err| switch (err) {
//                 error.ParseError => {}, // Skip
//                 else => return err,
//             };
//         }
//     }
// }

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
    "@branchHint",
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
