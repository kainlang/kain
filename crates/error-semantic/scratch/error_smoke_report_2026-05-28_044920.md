# Kain Error System Smoke Test Report

**Generated:** 2026-05-28_044920  
**Kain binary:** X:\.kain\bin\kain.exe
**Target:** llvm
**Files tested:** 21 (7 passed, 14 failed)

---

## Summary

| Status | Count |
|--------|-------|
| Passed | 7 |
| Failed | 14 |
| Total  | 21 |

---

## Detailed Results

### borrow_mismatch.kn -- PASS

**Header:** // @expected_code: KAIN-BORROW-0004

```
[kain] Cargo.Bazel.lock manifest drift detected (unreal_asset_base 0.1.0: missing enum_dispatch; unreal_asset_exports 0.1.0: missing enum_dispatch; unreal_asset_kismet 0.1.0: missing enum_dispatch; unreal_asset_properties 0.1.0: missing enum_dispatch; ue5-materials 0.1.0: missing indoc; ... (+1 more)); repinning crate_universe and retrying once...
2026/05/28 04:49:22 Downloading https://releases.bazel.build/9.1.0/release/bazel-9.1.0-windows-x86_64.exe...
2026/05/28 04:49:25 Signed by Bazel Developer (Bazel APT repository key) <bazel-dev@googlegroups.com>
[32mINFO: [0mInvocation ID: 3a7d7384-ec36-49b9-9656-880517978681
[32mINFO: [0mReading 'startup' options from x:\.bazelrc: --output_user_root=F:/_b/output-user-root
[32mINFO: [0mOptions provided by the client:
  Inherited 'common' options: --isatty=0 --terminal_columns=80
[32mINFO: [0mReading rc options for 'fetch' from x:\.bazelrc:
  Inherited 'common' options: --enable_bzlmod --announce_rc --color=yes --curses=yes --experimental_convenience_symlinks=ignore --repository_cache=F:/_b/repository-cache --repo_env=TMP=F:/_b/tmp --repo_env=TEMP=F:/_b/tmp --repo_env=TMPDIR=F:/_b/tmp --action_env=TMP=F:/_b/tmp --action_env=TEMP=F:/_b/tmp --action_env=TMPDIR=F:/_b/tmp --action_env=PATH --action_env=PATHEXT --action_env=SystemDrive --action_env=SystemRoot --action_env=ComSpec --action_env=windir --action_env=USERPROFILE --action_env=HOMEDRIVE --action_env=HOMEPATH --action_env=APPDATA --action_env=LOCALAPPDATA --action_env=ProgramData --action_env=ProgramFiles --action_env=ProgramFiles(x86) --action_env=CommonProgramFiles --action_env=CommonProgramFiles(x86) --action_env=NUMBER_OF_PROCESSORS --action_env=PROCESSOR_ARCHITECTURE --action_env=PROCESSOR_IDENTIFIER
[32mINFO: [0mReading rc options for 'fetch' from x:\.bazelrc:
  Inherited 'build' options: --disk_cache=F:/_b/disk-cache --jobs=HOST_CPUS*.625 --loading_phase_threads=HOST_CPUS*.5 --local_resources=cpu=HOST_CPUS*.625 --local_resources=memory=HOST_RAM*.75 --keep_going
[32mINFO: [0mReading rc options for 'fetch' from x:\.bazelrc:
  Inherited 'test' options: --test_output=errors --test_summary=short --local_test_jobs=HOST_CPUS*.25 --test_env=TMP=F:/_b/tmp --test_env=TEMP=F:/_b/tmp --test_env=TMPDIR=F:/_b/tmp --test_env=PATH --test_env=PATHEXT
[32mINFO: [0mFound applicable config definition build:dev in file x:\.bazelrc: --compilation_mode=dbg
[32mComputing main repo mapping:[0m 

[1A[K[32mComputing main repo mapping:[0m 
    Fetching repository @@rules_rust+; starting

[1A[K
[1A[K[32mComputing main repo mapping:[0m 

[1A[K[32mComputing main repo mapping:[0m 
    Fetching https://bcr.bazel.build/modules/platforms/1.1.0/MODULE.bazel

[1A[K
[1A[K[32mComputing main repo mapping:[0m 
    Fetching https://bcr.bazel.build/modules/package_metadata/metadata.json

[1A[K
[1A[K[35mWARNING: [0mFor repository 'platforms', the root module requires module version platforms@1.0.0, but got platforms@1.1.0 in the resolved dependency graph. Please update the version in your MODULE.bazel or set --check_direct_dependencies=off
[32mComputing main repo mapping:[0m 

[1A[K[32mLoading:[0m 

[1A[K[32mLoading:[0m 0 packages loaded

[1A[K[32mLoading:[0m 0 packages loaded
    currently loading: 
    Fetching ...l_features++version_extension+bazel_features_version; starting

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (1 packages loaded)
    currently loading: @@bazel_tools//tools

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)

[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...se:extensions.bzl%crate; Splicing Cargo workspace for `crates`

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...se:extensions.bzl%crate; Splicing Cargo workspace for `crates`

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...se:extensions.bzl%crate; Splicing Cargo workspace for `crates`

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...se:extensions.bzl%crate; Splicing Cargo workspace for `crates`

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...extensions.bzl%crate; Splicing Cargo workspace for `crates` 4s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...extensions.bzl%crate; Splicing Cargo workspace for `crates` 5s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...extensions.bzl%crate; Splicing Cargo workspace for `crates` 6s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...extensions.bzl%crate; Splicing Cargo workspace for `crates` 7s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...extensions.bzl%crate; Splicing Cargo workspace for `crates` 8s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...extensions.bzl%crate; Splicing Cargo workspace for `crates` 9s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 10s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 11s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 12s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 13s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 14s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 15s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 16s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 17s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 18s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 19s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 20s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 21s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 22s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 23s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 24s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 25s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 26s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 27s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 28s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 29s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...xtensions.bzl%crate; Splicing Cargo workspace for `crates` 30s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...sions.bzl%crate; Generating crate BUILD files for `crates` 30s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli
    Fetching ...e_universe:extensions.bzl%crate; Generating hub and spokes 30s

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (5 packages loaded, 6 targets configured)
    currently loading: crates/cli

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (6 packages loaded, 6 targets configured)
    currently loading: @@bazel_tools//tools/cpp

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (39 packages loaded, 8 targets configured)
    currently loading: @@rules_java++toolchains+local_jdk//
    Fetching ...++toolchains+remotejdk17_linux_toolchain_config_repo; starting
    Fetching ...ains+remotejdk25_macos_aarch64_toolchain_config_repo; starting

[1A[K
[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (76 packages loaded, 387 targets configured)
    currently loading: @@rules_python+//python/config_settings ... (6 packages\
)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (83 packages loaded, 529 targets configured)
    currently loading: crates/c-ffi

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (148 packages loaded, 1096 targets configured)
    currently loading: @@rules_rust++crate+crates// ... (5 packages)

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (167 packages loaded, 1962 targets configured)
    currently loading: @@rules_rust++crate+crates__clap-4.5.57// ... (7 packag\
es)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (180 packages loaded, 2720 targets configured)
    currently loading: @@rules_rust++crate+crates__syn-2.0.111// ... (8 packag\
es)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (199 packages loaded, 3192 targets configured)
    currently loading: @@rules_rust++crate+crates__tokio-1.48.0// ... (3 packa\
ges)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (220 packages loaded, 4655 targets configured)
    currently loading: @@rules_rust++crate+crates__pyo3-0.20.3// ... (2 packag\
es)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (248 packages loaded, 5839 targets configured)
    currently loading: @@rules_rust++crate+crates__thiserror-1.0.69// ... (8 p\
ackages)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (270 packages loaded, 6566 targets configured)
    currently loading: @@rules_rust++crate+crates__chrono-0.4.43// ... (4 pack\
ages)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (288 packages loaded, 7131 targets configured)
    currently loading: @@rules_rust++crate+crates__libc-0.2.178// ... (6 packa\
ges)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (303 packages loaded, 7601 targets configured)
    currently loading: @@rules_rust++crate+crates__libc-0.2.178// ... (8 packa\
ges)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (313 packages loaded, 8153 targets configured)
    currently loading: @@rules_rust++crate+crates__windows-sys-0.48.0// ... (7\
 packages)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (335 packages loaded, 9463 targets configured)
    currently loading: @@rules_rust++crate+crates__iri-string-0.7.10// ... (6 \
packages)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (367 packages loaded, 10944 targets configured)
    currently loading: @@rules_rust++i2+rrc__toml_edit-0.22.24// ... (8 packag\
es)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (387 packages loaded, 11423 targets configured)
    currently loading: @@rules_rust++crate+crates__windows-sys-0.61.2// ... (7\
 packages)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (428 packages loaded, 13298 targets configured)
    currently loading: @@rules_rust++crate+crates__thiserror-2.0.18// ... (3 p\
ackages)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (452 packages loaded, 14046 targets configured)
    currently loading: @@rules_rust++crate+crates__windows-sys-0.60.2// ... (7\
 packages)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (476 packages loaded, 14816 targets configured)
    currently loading: @@rules_rust++crate+crates__zerocopy-0.8.31// ... (7 pa\
ckages)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (503 packages loaded, 16094 targets configured)
    currently loading: @@rules_rust++crate+crates__winapi-0.3.9// ... (7 packa\
ges)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (526 packages loaded, 17484 targets configured)
    currently loading: @@rules_rust++crate+crates__windows-sys-0.59.0// ... (8\
 packages)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (570 packages loaded, 18823 targets configured)
    currently loading: @@rules_rust++crate+crates__pxfm-0.1.28// ... (6 packag\
es)

[1A[K
[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (599 packages loaded, 20168 targets configured)

[1A[K[32mINFO: [0mAnalyzed target //:kain (599 packages loaded, 21337 targets configured).
[0m no actions running

[1A[K[32mINFO: [0mFound 1 target...
[0m no actions running

[1A[K[32mINFO: [0mElapsed time: 46.415s, Critical Path: 0.00s
[0m no actions running

[1A[K[32mINFO: [0m0 processes.
[0m no actions running

[1A[K[32mINFO: [0mBuild completed successfully, 0 total actions
[32mINFO: [0mAll external dependencies for the requested targets fetched successfully.
[0m
[32mINFO: [0mInvocation ID: e3586dde-23bf-4314-b6b2-f504525d3d01
[32mINFO: [0mReading 'startup' options from x:\.bazelrc: --output_user_root=F:/_b/output-user-root
[32mINFO: [0mOptions provided by the client:
  Inherited 'common' options: --isatty=0 --terminal_columns=80
[32mINFO: [0mReading rc options for 'build' from x:\.bazelrc:
  Inherited 'common' options: --enable_bzlmod --announce_rc --color=yes --curses=yes --experimental_convenience_symlinks=ignore --repository_cache=F:/_b/repository-cache --repo_env=TMP=F:/_b/tmp --repo_env=TEMP=F:/_b/tmp --repo_env=TMPDIR=F:/_b/tmp --action_env=TMP=F:/_b/tmp --action_env=TEMP=F:/_b/tmp --action_env=TMPDIR=F:/_b/tmp --action_env=PATH --action_env=PATHEXT --action_env=SystemDrive --action_env=SystemRoot --action_env=ComSpec --action_env=windir --action_env=USERPROFILE --action_env=HOMEDRIVE --action_env=HOMEPATH --action_env=APPDATA --action_env=LOCALAPPDATA --action_env=ProgramData --action_env=ProgramFiles --action_env=ProgramFiles(x86) --action_env=CommonProgramFiles --action_env=CommonProgramFiles(x86) --action_env=NUMBER_OF_PROCESSORS --action_env=PROCESSOR_ARCHITECTURE --action_env=PROCESSOR_IDENTIFIER
[32mINFO: [0mReading rc options for 'build' from x:\.bazelrc:
  'build' options: --disk_cache=F:/_b/disk-cache --jobs=HOST_CPUS*.625 --loading_phase_threads=HOST_CPUS*.5 --local_resources=cpu=HOST_CPUS*.625 --local_resources=memory=HOST_RAM*.75 --keep_going
[32mINFO: [0mFound applicable config definition build:dev in file x:\.bazelrc: --compilation_mode=dbg
[32mComputing main repo mapping:[0m 

[1A[K[32mLoading:[0m 

[1A[K[32mLoading:[0m 0 packages loaded

[1A[K[32mAnalyzing:[0m target //:kain (0 packages loaded, 0 targets configured)

[1A[K[32mAnalyzing:[0m target //:kain (0 packages loaded, 0 targets configured)
    Fetching ...n @@rules_rust+//crate_universe:extensions.bzl%crate; starting

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (0 packages loaded, 0 targets configured)
    Fetching ...xtensions.bzl%crate; Generating crate BUILD files for `crates`

[1A[K
[1A[K[32mAnalyzing:[0m target //:kain (0 packages loaded, 0 targets configured)


[1A[K
[1A[K[32mINFO: [0mAnalyzed target //:kain (0 packages loaded, 1 target configured).


[1A[K[32m[62 / 432][0m [Prepa] @@rules_rust++crate+crates__serde-1.0.228//:_bs-

[1A[K[32m[109 / 801][0m [Prepa] @@rules_rust++crate+crates__crc32fast-1.5.0//:_bs-

[1A[K[32m[220 / 2,901][0m 2 actions running
    @@rules_rust++crate+crates__portable-atomic-1.13.1//:_bs-; 0s local

[1A[K
[1A[K[32m[256 / 10,028][0m checking cached actions

[1A[K[32m[402 / 10,028][0m [Prepa] Running Cargo build script cli

[1A[K[32m[532 / 10,028][0m 5 actions, 4 running
    Running Cargo build script cli; 0s local, disk-cache
    Running Cargo build script unreal_asset; 0s local, disk-cache
    Running Cargo build script prettyplease; 0s local, disk-cache
    Running Cargo build script typenum; 0s local, disk-cache
    [Prepa] Running Cargo build script windows_x86_64_msvc

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[646 / 10,028][0m 2 actions running
    Running Cargo build script windows_x86_64_msvc; 0s local, disk-cache
    Running Cargo build script quote; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[746 / 10,028][0m Running Cargo build script proc-macro2; 0s local, disk-cache

[1A[K[32m[779 / 10,028][0m 3 actions running
    Running Cargo build script proc-macro2; 0s local, disk-cache
    Running Cargo build script serde_json; 0s local, disk-cache
    Running Cargo build script serde_core; 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[932 / 10,028][0m Running Cargo build script native-tls; 0s local, disk-cache

[1A[K[32m[1,000 / 10,028][0m checking cached actions

[1A[K[32m[1,188 / 10,028][0m [Prepa] Running Cargo build script serde

[1A[K[32m[1,311 / 10,028][0m Running Cargo build script libc; 0s local, disk-cache

[1A[K[32m[1,372 / 10,028][0m checking cached actions

[1A[K[32m[1,545 / 10,028][0m [Prepa] Running Cargo build script httparse

[1A[K[32m[1,564 / 10,028][0m checking cached actions

[1A[K[32m[2,652 / 10,028][0m [Prepa] Running Cargo build script icu_properties_data

[1A[K[32m[2,726 / 10,028][0m checking cached actions

[1A[K[32m[3,668 / 10,028][0m [Prepa] Running Cargo build script rayon-core

[1A[K[32m[3,708 / 10,028][0m [Prepa] Running Cargo build script windows_x86_64_msvc

[1A[K[32m[3,750 / 10,028][0m [Prepa] Running Cargo build script icu_normalizer_data

[1A[K[32m[3,809 / 10,028][0m [Prepa] Running Cargo build script crossbeam-utils

[1A[K[32m[3,900 / 10,028][0m checking cached actions

[1A[K[32m[4,032 / 10,028][0m 2 actions running
    Running Cargo build script crc32fast; 0s local, disk-cache
    Running Cargo build script kain-error; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Running Cargo build script kain-error:
Build Script Warning: Codegen'd 162 diagnostic specs from TOML
[32m[4,052 / 10,028][0m 2 actions running
    Running Cargo build script crc32fast; 0s local, disk-cache
    Running Cargo build script kain-error; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[4,205 / 10,028][0m checking cached actions

[1A[K[32m[4,275 / 10,028][0m checking cached actions

[1A[K[32m[4,568 / 10,028][0m [Prepa] Running Cargo build script memoffset

[1A[K[32m[4,688 / 10,028][0m [Prepa] Running Cargo build script num-traits

[1A[K[32m[4,757 / 10,028][0m Running Cargo build script num-traits; 0s local, disk-cache

[1A[K[32m[4,846 / 10,028][0m checking cached actions

[1A[K[32m[5,030 / 10,028][0m [Prepa] Running Cargo build script zstd-sys

[1A[K[32m[5,060 / 10,028][0m Running Cargo build script zstd-sys; 0s local, disk-cache

[1A[K[32m[5,155 / 10,028][0m Running Cargo build script zstd-sys; 1s local, disk-cache

[1A[K[32m[5,221 / 10,028][0m 2 actions, 1 running
    Running Cargo build script zstd-sys; 1s local, disk-cache
    [Prepa] Running Cargo build script portable-atomic

[1A[K
[1A[K
[1A[K[32m[5,388 / 10,028][0m Running Cargo build script zstd-sys; 2s local, disk-cache

[1A[K[32m[5,652 / 10,028][0m Running Cargo build script zstd-sys; 3s local, disk-cache

[1A[K[32m[5,742 / 10,028][0m Running Cargo build script zstd-sys; 4s local, disk-cache

[1A[K[32m[5,780 / 10,028][0m Running Cargo build script zstd-sys; 5s local, disk-cache

[1A[K[32m[5,862 / 10,028][0m Running Cargo build script zstd-sys; 6s local, disk-cache

[1A[K[32m[5,900 / 10,028][0m Running Cargo build script zstd-sys; 7s local, disk-cache

[1A[K[32m[5,956 / 10,028][0m 2 actions, 1 running
    Running Cargo build script zstd-sys; 7s local, disk-cache
    [Prepa] Running Cargo build script zerocopy

[1A[K
[1A[K
[1A[K[32m[5,986 / 10,028][0m 3 actions running
    Running Cargo build script zstd-sys; 8s local, disk-cache
    Running Cargo build script zerocopy; 0s local, disk-cache
    Running Cargo build script thiserror; 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[6,021 / 10,028][0m 2 actions running
    Running Cargo build script zstd-sys; 8s local, disk-cache
    Running Cargo build script smartstring; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[6,080 / 10,028][0m 2 actions, 1 running
    Running Cargo build script zstd-sys; 8s local, disk-cache
    [Prepa] Running Cargo build script ahash

[1A[K
[1A[K
[1A[K[32m[6,177 / 10,028][0m Running Cargo build script zstd-sys; 9s local, disk-cache

[1A[K[32m[6,382 / 10,028][0m 2 actions, 1 running
    Running Cargo build script zstd-sys; 9s local, disk-cache
    [Prepa] Running Cargo build script windows_x86_64_msvc

[1A[K
[1A[K
[1A[K[32m[6,426 / 10,028][0m 2 actions running
    Running Cargo build script zstd-sys; 10s local, disk-cache
    Running Cargo build script windows_x86_64_msvc; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[6,495 / 10,028][0m 3 actions, 2 running
    Running Cargo build script zstd-sys; 10s local, disk-cache
    Running Cargo build script windows_x86_64_msvc; 0s local, disk-cache
    [Prepa] Running Cargo build script tree-sitter-language

[1A[K
[1A[K
[1A[K
[1A[K[32m[6,508 / 10,028][0m 3 actions, 2 running
    Running Cargo build script zstd-sys; 10s local, disk-cache
    Running Cargo build script tree-sitter-language; 0s local, disk-cache
    Running Cargo build script ash; 0s disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[6,533 / 10,028][0m 3 actions, 2 running
    Running Cargo build script zstd-sys; 11s local, disk-cache
    Running Cargo build script ash; 0s local, disk-cache
    [Prepa] Running Cargo build script tree-sitter

[1A[K
[1A[K
[1A[K
[1A[K[32m[6,591 / 10,028][0m 3 actions running
    Running Cargo build script zstd-sys; 11s local, disk-cache
    Running Cargo build script tree-sitter; 0s local, disk-cache
    Running Cargo build script indexmap; 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[6,602 / 10,028][0m 4 actions running
    Running Cargo build script zstd-sys; 11s local, disk-cache
    Running Cargo build script tree-sitter; 0s local, disk-cache
    Running Cargo build script indexmap; 0s local, disk-cache
    Running Cargo build script generic-array; 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[6,613 / 10,028][0m 3 actions running
    Running Cargo build script zstd-sys; 11s local, disk-cache
    Running Cargo build script tree-sitter; 0s local, disk-cache
    Running Cargo build script indexmap; 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[6,643 / 10,028][0m 2 actions running
    Running Cargo build script zstd-sys; 12s local, disk-cache
    Running Cargo build script tree-sitter; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[6,693 / 10,028][0m 3 actions, 2 running
    Running Cargo build script zstd-sys; 12s local, disk-cache
    Running Cargo build script tree-sitter; 1s local, disk-cache
    [Prepa] Running Cargo build script schemars

[1A[K
[1A[K
[1A[K
[1A[K[32m[6,807 / 10,028][0m 2 actions running
    Running Cargo build script zstd-sys; 12s local, disk-cache
    Running Cargo build script tree-sitter; 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Running Cargo build script zstd-sys:
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9002 : ignoring unknown option '-fvisibility=hidden'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
[32m[6,814 / 10,028][0m 2 actions running
    Running Cargo build script zstd-sys; 12s local, disk-cache
    Running Cargo build script tree-sitter; 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[6,828 / 10,028][0m Running Cargo build script tree-sitter; 1s local, disk-cache

[1A[K[32m[6,836 / 10,028][0m 2 actions running
    Running Cargo build script tree-sitter; 2s local, disk-cache
    ...//:zstd_sys; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[6,847 / 10,028][0m Running Cargo build script tree-sitter; 2s local, disk-cache

[1A[K[32mINFO: [0mFrom Running Cargo build script tree-sitter:
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
[32m[6,864 / 10,028][0m Running Cargo build script tree-sitter; 2s local, disk-cache

[1A[K[32m[6,932 / 10,028][0m checking cached actions

[1A[K[32m[6,950 / 10,028][0m Running Cargo build script winapi; 0s local, disk-cache

[1A[K[32m[6,967 / 10,028][0m checking cached actions

[1A[K[32m[7,004 / 10,028][0m [Prepa] Compiling Rust rlib zstd_safe v7.2.4 (7 files)

[1A[K[32m[7,067 / 10,028][0m ...//:zstd_safe; 0s local, disk-cache

[1A[K[32m[7,097 / 10,028][0m ...//:zstd; 0s local, disk-cache

[1A[K[32m[7,116 / 10,028][0m 2 actions running
    Compiling Rust rlib zstd v0.13.3 (24 files); 0s local, disk-cache
    Running Cargo build script pyo3-ffi; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[7,132 / 10,028][0m 3 actions, 2 running
    Compiling Rust rlib zstd v0.13.3 (24 files); 0s local, disk-cache
    Running Cargo build script pyo3-ffi; 0s local, disk-cache
    [Prepa] Running Cargo build script anyhow

[1A[K
[1A[K
[1A[K
[1A[K[32m[7,235 / 10,028][0m checking cached actions

[1A[K[32m[7,255 / 10,028][0m ...//:_bs; 0s local, disk-cache

[1A[K[32m[7,448 / 10,028][0m ...//:_bs; 1s local, disk-cache

[1A[K[32m[7,487 / 10,028][0m 2 actions, 1 running
    Running Cargo build script tree-sitter-hlsl; 1s local, disk-cache
    [Prepa] Running Cargo build script zstd-safe

[1A[K
[1A[K
[1A[K[32m[7,507 / 10,028][0m 2 actions running
    Running Cargo build script tree-sitter-hlsl; 1s local, disk-cache
    Compiling Rust rlib zstd_safe v6.0.6 (5 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[7,537 / 10,028][0m ...//:_bs; 2s local, disk-cache

[1A[K[32m[7,586 / 10,028][0m 2 actions running
    Running Cargo build script tree-sitter-hlsl; 2s local, disk-cache
    Compiling Rust rlib zstd v0.12.4 (24 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[7,624 / 10,028][0m ...//:_bs; 2s local, disk-cache

[1A[K[32m[7,791 / 10,028][0m ...//:_bs; 3s local, disk-cache

[1A[K[32mINFO: [0mFrom Running Cargo build script tree-sitter-hlsl:
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
[32m[7,804 / 10,028][0m ...//:_bs; 4s local, disk-cache

[1A[K[32m[7,834 / 10,028][0m checking cached actions

[1A[K[32m[7,992 / 10,028][0m [Prepa] Running Cargo build script thiserror

[1A[K[32m[8,042 / 10,028][0m Running Cargo build script thiserror; 0s local, disk-cache

[1A[K[32m[8,106 / 10,028][0m checking cached actions

[1A[K[32m[8,401 / 10,028][0m [Prepa] Running Cargo build script stacker

[1A[K[32m[8,475 / 10,028][0m Running Cargo build script stacker; 0s local, disk-cache

[1A[K[32m[8,545 / 10,028][0m Running Cargo build script stacker; 0s local, disk-cache

[1A[K[32m[8,589 / 10,028][0m 2 actions, 1 running
    Running Cargo build script stacker; 0s local, disk-cache
    [Prepa] Running Cargo build script parking_lot_core

[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Running Cargo build script stacker:
Build Script Warning: cl : Command line warning D9025 : overriding '/MD' with '/MDd'
[32m[8,623 / 10,028][0m Running Cargo build script stacker; 0s local, disk-cache

[1A[K[32m[8,675 / 10,028][0m ...//:winapi; 0s local, disk-cache

[1A[K[32m[8,746 / 10,028][0m 2 actions, 1 running
    Compiling Rust rlib winapi v0.3.9 (405 files); 0s local, disk-cache
    [Prepa] Running Cargo build script pyo3

[1A[K
[1A[K
[1A[K[32m[8,792 / 10,028][0m 2 actions running
    Compiling Rust rlib winapi v0.3.9 (405 files); 0s local, disk-cache
    Running Cargo build script pyo3; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[8,806 / 10,028][0m ...//:winapi; 1s local, disk-cache

[1A[K[32m[8,824 / 10,028][0m 2 actions running
    Compiling Rust rlib winapi v0.3.9 (405 files); 1s local, disk-cache
    Running Cargo build script psm; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[8,860 / 10,028][0m 2 actions, 1 running
    Compiling Rust rlib winapi v0.3.9 (405 files); 2s local, disk-cache
    [Prepa] Compiling Rust rlib psm v0.1.28 (10 files)

[1A[K
[1A[K
[1A[K[32m[8,877 / 10,028][0m 2 actions, 1 running
    Compiling Rust rlib winapi v0.3.9 (405 files); 2s local, disk-cache
    [Prepa] //crates/core:kain-core

[1A[K
[1A[K
[1A[K[32m[8,907 / 10,028][0m ...//:stacker; 0s local, disk-cache

[1A[K[32m[8,928 / 10,028][0m checking cached actions

[1A[K[32m[8,951 / 10,028][0m ...//:chumsky; 0s local, disk-cache

[1A[K[32m[8,967 / 10,028][0m 2 actions, 1 running
    Compiling Rust rlib chumsky v0.9.3 (16 files); 0s local, disk-cache
    [Prepa] Compiling Rust rlib kain-amalgamate (1 file)

[1A[K
[1A[K
[1A[K[32m[8,975 / 10,028][0m 2 actions running
    Compiling Rust rlib chumsky v0.9.3 (16 files); 0s local, disk-cache
    Compiling Rust rlib kain-amalgamate (1 file); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,002 / 10,028][0m 2 actions running
    Compiling Rust rlib chumsky v0.9.3 (16 files); 1s local, disk-cache
    Compiling Rust rlib kain-amalgamate (1 file); 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,065 / 10,028][0m //crates/amalgamate:kain-amalgamate; 1s local, disk-cache

[1A[K[32m[9,114 / 10,028][0m 2 actions, 1 running
    Compiling Rust rlib kain-amalgamate (1 file); 2s local, disk-cache
    [Prepa] Compiling Rust rlib swc_ecma_parser v0.150.0 (36 files)

[1A[K
[1A[K
[1A[K[32m[9,131 / 10,028][0m ...//:swc_ecma_parser; 0s local, disk-cache

[1A[K[32m[9,183 / 10,028][0m 2 actions, 1 running
    ...//:swc_ecma_parser; 1s local, disk-cache
    [Prepa] Compiling Rust rlib tree_sitter v0.24.7 (6 files)

[1A[K
[1A[K
[1A[K[32m[9,192 / 10,028][0m 2 actions running
    ...//:swc_ecma_parser; 1s local, disk-cache
    Compiling Rust rlib tree_sitter v0.24.7 (6 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,226 / 10,028][0m ...//:swc_ecma_parser; 1s local, disk-cache

[1A[K[32m[9,237 / 10,028][0m ...//:swc_ecma_parser; 2s local, disk-cache

[1A[K[32m[9,274 / 10,028][0m ...//:swc_ecma_parser; 3s local, disk-cache

[1A[K[32m[9,295 / 10,028][0m ...//:swc_ecma_parser; 4s local, disk-cache

[1A[K[32m[9,299 / 10,028][0m 2 actions running
    ...//:swc_ecma_parser; 4s local, disk-cache
    Compiling Rust rlib unreal_asset_base (39 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,315 / 10,028][0m 2 actions running
    ...//:swc_ecma_parser; 5s local, disk-cache
    Compiling Rust rlib unreal_asset_base (39 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,347 / 10,028][0m 2 actions running
    ...//:swc_ecma_parser; 6s local, disk-cache
    Compiling Rust rlib unreal_asset_base (39 files); 2s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,449 / 10,028][0m 2 actions running
    ...//:swc_ecma_parser; 7s local, disk-cache
    Compiling Rust rlib unreal_asset_base (39 files); 3s local, disk-cache

[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib unreal_asset_base (39 files):
[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
   [1m[96m--> [0mcrates/unreal/unreal_asset_base/src\unversioned\mod.rs:261:43
    [1m[96m|[0m
[1m[96m261[0m [1m[96m|[0m             self.custom_versions = reader.read_array(CustomVersion::read)?;
    [1m[96m|[0m                                           [1m[93m^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
    [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
    [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `ArchiveReader::read_array(...)` to keep using the current method
    [1m[96m= [0m[1m[97mnote[0m: `#[warn(unstable_name_collisions)]` (part of `#[warn(future_incompatible)]`) on by default

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
   [1m[96m--> [0mcrates/unreal/unreal_asset_base/src\unversioned\mod.rs:334:32
    [1m[96m|[0m
[1m[96m334[0m [1m[96m|[0m         self.name_map = reader.read_array(|reader| {
    [1m[96m|[0m                                [1m[93m^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
    [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
    [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: 2 warnings emitted[0m

[32m[9,504 / 10,028][0m 2 actions running
    ...//:swc_ecma_parser; 8s local, disk-cache
    Compiling Rust rlib unreal_asset_base (39 files); 4s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,542 / 10,028][0m ...//:swc_ecma_parser; 8s local, disk-cache

[1A[K[32m[9,586 / 10,028][0m 2 actions, 1 running
    ...//:swc_ecma_parser; 9s local, disk-cache
    [Prepa] Compiling Rust rlib unreal_asset_properties (53 files)

[1A[K
[1A[K
[1A[K[32m[9,597 / 10,028][0m 2 actions running
    ...//:swc_ecma_parser; 9s local, disk-cache
    .../unreal_asset_properties:unreal_asset_properties; 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,625 / 10,028][0m 3 actions, 2 running
    ...//:swc_ecma_parser; 10s local, disk-cache
    .../unreal_asset_properties:unreal_asset_properties; 1s local, disk-cache
    [Prepa] Compiling Rust rlib unreal_asset_kismet (1 file)

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,643 / 10,028][0m 3 actions running
    ...//:swc_ecma_parser; 10s local, disk-cache
    .../unreal_asset_properties:unreal_asset_properties; 1s local, disk-cache
    Compiling Rust rlib unreal_asset_kismet (1 file); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,651 / 10,028][0m 3 actions running
    ...//:swc_ecma_parser; 11s local, disk-cache
    .../unreal_asset_properties:unreal_asset_properties; 2s local, disk-cache
    Compiling Rust rlib unreal_asset_kismet (1 file); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,651 / 10,028][0m 4 actions running
    ...//:swc_ecma_parser; 11s local, disk-cache
    .../unreal_asset_properties:unreal_asset_properties; 2s local, disk-cache
    Compiling Rust rlib unreal_asset_kismet (1 file); 1s local, disk-cache
    Compiling Rust rlib kain-core (31 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,651 / 10,028][0m 4 actions running
    ...//:swc_ecma_parser; 12s local, disk-cache
    .../unreal_asset_properties:unreal_asset_properties; 3s local, disk-cache
    Compiling Rust rlib unreal_asset_kismet (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-core (31 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,652 / 10,028][0m 3 actions running
    .../unreal_asset_properties:unreal_asset_properties; 4s local, disk-cache
    Compiling Rust rlib unreal_asset_kismet (1 file); 3s local, disk-cache
    Compiling Rust rlib kain-core (31 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,661 / 10,028][0m 2 actions running
    .../unreal_asset_properties:unreal_asset_properties; 5s local, disk-cache
    Compiling Rust rlib kain-core (31 files); 2s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,682 / 10,028][0m //crates/core:kain-core; 3s local, disk-cache

[1A[K[32m[9,691 / 10,028][0m 2 actions, 1 running
    Compiling Rust rlib kain-core (31 files); 3s local, disk-cache
    Compiling Rust rlib unreal_asset_exports (17 files); 0s disk-cache

[1A[K
[1A[K
[1A[K[32m[9,699 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-core (31 files); 3s local, disk-cache
    Compiling Rust rlib unreal_asset_exports (17 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib unreal_asset_exports (17 files):
[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
  [1m[96m--> [0mcrates/unreal/unreal_asset_exports/src\level_export.rs:75:27
   [1m[96m|[0m
[1m[96m75[0m [1m[96m|[0m             actors: asset.read_array(|asset| Ok(PackageIndex::new(asset.read_i32::<LE>()?)))?,
   [1m[96m|[0m                           [1m[93m^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
   [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
   [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(unstable_name_collisions)]` (part of `#[warn(future_incompatible)]`) on by default

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
  [1m[96m--> [0mcrates/unreal/unreal_asset_exports/src\level_export.rs:81:32
   [1m[96m|[0m
[1m[96m81[0m [1m[96m|[0m                 options: asset.read_array(|asset| asset.read_fstring())?,
   [1m[96m|[0m                                [1m[93m^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
   [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
   [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
  [1m[96m--> [0mcrates/unreal/unreal_asset_exports/src\level_export.rs:87:18
   [1m[96m|[0m
[1m[96m87[0m [1m[96m|[0m                 .read_array(|asset| Ok(PackageIndex::new(asset.read_i32::<LE>()?)))?,
   [1m[96m|[0m                  [1m[93m^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
   [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
   [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
  [1m[96m--> [0mcrates/unreal/unreal_asset_exports/src\world_export.rs:47:18
   [1m[96m|[0m
[1m[96m47[0m [1m[96m|[0m                 .read_array(|asset| Ok(PackageIndex::new(asset.read_i32::<LE>()?)))?,
   [1m[96m|[0m                  [1m[93m^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
   [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
   [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
  [1m[96m--> [0mcrates/unreal/unreal_asset_exports/src\world_export.rs:49:18
   [1m[96m|[0m
[1m[96m49[0m [1m[96m|[0m                 .read_array(|asset| Ok(PackageIndex::new(asset.read_i32::<LE>()?)))?,
   [1m[96m|[0m                  [1m[93m^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
   [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
   [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: 5 warnings emitted[0m

[32m[9,699 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-core (31 files); 4s local, disk-cache
    Compiling Rust rlib unreal_asset_exports (17 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,716 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-core (31 files); 4s local, disk-cache
    Compiling Rust rlib unreal_asset_registry (10 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib unreal_asset_registry (10 files):
[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
  [1m[96m--> [0mcrates/unreal/unreal_asset_registry/src\objects\asset_bundle_data.rs:28:35
   [1m[96m|[0m
[1m[96m28[0m [1m[96m|[0m         let bundle_assets = asset.read_array(|asset: &mut Reader| {
   [1m[96m|[0m                                   [1m[93m^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
   [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
   [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(unstable_name_collisions)]` (part of `#[warn(future_incompatible)]`) on by default

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
  [1m[96m--> [0mcrates/unreal/unreal_asset_registry/src\objects\asset_bundle_data.rs:82:29
   [1m[96m|[0m
[1m[96m82[0m [1m[96m|[0m         let bundles = asset.read_array(|asset: &mut Reader| AssetBundleEntry::new(asset))?;
   [1m[96m|[0m                             [1m[93m^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
   [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
   [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
   [1m[96m--> [0mcrates/unreal/unreal_asset_registry/src\objects\asset_data.rs:123:31
    [1m[96m|[0m
[1m[96m123[0m [1m[96m|[0m [1m[96m...[0m   let chunk_ids = asset.read_array(|asset: &mut Reader| Ok(asset.read_i32::<LE>()?))?; // if we don't explicitly specify the ty[1m[96m...[0m
    [1m[96m|[0m                             [1m[93m^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
    [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
    [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
   [1m[96m--> [0mcrates/unreal/unreal_asset_registry/src\objects\asset_package_data.rs:108:28
    [1m[96m|[0m
[1m[96m108[0m [1m[96m|[0m                 Some(asset.read_array(|asset: &mut Reader| CustomVersion::read(asset))?);
    [1m[96m|[0m                            [1m[93m^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
    [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
    [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
   [1m[96m--> [0mcrates/unreal/unreal_asset_registry/src\objects\asset_package_data.rs:113:43
    [1m[96m|[0m
[1m[96m113[0m [1m[96m|[0m             imported_classes = Some(asset.read_array(|asset: &mut Reader| asset.read_fname())?);
    [1m[96m|[0m                                           [1m[93m^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
    [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
    [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
   [1m[96m--> [0mcrates/unreal/unreal_asset_registry/src\objects\depends_node.rs:169:37
    [1m[96m|[0m
[1m[96m169[0m [1m[96m|[0m         let in_dependencies = asset.read_array(|asset: &mut Reader| Ok(asset.read_i32::<LE>()?))?;
    [1m[96m|[0m                                     [1m[93m^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
    [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
    [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
   [1m[96m--> [0mcrates/unreal/unreal_asset_registry/src\objects\depends_node.rs:308:37
    [1m[96m|[0m
[1m[96m308[0m [1m[96m|[0m         let in_dependencies = asset.read_array(|asset: &mut Reader| Ok(asset.read_i32::<LE>()?))?;
    [1m[96m|[0m                                     [1m[93m^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
    [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
    [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
   [1m[96m--> [0mcrates/unreal/unreal_asset_registry/src/lib.rs:106:30
    [1m[96m|[0m
[1m[96m106[0m [1m[96m|[0m         *assets_data = asset.read_array(|asset: &mut Reader| AssetData::new(asset, version))?;
    [1m[96m|[0m                              [1m[93m^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
    [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
    [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: a method with this name may be added to the standard library in the future[0m
   [1m[96m--> [0mcrates/unreal/unreal_asset_registry/src/lib.rs:143:19
    [1m[96m|[0m
[1m[96m143[0m [1m[96m|[0m             asset.read_array(|asset: &mut Reader| AssetPackageData::new(asset, version))?;
    [1m[96m|[0m                   [1m[93m^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mwarning[0m: once this associated item is added to the standard library, the ambiguity may cause an error or change in behavior!
    [1m[96m= [0m[1m[97mnote[0m: for more information, see issue #48919 <https://github.com/rust-lang/rust/issues/48919>
    [1m[96m= [0m[1m[97mhelp[0m: call with fully qualified syntax `unreal_asset_base::reader::ArchiveReader::read_array(...)` to keep using the current method

[1m[93mwarning[0m[1m[97m: 9 warnings emitted[0m

[32m[9,716 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-core (31 files); 5s local, disk-cache
    Compiling Rust rlib unreal_asset_registry (10 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,732 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-core (31 files); 5s local, disk-cache
    Compiling Rust rlib unreal_asset (7 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,744 / 10,028][0m //crates/core:kain-core; 6s local, disk-cache

[1A[K[32m[9,747 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-core (31 files); 6s local, disk-cache
    Compiling Rust rlib ue5-asset-utils (5 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 6s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 7s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 8s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 9s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 10s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 11s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 12s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 13s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 14s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 15s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 16s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 17s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 18s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 19s local, disk-cache

[1A[K[32m[9,760 / 10,028][0m //crates/core:kain-core; 20s local, disk-cache

[1A[K[32m[9,761 / 10,028][0m checking cached actions

[1A[K[32m[9,761 / 10,028][0m checking cached actions

[1A[K[32m[9,770 / 10,028][0m [Prepa] Compiling Rust rlib kain-script (4 files)

[1A[K[32m[9,771 / 10,028][0m 4 actions running
    Compiling Rust rlib kain-script (4 files); 0s local, disk-cache
    Compiling Rust rlib kain-wasm (3 files); 0s local, disk-cache
    Compiling Rust rlib kain-interop (1 file); 0s local, disk-cache
    Compiling Rust rlib kain-ui-tauri (1 file); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,771 / 10,028][0m 4 actions running
    Compiling Rust rlib kain-script (4 files); 1s local, disk-cache
    Compiling Rust rlib kain-wasm (3 files); 1s local, disk-cache
    Compiling Rust rlib kain-interop (1 file); 1s local, disk-cache
    Compiling Rust rlib kain-ui-tauri (1 file); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,778 / 10,028][0m 3 actions running
    Compiling Rust rlib kain-script (4 files); 1s local, disk-cache
    Compiling Rust rlib kain-wasm (3 files); 1s local, disk-cache
    Compiling Rust rlib kain-ui-tauri (1 file); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,783 / 10,028][0m 6 actions, 5 running
    Compiling Rust rlib kain-script (4 files); 1s local, disk-cache
    Compiling Rust rlib kain-wasm (3 files); 1s local, disk-cache
    Compiling Rust rlib kain-ui-tauri (1 file); 1s local, disk-cache
    Compiling Rust rlib ue5-shaders (6 files); 0s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 0s local, disk-cache
    [Prepa] Compiling Rust rlib ue5-materials (8 files)

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,789 / 10,028][0m 7 actions, 6 running
    Compiling Rust rlib kain-script (4 files); 2s local, disk-cache
    Compiling Rust rlib kain-wasm (3 files); 2s local, disk-cache
    Compiling Rust rlib kain-ui-tauri (1 file); 2s local, disk-cache
    Compiling Rust rlib ue5-shaders (6 files); 0s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 0s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 0s local, disk-cache
    [Prepa] Compiling Rust rlib ue5-graphs (15 files)

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,790 / 10,028][0m 7 actions running
    Compiling Rust rlib kain-wasm (3 files); 2s local, disk-cache
    Compiling Rust rlib kain-ui-tauri (1 file); 2s local, disk-cache
    Compiling Rust rlib ue5-shaders (6 files); 0s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 0s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 0s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 0s local, disk-cache
    Compiling Rust rlib kain-node (1 file); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,793 / 10,028][0m 6 actions running
    Compiling Rust rlib kain-wasm (3 files); 2s local, disk-cache
    Compiling Rust rlib ue5-shaders (6 files); 0s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 0s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 0s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 0s local, disk-cache
    Compiling Rust rlib kain-node (1 file); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,812 / 10,028][0m 6 actions running
    Compiling Rust rlib kain-wasm (3 files); 3s local, disk-cache
    Compiling Rust rlib ue5-shaders (6 files); 1s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 1s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 1s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 1s local, disk-cache
    Compiling Rust rlib kain-node (1 file); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib kain-wasm (3 files):
[1m[93mwarning[0m[1m[97m: fields `actor_name`, `message_name`, and `span` are never read[0m
   [1m[96m--> [0mcrates/wasm/src\codegen_wasm.rs:439:5
    [1m[96m|[0m
[1m[96m438[0m [1m[96m|[0m struct WasmActorHandler {
    [1m[96m|[0m        [1m[96m----------------[0m [1m[96mfields in this struct[0m
[1m[96m439[0m [1m[96m|[0m     actor_name: String,
    [1m[96m|[0m     [1m[93m^^^^^^^^^^[0m
[1m[96m440[0m [1m[96m|[0m     message_name: String,
    [1m[96m|[0m     [1m[93m^^^^^^^^^^^^[0m
[1m[96m...[0m
[1m[96m446[0m [1m[96m|[0m     span: kain_core::span::Span,
    [1m[96m|[0m     [1m[93m^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mnote[0m: `WasmActorHandler` has derived impls for the traits `Clone` and `Debug`, but these are intentionally ignored during dead code analysis
    [1m[96m= [0m[1m[97mnote[0m: `#[warn(dead_code)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: 1 warning emitted[0m

[32m[9,812 / 10,028][0m 6 actions running
    Compiling Rust rlib kain-wasm (3 files); 3s local, disk-cache
    Compiling Rust rlib ue5-shaders (6 files); 1s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 1s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 1s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 1s local, disk-cache
    Compiling Rust rlib kain-node (1 file); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,814 / 10,028][0m 5 actions running
    Compiling Rust rlib ue5-shaders (6 files); 2s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 2s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 2s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 1s local, disk-cache
    Compiling Rust rlib kain-node (1 file); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,819 / 10,028][0m 4 actions running
    Compiling Rust rlib ue5-shaders (6 files); 2s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 2s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 2s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 2s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,827 / 10,028][0m 4 actions running
    Compiling Rust rlib ue5-shaders (6 files); 3s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 3s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 3s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 3s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,838 / 10,028][0m 5 actions, 4 running
    Compiling Rust rlib ue5-shaders (6 files); 4s local, disk-cache
    Compiling Rust rlib kain-asm (6 files); 4s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 4s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 4s local, disk-cache
    [Prepa] Compiling Rust rlib kain-web (2 files)

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,839 / 10,028][0m 6 actions, 5 running
    Compiling Rust rlib kain-asm (6 files); 4s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 4s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 4s local, disk-cache
    Compiling Rust rlib kain-web (2 files); 0s local, disk-cache
    Compiling Rust rlib ue5-gas (15 files); 0s local, disk-cache
    [Prepa] Compiling Rust rlib kain-python (1 file)

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,839 / 10,028][0m 6 actions running
    Compiling Rust rlib kain-asm (6 files); 4s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 4s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 4s local, disk-cache
    Compiling Rust rlib kain-web (2 files); 0s local, disk-cache
    Compiling Rust rlib ue5-gas (15 files); 0s local, disk-cache
    Compiling Rust rlib kain-python (1 file); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,845 / 10,028][0m 7 actions, 6 running
    Compiling Rust rlib kain-asm (6 files); 5s local, disk-cache
    Compiling Rust rlib ue5-materials (8 files); 5s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 5s local, disk-cache
    Compiling Rust rlib kain-web (2 files); 1s local, disk-cache
    Compiling Rust rlib ue5-gas (15 files); 1s local, disk-cache
    Compiling Rust rlib kain-python (1 file); 1s local, disk-cache
    [Prepa] Compiling Rust rlib kain-stdlib-map (2 files)

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,851 / 10,028][0m 5 actions running
    Compiling Rust rlib ue5-materials (8 files); 6s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 5s local, disk-cache
    Compiling Rust rlib ue5-gas (15 files); 1s local, disk-cache
    Compiling Rust rlib kain-python (1 file); 1s local, disk-cache
    Compiling Rust rlib kain-stdlib-map (2 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib ue5-graphs (15 files):
[1m[93mwarning[0m[1m[97m: unused import: `ExportBaseTrait`[0m
  [1m[96m--> [0mcrates/ue5-graphs/src\binary_serializer.rs:10:35
   [1m[96m|[0m
[1m[96m10[0m [1m[96m|[0m     exports::{BaseExport, Export, ExportBaseTrait, NormalExport},
   [1m[96m|[0m                                   [1m[93m^^^^^^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_imports)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: unused import: `BoolProperty`[0m
  [1m[96m--> [0mcrates/ue5-graphs/src\binary_serializer.rs:17:20
   [1m[96m|[0m
[1m[96m17[0m [1m[96m|[0m     int_property::{BoolProperty, IntProperty},
   [1m[96m|[0m                    [1m[93m^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: unused imports: `PinDefinition` and `PinType`[0m
  [1m[96m--> [0mcrates/ue5-graphs/src\binary_serializer.rs:23:48
   [1m[96m|[0m
[1m[96m23[0m [1m[96m|[0m use crate::{GraphEditor, GraphError, NodeType, PinDefinition, PinType, Result};
   [1m[96m|[0m                                                [1m[93m^^^^^^^^^^^^^[0m  [1m[93m^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: use of deprecated function `runtime_converter::convert_runtime_graph`: Use convert_graph_runtime_to_ir instead[0m
  [1m[96m--> [0mcrates/ue5-graphs/src/lib.rs:45:58
   [1m[96m|[0m
[1m[96m45[0m [1m[96m|[0m pub use runtime_converter::{convert_graph_runtime_to_ir, convert_runtime_graph};
   [1m[96m|[0m                                                          [1m[93m^^^^^^^^^^^^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(deprecated)]` on by default

[1m[93mwarning[0m[1m[97m: unused variable: `stmt`[0m
   [1m[96m--> [0mcrates/ue5-graphs/src\runtime_converter.rs:576:9
    [1m[96m|[0m
[1m[96m576[0m [1m[96m|[0m     for stmt in &block.stmts {
    [1m[96m|[0m         [1m[93m^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_stmt`[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_variables)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: field `properties` is never read[0m
  [1m[96m--> [0mcrates/ue5-graphs/src\ast_converter.rs:13:5
   [1m[96m|[0m
[1m[96m11[0m [1m[96m|[0m pub struct GraphEditorConverter {
   [1m[96m|[0m            [1m[96m--------------------[0m [1m[96mfield in this struct[0m
[1m[96m12[0m [1m[96m|[0m     /// Track extracted properties for validation
[1m[96m13[0m [1m[96m|[0m     properties: GraphProperties,
   [1m[96m|[0m     [1m[93m^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(dead_code)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: fields `core_uobject_import`, `node_class_import`, and `pin_class_import` are never read[0m
  [1m[96m--> [0mcrates/ue5-graphs/src\binary_serializer.rs:44:5
   [1m[96m|[0m
[1m[96m38[0m [1m[96m|[0m pub struct GraphAssetBuilder {
   [1m[96m|[0m            [1m[96m-----------------[0m [1m[96mfields in this struct[0m
[1m[96m...[0m
[1m[96m44[0m [1m[96m|[0m     core_uobject_import: PackageIndex,
   [1m[96m|[0m     [1m[93m^^^^^^^^^^^^^^^^^^^[0m
[1m[96m45[0m [1m[96m|[0m     graph_class_import: PackageIndex,
[1m[96m46[0m [1m[96m|[0m     node_class_import: PackageIndex,
   [1m[96m|[0m     [1m[93m^^^^^^^^^^^^^^^^^[0m
[1m[96m47[0m [1m[96m|[0m     pin_class_import: PackageIndex,
   [1m[96m|[0m     [1m[93m^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: field `plugin_name` is never read[0m
  [1m[96m--> [0mcrates/ue5-graphs/src\factory_generator.rs:29:5
   [1m[96m|[0m
[1m[96m28[0m [1m[96m|[0m pub struct FactoryGenerator {
   [1m[96m|[0m            [1m[96m----------------[0m [1m[96mfield in this struct[0m
[1m[96m29[0m [1m[96m|[0m     plugin_name: String,
   [1m[96m|[0m     [1m[93m^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: 8 warnings emitted[0m

[32m[9,862 / 10,028][0m 5 actions running
    Compiling Rust rlib ue5-materials (8 files); 6s local, disk-cache
    Compiling Rust rlib ue5-graphs (15 files); 6s local, disk-cache
    Compiling Rust rlib ue5-gas (15 files); 2s local, disk-cache
    Compiling Rust rlib kain-python (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-stdlib-map (2 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib ue5-gas (15 files):
[1m[93mwarning[0m[1m[97m: unused import: `TraceTypeIR`[0m
 [1m[96m--> [0mcrates/ue5-gas/src\target_codegen.rs:5:39
  [1m[96m|[0m
[1m[96m5[0m [1m[96m|[0m use crate::target_ir::{TargetActorIR, TraceTypeIR};
  [1m[96m|[0m                                       [1m[93m^^^^^^^^^^^[0m
  [1m[96m|[0m
  [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_imports)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: unused import: `TargetFilter`[0m
 [1m[96m--> [0mcrates/ue5-gas/src\target_ir.rs:5:38
  [1m[96m|[0m
[1m[96m5[0m [1m[96m|[0m use kain_core::ast::{TargetActorDef, TargetFilter, TraceType};
  [1m[96m|[0m                                      [1m[93m^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: unused import: `DelegateIR`[0m
 [1m[96m--> [0mcrates/ue5-gas/src\task_codegen.rs:8:37
  [1m[96m|[0m
[1m[96m8[0m [1m[96m|[0m use crate::task_ir::{AbilityTaskIR, DelegateIR, DelegateTypeIR};
  [1m[96m|[0m                                     [1m[93m^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: unused import: `TaskDelegateDef`[0m
 [1m[96m--> [0mcrates/ue5-gas/src\task_ir.rs:8:38
  [1m[96m|[0m
[1m[96m8[0m [1m[96m|[0m use kain_core::ast::{AbilityTaskDef, TaskDelegateDef};
  [1m[96m|[0m                                      [1m[93m^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: ambiguous glob re-exports[0m
  [1m[96m--> [0mcrates/ue5-gas/src/lib.rs:17:9
   [1m[96m|[0m
[1m[96m17[0m [1m[96m|[0m pub use ability_ir::*;
   [1m[96m|[0m         [1m[93m^^^^^^^^^^^^^[0m [1m[93mthe name `FunctionIR` in the type namespace is first re-exported here[0m
[1m[96m18[0m [1m[96m|[0m pub use attribute_set_codegen::generate as generate_attribute_set;
[1m[96m19[0m [1m[96m|[0m pub use attribute_set_ir::*;
   [1m[96m|[0m         [1m[96m-------------------[0m [1m[96mbut the name `FunctionIR` in the type namespace is also re-exported here[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(ambiguous_glob_reexports)]` on by default

[1m[93mwarning[0m[1m[97m: ambiguous glob re-exports[0m
  [1m[96m--> [0mcrates/ue5-gas/src/lib.rs:19:9
   [1m[96m|[0m
[1m[96m19[0m [1m[96m|[0m pub use attribute_set_ir::*;
   [1m[96m|[0m         [1m[93m^^^^^^^^^^^^^^^^^^^[0m [1m[93mthe name `DelegateIR` in the type namespace is first re-exported here[0m
[1m[96m...[0m
[1m[96m29[0m [1m[96m|[0m pub use task_ir::*;
   [1m[96m|[0m         [1m[96m----------[0m [1m[96mbut the name `DelegateIR` in the type namespace is also re-exported here[0m

[1m[93mwarning[0m[1m[97m: ambiguous glob re-exports[0m
  [1m[96m--> [0mcrates/ue5-gas/src/lib.rs:21:9
   [1m[96m|[0m
[1m[96m21[0m [1m[96m|[0m pub use cue_ir::*;
   [1m[96m|[0m         [1m[93m^^^^^^^^^[0m [1m[93mthe name `StateFieldIR` in the type namespace is first re-exported here[0m
[1m[96m...[0m
[1m[96m29[0m [1m[96m|[0m pub use task_ir::*;
   [1m[96m|[0m         [1m[96m----------[0m [1m[96mbut the name `StateFieldIR` in the type namespace is also re-exported here[0m

[1m[93mwarning[0m[1m[97m: ambiguous glob re-exports[0m
  [1m[96m--> [0mcrates/ue5-gas/src/lib.rs:27:9
   [1m[96m|[0m
[1m[96m27[0m [1m[96m|[0m pub use target_ir::*;
   [1m[96m|[0m         [1m[93m^^^^^^^^^^^^[0m [1m[93mthe name `MethodIR` in the type namespace is first re-exported here[0m
[1m[96m28[0m [1m[96m|[0m pub use task_codegen::generate as generate_task;
[1m[96m29[0m [1m[96m|[0m pub use task_ir::*;
   [1m[96m|[0m         [1m[96m----------[0m [1m[96mbut the name `MethodIR` in the type namespace is also re-exported here[0m

[1m[93mwarning[0m[1m[97m: unused variable: `plugin_name`[0m
  [1m[96m--> [0mcrates/ue5-gas/src\task_codegen.rs:19:42
   [1m[96m|[0m
[1m[96m19[0m [1m[96m|[0m pub fn generate(task_ir: &AbilityTaskIR, plugin_name: &str) -> KainResult<AbilityTaskOutput> {
   [1m[96m|[0m                                          [1m[93m^^^^^^^^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_plugin_name`[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_variables)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: 9 warnings emitted[0m

[32m[9,865 / 10,028][0m 4 actions running
    Compiling Rust rlib ue5-materials (8 files); 6s local, disk-cache
    Compiling Rust rlib ue5-gas (15 files); 2s local, disk-cache
    Compiling Rust rlib kain-python (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-stdlib-map (2 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,871 / 10,028][0m 3 actions running
    Compiling Rust rlib ue5-materials (8 files); 7s local, disk-cache
    Compiling Rust rlib kain-python (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-stdlib-map (2 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib ue5-materials (8 files):
[1m[93mwarning[0m[1m[97m: unused imports: `CallArg` and `MaterialOutput`[0m
 [1m[96m--> [0mcrates/ue5-materials/src\ast_converter.rs:3:15
  [1m[96m|[0m
[1m[96m3[0m [1m[96m|[0m     BinaryOp, CallArg, Expr, MaterialGraphDef, MaterialInput, MaterialOutput, MaterialStatement,
  [1m[96m|[0m               [1m[93m^^^^^^^[0m                                         [1m[93m^^^^^^^^^^^^^^[0m
  [1m[96m|[0m
  [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_imports)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: unused import: `Import`[0m
  [1m[96m--> [0mcrates/ue5-materials/src\material_function_builder.rs:11:12
   [1m[96m|[0m
[1m[96m11[0m [1m[96m|[0m     Asset, Import,
   [1m[96m|[0m            [1m[93m^^^^^^[0m

[1m[93mwarning[0m[1m[97m: unused import: `Import`[0m
  [1m[96m--> [0mcrates/ue5-materials/src\material_serializer.rs:11:12
   [1m[96m|[0m
[1m[96m11[0m [1m[96m|[0m     Asset, Import,
   [1m[96m|[0m            [1m[93m^^^^^^[0m

[1m[93mwarning[0m[1m[97m: unused variable: `arg`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\ast_converter.rs:1405:21
     [1m[96m|[0m
[1m[96m1405[0m [1m[96m|[0m                 for arg in &attr.args {
     [1m[96m|[0m                     [1m[93m^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_arg`[0m
     [1m[96m|[0m
     [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_variables)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: variable does not need to be mutable[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:894:21
    [1m[96m|[0m
[1m[96m894[0m [1m[96m|[0m                 let mut result = format!(
    [1m[96m|[0m                     [1m[96m----[0m[1m[93m^^^^^^[0m
    [1m[96m|[0m                     [1m[96m|[0m
    [1m[96m|[0m                     [1m[96mhelp: remove this `mut`[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_mut)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: unused variable: `texture_input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:205:17
    [1m[96m|[0m
[1m[96m205[0m [1m[96m|[0m                 texture_input,
    [1m[96m|[0m                 [1m[93m^^^^^^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `texture_input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `uv_input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:206:17
    [1m[96m|[0m
[1m[96m206[0m [1m[96m|[0m                 uv_input,
    [1m[96m|[0m                 [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `uv_input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `default_texture`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:220:17
    [1m[96m|[0m
[1m[96m220[0m [1m[96m|[0m                 default_texture,
    [1m[96m|[0m                 [1m[93m^^^^^^^^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `default_texture: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `uv_input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:221:17
    [1m[96m|[0m
[1m[96m221[0m [1m[96m|[0m                 uv_input,
    [1m[96m|[0m                 [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `uv_input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:280:42
    [1m[96m|[0m
[1m[96m280[0m [1m[96m|[0m             MaterialNodeType::Multiply { a, b } => {
    [1m[96m|[0m                                          [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:280:45
    [1m[96m|[0m
[1m[96m280[0m [1m[96m|[0m             MaterialNodeType::Multiply { a, b } => {
    [1m[96m|[0m                                             [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:291:37
    [1m[96m|[0m
[1m[96m291[0m [1m[96m|[0m             MaterialNodeType::Add { a, b } => {
    [1m[96m|[0m                                     [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:291:40
    [1m[96m|[0m
[1m[96m291[0m [1m[96m|[0m             MaterialNodeType::Add { a, b } => {
    [1m[96m|[0m                                        [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:302:42
    [1m[96m|[0m
[1m[96m302[0m [1m[96m|[0m             MaterialNodeType::Subtract { a, b } => {
    [1m[96m|[0m                                          [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:302:45
    [1m[96m|[0m
[1m[96m302[0m [1m[96m|[0m             MaterialNodeType::Subtract { a, b } => {
    [1m[96m|[0m                                             [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:313:40
    [1m[96m|[0m
[1m[96m313[0m [1m[96m|[0m             MaterialNodeType::Divide { a, b } => {
    [1m[96m|[0m                                        [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:313:43
    [1m[96m|[0m
[1m[96m313[0m [1m[96m|[0m             MaterialNodeType::Divide { a, b } => {
    [1m[96m|[0m                                           [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:324:38
    [1m[96m|[0m
[1m[96m324[0m [1m[96m|[0m             MaterialNodeType::Lerp { a, b, alpha } => {
    [1m[96m|[0m                                      [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:324:41
    [1m[96m|[0m
[1m[96m324[0m [1m[96m|[0m             MaterialNodeType::Lerp { a, b, alpha } => {
    [1m[96m|[0m                                         [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `alpha`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:324:44
    [1m[96m|[0m
[1m[96m324[0m [1m[96m|[0m             MaterialNodeType::Lerp { a, b, alpha } => {
    [1m[96m|[0m                                            [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `alpha: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `base`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:335:39
    [1m[96m|[0m
[1m[96m335[0m [1m[96m|[0m             MaterialNodeType::Power { base, exponent } => {
    [1m[96m|[0m                                       [1m[93m^^^^[0m [1m[93mhelp: try ignoring the field: `base: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `exponent`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:335:45
    [1m[96m|[0m
[1m[96m335[0m [1m[96m|[0m             MaterialNodeType::Power { base, exponent } => {
    [1m[96m|[0m                                             [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `exponent: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:346:39
    [1m[96m|[0m
[1m[96m346[0m [1m[96m|[0m             MaterialNodeType::Clamp { input, min, max } => {
    [1m[96m|[0m                                       [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `min`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:346:46
    [1m[96m|[0m
[1m[96m346[0m [1m[96m|[0m             MaterialNodeType::Clamp { input, min, max } => {
    [1m[96m|[0m                                              [1m[93m^^^[0m [1m[93mhelp: try ignoring the field: `min: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `max`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:346:51
    [1m[96m|[0m
[1m[96m346[0m [1m[96m|[0m             MaterialNodeType::Clamp { input, min, max } => {
    [1m[96m|[0m                                                   [1m[93m^^^[0m [1m[93mhelp: try ignoring the field: `max: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `exponent`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:358:17
    [1m[96m|[0m
[1m[96m358[0m [1m[96m|[0m                 exponent,
    [1m[96m|[0m                 [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `exponent: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `base_reflect_fraction`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:359:17
    [1m[96m|[0m
[1m[96m359[0m [1m[96m|[0m                 base_reflect_fraction,
    [1m[96m|[0m                 [1m[93m^^^^^^^^^^^^^^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `base_reflect_fraction: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:371:47
    [1m[96m|[0m
[1m[96m371[0m [1m[96m|[0m             MaterialNodeType::ComponentMask { input, mask } => {
    [1m[96m|[0m                                               [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:496:37
    [1m[96m|[0m
[1m[96m496[0m [1m[96m|[0m             MaterialNodeType::Dot { a, b } => {
    [1m[96m|[0m                                     [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:496:40
    [1m[96m|[0m
[1m[96m496[0m [1m[96m|[0m             MaterialNodeType::Dot { a, b } => {
    [1m[96m|[0m                                        [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:507:40
    [1m[96m|[0m
[1m[96m507[0m [1m[96m|[0m             MaterialNodeType::Append { a, b } => {
    [1m[96m|[0m                                        [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:507:43
    [1m[96m|[0m
[1m[96m507[0m [1m[96m|[0m             MaterialNodeType::Append { a, b } => {
    [1m[96m|[0m                                           [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:586:39
    [1m[96m|[0m
[1m[96m586[0m [1m[96m|[0m             MaterialNodeType::Cross { a, b } => {
    [1m[96m|[0m                                       [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:586:42
    [1m[96m|[0m
[1m[96m586[0m [1m[96m|[0m             MaterialNodeType::Cross { a, b } => {
    [1m[96m|[0m                                          [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:597:43
    [1m[96m|[0m
[1m[96m597[0m [1m[96m|[0m             MaterialNodeType::Normalize { input } => {
    [1m[96m|[0m                                           [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:608:40
    [1m[96m|[0m
[1m[96m608[0m [1m[96m|[0m             MaterialNodeType::Length { input } => {
    [1m[96m|[0m                                        [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:619:42
    [1m[96m|[0m
[1m[96m619[0m [1m[96m|[0m             MaterialNodeType::Distance { a, b } => {
    [1m[96m|[0m                                          [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:619:45
    [1m[96m|[0m
[1m[96m619[0m [1m[96m|[0m             MaterialNodeType::Distance { a, b } => {
    [1m[96m|[0m                                             [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:630:37
    [1m[96m|[0m
[1m[96m630[0m [1m[96m|[0m             MaterialNodeType::Abs { input } => {
    [1m[96m|[0m                                     [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:641:37
    [1m[96m|[0m
[1m[96m641[0m [1m[96m|[0m             MaterialNodeType::Min { a, b } => {
    [1m[96m|[0m                                     [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:641:40
    [1m[96m|[0m
[1m[96m641[0m [1m[96m|[0m             MaterialNodeType::Min { a, b } => {
    [1m[96m|[0m                                        [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `a`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:652:37
    [1m[96m|[0m
[1m[96m652[0m [1m[96m|[0m             MaterialNodeType::Max { a, b } => {
    [1m[96m|[0m                                     [1m[93m^[0m [1m[93mhelp: try ignoring the field: `a: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `b`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:652:40
    [1m[96m|[0m
[1m[96m652[0m [1m[96m|[0m             MaterialNodeType::Max { a, b } => {
    [1m[96m|[0m                                        [1m[93m^[0m [1m[93mhelp: try ignoring the field: `b: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:663:42
    [1m[96m|[0m
[1m[96m663[0m [1m[96m|[0m             MaterialNodeType::Saturate { input } => {
    [1m[96m|[0m                                          [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:674:38
    [1m[96m|[0m
[1m[96m674[0m [1m[96m|[0m             MaterialNodeType::Frac { input } => {
    [1m[96m|[0m                                      [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:685:39
    [1m[96m|[0m
[1m[96m685[0m [1m[96m|[0m             MaterialNodeType::Floor { input } => {
    [1m[96m|[0m                                       [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:696:38
    [1m[96m|[0m
[1m[96m696[0m [1m[96m|[0m             MaterialNodeType::Ceil { input } => {
    [1m[96m|[0m                                      [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:707:39
    [1m[96m|[0m
[1m[96m707[0m [1m[96m|[0m             MaterialNodeType::Round { input } => {
    [1m[96m|[0m                                       [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:718:38
    [1m[96m|[0m
[1m[96m718[0m [1m[96m|[0m             MaterialNodeType::Sqrt { input } => {
    [1m[96m|[0m                                      [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:729:37
    [1m[96m|[0m
[1m[96m729[0m [1m[96m|[0m             MaterialNodeType::Exp { input } => {
    [1m[96m|[0m                                     [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:740:37
    [1m[96m|[0m
[1m[96m740[0m [1m[96m|[0m             MaterialNodeType::Log { input } => {
    [1m[96m|[0m                                     [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:751:38
    [1m[96m|[0m
[1m[96m751[0m [1m[96m|[0m             MaterialNodeType::Sine { input } => {
    [1m[96m|[0m                                      [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:762:40
    [1m[96m|[0m
[1m[96m762[0m [1m[96m|[0m             MaterialNodeType::Cosine { input } => {
    [1m[96m|[0m                                        [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `input_type_enum`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:806:25
    [1m[96m|[0m
[1m[96m806[0m [1m[96m|[0m                     let input_type_enum = match input.input_type {
    [1m[96m|[0m                         [1m[93m^^^^^^^^^^^^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_input_type_enum`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `uv_input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:833:17
    [1m[96m|[0m
[1m[96m833[0m [1m[96m|[0m                 uv_input,
    [1m[96m|[0m                 [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `uv_input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `offset_x`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:834:17
    [1m[96m|[0m
[1m[96m834[0m [1m[96m|[0m                 offset_x,
    [1m[96m|[0m                 [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `offset_x: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `offset_y`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:835:17
    [1m[96m|[0m
[1m[96m835[0m [1m[96m|[0m                 offset_y,
    [1m[96m|[0m                 [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `offset_y: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `uv_input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:852:17
    [1m[96m|[0m
[1m[96m852[0m [1m[96m|[0m                 uv_input,
    [1m[96m|[0m                 [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `uv_input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `scale_x`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:853:17
    [1m[96m|[0m
[1m[96m853[0m [1m[96m|[0m                 scale_x,
    [1m[96m|[0m                 [1m[93m^^^^^^^[0m [1m[93mhelp: try ignoring the field: `scale_x: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `scale_y`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:854:17
    [1m[96m|[0m
[1m[96m854[0m [1m[96m|[0m                 scale_y,
    [1m[96m|[0m                 [1m[93m^^^^^^^[0m [1m[93mhelp: try ignoring the field: `scale_y: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `uv_input`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:870:17
    [1m[96m|[0m
[1m[96m870[0m [1m[96m|[0m                 uv_input,
    [1m[96m|[0m                 [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `uv_input: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `angle`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:871:17
    [1m[96m|[0m
[1m[96m871[0m [1m[96m|[0m                 angle,
    [1m[96m|[0m                 [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `angle: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `center`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:872:17
    [1m[96m|[0m
[1m[96m872[0m [1m[96m|[0m                 center,
    [1m[96m|[0m                 [1m[93m^^^^^^[0m [1m[93mhelp: try ignoring the field: `center: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `inputs`[0m
   [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:890:17
    [1m[96m|[0m
[1m[96m890[0m [1m[96m|[0m                 inputs,
    [1m[96m|[0m                 [1m[93m^^^^^^[0m [1m[93mhelp: try ignoring the field: `inputs: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `base_layer`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:1020:17
     [1m[96m|[0m
[1m[96m1020[0m [1m[96m|[0m                 base_layer,
     [1m[96m|[0m                 [1m[93m^^^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `base_layer: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `blend_layer`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:1021:17
     [1m[96m|[0m
[1m[96m1021[0m [1m[96m|[0m                 blend_layer,
     [1m[96m|[0m                 [1m[93m^^^^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `blend_layer: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `alpha`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:1023:17
     [1m[96m|[0m
[1m[96m1023[0m [1m[96m|[0m                 alpha,
     [1m[96m|[0m                 [1m[93m^^^^^[0m [1m[93mhelp: try ignoring the field: `alpha: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `layers`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:1047:17
     [1m[96m|[0m
[1m[96m1047[0m [1m[96m|[0m                 layers,
     [1m[96m|[0m                 [1m[93m^^^^^^[0m [1m[93mhelp: try ignoring the field: `layers: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `blend_modes`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:1048:17
     [1m[96m|[0m
[1m[96m1048[0m [1m[96m|[0m                 blend_modes,
     [1m[96m|[0m                 [1m[93m^^^^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `blend_modes: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `alphas`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:1049:17
     [1m[96m|[0m
[1m[96m1049[0m [1m[96m|[0m                 alphas,
     [1m[96m|[0m                 [1m[93m^^^^^^[0m [1m[93mhelp: try ignoring the field: `alphas: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `offset_y`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:1215:21
     [1m[96m|[0m
[1m[96m1215[0m [1m[96m|[0m                     offset_y,
     [1m[96m|[0m                     [1m[93m^^^^^^^^[0m [1m[93mhelp: try ignoring the field: `offset_y: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `scale_y`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:1227:21
     [1m[96m|[0m
[1m[96m1227[0m [1m[96m|[0m                     scale_y,
     [1m[96m|[0m                     [1m[93m^^^^^^^[0m [1m[93mhelp: try ignoring the field: `scale_y: _`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `center`[0m
    [1m[96m--> [0mcrates/ue5-materials/src\material_factory.rs:1238:21
     [1m[96m|[0m
[1m[96m1238[0m [1m[96m|[0m                     center,
     [1m[96m|[0m                     [1m[93m^^^^^^[0m [1m[93mhelp: try ignoring the field: `center: _`[0m

[1m[93mwarning[0m[1m[97m: field `core_uobject_import` is never read[0m
  [1m[96m--> [0mcrates/ue5-materials/src\material_function_builder.rs:52:5
   [1m[96m|[0m
[1m[96m46[0m [1m[96m|[0m pub struct MaterialFunctionBuilder {
   [1m[96m|[0m            [1m[96m-----------------------[0m [1m[96mfield in this struct[0m
[1m[96m...[0m
[1m[96m52[0m [1m[96m|[0m     core_uobject_import: PackageIndex,
   [1m[96m|[0m     [1m[93m^^^^^^^^^^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(dead_code)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: field `core_uobject_import` is never read[0m
  [1m[96m--> [0mcrates/ue5-materials/src\material_serializer.rs:52:5
   [1m[96m|[0m
[1m[96m46[0m [1m[96m|[0m pub struct MaterialAssetBuilder {
   [1m[96m|[0m            [1m[96m--------------------[0m [1m[96mfield in this struct[0m
[1m[96m...[0m
[1m[96m52[0m [1m[96m|[0m     core_uobject_import: PackageIndex,
   [1m[96m|[0m     [1m[93m^^^^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: 75 warnings emitted[0m

[32m[9,871 / 10,028][0m 3 actions running
    Compiling Rust rlib ue5-materials (8 files); 7s local, disk-cache
    Compiling Rust rlib kain-python (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-stdlib-map (2 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,873 / 10,028][0m 3 actions, 2 running
    Compiling Rust rlib kain-python (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-stdlib-map (2 files); 1s local, disk-cache
    [Prepa] Compiling Rust rlib ue5-blueprints (7 files)

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,878 / 10,028][0m 3 actions running
    Compiling Rust rlib kain-python (1 file); 3s local, disk-cache
    Compiling Rust rlib kain-stdlib-map (2 files); 2s local, disk-cache
    Compiling Rust rlib ue5-blueprints (7 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,880 / 10,028][0m 4 actions, 3 running
    Compiling Rust rlib kain-python (1 file); 3s local, disk-cache
    Compiling Rust rlib kain-stdlib-map (2 files); 2s local, disk-cache
    Compiling Rust rlib ue5-blueprints (7 files); 0s local, disk-cache
    [Prepa] Compiling Rust rlib kain-crate-ffi (6 files)

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,880 / 10,028][0m 5 actions running
    Compiling Rust rlib kain-python (1 file); 3s local, disk-cache
    Compiling Rust rlib kain-stdlib-map (2 files); 2s local, disk-cache
    Compiling Rust rlib ue5-blueprints (7 files); 0s local, disk-cache
    Compiling Rust rlib kain-codebase (1 file); 0s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,881 / 10,028][0m 5 actions, 4 running
    Compiling Rust rlib kain-python (1 file); 4s local, disk-cache
    Compiling Rust rlib ue5-blueprints (7 files); 1s local, disk-cache
    Compiling Rust rlib kain-codebase (1 file); 0s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 0s local, disk-cache
    [Prepa] Compiling Rust rlib ue5 (32 files)

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,885 / 10,028][0m 5 actions running
    Compiling Rust rlib kain-python (1 file); 4s local, disk-cache
    Compiling Rust rlib ue5-blueprints (7 files); 1s local, disk-cache
    Compiling Rust rlib kain-codebase (1 file); 1s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 1s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,885 / 10,028][0m 6 actions running
    Compiling Rust rlib kain-python (1 file); 4s local, disk-cache
    Compiling Rust rlib ue5-blueprints (7 files); 1s local, disk-cache
    Compiling Rust rlib kain-codebase (1 file); 1s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 1s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 0s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,887 / 10,028][0m 5 actions running
    Compiling Rust rlib ue5-blueprints (7 files); 2s local, disk-cache
    Compiling Rust rlib kain-codebase (1 file); 1s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 1s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 0s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,890 / 10,028][0m 6 actions running
    Compiling Rust rlib ue5-blueprints (7 files); 2s local, disk-cache
    Compiling Rust rlib kain-codebase (1 file); 1s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 1s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 0s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 0s local, disk-cache
    Compiling Rust rlib gpu (6 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,892 / 10,028][0m 7 actions, 6 running
    Compiling Rust rlib ue5-blueprints (7 files); 2s local, disk-cache
    Compiling Rust rlib kain-codebase (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 2s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 1s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 0s local, disk-cache
    Compiling Rust rlib gpu (6 files); 0s local, disk-cache
    [Prepa] Compiling Rust rlib kain-sys-codegen (9 files)

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,896 / 10,028][0m 7 actions running
    Compiling Rust rlib ue5-blueprints (7 files); 2s local, disk-cache
    Compiling Rust rlib kain-codebase (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 2s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 1s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 1s local, disk-cache
    Compiling Rust rlib gpu (6 files); 0s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,899 / 10,028][0m 6 actions running
    Compiling Rust rlib ue5-blueprints (7 files); 3s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 2s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 1s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 1s local, disk-cache
    Compiling Rust rlib gpu (6 files); 1s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib ue5-blueprints (7 files):
[1m[93mwarning[0m[1m[97m: unused import: `PropertyValue`[0m
  [1m[96m--> [0mcrates/ue5-blueprints/src\writer.rs:15:51
   [1m[96m|[0m
[1m[96m15[0m [1m[96m|[0m use ue5_asset_utils::{ImportBuilder, PropertyDef, PropertyValue};
   [1m[96m|[0m                                                   [1m[93m^^^^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_imports)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: 1 warning emitted[0m

[32m[9,905 / 10,028][0m 6 actions running
    Compiling Rust rlib ue5-blueprints (7 files); 3s local, disk-cache
    Compiling Rust rlib kain-crate-ffi (6 files); 2s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 2s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 1s local, disk-cache
    Compiling Rust rlib gpu (6 files); 1s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,910 / 10,028][0m 6 actions running
    Compiling Rust rlib kain-crate-ffi (6 files); 3s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 2s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 2s local, disk-cache
    Compiling Rust rlib gpu (6 files); 1s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 1s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,910 / 10,028][0m 6 actions running
    Compiling Rust rlib kain-crate-ffi (6 files); 4s local, disk-cache
    Compiling Rust rlib ue5 (32 files); 3s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 3s local, disk-cache
    Compiling Rust rlib gpu (6 files); 2s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 2s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,911 / 10,028][0m 5 actions running
    Compiling Rust rlib ue5 (32 files); 3s local, disk-cache
    Compiling Rust rlib kain-gpu-runtime (4 files); 3s local, disk-cache
    Compiling Rust rlib gpu (6 files); 2s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 2s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,912 / 10,028][0m 4 actions running
    Compiling Rust rlib ue5 (32 files); 3s local, disk-cache
    Compiling Rust rlib gpu (6 files); 3s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 2s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,912 / 10,028][0m 4 actions running
    Compiling Rust rlib ue5 (32 files); 3s local, disk-cache
    Compiling Rust rlib gpu (6 files); 3s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 2s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,912 / 10,028][0m 4 actions running
    Compiling Rust rlib ue5 (32 files); 4s local, disk-cache
    Compiling Rust rlib gpu (6 files); 3s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 3s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 2s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,912 / 10,028][0m 4 actions running
    Compiling Rust rlib ue5 (32 files); 4s local, disk-cache
    Compiling Rust rlib gpu (6 files); 3s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 3s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 2s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,926 / 10,028][0m 4 actions running
    Compiling Rust rlib ue5 (32 files); 5s local, disk-cache
    Compiling Rust rlib gpu (6 files); 4s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 4s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 3s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K
[1A[K[32m[9,929 / 10,028][0m 3 actions running
    Compiling Rust rlib ue5 (32 files); 5s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 4s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 3s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,936 / 10,028][0m 3 actions running
    Compiling Rust rlib ue5 (32 files); 6s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 5s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 4s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,936 / 10,028][0m 3 actions running
    Compiling Rust rlib ue5 (32 files); 7s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 6s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 5s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,936 / 10,028][0m 3 actions running
    Compiling Rust rlib ue5 (32 files); 8s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 7s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 6s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,936 / 10,028][0m 3 actions running
    Compiling Rust rlib ue5 (32 files); 9s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 8s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 7s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,936 / 10,028][0m 3 actions running
    Compiling Rust rlib ue5 (32 files); 10s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 9s local, disk-cache
    Compiling Rust rlib kain-import (26 files); 8s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,937 / 10,028][0m 2 actions running
    Compiling Rust rlib ue5 (32 files); 11s local, disk-cache
    Compiling Rust rlib kain-sys-codegen (9 files); 10s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,946 / 10,028][0m //crates/sys-codegen:kain-sys-codegen; 11s local, disk-cache

[1A[K[32m[9,947 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-sys-codegen (9 files); 12s local, disk-cache
    Compiling Rust rlib kain-c-ffi (6 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,952 / 10,028][0m //crates/c-ffi:kain-c-ffi; 0s local, disk-cache

[1A[K[32m[9,955 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-c-ffi (6 files); 0s local, disk-cache
    Compiling Rust rlib ue5-config (7 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,955 / 10,028][0m 3 actions, 2 running
    Compiling Rust rlib kain-c-ffi (6 files); 1s local, disk-cache
    Compiling Rust rlib ue5-config (7 files); 0s local, disk-cache
    [Prepa] Compiling Rust rlib ue5-editor (13 files)

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,959 / 10,028][0m 3 actions running
    Compiling Rust rlib kain-c-ffi (6 files); 1s local, disk-cache
    Compiling Rust rlib ue5-config (7 files); 0s local, disk-cache
    Compiling Rust rlib ue5-editor (13 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib ue5-config (7 files):
[1m[93mwarning[0m[1m[97m: unused import: `Attribute`[0m
 [1m[96m--> [0mcrates/ue5-config/src\parser.rs:8:22
  [1m[96m|[0m
[1m[96m8[0m [1m[96m|[0m use kain_core::ast::{Attribute, Expr, Field, Struct};
  [1m[96m|[0m                      [1m[93m^^^^^^^^^[0m
  [1m[96m|[0m
  [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_imports)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: 1 warning emitted[0m

[32m[9,969 / 10,028][0m 3 actions running
    Compiling Rust rlib kain-c-ffi (6 files); 2s local, disk-cache
    Compiling Rust rlib ue5-config (7 files); 1s local, disk-cache
    Compiling Rust rlib ue5-editor (13 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K
[1A[K[32m[9,971 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-c-ffi (6 files); 2s local, disk-cache
    Compiling Rust rlib ue5-editor (13 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,971 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-c-ffi (6 files); 3s local, disk-cache
    Compiling Rust rlib ue5-editor (13 files); 2s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[9,972 / 10,028][0m //crates/ue5-editor:ue5-editor; 2s local, disk-cache

[1A[K[32m[9,982 / 10,028][0m checking cached actions

[1A[K[32m[9,991 / 10,028][0m [Prepa] Compiling Rust rlib kain-driver (4 files)

[1A[K[32m[9,991 / 10,028][0m //crates/driver:kain-driver; 0s local, disk-cache

[1A[K[32m[9,991 / 10,028][0m //crates/driver:kain-driver; 1s local, disk-cache

[1A[K[32m[9,991 / 10,028][0m //crates/driver:kain-driver; 2s local, disk-cache

[1A[K[32m[9,991 / 10,028][0m //crates/driver:kain-driver; 3s local, disk-cache

[1A[K[32m[9,991 / 10,028][0m //crates/driver:kain-driver; 4s local, disk-cache

[1A[K[32m[9,992 / 10,028][0m checking cached actions

[1A[K[32m[9,996 / 10,028][0m [Prepa] Compiling Rust rlib kain-check (1 file)

[1A[K[32m[10,000 / 10,028][0m //crates/check:kain-check; 0s local, disk-cache

[1A[K[32m[10,000 / 10,028][0m 2 actions, 1 running
    Compiling Rust rlib kain-check (1 file); 0s local, disk-cache
    [Prepa] Compiling Rust rlib kain-omni (2 files)

[1A[K
[1A[K
[1A[K[32m[10,000 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-check (1 file); 0s local, disk-cache
    Compiling Rust rlib kain-omni (2 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[10,004 / 10,028][0m 2 actions, 1 running
    Compiling Rust rlib kain-omni (2 files); 1s local, disk-cache
    [Prepa] Compiling Rust rlib kain-test (1 file)

[1A[K
[1A[K
[1A[K[32m[10,004 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-omni (2 files); 1s local, disk-cache
    Compiling Rust rlib kain-test (1 file); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[10,004 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-omni (2 files); 2s local, disk-cache
    Compiling Rust rlib kain-test (1 file); 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[10,007 / 10,028][0m //crates/omni:kain-omni; 2s local, disk-cache

[1A[K[32m[10,008 / 10,028][0m checking cached actions

[1A[K[32m[10,012 / 10,028][0m checking cached actions

[1A[K[32m[10,013 / 10,028][0m [Prepa] Compiling Rust rlib kain-host (2 files)

[1A[K[32m[10,013 / 10,028][0m //crates/host:kain-host; 0s local, disk-cache

[1A[K[32m[10,013 / 10,028][0m //crates/host:kain-host; 1s local, disk-cache

[1A[K[32m[10,013 / 10,028][0m //crates/host:kain-host; 2s local, disk-cache

[1A[K[32m[10,014 / 10,028][0m checking cached actions

[1A[K[32m[10,018 / 10,028][0m [Prepa] Compiling Rust rlib kain-run (1 file)

[1A[K[32m[10,018 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-run (1 file); 0s local, disk-cache
    Compiling Rust rlib kain-build (2 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[10,018 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-run (1 file); 1s local, disk-cache
    Compiling Rust rlib kain-build (2 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[10,018 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-run (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-build (2 files); 2s local, disk-cache

[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib kain-run (1 file):
[1m[93mwarning[0m[1m[97m: function `load_run_section` is never used[0m
    [1m[96m--> [0mcrates/run/src/lib.rs:2052:4
     [1m[96m|[0m
[1m[96m2052[0m [1m[96m|[0m fn load_run_section(path: &Path) -> RunResult<KainRunSection> {
     [1m[96m|[0m    [1m[93m^^^^^^^^^^^^^^^^[0m
     [1m[96m|[0m
     [1m[96m= [0m[1m[97mnote[0m: `#[warn(dead_code)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: 1 warning emitted[0m

[32m[10,018 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-run (1 file); 2s local, disk-cache
    Compiling Rust rlib kain-build (2 files); 2s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[10,021 / 10,028][0m 2 actions, 1 running
    Compiling Rust rlib kain-build (2 files); 2s local, disk-cache
    [Sched] Compiling Rust rlib kain-repl (10 files)

[1A[K
[1A[K
[1A[K[32m[10,021 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-build (2 files); 2s local, disk-cache
    Compiling Rust rlib kain-repl (10 files); 0s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[10,021 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-build (2 files); 3s local, disk-cache
    Compiling Rust rlib kain-repl (10 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32mINFO: [0mFrom Compiling Rust rlib kain-build (2 files):
[1m[93mwarning[0m[1m[97m: struct `ProjectManifest` is never constructed[0m
   [1m[96m--> [0mcrates/build/src\workspace.rs:666:8
    [1m[96m|[0m
[1m[96m666[0m [1m[96m|[0m struct ProjectManifest {
    [1m[96m|[0m        [1m[93m^^^^^^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mnote[0m: `#[warn(dead_code)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: struct `ProjectPackageSection` is never constructed[0m
   [1m[96m--> [0mcrates/build/src\workspace.rs:674:8
    [1m[96m|[0m
[1m[96m674[0m [1m[96m|[0m struct ProjectPackageSection {
    [1m[96m|[0m        [1m[93m^^^^^^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: struct `ProjectBuildSection` is never constructed[0m
   [1m[96m--> [0mcrates/build/src\workspace.rs:680:8
    [1m[96m|[0m
[1m[96m680[0m [1m[96m|[0m struct ProjectBuildSection {
    [1m[96m|[0m        [1m[93m^^^^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: function `load_project_manifest` is never used[0m
    [1m[96m--> [0mcrates/build/src\workspace.rs:4935:4
     [1m[96m|[0m
[1m[96m4935[0m [1m[96m|[0m fn load_project_manifest(path: &Path) -> BuildResult<ProjectManifest> {
     [1m[96m|[0m    [1m[93m^^^^^^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: 4 warnings emitted[0m

[32m[10,021 / 10,028][0m 2 actions running
    Compiling Rust rlib kain-build (2 files); 3s local, disk-cache
    Compiling Rust rlib kain-repl (10 files); 1s local, disk-cache

[1A[K
[1A[K
[1A[K[32m[10,023 / 10,028][0m //crates/repl:kain-repl; 1s local, disk-cache

[1A[K[32m[10,024 / 10,028][0m checking cached actions

[1A[K[32m[10,025 / 10,028][0m [Prepa] Compiling Rust rlib cli (51 files)

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 0s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 1s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 2s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 3s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 4s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 5s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 6s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 7s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 8s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 9s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 9s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 10s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 11s local, disk-cache

[1A[K[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 12s local, disk-cache

[1A[K[32mINFO: [0mFrom Compiling Rust rlib cli (51 files):
[1m[93mwarning[0m[1m[97m: unused import: `MaterialInputType`[0m
    [1m[96m--> [0mcrates/cli/src\packager\ue5_pipeline.rs:2688:66
     [1m[96m|[0m
[1m[96m2688[0m [1m[96m|[0m         BlendMode, MaterialDomain, MaterialGraph, MaterialInput, MaterialInputType, MaterialNode,
     [1m[96m|[0m                                                                  [1m[93m^^^^^^^^^^^^^^^^^[0m
     [1m[96m|[0m
     [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_imports)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: unused import: `MaterialInputType`[0m
    [1m[96m--> [0mcrates/cli/src\packager\ue5_pipeline.rs:3209:50
     [1m[96m|[0m
[1m[96m3209[0m [1m[96m|[0m         MaterialFunction, MaterialFunctionInput, MaterialInputType, MaterialNode, MaterialNodeType,
     [1m[96m|[0m                                                  [1m[93m^^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: use of deprecated function `ue5_graphs::convert_runtime_graph`: Use convert_graph_runtime_to_ir instead[0m
   [1m[96m--> [0mcrates/cli/src\packager\ue5_pipeline.rs:393:35
    [1m[96m|[0m
[1m[96m393[0m [1m[96m|[0m                 match ue5_graphs::convert_runtime_graph(graph_def) {
    [1m[96m|[0m                                   [1m[93m^^^^^^^^^^^^^^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mnote[0m: `#[warn(deprecated)]` on by default

[1m[93mwarning[0m[1m[97m: unused variable: `header_path`[0m
    [1m[96m--> [0mcrates/cli/src\kain_launcher.rs:1231:33
     [1m[96m|[0m
[1m[96m1231[0m [1m[96m|[0m [1m[96m...[0m                   let header_path = output_path.with_extension("h");
     [1m[96m|[0m                           [1m[93m^^^^^^^^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_header_path`[0m
     [1m[96m|[0m
     [1m[96m= [0m[1m[97mnote[0m: `#[warn(unused_variables)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: unused variable: `cpp_path`[0m
    [1m[96m--> [0mcrates/cli/src\kain_launcher.rs:1250:33
     [1m[96m|[0m
[1m[96m1250[0m [1m[96m|[0m [1m[96m...[0m                   let cpp_path = output_path.with_extension("cpp");
     [1m[96m|[0m                           [1m[93m^^^^^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_cpp_path`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `header_path`[0m
    [1m[96m--> [0mcrates/cli/src\kain_launcher.rs:1288:33
     [1m[96m|[0m
[1m[96m1288[0m [1m[96m|[0m [1m[96m...[0m                   let header_path = output_path.with_extension("h");
     [1m[96m|[0m                           [1m[93m^^^^^^^^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_header_path`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `source_path`[0m
    [1m[96m--> [0mcrates/cli/src\kain_launcher.rs:1305:33
     [1m[96m|[0m
[1m[96m1305[0m [1m[96m|[0m [1m[96m...[0m                   let source_path = output_path.with_extension("cpp");
     [1m[96m|[0m                           [1m[93m^^^^^^^^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_source_path`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `shader_path`[0m
    [1m[96m--> [0mcrates/cli/src\kain_launcher.rs:1328:41
     [1m[96m|[0m
[1m[96m1328[0m [1m[96m|[0m [1m[96m...[0m                   let shader_path = output_path.with_file_name(filename);
     [1m[96m|[0m                           [1m[93m^^^^^^^^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_shader_path`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `header_path`[0m
    [1m[96m--> [0mcrates/cli/src\kain_launcher.rs:1363:33
     [1m[96m|[0m
[1m[96m1363[0m [1m[96m|[0m [1m[96m...[0m                   let header_path = output_path.with_extension("h");
     [1m[96m|[0m                           [1m[93m^^^^^^^^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_header_path`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `source_path`[0m
    [1m[96m--> [0mcrates/cli/src\kain_launcher.rs:1380:33
     [1m[96m|[0m
[1m[96m1380[0m [1m[96m|[0m [1m[96m...[0m                   let source_path = output_path.with_extension("cpp");
     [1m[96m|[0m                           [1m[93m^^^^^^^^^^^[0m [1m[93mhelp: if this is intentional, prefix it with an underscore: `_source_path`[0m

[1m[93mwarning[0m[1m[97m: unused variable: `name`[0m
   [1m[96m--> [0mcrates/cli/src\packager\codegen.rs:696:58
    [1m[96m|[0m
[1m[96m696[0m [1m[96m|[0m                     if let kain_core::ast::Type::Named { name, .. } = ty {
    [1m[96m|[0m                                                          [1m[93m^^^^[0m [1m[93mhelp: try ignoring the field: `name: _`[0m

[1m[93mwarning[0m[1m[97m: irrefutable `if let` pattern[0m
    [1m[96m--> [0mcrates/cli/src\packager\ue5_pipeline.rs:2806:12
     [1m[96m|[0m
[1m[96m2806[0m [1m[96m|[0m         if let kain_core::ast::MaterialStatement::Let { name, value, .. } = stmt {
     [1m[96m|[0m            [1m[93m^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^[0m
     [1m[96m|[0m
     [1m[96m= [0m[1m[97mnote[0m: this pattern will always match, so the `if let` is useless
     [1m[96m= [0m[1m[97mhelp[0m: consider replacing the `if let` with a `let`
     [1m[96m= [0m[1m[97mnote[0m: `#[warn(irrefutable_let_patterns)]` on by default

[1m[93mwarning[0m[1m[97m: irrefutable `if let` pattern[0m
    [1m[96m--> [0mcrates/cli/src\packager\ue5_pipeline.rs:3252:12
     [1m[96m|[0m
[1m[96m3252[0m [1m[96m|[0m         if let kain_core::ast::MaterialStatement::Let { name, value, .. } = stmt {
     [1m[96m|[0m            [1m[93m^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^[0m
     [1m[96m|[0m
     [1m[96m= [0m[1m[97mnote[0m: this pattern will always match, so the `if let` is useless
     [1m[96m= [0m[1m[97mhelp[0m: consider replacing the `if let` with a `let`

[1m[93mwarning[0m[1m[97m: function `enhance_codegen_result` is never used[0m
  [1m[96m--> [0mcrates/cli/src\packager\codegen.rs:16:4
   [1m[96m|[0m
[1m[96m16[0m [1m[96m|[0m fn enhance_codegen_result<T>(
   [1m[96m|[0m    [1m[93m^^^^^^^^^^^^^^^^^^^^^^[0m
   [1m[96m|[0m
   [1m[96m= [0m[1m[97mnote[0m: `#[warn(dead_code)]` (part of `#[warn(unused)]`) on by default

[1m[93mwarning[0m[1m[97m: field `checksum` is never read[0m
   [1m[96m--> [0mcrates/cli/src\packager\config.rs:335:9
    [1m[96m|[0m
[1m[96m333[0m [1m[96m|[0m pub(crate) struct PackageVersion {
    [1m[96m|[0m                   [1m[96m--------------[0m [1m[96mfield in this struct[0m
[1m[96m334[0m [1m[96m|[0m     pub url: String,
[1m[96m335[0m [1m[96m|[0m     pub checksum: String,
    [1m[96m|[0m         [1m[93m^^^^^^^^[0m
    [1m[96m|[0m
    [1m[96m= [0m[1m[97mnote[0m: `PackageVersion` has a derived impl for the trait `Debug`, but this is intentionally ignored during dead code analysis

[1m[93mwarning[0m[1m[97m: function `enhance_codegen_result` is never used[0m
  [1m[96m--> [0mcrates/cli/src\packager\ue5_pipeline.rs:11:4
   [1m[96m|[0m
[1m[96m11[0m [1m[96m|[0m fn enhance_codegen_result<T>(
   [1m[96m|[0m    [1m[93m^^^^^^^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: function `resolve_workspace_root` is never used[0m
   [1m[96m--> [0mcrates/cli/src\rust_build.rs:342:4
    [1m[96m|[0m
[1m[96m342[0m [1m[96m|[0m fn resolve_workspace_root() -> Result<PathBuf, KainError> {
    [1m[96m|[0m    [1m[93m^^^^^^^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: function `diff_paths` is never used[0m
   [1m[96m--> [0mcrates/cli/src\rust_build.rs:353:4
    [1m[96m|[0m
[1m[96m353[0m [1m[96m|[0m fn diff_paths(path: &Path, base: &Path) -> Option<PathBuf> {
    [1m[96m|[0m    [1m[93m^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: function `shared_path_prefix_len` is never used[0m
   [1m[96m--> [0mcrates/cli/src\rust_build.rs:383:4
    [1m[96m|[0m
[1m[96m383[0m [1m[96m|[0m fn shared_path_prefix_len(path: &[Component<'_>], base: &[Component<'_>]) -> usize {
    [1m[96m|[0m    [1m[93m^^^^^^^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: function `write_nested_item_stub` is never used[0m
    [1m[96m--> [0mcrates/cli/src\selfhost.rs:5440:4
     [1m[96m|[0m
[1m[96m5440[0m [1m[96m|[0m fn write_nested_item_stub(output: &mut String, item: &Item, indent: usize) -> KainResult<()> {
     [1m[96m|[0m    [1m[93m^^^^^^^^^^^^^^^^^^^^^^[0m

[1m[93mwarning[0m[1m[97m: 20 warnings emitted[0m

[32m[10,025 / 10,028][0m Compiling Rust rlib cli (51 files); 13s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m checking cached actions

[1A[K[32m[10,026 / 10,028][0m checking cached actions

[1A[K[32m[10,026 / 10,028][0m [Prepa] Compiling Rust bin kain (51 files); 6s

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 0s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 1s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 2s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 3s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 4s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 4s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 6s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 7s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 8s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 9s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 10s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 11s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 12s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 13s local, disk-cache

[1A[K[32m[10,026 / 10,028][0m Compiling Rust bin kain (51 files); 14s local, disk-cache

[1A[K[32m[10,027 / 10,028][0m [Prepa] runfiles for //crates/cli:kain

[1A[K[32mINFO: [0mFound 1 target...
[32m[10,028 / 10,028][0m no actions running

[1A[KTarget //crates/cli:kain up-to-date:
[32m[10,028 / 10,028][0m no actions running

[1A[K  F:/_b/output-user-root/n2kwlvv2/execroot/_main/bazel-out/x64_windows-dbg/bin/crates/cli/kain.exe
[32m[10,028 / 10,028][0m no actions running

[1A[K[32mINFO: [0mElapsed time: 173.707s, Critical Path: 108.38s
[32m[10,028 / 10,028][0m no actions running

[1A[K[32mINFO: [0m190 processes: 9838 action cache hit, 86 internal, 104 local.
[32m[10,028 / 10,028][0m no actions running

[1A[K[32mINFO: [0mBuild completed successfully, 190 total actions
[32mINFO:[0m 

[1A[K[32mINFO:[0m 

[1A[K[32mINFO:[0m 

[1A[K[32mINFO:[0m 
Waiting for remote cache: 1 upload

[1A[K
[1A[K[32mINFO:[0m 
Waiting for remote cache: 1 upload

[1A[K
[1A[K[32mINFO:[0m 
Waiting for remote cache: 1 upload; 3s

[1A[K
[1A[K[32mINFO:[0m 
Waiting for remote cache: 1 upload; 4s

[1A[K
[1A[K[32mINFO:[0m 
Waiting for remote cache: 1 upload; 5s

[1A[K
[1A[K[32mINFO:[0m 
Waiting for remote cache: 1 upload; 5s

[1A[K
[1A[K[0m
 Check passed: 1/1 passed
```

---

### borrow_mutability_conflict.kn -- PASS

**Header:** // ERROR: Mutable/immutable conflict

```
Check passed: 1/1 passed
```

---

### borrow_use_after_move.kn -- PASS

**Header:** // ERROR: Use after move

```
Check passed: 1/1 passed
```

---

### effect_pure_calls_io.kn -- FAIL (exit 1)

**Header:** // ERROR: Pure function calling IO

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\effect_pure_calls_io.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...e_calls_io.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error[TYPE:KAIN-TYPE-0005]: function 'read_file' shadows an existing global symbol
  --> X:\crates\error-semantic\scratch\effect_pure_calls_io.kn:2:1
   |
  2 | fn read_file() -> String with IO:
   | ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ shadowed global symbol 'read_file'
   |
   = help: Pick a distinct name, or import the existing symbol with an alias to keep both visible.
   = help: choose a distinct local name, or import the builtin under an alias.
   = reference: types/shadowed-builtin
```

---

### import_unresolved.kn -- PASS

**Header:** // ERROR: Import path does not exist

```
Check passed: 1/1 passed
```

---

### multi_error.kn -- FAIL (exit 1)

**Header:** // ERROR: Multiple errors in one file

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\multi_error.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...ulti_error.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error: 1 error(s) found:

--- [1/1] ---

error[PARSE:KAIN-PARSE-0002]: Expected '=', got newline
  --> X:\crates\error-semantic\scratch\multi_error.kn:5:10
   |
  5 |     let c
   |          ^ expected '=' here
   |
   = note: Parser was in a grammar state that accepts '=' but saw newline instead.
   = help: Check the token immediately before this point; most parse errors are caused by the previous unfinished 
construct.
   = help: insert the missing token before continuing, or restructure the
   = reference: parser/expected-token
```

---

### parse_mismatched_delim.kn -- FAIL (exit 1)

**Header:** // ERROR: Mismatched delimiter - [ opened, } closed

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\parse_mismatched_delim.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...ched_delim.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error: 1 error(s) found:

--- [1/1] ---

error[PARSE:KAIN-PARSE-0001]: Unexpected token: '}'
  --> X:\crates\error-semantic\scratch\parse_mismatched_delim.kn:3:23
   |
  3 |     let arr = [1, 2, 3}
   |                       ^ parser stopped here
   |
   = reference: parser/general
```

---

### parse_missing_colon.kn -- FAIL (exit 1)

**Header:** // ERROR: Missing colon after fn header

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\parse_missing_colon.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...sing_colon.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error: 1 error(s) found:

--- [1/1] ---

error[PARSE:KAIN-PARSE-0005]: Missing ':' before line break
  --> X:\crates\error-semantic\scratch\parse_missing_colon.kn:2:9
   |
  2 | fn main()
   |         ^ this header or declaration ended without ':'
   |
   = label 2:10: the next line started while ':' was still expected
   = note: Expected ':' before newline: Kain block headers and declarations must end with ':'.
   = note: If this was meant to be a continued expression, wrap it in parentheses or keep it on one logical line.
   = help: Look immediately before the highlighted line break; the following line may only be where recovery noticed 
the damage.
   = fix-it X:\crates\error-semantic\scratch\parse_missing_colon.kn:2:10: insert ':' at the end of the header -> ":"
   = help: :
   = reference: parser/missing-delimiter-before-newline
```

---

### parse_reserved_ident.kn -- FAIL (exit 1)

**Header:** // ERROR: Reserved identifier used as name

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\parse_reserved_ident.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...rved_ident.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error: 3 error(s) found:

--- [1/3] ---

error[PARSE:KAIN-PARSE-0001]: Expected pattern (identifier, integer, string, tuple, or array), found keyword 'fn'
  --> X:\crates\error-semantic\scratch\parse_reserved_ident.kn:3:9
   |
  3 |     let fn = 5
   |         ^^ parser stopped here
   |
   = reference: parser/general

--- [2/3] ---

error[PARSE:KAIN-PARSE-0002]: Expected identifier, got '='
  --> X:\crates\error-semantic\scratch\parse_reserved_ident.kn:3:12
   |
  3 |     let fn = 5
   |            ^ parser stopped here
   |
   = help: insert the missing token before continuing, or restructure the
   = reference: parser/expected-token

--- [3/3] ---

error[PARSE:KAIN-PARSE-0002]: Expected identifier, got newline
  --> X:\crates\error-semantic\scratch\parse_reserved_ident.kn:4:14
   |
  4 |     return fn
   |              ^ parser stopped here
   |
   = help: insert the missing token before continuing, or restructure the
   = reference: parser/expected-token
```

---

### parse_unclosed_paren.kn -- FAIL (exit 1)

**Header:** // ERROR: Unclosed parenthesis

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\parse_unclosed_paren.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...osed_paren.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error: 1 error(s) found:

--- [1/1] ---

error[PARSE:KAIN-PARSE-0002]: Expected ')', got keyword 'return'
  --> X:\crates\error-semantic\scratch\parse_unclosed_paren.kn:4:5
   |
  4 |     return x
   |     ^^^^^^ expected ')' here
   |
   = note: Parser was in a grammar state that accepts ')' but saw keyword 'return' instead.
   = help: Check the token immediately before this point; most parse errors are caused by the previous unfinished 
construct.
   = help: insert the missing token before continuing, or restructure the
   = reference: parser/expected-token
```

---

### parse_unexpected_token.kn -- FAIL (exit 1)

**Header:** // ERROR: Unexpected token

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\parse_unexpected_token.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...cted_token.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error: 2 error(s) found:

--- [1/2] ---

error[PARSE:KAIN-PARSE-0001]: Expected attribute name, got '@'
  --> X:\crates\error-semantic\scratch\parse_unexpected_token.kn:4:6
   |
  4 |     @@
   |      ^ parser stopped here
   |
   = reference: parser/general

--- [2/2] ---

error[PARSE:KAIN-PARSE-0001]: Expected attribute name, got newline
  --> X:\crates\error-semantic\scratch\parse_unexpected_token.kn:4:7
   |
  4 |     @@
   |       ^ parser stopped here
   |
   = reference: parser/general
```

---

### type_cyclic.kn -- PASS

**Header:** // ERROR: Cyclic type definition

```
Check passed: 1/1 passed
```

---

### type_duplicate_symbol.kn -- FAIL (exit 1)

**Header:** // ERROR: Duplicate function definition

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\type_duplicate_symbol.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...ate_symbol.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error[TYPE:KAIN-TYPE-0004]: function 'helper' collides with an existing global from function
  --> X:\crates\error-semantic\scratch\type_duplicate_symbol.kn:5:1
   |
  5 | fn helper() -> Int:
   | ^^^^^^^^^^^^^^^^^^^ redeclared global 'helper'
   |
   = label 2:1: previous function 'helper' is here (X:\crates\error-semantic\scratch\type_duplicate_symbol.kn:2:1)
   = help: Rename one of the declarations, or import the older symbol under an explicit alias.
   = help: rename one declaration or use an explicit alias on import.
   = reference: types/duplicate-symbol
```

---

### type_inexhaustive_match.kn -- PASS

**Header:** // ERROR: Pattern match inexhaustive

```
Check passed: 1/1 passed
```

---

### type_mismatch.kn -- FAIL (exit 1)

**Header:** // ERROR: Type mismatch - assigning string to Int

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\type_mismatch.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...e_mismatch.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error[TYPE:KAIN-TYPE-0001]: let binding expected Int, found String
  --> X:\crates\error-semantic\scratch\type_mismatch.kn:3:5
   |
  3 |     let x: Int = "hello"
   |     ^^^^^^^^^^^^^^^^^^^^ typechecker stopped here
   |
   = reference: types/general
```

---

### type_missing_annotation.kn -- FAIL (exit 1)

**Header:** // ERROR: Missing type annotation

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\type_missing_annotation.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...annotation.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error: 1 error(s) found:

--- [1/1] ---

error[PARSE:KAIN-PARSE-0002]: Expected '=', got newline
  --> X:\crates\error-semantic\scratch\type_missing_annotation.kn:3:10
   |
  3 |     let x
   |          ^ expected '=' here
   |
   = note: Parser was in a grammar state that accepts '=' but saw newline instead.
   = help: Check the token immediately before this point; most parse errors are caused by the previous unfinished 
construct.
   = help: insert the missing token before continuing, or restructure the
   = reference: parser/expected-token
```

---

### type_return_mismatch.kn -- FAIL (exit 1)

**Header:** // ERROR: fn returning nothing when Int expected

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\type_return_mismatch.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...n_mismatch.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error[TYPE:KAIN-TYPE-0001]: return expected Int, found Unit
  --> X:\crates\error-semantic\scratch\type_return_mismatch.kn:3:5
   |
  3 |     return
   |     ^^^^^^ typechecker stopped here
   |
   = reference: types/general
```

---

### type_unknown_identifier.kn -- FAIL (exit 1)

**Header:** // ERROR: Unknown identifier - typo in function name

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\type_unknown_identifier.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...identifier.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error[TYPE:KAIN-TYPE-0002]: Unknown identifier 'prntln'
  --> X:\crates\error-semantic\scratch\type_unknown_identifier.kn:3:18
   |
  3 |     let result = prntln("hello")
   |                  ^^^^^^ 'prntln' is not in scope
   |
   = help: Check for a misspelling, add the missing import, or explicitly bridge the value into Kain.
   = help: check spelling, add a `use` statement, or bridge the host symbol.
   = reference: types/unknown-identifier
```

---

### type_wrong_arg_count.kn -- PASS

**Header:** // ERROR: Calling function with wrong arg count

```
Check passed: 1/1 passed
```

---

### typo_math.kn -- FAIL (exit 1)

**Header:** // @expected_code: KAIN-TYPE-0002

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\typo_math.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...\typo_math.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error[TYPE:KAIN-TYPE-0002]: Unknown identifier 'mix_scalr'
  --> X:\crates\error-semantic\scratch\typo_math.kn:6:18
   |
  6 |     let result = mix_scalr(42)
   |                  ^^^^^^^^^ 'mix_scalr' is not in scope
   |
   = help: Check for a misspelling, add the missing import, or explicitly bridge the value into Kain.
   = help: check spelling, add a `use` statement, or bridge the host symbol.
   = reference: types/unknown-identifier
```

---

### world_missing_surface.kn -- FAIL (exit 1)

**Header:** // ERROR: World missing surface

```
Check failed: 0/1 passed
kain.exe :   X:\crates\error-semantic\scratch\world_missing_surface.kn: 
At X:\crates\error-semantic\scratch\run_error_smoke.ps1:36 char:17
+ ...   $captured = & $kain check $f.FullName --target $Target 2>&1 | Out-S ...
+                   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    + CategoryInfo          : NotSpecified: (  X:\crates\err...ng_surface.kn: :String) [], RemoteException
    + FullyQualifiedErrorId : NativeCommandError
 
error: 1 error(s) found:

--- [1/1] ---

error[PARSE:KAIN-PARSE-0002]: Expected '=', got newline
  --> X:\crates\error-semantic\scratch\world_missing_surface.kn:4:1
   |
  4 | 
   | ^ expected '=' here
   |
   = note: Parser was in a grammar state that accepts '=' but saw newline instead.
   = help: Check the token immediately before this point; most parse errors are caused by the previous unfinished 
construct.
   = help: insert the missing token before continuing, or restructure the
   = reference: parser/expected-token
```

---

## Failed Files

- **effect_pure_calls_io.kn** -- // ERROR: Pure function calling IO (exit 1)
- **multi_error.kn** -- // ERROR: Multiple errors in one file (exit 1)
- **parse_mismatched_delim.kn** -- // ERROR: Mismatched delimiter - [ opened, } closed (exit 1)
- **parse_missing_colon.kn** -- // ERROR: Missing colon after fn header (exit 1)
- **parse_reserved_ident.kn** -- // ERROR: Reserved identifier used as name (exit 1)
- **parse_unclosed_paren.kn** -- // ERROR: Unclosed parenthesis (exit 1)
- **parse_unexpected_token.kn** -- // ERROR: Unexpected token (exit 1)
- **type_duplicate_symbol.kn** -- // ERROR: Duplicate function definition (exit 1)
- **type_mismatch.kn** -- // ERROR: Type mismatch - assigning string to Int (exit 1)
- **type_missing_annotation.kn** -- // ERROR: Missing type annotation (exit 1)
- **type_return_mismatch.kn** -- // ERROR: fn returning nothing when Int expected (exit 1)
- **type_unknown_identifier.kn** -- // ERROR: Unknown identifier - typo in function name (exit 1)
- **typo_math.kn** -- // @expected_code: KAIN-TYPE-0002 (exit 1)
- **world_missing_surface.kn** -- // ERROR: World missing surface (exit 1)

## Notes

- Exit 0 = check passed (no errors)
- Exit 1 = check failed (errors found)
- Exit 2 = usage error
- Other = compiler crash or internal error

