use kain_core::{diagnostics, error, lexer, parser, types};

fn parse_and_typecheck(source: &str) -> Result<types::TypedProgram, error::KainError> {
    let tokens = lexer::Lexer::new(source).tokenize()?;
    let span_mapper = diagnostics::SpanMapper::new(source);
    let ast = parser::Parser::new(&tokens, &span_mapper, "<actor-contract-test>").parse()?;
    types::check(&ast, &span_mapper, "<actor-contract-test>")
}

#[test]
fn typecheck_builds_actor_contract_for_handlers_state_and_methods() {
    let source = r#"actor Counter:
    state count: Int = 0

    on Increment(amount: Int):
        self.count = self.count + amount

    fn current() -> Int:
        return 1
"#;

    let program = parse_and_typecheck(source).expect("actor contract should typecheck");
    let actor = match &program.items[0] {
        types::TypedItem::Actor(actor) => actor,
        other => panic!("expected actor item, found {other:?}"),
    };

    assert_eq!(actor.actor_contract.name, "Counter");
    assert_eq!(actor.actor_contract.state[0].name, "count");
    assert_eq!(actor.actor_contract.handlers[0].message.name, "Increment");
    assert_eq!(
        actor.actor_contract.handlers[0].message.parameters[0].name,
        "amount"
    );
    assert_eq!(actor.actor_contract.methods[0].name, "current");
}

#[test]
fn typecheck_rejects_duplicate_actor_handlers_through_actor_model_validator() {
    let source = r#"actor Counter:
    on Tick():
        let first = 1

    on Tick():
        let second = 2
"#;

    let error = parse_and_typecheck(source).expect_err("duplicate handlers should fail");
    assert!(error.to_string().contains("duplicate handler"));
}

#[test]
fn typecheck_infers_actor_call_reply_contract_from_generic_port_even_without_reply_to_name() {
    let source = r#"actor Worker:
    state done: Bool = true

    on Done(done_port: P, request: Int):
        send done_port.Reply(value = self.done)

fn main() -> Bool:
    let worker = spawn Worker()
    return ask(worker, "Done", 0)
"#;

    let program = parse_and_typecheck(source).expect("actor ask contract should typecheck");
    let actor = match &program.items[0] {
        types::TypedItem::Actor(actor) => actor,
        other => panic!("expected actor item, found {other:?}"),
    };

    let reply = actor.actor_contract.handlers[0]
        .message
        .reply
        .as_ref()
        .expect("Done handler should carry a reply contract");
    assert_eq!(reply.type_name, "Bool");
}
