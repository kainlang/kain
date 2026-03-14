use kain_host::{FromKainValue, HostSession, KainReflect, ToKainValue, TypeRegistry};

#[derive(Debug, Clone, PartialEq, ToKainValue, FromKainValue, KainReflect)]
#[kain(rename = "Vec3", version = "1")]
struct Transform {
    #[kain(rename = "px")]
    x: f32,
    #[kain(rename = "py")]
    y: f32,
    #[kain(rename = "pz")]
    z: f32,
}

#[derive(Debug, Clone, PartialEq, ToKainValue, FromKainValue, KainReflect)]
enum EngineCommand {
    Idle,
    Move(i64, Transform),
    #[kain(rename = "Label")]
    Rename {
        id: i64,
        name: String,
    },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, ToKainValue, FromKainValue, KainReflect)]
#[kain(rename = "EntityId", transparent, version = "1")]
struct EntityId(u64);

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
struct Vec3:
    px: Float
    py: Float
    pz: Float

fn echo_transform(value: Vec3) -> Vec3:
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
struct Vec3:
    px: Float
    py: Float
    pz: Float

enum EngineCommand:
    Idle
    Move(Int, Vec3)
    Label(Int, String)

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

#[test]
fn transparent_wrapper_roundtrips_as_inner_value() {
    let id = EntityId(99);
    let decoded = EntityId::from_kain_value(id.to_kain_value()).expect("decode entity id");
    assert_eq!(decoded, id);
}

#[test]
fn registry_captures_schema_metadata() {
    let mut registry = TypeRegistry::new();
    registry.register::<Transform>();
    registry.register::<EngineCommand>();
    registry.register::<EntityId>();

    let transform = registry.get("Vec3").expect("vec3 schema");
    assert_eq!(transform.rust_name, "Transform");
    assert_eq!(
        transform.attrs.get("version").map(String::as_str),
        Some("1")
    );

    let entity = registry.get("EntityId").expect("entity schema");
    assert_eq!(entity.attrs.get("version").map(String::as_str), Some("1"));

    assert_eq!(registry.len(), 3);
}

#[test]
fn reflected_types_are_visible_without_manual_kain_type_definitions() {
    let mut host = HostSession::new();
    host.register_type::<Transform>();
    host.register_type::<EngineCommand>();
    host.register_type::<EntityId>();

    host.load_source(
        r#"
fn echo_transform(value: Vec3) -> Vec3:
    return value

fn echo_command(value: EngineCommand) -> EngineCommand:
    return value

fn increment_id(value: EntityId) -> EntityId:
    return value + 1
"#,
    )
    .expect("load reflected kain source");

    let transform = Transform {
        x: 9.0,
        y: 8.0,
        z: 7.0,
    };
    let command = EngineCommand::Move(5, transform.clone());

    let echoed_transform = host
        .call::<Transform>("echo_transform", vec![transform.clone().to_kain_value()])
        .expect("echo transform");
    let echoed_command = host
        .call::<EngineCommand>("echo_command", vec![command.clone().to_kain_value()])
        .expect("echo command");
    let incremented_id = host
        .call::<EntityId>("increment_id", vec![EntityId(41).to_kain_value()])
        .expect("increment id");

    assert_eq!(echoed_transform, transform);
    assert_eq!(echoed_command, command);
    assert_eq!(incremented_id, EntityId(42));
}

#[test]
fn reflected_engine_module_supports_plain_scripts() {
    let mut host = HostSession::new();
    host.register_type::<Transform>();
    host.register_type::<EntityId>();
    host.register_native_fn(
        "host_double",
        vec![kain_host::NativeParam::new(
            "value",
            kain_host::HostType::Int,
        )],
        kain_host::HostType::Int,
        |_env, args| match args.as_slice() {
            [kain_host::Value::Int(value)] => Ok(kain_host::Value::Int(value * 2)),
            _ => Err(kain_host::KainError::runtime("expected Int")),
        },
    );

    host.load_source(
        r#"
fn promote(value: EntityId, position: Vec3) -> Int:
    let doubled = host_double(value)
    let _x = position.px
    return doubled
"#,
    )
    .expect("load engine module source");

    let result = host
        .call::<i64>(
            "promote",
            vec![
                EntityId(20).to_kain_value(),
                Transform {
                    x: 2.0,
                    y: 3.0,
                    z: 4.0,
                }
                .to_kain_value(),
            ],
        )
        .expect("call promote");

    assert_eq!(result, 40);
}
