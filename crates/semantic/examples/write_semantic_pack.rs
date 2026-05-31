use std::env;
use std::path::PathBuf;

fn main() {
    let out_dir = env::args().nth(1).map(PathBuf::from).unwrap_or_else(|| {
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .join(".kain")
            .join("oracle")
            .join("sempack")
            .join("current")
    });
    kain_semantic::pack::write_semantic_pack_from_corpus(&out_dir)
        .expect("failed to write semantic pack");
    println!("{}", out_dir.display());
}
