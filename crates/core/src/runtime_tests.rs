use crate::comptime;
use crate::diagnostics::SpanMapper;
use crate::lexer::Lexer;
use crate::parser::Parser;
use crate::runtime::{interpret, Value};
use crate::stdlib::StdLib;
use crate::types;
use std::path::{Path, PathBuf};
use std::sync::{Mutex, MutexGuard};

static CURRENT_DIR_TEST_LOCK: Mutex<()> = Mutex::new(());

struct CurrentDirGuard {
    previous_dir: PathBuf,
    _lock: MutexGuard<'static, ()>,
}

impl CurrentDirGuard {
    fn enter(next_dir: &Path) -> Self {
        let lock = CURRENT_DIR_TEST_LOCK.lock().expect("current dir test lock");
        let previous_dir = std::env::current_dir().expect("current dir");
        std::env::set_current_dir(next_dir).expect("set current dir");
        Self {
            previous_dir,
            _lock: lock,
        }
    }
}

impl Drop for CurrentDirGuard {
    fn drop(&mut self) {
        std::env::set_current_dir(&self.previous_dir).expect("restore current dir");
    }
}

fn interpret_test_source(source: &str) -> Value {
    let tokens = Lexer::new(source).tokenize().expect("tokenize test source");
    let span_mapper = SpanMapper::new(source);
    let mut ast = Parser::new(&tokens, &span_mapper, "<runtime-test>")
        .parse()
        .expect("parse test source");
    comptime::eval_program(&mut ast).expect("run comptime");
    let typed_program = types::check(&ast, &span_mapper, "<runtime-test>").expect("typecheck");
    interpret(&typed_program).expect("interpret")
}

#[test]
fn elif_chain_executes_matching_arm() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let opcode = "+"
    if opcode == ">":
        return 1
    elif opcode == "+":
        return 2
    else:
        return 3
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 2),
        other => panic!("expected Int(2), got {:?}", other),
    }
}

#[test]
fn string_len_method_matches_typechecker() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let text = "brainfuck"
    return text.len()
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 9),
        other => panic!("expected Int(9), got {:?}", other),
    }
}

#[test]
fn char_at_out_of_range_returns_empty_string() {
    let value = interpret_test_source(
        r#"
fn main() -> String:
    return char_at("abc", 99)
"#,
    );

    match value {
        Value::String(result) => assert_eq!(result, ""),
        other => panic!("expected empty string, got {:?}", other),
    }
}

#[test]
fn string_byte_and_find_helpers_follow_byte_offsets() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let text = "{\"id\":17,\"name\":\"orbital\"}"
    let marker = find_substring_from(text, "\"name\":\"", 0)
    let label = substring(text, marker + 8, marker + 11)
    if marker == 9 and byte_at(text, marker + 8) == 111 and byte_at(text, 999) == -1 and label == "orb":
        return 1
    return 0
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 1),
        other => panic!("expected Int(1), got {:?}", other),
    }
}

#[test]
fn stdlib_registry_exposes_ord_and_chr() {
    let stdlib = StdLib::new();

    assert!(stdlib.functions.contains_key("ord"));
    assert!(stdlib.functions.contains_key("chr"));
    assert!(stdlib.functions.contains_key("byte_at"));
    assert!(stdlib.functions.contains_key("find_substring_from"));
    assert!(stdlib.functions.contains_key("ask"));
    assert!(stdlib.functions.contains_key("ask_timeout"));
    assert!(stdlib.functions.contains_key("command_run"));
    assert!(stdlib.functions.contains_key("json_parse"));
    assert!(stdlib.functions.contains_key("json_object_new"));
    assert!(stdlib.functions.contains_key("read_line"));
    assert!(stdlib.functions.contains_key("stdout_write"));
    assert!(stdlib.functions.contains_key("stderr_write"));
    assert!(stdlib.functions.contains_key("stdin_read_exact"));
    assert!(stdlib.functions.contains_key("to_int"));
    assert!(stdlib.functions.contains_key("fs_read_text"));
    assert!(stdlib.functions.contains_key("fs_try_read_text"));
    assert!(stdlib.functions.contains_key("fs_metadata"));
    assert!(stdlib.functions.contains_key("fs_path_join"));
}

#[test]
fn gen_server_stdlib_round_trip() {
    let stdlib = include_str!("../../../stdlib/gen_server.kn");
    let value = interpret_test_source(&format!(
        r#"
{stdlib}

fn main() -> Int:
    let server = gen_server_start_link(
        0,
        |request, state| gen_server_call_result(state + request, state + request),
        |request, state| state + request,
        |message, state| state + message
    )
    let first = gen_server_call(server, 5)
    gen_server_cast(server, 10)
    gen_server_info(server, 2)
    let second = gen_server_call(server, 3)
    return first + second
"#
    ));

    match value {
        Value::Int(result) => assert_eq!(result, 25),
        other => panic!("expected Int(25), got {:?}", other),
    }
}

