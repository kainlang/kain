struct Pair {
    int x;
    int y;
};

union Payload {
    struct Pair pair;
    long long raw;
};

struct Pair read_pair(union Payload u) {
    return u.pair;
}
