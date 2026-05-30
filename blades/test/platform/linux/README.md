# platform-linux

Linux / Unix proof blade for Kain's LLVM lane.

This blade focuses on Linux-specific and Unix-flavored edges that are easy to accidentally regress while developing the runtime and compiler:

- runtime boot / heap validation / shutdown
- Linux identity and procfs visibility
- libc dynamic loading (`libc.so.6` / `getpid`)
- temp-dir, hidden-file, and Unix path behavior
- the current Linux-native process gap (`unsupported-platform`) as an explicit test
- TCP + HTTP loopback server behavior
- malformed HTTP request rejection
- software graphics command recording and backend probing
- GPU shared-resource contracts that should remain stable on Linux

The blade writes a human-readable report to:

- `.kain/run/linux_platform_report.txt`

The suite is Linux-first, but it exits cleanly with a skip report when run on a non-Linux host.
