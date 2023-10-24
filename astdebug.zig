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

pub fn dumpAstFlat(ast: *Ast, stream: anytype) !void {
    var i: usize = 0;
    const tags = ast.nodes.items(.tag);
    const node_data = ast.nodes.items(.data);
    const main_tokens = ast.nodes.items(.main_token);
    try stream.writeAll("|  #  | Tag                  |   Data    |    Main token   |\n");
    try stream.writeAll("|-----|----------------------|-----------|-----------------|\n");
    while (i < ast.nodes.len) : (i += 1) {
        const tag = tags[i];
        const data = node_data[i];
        const main_token = main_tokens[i];
        try stream.print("|{: >5}| {s: <20} | {: >4},{: >4} | {s: <15} |\n", .{
            i, @tagName(tag), data.lhs, data.rhs, ast.tokenSlice(main_token)
        });
    }
}

pub fn dumpAstRoots(ast: Ast, stream: anytype) WriteError!void {
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
    var ast = try std.zig.Ast.parse(
        allocator,
        \\ pub fn main() void {}
        \\ test {
        \\    foo.main()
        \\ }
        , .zig
    );
    defer ast.deinit(allocator);

    var stdout = std.io.getStdOut().writer();
    //try dumpAst(stdout, ast);
    //const r = try ast.render(allocator);
    //try stdout.writeAll(r);
    try dumpAstFlat(&ast, stdout);
    //try dumpAstRoots(stdout, ast);
}
