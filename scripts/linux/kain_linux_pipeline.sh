#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  ./scripts/linux/kain_linux_pipeline.sh [smoke|selfhost|all] [options]

Modes:
  smoke      Sync a minimal Linux validation stage and run cargo/FFI smoke proofs.
  selfhost   Sync a full Linux validation stage and run selfhost phase1/phase2.
  all        Run smoke first, then selfhost.

Options:
  --stage-dir PATH        Local ext4 staging directory.
  --inventory-dir PATH    Selfhost inventory directory.
  --phase PHASE           phase1, phase2, or both. Default: both.
  --no-sync               Reuse an existing staged repo instead of rsyncing again.
  --writeback             Copy smoke outputs and selfhost reports back to the source repo.
  --full-cli-features     Use default CLI features instead of --no-default-features.
  --help                  Show this help text.

Examples:
  ./scripts/linux/kain_linux_pipeline.sh smoke
  ./scripts/linux/kain_linux_pipeline.sh selfhost --inventory-dir /abs/path/to/inventories
  ./scripts/linux/kain_linux_pipeline.sh all --phase phase1 --writeback
EOF
}

source_repo="$(git rev-parse --show-toplevel)"
repo_name="$(basename -- "${source_repo}")"
mode="smoke"
phase="both"
stage_parent="${HOME}/.codex_tmp/kain-linux-stage"
inventory_dir=""
no_sync=0
writeback=0
full_cli_features=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        smoke|selfhost|all)
            mode="$1"
            shift
            ;;
        --stage-dir)
            stage_parent="$2"
            shift 2
            ;;
        --inventory-dir)
            inventory_dir="$2"
            shift 2
            ;;
        --phase)
            phase="$2"
            shift 2
            ;;
        --no-sync)
            no_sync=1
            shift
            ;;
        --writeback)
            writeback=1
            shift
            ;;
        --full-cli-features)
            full_cli_features=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown argument: %s\n\n' "$1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

case "${phase}" in
    phase1|phase2|both)
        ;;
    *)
        printf 'Invalid --phase value: %s\n' "${phase}" >&2
        exit 1
        ;;
esac

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

require_command cargo
require_command rustc
require_command git
require_command python3
require_command node
require_command npm
require_command rsync
require_command df

source_fs_type="$(df -T "${source_repo}" | awk 'NR==2 {print $2}')"
stage_repo="${stage_parent}/${repo_name}"

default_inventory_dir="$(cd -- "${source_repo}/.." && pwd)/OuroborosV2/docs/selfhost/inventories"
if [[ -z "${inventory_dir}" && -d "${default_inventory_dir}" ]]; then
    inventory_dir="${default_inventory_dir}"
fi

mkdir -p "${stage_parent}"

sync_stage() {
    local sync_mode="$1"
    mkdir -p "${stage_repo}"
    if [[ "${sync_mode}" == "smoke" ]]; then
        (
            cd "${source_repo}"
            rsync -aR \
                Cargo.toml \
                Cargo.lock \
                .cargo \
                stdlib \
                runtime \
                crates/cli \
                crates/kain-core \
                crates/kain-driver \
                crates/kain-crate-ffi \
                crates/kain-import \
                crates/kain-host \
                crates/kain-host-derive \
                crates/kain-interop \
                crates/kain-node \
                crates/kain-python \
                crates/kain-reflect \
                crates/kain-asm \
                crates/kain-omni \
                crates/kain-selfhost \
                smoketest/cargo/local_crate_synth \
                "${stage_repo}/"
        )
    else
        rsync -a \
            --delete \
            --exclude '.git/' \
            --exclude 'target/' \
            --exclude '.kain/' \
            --exclude 'smoketest/**/.kain/' \
            --exclude 'runtime/**/target/' \
            "${source_repo}/" \
            "${stage_repo}/"
    fi
}

