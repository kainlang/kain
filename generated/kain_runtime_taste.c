typedef long long int64_t;
typedef unsigned long long size_t;

extern void *malloc(size_t size);
extern void *realloc(void *ptr, size_t size);
extern void free(void *ptr);
extern size_t strlen(const char *s);
extern char *strcpy(char *dst, const char *src);
extern int printf(const char *fmt, ...);

char* kain_strdup(const char* s) {
    size_t len = strlen(s);
    char* d = malloc(len + 1);
    strcpy(d, s);
    return d;
}

void kain_print_str(const char* s) {
    printf("%s", s);
}

struct KainArray {
    int64_t* data;
    int64_t len;
    int64_t cap;
};

int64_t kain_array_new(void) {
    struct KainArray* arr = malloc(24);
    arr->data = malloc(64);
    arr->len = 0;
    arr->cap = 8;
    return (int64_t)arr;
}

void kain_array_push(int64_t arr_ptr, int64_t value) {
    struct KainArray* arr = (struct KainArray*)arr_ptr;
    if (arr->len >= arr->cap) {
        arr->cap = arr->cap * 2;
        arr->data = realloc(arr->data, (size_t)(arr->cap * 8));
    }
    arr->data[arr->len] = value;
    arr->len = arr->len + 1;
}

int64_t kain_array_get(int64_t arr_ptr, int64_t index) {
    struct KainArray* arr = (struct KainArray*)arr_ptr;
    if (index < 0 || index >= arr->len) {
        return 0;
    }
    return arr->data[index];
}

int64_t kain_add_op(int64_t a, int64_t b) {
    return a + b;
}
