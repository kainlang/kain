// Minimal tablegen that always succeeds.
// No CRT dependency, no Windows headers, no crashes.

int main(int argc, char **argv) {
    // Always succeed. The generated .inc files are expected to already exist.
    return 0;
}
