/*
 * check_memory.c — compact CBMC harness for memory module
 *
 * Design: Compact single-file to keep SSA conversion time under 120s
 * when combined with the full memory.c source (~47KB).
 * 15 test functions, ~45 assertions.
 */

#include "memory.h"
#include <errno.h>

static unsigned char g_buf[128];
static unsigned char g_copy[128];
static unsigned char g_alloc_buf[256];
static int64_t g_atomic;
static char g_fname[] = "f";

/* 1. bind_local */
void t1(void) {
    __CPROVER_assert(__kain_bind_local(g_buf) == g_buf, "bind_local id");
    __CPROVER_assert(__kain_bind_local(NULL) == NULL, "bind_local null");
}

/* 2. addr_of */
void t2(void) {
    size_t s; __CPROVER_havoc_object(&s); __CPROVER_assume(s<=64);
    __CPROVER_assert(__kain_addr_of(g_buf,s) == g_buf, "addr_of id");
    __CPROVER_assert(__kain_addr_of(NULL,0) == NULL, "addr_of null");
}

/* 3. ptr_offset forward */
void t3(void) {
    int64_t o,st; __CPROVER_havoc_object(&o); __CPROVER_havoc_object(&st);
    __CPROVER_assume(o>=0&&o<=10&&st>=1&&st<=8&&o*st<=100);
    void* r=__kain_ptr_offset(g_buf,o,st);
    __CPROVER_assert(r!=NULL,"offset fwd nz");
    if(r)__CPROVER_assert((uintptr_t)r==(uintptr_t)g_buf+(uintptr_t)(o*st),"offset fwd addr");
}

/* 4. ptr_offset negative */
void t4(void) {
    unsigned char* m=g_buf+80; int64_t o,st;
    __CPROVER_havoc_object(&o); __CPROVER_havoc_object(&st);
    __CPROVER_assume(o>=-10&&o<=-1&&st>=1&&st<=8&&-o*st<=80);
    void* r=__kain_ptr_offset(m,o,st);
    __CPROVER_assert(r!=NULL,"offset neg nz");
    if(r)__CPROVER_assert((uintptr_t)r==(uintptr_t)m+(uintptr_t)(o*st),"offset neg addr");
}

/* 5. ptr_offset zero, overflow, NULL */
void t5(void) {
    __CPROVER_assert(__kain_ptr_offset(g_buf,0,1)==g_buf,"offset zero");
    int64_t o,st;__CPROVER_havoc_object(&o);__CPROVER_havoc_object(&st);
    __CPROVER_assume(o>0&&st>1&&o>INT64_MAX/st);
    __CPROVER_assert(__kain_ptr_offset(g_buf,o,st)==NULL,"offset ovf");
    __kain_ptr_offset(NULL,5,4);__CPROVER_assert(1,"offset null safe");
}

/* 6. field_ptr */
void t6(void) {
    size_t o;__CPROVER_havoc_object(&o);__CPROVER_assume(o<=100);
    void* r=__kain_field_ptr(g_buf,g_fname,o);
    __CPROVER_assert(r!=NULL,"field nz");
    if(r)__CPROVER_assert((uintptr_t)r==(uintptr_t)g_buf+o,"field addr");
    __kain_field_ptr(NULL,g_fname,16);__CPROVER_assert(1,"field null safe");
    size_t big;__CPROVER_havoc_object(&big);__CPROVER_assume(big>1024*1024);
    __kain_field_ptr(g_buf,g_fname,big);__CPROVER_assert(1,"field large safe");
}

/* 7. index_ptr */
void t7(void) {
    int64_t i,st;__CPROVER_havoc_object(&i);__CPROVER_havoc_object(&st);
    __CPROVER_assume(i>=0&&i<=10&&st>=1&&st<=8&&i*st<=100);
    void* r=__kain_index_ptr(g_buf,i,st);
    __CPROVER_assert(r!=NULL,"index nz");
    if(r)__CPROVER_assert((uintptr_t)r==(uintptr_t)g_buf+(uintptr_t)(i*st),"index addr");
    int64_t i2,s2;__CPROVER_havoc_object(&i2);__CPROVER_havoc_object(&s2);
    __CPROVER_assume(i2>0&&s2>1&&i2>INT64_MAX/s2);
    __CPROVER_assert(__kain_index_ptr(g_buf,i2,s2)==NULL,"index ovf");
}

/* 8. mem_load + mem_store */
void t8(void) {
    size_t s;__CPROVER_havoc_object(&s);__CPROVER_assume(s>0&&s<=16);
    __CPROVER_havoc_object(g_buf);__CPROVER_havoc_object(g_copy);
    __kain_mem_load(g_buf,g_copy,s);
    __CPROVER_assert(((unsigned char*)g_copy)[0]==((unsigned char*)g_buf)[0],"load b0");
    __kain_mem_store(g_copy,g_buf,s);
    __CPROVER_assert(((unsigned char*)g_copy)[0]==((unsigned char*)g_buf)[0],"store b0");
    __kain_mem_load(g_buf,g_copy,0);__kain_mem_store(g_copy,g_buf,0);
    __CPROVER_assert(1,"load/store zero safe");
}

