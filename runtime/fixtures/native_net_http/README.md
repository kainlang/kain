# Native Net HTTP Fixture

This fixture proves that LLVM/direct-native Kain source can author TCP and HTTP/1.1 flows through the `kain_native_net_*` ABI.

It starts a loopback HTTP server, registers an actor route, sends a raw HTTP request through the TCP API, pumps the server, responds through the HTTP API, and verifies that the TCP client sees the response.
