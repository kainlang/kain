#!/bin/bash
# setup.sh -- one-command Kain Linux prerequisites + PATH setup.
# Run from the extracted kain-*-linux-*/ directory:
#   ./setup.sh            (prompts before sudo + PATH changes)
#   ./setup.sh --yes      (no prompts; for CI / containers)
#
# Installs (via the native package manager): a C toolchain capable of
# linking Kain native programs (clang + lld + libc dev files) and the
# ALSA dev library (needed to build programs that use audio).
set -u
YES=0
for a in "$@"; do
  case "$a" in
    -y|--yes) YES=1 ;;
    -h|--help) sed -n '1,12p' "$0"; exit 0 ;;
    *) echo "unknown arg: $a (try --help)"; exit 2 ;;
  esac
done

ROOT="$(cd "$(dirname "$0")" && pwd)"
KAIN_BIN="$ROOT/bin"

say()  { printf '[setup] %s\n' "$*"; }
have() { command -v "$1" >/dev/null 2>&1; }

# -- pick a clang: newest versioned binary wins, plain clang as fallback --
pick_clang() {
  for c in clang-21 clang-20 clang-19 clang-18 clang-17 clang-16 clang-15 clang-14 clang; do
    if have "$c"; then echo "$c"; return 0; fi
  done
  return 1
}

if have clang; then
  say "clang present: $(clang --version 2>/dev/null | head -n 1)"
elif CLANG_BIN="$(pick_clang)"; then
  say "clang present via $CLANG_BIN"
else
  say "no clang found."
  NEED_TOOLCHAIN=1
fi

# -- distro detect --
OS_ID="unknown"
if [ -f /etc/os-release ]; then
  # shellcheck disable=SC1091
  . /etc/os-release
  OS_ID="${ID:-unknown} ${ID_LIKE:-}"
fi

install_pkgs() {
  # $1 = manager string for logging; rest = packages
  mgr="$1"; shift
  say "installing via $mgr: $*"
  case "$mgr" in
    apt) sudo apt-get update -y && sudo apt-get install -y "$@" ;;
    dnf) sudo dnf install -y "$@" ;;
    pacman) sudo pacman -Sy --noconfirm "$@" ;;
    apk) sudo apk add "$@" ;;
    zypper) sudo zypper install -y "$@" ;;
  esac
}

confirm() {
  # confirm "prompt" -> 0 if approved (or --yes)
  if [ "$YES" = 1 ]; then return 0; fi
  printf '[setup] %s [y/N] ' "$1"
  read -r ans
  [ "$ans" = "y" ] || [ "$ans" = "Y" ]
}

NEED_INSTALL=0
[ "${NEED_TOOLCHAIN:-0}" = 1 ] && NEED_INSTALL=1
# libc dev files (crt1.o / libc.so — the linker needs these even with our
# prebuilt runtime) and ALSA (audio programs) may be missing independently.
for t in /usr/lib/x86_64-linux-gnu/crt1.o /usr/lib/crt1.o /lib/crt1.o \
         /usr/lib/x86_64-linux-gnu/libc.so /usr/lib/libc.so; do
  [ -f "$t" ] && HAVE_CRT=1 && break
done
[ "${HAVE_CRT:-0}" = 0 ] && NEED_INSTALL=1
[ -f /usr/include/alsa/asoundlib.h ] || NEED_ALSA=1
[ "${NEED_ALSA:-0}" = 1 ] && NEED_INSTALL=1

if [ "$NEED_INSTALL" = 1 ]; then
  case "$OS_ID" in
    *debian*|*ubuntu*)   SPEC="apt:clang lld llvm binutils libc6-dev libasound2-dev" ;;
    *fedora*|*rhel*|*centos*) SPEC="dnf:clang lld llvm binutils glibc-devel alsa-lib-devel" ;;
    *arch*)              SPEC="pacman:clang lld llvm binutils glibc alsa-lib" ;;
    *alpine*)            SPEC="apk:clang lld llvm binutils musl-dev libc-dev alsa-lib-dev" ;;
    *suse*|*opensuse*)   SPEC="zypper:clang lld llvm binutils glibc-devel alsa-devel" ;;
    *) say "unsupported distro ($OS_ID). Install by hand: clang, lld/LLVM, your libc dev package, and ALSA dev headers, then rerun."; exit 1 ;;
  esac
  MGR="${SPEC%%:*}"; PKGS="${SPEC#*:}"
  if confirm "install [$PKGS] with $MGR (sudo)?"; then
    # shellcheck disable=SC2086
    install_pkgs "$MGR" $PKGS || { say "package install failed."; exit 1; }
  else
    say "skipped. Install [$PKGS] yourself, then rerun."
    exit 1
  fi
else
  say "toolchain + libs look complete, skipping package install."
fi

# -- sanity: clang must actually run --
if have clang; then
  say "clang OK: $(clang --version 2>/dev/null | head -n 1)"
elif CLANG_BIN="$(pick_clang)"; then
  say "clang OK via $CLANG_BIN ($( "$CLANG_BIN" --version 2>/dev/null | head -n 1))."
  say "hint: sudo update-alternatives --install /usr/bin/clang clang \"\$(command -v \"$CLANG_BIN\")\" 100"
else
  say "clang still missing — install it by hand and rerun."; exit 1
fi

# -- PATH --
case ":$PATH:" in
  *":$KAIN_BIN:"*) say "PATH already contains bin/." ;;
  *)
    if confirm "append $KAIN_BIN to PATH in ~/.bashrc and ~/.zshrc?"; then
      BLOCK="$(printf '\n# >>> KAIN (setup.sh) >>>\nexport PATH=\"%s:$PATH\"\n# <<< KAIN <<<\n' "$KAIN_BIN")"
      for rc in "$HOME/.bashrc" "$HOME/.zshrc"; do
        [ -f "$rc" ] || continue
        grep -qF "$KAIN_BIN" "$rc" 2>/dev/null && continue
        printf '%s\n' "$BLOCK" >> "$rc" && say "PATH block written to $rc"
      done
      say "restart your shell (or: export PATH=\"$KAIN_BIN:\$PATH\")"
    else
      say "manual: export PATH=\"$KAIN_BIN:\$PATH\""
    fi
    ;;
esac

# -- verify --
if [ -x "$KAIN_BIN/kain" ]; then
  say "verifying: kain --version"
  "$KAIN_BIN/kain" --version || { say "kain binary failed to run."; exit 1; }
  say "SETUP COMPLETE. Try: kain doctor"
else
  say "kain binary not found at $KAIN_BIN/kain (run setup.sh from the extracted dir)."
  exit 1
fi
