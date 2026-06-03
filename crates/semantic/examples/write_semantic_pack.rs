use std::env;
use std::path::PathBuf;

fn main() {
    let mut cuda_forged = false;
    let mut out_dir: Option<PathBuf> = None;
    let mut oracle_root: Option<PathBuf> = None;
    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--cuda-forged" => cuda_forged = true,
            "--oracle-root" => {
                oracle_root = args.next().map(PathBuf::from);
            }
            _ if out_dir.is_none() => out_dir = Some(PathBuf::from(arg)),
            _ if oracle_root.is_none() => oracle_root = Some(PathBuf::from(arg)),
            _ => {}
        }
    }

    let crate_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let out_dir = out_dir.unwrap_or_else(|| {
        if cuda_forged {
            crate_dir
                .join(".kain")
                .join("oracle")
                .join("sempack")
                .join("cuda_forged")
                .join("current")
        } else {
            crate_dir
                .join(".kain")
                .join("oracle")
                .join("sempack")
                .join("current")
        }
    });

    if cuda_forged {
        let oracle_root = oracle_root.unwrap_or_else(|| crate_dir.join(".kain").join("oracle"));
        kain_semantic::pack::write_cuda_forged_semantic_pack_from_corpus(&out_dir, &oracle_root)
            .expect("failed to write cuda-forged semantic pack");
    } else {
        kain_semantic::pack::write_semantic_pack_from_corpus(&out_dir)
            .expect("failed to write semantic pack");
    }
    println!("{}", out_dir.display());
}
