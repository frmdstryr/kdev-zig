const std = @import("std");

const Ast = std.zig.Ast;
const Index = Ast.Node.Index;
const WriteError = std.os.WriteError;

const END = 2863311530;

pub fn openNode(stream: anytype, ast: Ast, i: Index, indent: usize) WriteError!void {
    const tag = ast.nodes.items(.tag)[i];
    try stream.writeByteNTimes(' ', indent);
    try stream.print("{s}", .{@tagName(tag)});
    try stream.writeAll("{");
}

pub fn closeNode(stream: anytype, indent: usize) WriteError!void {
    try stream.writeByteNTimes(' ', indent);
    try stream.writeAll("}\n");
}


pub fn dumpNode(stream: anytype, ast: Ast, i: Index, indent: usize) WriteError!Index {
    try openNode(stream, ast, i, indent);
    const data = ast.nodes.items(.data)[i];
    if (data.rhs != END) {
        try stream.writeAll("\n");
        try dumpNode(stream, ast, data.rhs, indent + 2);
        try closeNode(stream, indent);
    } else {
        try closeNode(stream, 0);
    }
}

pub fn dumpAst(stream: anytype, ast: Ast) WriteError!void {
    try dumpNode(stream, ast, 0, 0);
}

pub fn dumpAstFlat(stream: anytype, ast: Ast) WriteError!void {
    var i: usize = 0;
    var indent: usize = 0;
    while (i < ast.nodes.len) : (i += 1) {
        const tag = ast.nodes.items(.tag)[i];
        const data = ast.nodes.items(.data)[i];
        try stream.writeByteNTimes(' ', indent);
        try stream.print("{s}", .{@tagName(tag)});
        try stream.writeAll("{\n");
        {
            indent += 2;
            try stream.writeByteNTimes(' ', indent);
            try stream.print("i={}, data=({},{})\n", .{i, data.lhs, data.rhs});
            try stream.writeByteNTimes(' ', indent);
            indent -= 2;
        }
        try stream.writeByteNTimes(' ', indent);
        try stream.writeAll("}\n");
    }
}

pub fn dumpAstRoots(stream: anytype, ast: Ast) WriteError!void {
    var indent: usize = 0;
    for (ast.rootDecls()) |i| {
        const tag = ast.nodes.items(.tag)[i];
        const data = ast.nodes.items(.data)[i];
        try stream.writeByteNTimes(' ', indent);
        try stream.print("{s}", .{@tagName(tag)});
        try stream.writeAll("{\n");
        {
            indent += 2;
            try stream.writeByteNTimes(' ', indent);
            try stream.print("i={}, data=({},{})\n", .{i, data.lhs, data.rhs});
            try stream.writeByteNTimes(' ', indent);
            indent -= 2;
        }
        try stream.writeByteNTimes(' ', indent);
        try stream.writeAll("}\n");
    }
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    var ast = try std.zig.parse(
        allocator,
        \\ pub fn foo() u8 {
        \\    return 1;
        \\ }
    );
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    //try dumpAst(stdout, ast);
    try dumpAstFlat(stdout, ast);
    //try dumpAstRoots(stdout, ast);
}
