typedef struct kain_str {
    int length;
    int capacity;
} kain_str;

int kain_str_new(int requested_length) {
    if (requested_length < 0) {
        return 0;
    }
    return requested_length + 1;
}

int kain_runtime_checksum(kain_str input) {
    return input.length * 17 + input.capacity;
}