write_back_outputs() {
    local relative_path="$1"
    if [[ ${writeback} -ne 1 ]]; then
        return 0
    fi
    if [[ ! -e "${stage_repo}/${relative_path}" ]]; then
        return 0
    fi
    mkdir -p "${source_repo}/$(dirname -- "${relative_path}")"
    rsync -a "${stage_repo}/${relative_path}" "${source_repo}/${relative_path}"
}

run_in_stage() {
    (
        cd "${stage_repo}"
        export CARGO_TARGET_DIR="${stage_repo}/target-linux"
        "$@"
    )
}

cli_feature_args=("--no-default-features")
if [[ ${full_cli_features} -eq 1 ]]; then
    cli_feature_args=()
fi

run_smoke() {
    printf '==> Linux smoke stage\n'
    printf 'source repo: %s\n' "${source_repo}"
    printf 'source fs:   %s\n' "${source_fs_type}"
    printf 'stage repo:  %s\n' "${stage_repo}"

    if [[ ${no_sync} -ne 1 ]]; then
        sync_stage "smoke"
    fi

    run_in_stage cargo test -p kain-crate-ffi
    run_in_stage cargo test -p cli "${cli_feature_args[@]}"

    (
        cd "${stage_repo}/smoketest/cargo/local_crate_synth"
        export CARGO_TARGET_DIR="${stage_repo}/target-linux"
        cargo run --manifest-path "${stage_repo}/Cargo.toml" -q -p cli "${cli_feature_args[@]}" -- \
            import-crate cargo_smoke_lab \
            --mode both \
            --output outputs/generated \
            --report-json outputs/generated/cargo_smoke_lab_report_override.json
        cargo run --manifest-path "${stage_repo}/Cargo.toml" -q -p cli "${cli_feature_args[@]}" -- \
            smoke.kn -t test
        cargo run --manifest-path "${stage_repo}/Cargo.toml" -q -p cli "${cli_feature_args[@]}" -- \
            smoke.kn -t interpret
    )

    write_back_outputs "smoketest/cargo/local_crate_synth/outputs/"
}

run_selfhost() {
    local output_root="${stage_repo}/out/linux-selfhost"
    printf '==> Linux selfhost stage\n'
    printf 'source repo: %s\n' "${source_repo}"
    printf 'source fs:   %s\n' "${source_fs_type}"
    printf 'stage repo:  %s\n' "${stage_repo}"

    if [[ ${no_sync} -ne 1 ]]; then
        sync_stage "full"
    fi

    if [[ -z "${inventory_dir}" ]]; then
        printf 'Selfhost skipped: no inventory directory provided and default sibling path is missing.\n' >&2
        return 0
    fi
    if [[ ! -d "${inventory_dir}" ]]; then
        printf 'Selfhost skipped: inventory directory not found: %s\n' "${inventory_dir}" >&2
        return 0
    fi

    mkdir -p "${output_root}"

    if [[ "${phase}" == "phase1" || "${phase}" == "both" ]]; then
        run_in_stage cargo run -q -p cli "${cli_feature_args[@]}" -- \
            selfhost phase1 \
            --inventory-dir "${inventory_dir}" \
            --output-dir "${output_root}"
    fi

    if [[ "${phase}" == "phase2" || "${phase}" == "both" ]]; then
        run_in_stage cargo run -q -p cli "${cli_feature_args[@]}" -- \
            selfhost phase2 \
            --inventory-dir "${inventory_dir}" \
            --output-dir "${output_root}/phase2"
    fi

    write_back_outputs "out/linux-selfhost/"
}

if [[ "${source_fs_type}" == fuse.sshfs ]]; then
    printf 'Detected SSHFS source workspace; local staging is strongly recommended.\n'
fi

case "${mode}" in
    smoke)
        run_smoke
        ;;
    selfhost)
        run_selfhost
        ;;
    all)
        run_smoke
        run_selfhost
        ;;
esac

printf 'Pipeline complete.\n'
printf 'Staged repo: %s\n' "${stage_repo}"
