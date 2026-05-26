struct Wide {
    char tag;
    int value;
} __attribute__((aligned(16)));

int wide_size(void) { return sizeof(struct Wide); }
int wide_align(void) { return _Alignof(struct Wide); }
