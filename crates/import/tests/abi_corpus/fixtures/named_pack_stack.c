#pragma pack(push, outer, 4)
struct Outer {
    char tag;
    int value;
};
#pragma pack(push, inner, 1)
struct Inner {
    char tag;
    int value;
};
#pragma pack(pop, inner)
struct AfterInner {
    char tag;
    int value;
};
#pragma pack(pop, outer)
struct Natural {
    char tag;
    int value;
};

int inner_size(void) { return sizeof(struct Inner); }
int after_inner_align(void) { return _Alignof(struct AfterInner); }
int natural_align(void) { return _Alignof(struct Natural); }