#[test]
fn command_run_builtin_captures_stdout_and_status() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let result = command_run("bash", ["-lc", "printf hello"], "")
    if result.success && result.status == 0 && result.stdout == "hello":
        return 1
    return 0
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 1),
        other => panic!("expected Int(1), got {:?}", other),
    }
}

#[test]
fn filesystem_builtins_round_trip_text_metadata_walk_and_hash() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let dir = fs_temp_dir("kain-runtime-fs")
    let path = fs_path_join(dir, "note.txt")
    fs_write_text(path, "hello")
    fs_append_text(path, " fs")

    let text = fs_read_text(path)
    let bytes = fs_read_bytes(path)
    let metadata = fs_metadata(path)
    let entries = fs_read_dir(dir)
    let digest = fs_hash_file(path)

    fs_remove_dir_all(dir)

    if text == "hello fs" && len(bytes) == 8 && metadata.file_type == "file" && metadata.len == 8 && len(entries) == 1 && len(digest) == 64:
        return 1
    return 0
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 1),
        other => panic!("expected Int(1), got {:?}", other),
    }
}

#[test]
fn filesystem_v2_virtual_stream_watch_and_transaction_flow() {
    let temp = tempfile::tempdir().expect("temp dir");
    let root = temp.path().to_string_lossy().replace('\\', "/");
    let source = format!(
        r#"
fn main() -> Int:
    fs_mount("v2test", "{root}", "read_write")
    let path = "fs://v2test/main.txt"
    fs_write_text_at(path, 0, "abcdef")
    let slice = fs_read_text_range(path, 1, 3)
    fs_write_text_at(path, 3, "XYZ")
    let rewritten = fs_read_text_range(path, 0, 6)
    let chunks = fs_stream_chunks(path, 2)
    let copied = fs_copy_file_streaming(path, "fs://v2test/copy.txt", 2)

    let watcher = fs_watch("fs://v2test", false)
    fs_write_text_at("fs://v2test/watched.txt", 0, "watch")
    let events = fs_watch_poll(watcher)
    let closed = fs_watch_close(watcher)

    let tx = fs_tx_begin()
    fs_tx_write_text(tx, "fs://v2test/tx.txt", "one")
    fs_tx_append_text(tx, "fs://v2test/tx.txt", " two")
    let journal = fs_tx_commit(tx)
    let tx_text = fs_read_text_range("fs://v2test/tx.txt", 0, 7)

    if slice == "bcd" && rewritten == "abcXYZ" && len(chunks) == 3 && copied == 6 && len(events) == 1 && closed && len(journal) == 2 && tx_text == "one two" && fs_capability_has("fs.read"):
        return 1
    return 0
"#
    );
    let value = interpret_test_source(&source);

    match value {
        Value::Int(result) => assert_eq!(result, 1),
        other => panic!("expected Int(1), got {:?}", other),
    }
}

#[test]
fn json_helpers_extract_nested_fields() {
    let value = interpret_test_source(
        r#"
fn main() -> Int:
    let payload = json_parse("{\"method\":\"tools/call\",\"params\":{\"count\":7,\"enabled\":true}}")
    let params = json_get(payload, "params")
    if json_get_string(payload, "method") == "tools/call" && json_get_int(params, "count") == 7 && json_get_bool(params, "enabled"):
        return 1
    return 0
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 1),
        other => panic!("expected Int(1), got {:?}", other),
    }
}

#[test]
fn ask_timeout_builtin_round_trips_actor_reply() {
    let value = interpret_test_source(
        r#"
actor Echo:
    on Call(reply_to: P, request: Int):
        send reply_to.Reply(value = request + 1)

fn main() -> Int:
    let echo = spawn Echo()
    return ask_timeout(echo, "Call", 9, 1000)
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 10),
        other => panic!("expected Int(10), got {:?}", other),
    }
}

#[test]
fn filesystem_named_use_loads_item_from_sibling_module_file() {
    let temp = tempfile::tempdir().expect("temp dir");
    std::fs::write(
        temp.path().join("host_reflection.kn"),
        r#"
fn build_control_plane_catalog() -> Int:
    return 42
"#,
    )
    .expect("write helper module");

    let _cwd = CurrentDirGuard::enter(temp.path());
    let value = interpret_test_source(
        r#"
use host_reflection::build_control_plane_catalog

fn main() -> Int:
    return build_control_plane_catalog()
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 42),
        other => panic!("expected Int(42), got {:?}", other),
    }
}

#[test]
fn filesystem_glob_use_registers_sibling_module_symbols_for_typechecking() {
    let temp = tempfile::tempdir().expect("temp dir");
    std::fs::write(
        temp.path().join("plugin_authoring.kn"),
        r#"
fn emit_plugin_manifest() -> Int:
    return 7

fn emit_debug_value() -> Int:
    return 3
"#,
    )
    .expect("write helper module");

    let _cwd = CurrentDirGuard::enter(temp.path());
    let value = interpret_test_source(
        r#"
use plugin_authoring::*

fn main() -> Int:
    return emit_plugin_manifest() + emit_debug_value()
"#,
    );

    match value {
        Value::Int(result) => assert_eq!(result, 10),
        other => panic!("expected Int(10), got {:?}", other),
    }
}
