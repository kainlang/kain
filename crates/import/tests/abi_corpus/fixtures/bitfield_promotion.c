struct Flags {
    signed int small : 5;
    unsigned long long wide : 40;
};

int promote(struct Flags f) {
    return f.small + f.wide;
}