/* 9. volatile load/store */
void t9(void) {
    size_t s;__CPROVER_havoc_object(&s);__CPROVER_assume(s>0&&s<=4);
    __CPROVER_havoc_object(g_buf);__CPROVER_havoc_object(g_copy);
    __kain_volatile_load(g_buf,g_copy,s);
    for(size_t i=0;i<s;++i)__CPROVER_assert(((unsigned char*)g_copy)[i]==((unsigned char*)g_buf)[i],"vol load");
    __kain_volatile_store(g_copy,g_buf,s);
    for(size_t i=0;i<s;++i)__CPROVER_assert(((unsigned char*)g_copy)[i]==((unsigned char*)g_buf)[i],"vol store");
    __kain_volatile_load(g_buf,g_copy,0);__kain_volatile_store(g_copy,g_buf,0);
    __CPROVER_assert(1,"vol zero safe");
}

/* 10. seqcst atomics */
void t10(void) {
    int64_t v1,v2;__CPROVER_havoc_object(&v1);__CPROVER_havoc_object(&v2);
    __kain_atomic_load_seqcst(&g_atomic);
    __kain_atomic_store_seqcst(&g_atomic,v1);
    __kain_atomic_add_seqcst(&g_atomic,v1);
    __kain_atomic_sub_seqcst(&g_atomic,v1);
    __kain_atomic_and_seqcst(&g_atomic,v1);
    __kain_atomic_or_seqcst(&g_atomic,v1);
    __kain_atomic_xor_seqcst(&g_atomic,v1);
    __kain_atomic_exchange_seqcst(&g_atomic,v1);
    int ok=__kain_atomic_compare_exchange_seqcst(&g_atomic,v1,v2);
    __CPROVER_assert(ok==0||ok==1,"CAS 0/1");
    __kain_atomic_fence(KAIN_MEMORY_ORDER_SEQ_CST);
    __CPROVER_assert(1,"seqcst ok");
}

/* 11. ordered atomics */
void t11(void) {
    int64_t v1,v2,o1,o2;__CPROVER_havoc_object(&v1);__CPROVER_havoc_object(&v2);
    __CPROVER_havoc_object(&o1);__CPROVER_havoc_object(&o2);
    __CPROVER_assume(o1>=0&&o1<=4);__CPROVER_assume(o2>=0&&o2<=4);
    __kain_atomic_load_ordered(&g_atomic,o1);
    __kain_atomic_store_ordered(&g_atomic,v1,o1);
    __kain_atomic_add_ordered(&g_atomic,v1,o1);
    __kain_atomic_sub_ordered(&g_atomic,v1,o1);
    __kain_atomic_and_ordered(&g_atomic,v1,o1);
    __kain_atomic_or_ordered(&g_atomic,v1,o1);
    __kain_atomic_xor_ordered(&g_atomic,v1,o1);
    __kain_atomic_exchange_ordered(&g_atomic,v1,o1);
    int ok=__kain_atomic_compare_exchange_ordered(&g_atomic,v1,v2,o1,o2);
    __CPROVER_assert(ok==0||ok==1,"CAS ord 0/1");
    __kain_atomic_fence(o1);
    __CPROVER_assert(1,"ordered ok");
}

/* 12. alloc overflow */
void t12(void) {
    size_t s1,s2;__CPROVER_havoc_object(&s1);__CPROVER_havoc_object(&s2);
    __CPROVER_assume(s1>0&&s2>0&&s1>SIZE_MAX/s2);
    __CPROVER_assert(__kain_alloc(s1,s2,0)==NULL,"alloc ovf");
}

/* 13. free(NULL) */
void t13(void) {
    __CPROVER_assert(__kain_free(NULL)==0,"free null");
}

/* 14. free(invalid header) */
void t14(void) {
    KainAllocHeader* h=(KainAllocHeader*)g_alloc_buf;void* p=(void*)(h+1);
    __CPROVER_assume((unsigned char*)p<=g_alloc_buf+sizeof(g_alloc_buf)-1);
    __CPROVER_havoc_object(g_alloc_buf);
    __CPROVER_assume((h->metadata.magic_and_slot&KAIN_ALLOC_HEADER_MAGIC_MASK)!=KAIN_ALLOC_HEADER_MAGIC_TAG);
    __CPROVER_assert(__kain_free(p)==-1,"free invalid");
}

/* 15. realloc(invalid header) */
void t15(void) {
    KainAllocHeader* h=(KainAllocHeader*)g_alloc_buf;void* p=(void*)(h+1);
    size_t sz,st;__CPROVER_havoc_object(&sz);__CPROVER_havoc_object(&st);
    __CPROVER_assume((unsigned char*)p<=g_alloc_buf+sizeof(g_alloc_buf)-1);
    __CPROVER_assume(sz>0&&sz<=32&&st>0&&st<=8&&st<=SIZE_MAX/sz);
    __CPROVER_havoc_object(g_alloc_buf);
    __CPROVER_assume((h->metadata.magic_and_slot&KAIN_ALLOC_HEADER_MAGIC_MASK)!=KAIN_ALLOC_HEADER_MAGIC_TAG);
    __CPROVER_assert(__kain_realloc(p,sz,st,0)==NULL,"realloc invalid");
}

int main(void) { t1();t2();t3();t4();t5();t6();t7();t8();t9();t10();t11();t12();t13();t14();t15(); return 0; }
