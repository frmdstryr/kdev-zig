const a = @import("config_a.zig");
const b = @import("config_b.zig");

// Make sure structs in different modules do not clash
const config_a = a.Config{};
const x = config_a.enabled;
const config_b = b.Config{};
const y = config_b.data;
