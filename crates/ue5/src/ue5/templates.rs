use minijinja::{Environment, context};
use once_cell::sync::Lazy;

pub static TEMPLATES: Lazy<Ue5Templates> = Lazy::new(|| Ue5Templates::new());

pub struct Ue5Templates {
    env: Environment<'static>,
}

impl Ue5Templates {
    pub fn new() -> Self {
        let mut env = Environment::new();
        
        env.add_template("header_preamble", include_str!("templates/header_preamble.jinja")).unwrap();
        env.add_template("source_preamble", include_str!("templates/source_preamble.jinja")).unwrap();
        env.add_template("uclass_header", include_str!("templates/uclass_header.jinja")).unwrap();
        
        Self { env }
    }

    pub fn render(&self, name: &str, ctx: serde_json::Value) -> Result<String, String> {
        let tmpl = self.env.get_template(name).map_err(|e| e.to_string())?;
        tmpl.render(ctx).map_err(|e| e.to_string())
    }
}
