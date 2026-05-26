use clap::Parser;
use std::path::PathBuf;

#[derive(Debug, Parser)]
#[command(name = "kain-stdlib-map-tool")]
#[command(about = "Generate or check Kain's stdlib symbol atlas")]
struct Args {
    #[arg(long)]
    repo_root: Option<PathBuf>,

    #[arg(long)]
    stdlib_root: Option<PathBuf>,

    #[arg(long = "native-manifest")]
    native_manifests: Vec<PathBuf>,

    #[arg(long)]
    json_out: Option<PathBuf>,

    #[arg(long)]
    llm_out: Option<PathBuf>,

    #[arg(long)]
    write: bool,

    #[arg(long)]
    check: bool,

    #[arg(long)]
    json: bool,
}

fn main() {
    if let Err(err) = run_on_large_stack() {
        eprintln!("stdlib-map failed: {err}");
        std::process::exit(1);
    }
}

fn run_on_large_stack() -> kain_stdlib_map::Result<()> {
    const STDLIB_MAP_STACK_BYTES: usize = 32 * 1024 * 1024;

    let worker = std::thread::Builder::new()
        .name("kain-stdlib-map".to_string())
        .stack_size(STDLIB_MAP_STACK_BYTES)
        .spawn(|| run().map_err(|err| err.to_string()))
        .map_err(|err| format!("failed to spawn stdlib-map worker thread: {err}"))?;

    match worker.join() {
        Ok(result) => result.map_err(Into::into),
        Err(_) => Err("stdlib-map worker thread panicked".into()),
    }
}

fn run() -> kain_stdlib_map::Result<()> {
    let args = Args::parse();
    let repo_root = match args.repo_root {
        Some(path) => path,
        None => kain_stdlib_map::discover_repo_root(std::env::current_dir()?)?,
    };
    let options = kain_stdlib_map::StdlibMapOptions::from_repo_root(repo_root)
        .with_stdlib_root(args.stdlib_root)
        .with_native_manifests(args.native_manifests)
        .with_json_out(args.json_out)
        .with_llm_out(args.llm_out);

    if args.check {
        kain_stdlib_map::check_generated_files(&options)?;
        println!(
            "checked {} and {}",
            options.json_out.display(),
            options.llm_out.display()
        );
        return Ok(());
    }

    if args.write {
        let report = kain_stdlib_map::write_generated_files(&options)?;
        println!(
            "wrote {} symbols across {} modules",
            report.map.summary.symbol_count, report.map.summary.module_count
        );
        println!("json: {}", report.json_path.display());
        println!("llm: {}", report.llm_path.display());
        return Ok(());
    }

    let map = kain_stdlib_map::generate_stdlib_map(&options)?;
    if args.json {
        println!("{}", serde_json::to_string(&map)?);
    } else {
        print!("{}", kain_stdlib_map::render_llm_markdown(&map));
    }
    Ok(())
}
