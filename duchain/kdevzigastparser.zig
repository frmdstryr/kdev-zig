// Copyright 2023 Jairus Martin <frmdstryr@protonmail.com>
// LGPL
const std = @import("std");
const Ast = std.zig.Ast;

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

const NodeKind = enum(c_int) {
    Unknown,
    StructDecl,
    UnionDecl,
    EnumDecl,
    TraitDecl,
    ImplDecl,
    TypeAliasDecl,
    FieldDecl,
    EnumVariantDecl,
    FunctionDecl,
    ParmDecl,
    VarDecl,
    Path,
    PathSegment,
    Block,
    Arm,
    Unexposed
};

const ZError = extern struct {
    const Self = @This();
    severity: Severity,
    range: ZAstRange,
    message: [*c]const u8,

    pub fn deinit(self: *Self, allocator: std.mem.Allocator) void {
        const msg_len = std.mem.len(self.message);
        if (msg_len > 0) {
            allocator.free(self.message[0..msg_len]);
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

export fn parse_ast(name_ptr: [*c]const u8, source_ptr: [*c]const u8) ?*Ast {
    const name_len = std.mem.len(name_ptr);
    const source_len = std.mem.len(source_ptr);

    if (name_len == 0 or source_len == 0) {
        std.log.warn("zig: name or source is empty", .{});
        return null;
    }

    const name = name_ptr[0..name_len];
    std.log.warn("zig: parsing filename '{s}'...", .{name});

    const source: [:0]const u8 = source_ptr[0..source_len:0];
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
    }

    // Parsed successuflly
    std.log.warn("zig: parsed sucessfully! {s}", .{name});
    return ast;
}

export fn ast_error_count(ptr: ?*Ast) u32 {
    if (ptr) |ast| {
        return @intCast(ast.errors.len);
    }
    return 0;
}

export fn ast_error_at(ptr: ?*Ast, index: u32) ?*ZError {
    if (ptr) |ast| {
        if (index >= ast.errors.len) {
            return null;
        }
        const allocator = gpa.allocator();
        const result = allocator.create(ZError) catch {
            std.log.warn("ast_error_at alloc failed", .{});
            return null;
        };
        // TODO: Expanding buffer?
        var buf: [4096]u8 = undefined;
        var fbs = std.io.fixedBufferStream(&buf);

        const err = ast.errors[index];
        const loc = ast.tokenLocation(0, err.token);
        ast.renderError(err, fbs.writer()) catch {
            std.log.warn("ast_error_at alloc error message too large", .{});
            allocator.destroy(result);
            return null;
        };
        const msg = allocator.dupeZ(u8, fbs.getWritten()) catch {
            std.log.warn("ast_error_at message alloc failed", .{});
            allocator.destroy(result);
            return null;
        };
        result.* = ZError{
            .severity=if (err.is_note) .Hint else .Error,
            .range=ZAstRange{
                .start=ZAstLocation{
                    .line=@intCast(loc.line_start),
                    .column=@intCast(loc.column),
                },
                .end=ZAstLocation{
                    .column=@intCast(loc.column),
                    .line=@intCast(loc.line_end),
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
//             std.log.warn("ast_node_from_ast alloc failed", .{});
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
    if (ptr) |err| {
        err.deinit(gpa.allocator());
    }
}


// export fn ast_node_index(ptr: ?*ZNode) u32 {
//     if (ptr) |node| {
//         return node.index;
//     }
//     return 0;
// }

export fn ast_node_spelling_range(node: ZNode) ZAstRange {
    if (node.ast) |ast| {
        if (node.index < ast.nodes.len) {
            // TODO: What is the correct start_offset?
            const loc = ast.tokenLocation(0, node.index);
            return ZAstRange{
                .start=ZAstLocation{
                    .line=@intCast(loc.line_start),
                    .column=@intCast(loc.column),
                },
                .end=ZAstLocation{
                    .column=@intCast(loc.column),
                    .line=@intCast(loc.line_end),
                },
            };
        }
    }
    return ZAstRange{};
}

export fn ast_node_extent(node: ZNode) ZAstRange {
    if (node.ast) |ast| {
        if (node.index < ast.nodes.len) {
            // TODO: What is the correct start_offset?
            const loc = ast.tokenLocation(0, node.index);
            return ZAstRange{
                .start=ZAstLocation{
                    .line=@intCast(loc.line_start),
                    .column=@intCast(loc.column),
                },
                .end=ZAstLocation{
                    .column=@intCast(loc.column),
                    .line=@intCast(loc.line_end),
                },
            };
        }
    }
    return ZAstRange{};
}


export fn ast_node_kind(node: ZNode) NodeKind {
    if (node.ast) |ast| {
        if (node.index >= ast.nodes.len) {
            return .Unknown;
        }
    }
    return .Unknown;
}

// Caller must free with destroy_string
export fn ast_node_new_spelling_name(node: ZNode) ?[*]const u8 {
    if (node.ast) |ast| {
        if (node.index >= ast.nodes.len) {
            return null;
        }
        const allocator = gpa.allocator();
        const tag = ast.nodes.items(.tag)[node.index];
        const copy = allocator.dupeZ(u8, @tagName(tag)) catch {
            return null;
        };
        return copy.ptr;
    }
    return null;
}

export fn destroy_string(str: [*c]const u8) void {
    const len = std.mem.len(str);
    if (len > 0) {
        const allocator = gpa.allocator();
        allocator.free(str[0..len]);
    }
}


export fn ast_format(
    source_ptr: [*c]const u8,
) ?[*]const u8 {
    const source_len = std.mem.len(source_ptr);
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


const CallbackFn = *const fn(node: ZNode, parent: ZNode, data: ?*anyopaque) VisitResult;
export fn ast_visit_children(node: ZNode, callback: CallbackFn , data: ?*anyopaque) void {
    if (node.ast == null) {
        std.log.warn("ast_visit_children node.ast is null", .{});
        return;
    }
    const ast = node.ast.?;
    if (node.index >= ast.nodes.len) {
        std.log.warn("ast_visit_children node.index is out of range", .{});
        return;
    }

    _ = callback;
    _ = data;

    var d = ast.nodes.items(.data)[node.index];
    const start = d.lhs;
    const end = d.rhs;
    _ = start;
    _ = end;
//     while (true) {
//         var parent = ZNode{.ast=ast, .index=i};
//         var current = ZNode{.ast=ast, .index=data.rhs};
//         callback(
//
//     }
}
