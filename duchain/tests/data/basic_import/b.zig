const a = @import("a.zig");
// Different name to avoid duplicate here...
const ImportedA = a.A;

const B = struct {
    data: ImportedA,
};

var b = B{};
const c = b.data.a;
