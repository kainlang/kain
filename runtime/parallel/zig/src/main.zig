const std = @import("std");

const default_manifest_path = "../config/runtime_pairing_manifest.json";
const default_toolchain_config_path = "../config/toolchains.json";

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var args = try std.process.argsWithAllocator(allocator);
    defer args.deinit();

    _ = args.next();
    const command = args.next() orelse "summary";

    if (std.mem.eql(u8, command, "summary")) return printSummary(allocator);
    if (std.mem.eql(u8, command, "lane")) return printLane(allocator, args.next() orelse return error.MissingLane);
    if (std.mem.eql(u8, command, "json")) return printJsonSummary(allocator);
    if (std.mem.eql(u8, command, "check")) return runChecks(allocator);
    if (std.mem.eql(u8, command, "toolchains")) return printToolchains(allocator);

    std.debug.print(
        "unknown command '{s}'. expected 'summary', 'lane', 'json', 'check', or 'toolchains'\n",
        .{command},
    );
    return error.UnknownCommand;
}

fn printSummary(allocator: std.mem.Allocator) !void {
    const manifest = try loadJsonFile(allocator, default_manifest_path);
    defer manifest.deinit();

    const schema_version = manifest.value.object.get("schema_version") orelse return error.InvalidManifest;
    const components = getComponents(manifest.value);

    var lanes = std.StringHashMap(usize).init(allocator);
    defer lanes.deinit();

    for (components.items) |component| {
        const lane_name = getString(component, "lane") orelse continue;
        const entry = try lanes.getOrPut(lane_name);
        if (!entry.found_existing) entry.value_ptr.* = 0;
        entry.value_ptr.* += 1;
    }

    std.debug.print("Kain Parallel Runtime Zig Summary\n", .{});
    std.debug.print("schema_version: {}\n", .{schema_version.integer});
    std.debug.print("components: {}\n", .{components.items.len});

    var lane_iter = lanes.iterator();
    while (lane_iter.next()) |entry| {
        std.debug.print("  {s}: {}\n", .{ entry.key_ptr.*, entry.value_ptr.* });
    }
}

fn printLane(allocator: std.mem.Allocator, lane_name: []const u8) !void {
    const manifest = try loadJsonFile(allocator, default_manifest_path);
    defer manifest.deinit();

    const components = getComponents(manifest.value);
    std.debug.print("lane: {s}\n", .{lane_name});

    for (components.items) |component| {
        const lane = getString(component, "lane") orelse continue;
        if (!std.mem.eql(u8, lane, lane_name)) continue;
        const id = getString(component, "id") orelse continue;
        const status = getString(component, "status") orelse continue;
        const summary = getString(component, "summary") orelse continue;
        std.debug.print("- {s} [{s}] {s}\n", .{ id, status, summary });
    }
}

fn printJsonSummary(allocator: std.mem.Allocator) !void {
    const manifest = try loadJsonFile(allocator, default_manifest_path);
    defer manifest.deinit();
    const toolchains = try loadJsonFile(allocator, default_toolchain_config_path);
    defer toolchains.deinit();

    const components = getComponents(manifest.value);
    const shared_count = countLane(components, "shared");
    const rust_count = countLane(components, "rust");
    const zig_count = countLane(components, "zig");
    std.debug.print(
        "{{\n  \"schema_version\": 1,\n  \"component_count\": {d},\n  \"toolchains\": {{\n    \"zig\": \"{s}\",\n    \"cargo\": \"{s}\",\n    \"clang\": \"{s}\"\n  }},\n  \"lanes\": {{\n    \"shared\": {d},\n    \"rust\": {d},\n    \"zig\": {d}\n  }}\n}}\n",
        .{
            components.items.len,
            getToolStatus(toolchains.value, "zig"),
            getToolStatus(toolchains.value, "cargo"),
            getEnvStatus(toolchains.value, "clang"),
            shared_count,
            rust_count,
            zig_count,
        },
    );
}

fn runChecks(allocator: std.mem.Allocator) !void {
    const manifest = try loadJsonFile(allocator, default_manifest_path);
    defer manifest.deinit();
    const toolchains = try loadJsonFile(allocator, default_toolchain_config_path);
    defer toolchains.deinit();

    _ = getComponents(manifest.value);
    if (!fileExists(default_manifest_path)) return error.InvalidManifest;
    if (!fileExists(default_toolchain_config_path)) return error.InvalidManifest;

    std.debug.print("zig parallel check passed\n", .{});
    std.debug.print("  zig: {s}\n", .{getToolStatus(toolchains.value, "zig")});
    std.debug.print("  cargo: {s}\n", .{getToolStatus(toolchains.value, "cargo")});
    std.debug.print("  clang: {s}\n", .{getEnvStatus(toolchains.value, "clang")});
}

fn printToolchains(allocator: std.mem.Allocator) !void {
    const toolchains = try loadJsonFile(allocator, default_toolchain_config_path);
    defer toolchains.deinit();
    std.debug.print("zig: {s}\n", .{getToolStatus(toolchains.value, "zig")});
    std.debug.print("cargo: {s}\n", .{getToolStatus(toolchains.value, "cargo")});
    std.debug.print("clang: {s}\n", .{getEnvStatus(toolchains.value, "clang")});
}

fn getComponents(root: std.json.Value) std.json.Array {
    return root.object.get("pairing_components").?.array;
}

fn getString(value: std.json.Value, key: []const u8) ?[]const u8 {
    const field = value.object.get(key) orelse return null;
    return field.string;
}

fn loadJsonFile(allocator: std.mem.Allocator, path: []const u8) !std.json.Parsed(std.json.Value) {
    const bytes = try std.fs.cwd().readFileAlloc(allocator, path, 1 << 20);
    defer allocator.free(bytes);
    return try std.json.parseFromSlice(std.json.Value, allocator, bytes, .{});
}

fn getToolStatus(root: std.json.Value, tool_name: []const u8) []const u8 {
    const tool = root.object.get("tools").?.object.get(tool_name) orelse return "missing";
    const command_path = tool.object.get("command").?.string;
    if (fileExists(command_path)) return "available";
    return "missing";
}

fn getEnvStatus(root: std.json.Value, tool_name: []const u8) []const u8 {
    const tool = root.object.get("tools").?.object.get(tool_name) orelse return "missing";
    const env_name = tool.object.get("env").?.string;
    const value = std.process.getEnvVarOwned(std.heap.page_allocator, env_name) catch return "missing";
    defer std.heap.page_allocator.free(value);
    if (value.len == 0) return "missing";
    return "available";
}

fn fileExists(path: []const u8) bool {
    std.fs.cwd().access(path, .{}) catch return false;
    return true;
}

fn countLane(components: std.json.Array, lane_name: []const u8) usize {
    var count: usize = 0;
    for (components.items) |component| {
        const lane = getString(component, "lane") orelse continue;
        if (std.mem.eql(u8, lane, lane_name)) count += 1;
    }
    return count;
}
