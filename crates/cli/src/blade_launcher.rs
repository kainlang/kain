use clap::Parser;
use kain_commands::blade::{BladeCli, BladeCommand, BladesCommand};

pub fn main_entry() {
    let cli = BladeCli::parse();
    if let Err(error) = run(cli.command) {
        eprintln!("Blade command failed: {error}");
        std::process::exit(1);
    }
}

fn run(command: BladeCommand) -> Result<(), String> {
    match command {
        BladeCommand::Build {
            path,
            profile,
            target,
            dry_run,
            clean,
            include_vulkan,
            json,
        } => cli::blades::run_build(path, profile, target, dry_run, clean, include_vulkan, json),
        BladeCommand::Run {
            blade,
            path,
            target,
            json,
            trace,
            keep_artifacts,
            dry_run,
            args,
        } => cli::blades::run(BladesCommand::Run {
            blade,
            path,
            target,
            json,
            trace,
            keep_artifacts,
            dry_run,
            args,
        }),
        BladeCommand::List { path, json } => cli::blades::run(BladesCommand::List { path, json }),
        BladeCommand::Graph { path, json } => cli::blades::run(BladesCommand::Graph { path, json }),
        BladeCommand::Check { path, json } => cli::blades::run(BladesCommand::Check { path, json }),
        BladeCommand::Equip { blade, path, json } => cli::blades::run_equip(blade, path, json),
        BladeCommand::External(argv) => Err(format!(
            "unknown blade command or unsupported runtime handler: {}",
            argv.join(" ")
        )),
    }
}
