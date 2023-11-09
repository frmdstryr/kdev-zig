const a = @import("a.zig");
const A = a.A;

const B = struct {
    data: A,
};

var b = B{};
const c = b.data.a;
