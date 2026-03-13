use kain_host::{FromKainValue, HostSession, ToKainValue};

#[derive(Debug, Clone, PartialEq, ToKainValue, FromKainValue)]
struct Transform {
    x: f32,
    y: f32,
    z: f32,
}

#[derive(Debug, Clone, PartialEq, ToKainValue, FromKainValue)]
enum EngineCommand {
    Idle,
    Move(i64, Transform),
    Rename { id: i64, name: String },
}

#[test]
fn derived_struct_roundtrips_through_value_bridge() {
    let original = Transform {
        x: 1.5,
        y: -2.0,
        z: 8.25,
    };

    let value = original.clone().to_kain_value();
    let decoded = Transform::from_kain_value(value).expect("decode transform");

    assert_eq!(decoded, original);
}

#[test]
fn derived_enum_roundtrips_through_value_bridge() {
    let original = EngineCommand::Rename {
        id: 7,
        name: "camera".to_string(),
    };

    let value = original.clone().to_kain_value();
    let decoded = EngineCommand::from_kain_value(value).expect("decode command");

    assert_eq!(decoded, original);
}

#[test]
fn derived_struct_roundtrips_through_live_kain() {
    let mut host = HostSession::new();
    host.load_source(
        r#"
struct Transform:
    x: Float
    y: Float
    z: Float

fn echo_transform(value: Transform) -> Transform:
    return value
"#,
    )
    .expect("load kain source");

    let original = Transform {
        x: 32.0,
        y: 4.5,
        z: -9.0,
    };

    let decoded = host
        .call::<Transform>("echo_transform", vec![original.clone().to_kain_value()])
        .expect("call echo_transform");

    assert_eq!(decoded, original);
}

#[test]
fn derived_enum_roundtrips_through_live_kain() {
    let mut host = HostSession::new();
    host.load_source(
        r#"
struct Transform:
    x: Float
    y: Float
    z: Float

enum EngineCommand:
    Idle
    Move(Int, Transform)
    Rename(Int, String)

fn echo_command(value: EngineCommand) -> EngineCommand:
    return value
"#,
    )
    .expect("load kain source");

    let original = EngineCommand::Move(
        42,
        Transform {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        },
    );

    let decoded = host
        .call::<EngineCommand>("echo_command", vec![original.clone().to_kain_value()])
        .expect("call echo_command");

    assert_eq!(decoded, original);
}
