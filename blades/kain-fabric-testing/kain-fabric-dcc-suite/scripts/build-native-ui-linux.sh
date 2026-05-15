#!/usr/bin/env bash
set -euo pipefail

APP_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${APP_ROOT}/../.." && pwd)"

"${APP_ROOT}/scripts/build-native-library.sh"
python3 "${APP_ROOT}/scripts/materialize_session_state.py"
python3 "${APP_ROOT}/scripts/materialize_shell.py"

cd "${REPO_ROOT}"
cargo run -p cli --bin kain -- build native-ui \
    "${APP_ROOT}/generated/main.generated.kn" \
    --bundle-only \
    --app-name kain-fabric-dcc-suite \
    --window-title "Kain Fabric DCC Suite" \
    -o "${APP_ROOT}/native-app"

python3 "${APP_ROOT}/scripts/patch_native_app_bridge.py"

cargo build --manifest-path "${APP_ROOT}/native-app/Cargo.toml"

echo "Built ${APP_ROOT}/native-app/target/debug/kain-fabric-dcc-suite"
