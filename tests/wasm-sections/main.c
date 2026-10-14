#include <stdio.h>
/* Idiom from swift/stdlib/public/runtime/SwiftRT-ELF-WASM.cpp: declare an empty retained
   section so wasm-ld always synthesizes __start_/__stop_ for a C-identifier section name. */
__asm__("\t.section objc_classlist,\"R\",@\n\t.p2align 2\n\t.text\n");
extern void *__start_objc_classlist __attribute__((weak));
extern void *__stop_objc_classlist __attribute__((weak));
int main(void) {
    void **begin = &__start_objc_classlist, **end = &__stop_objc_classlist;
    int n = 0, sum = 0;
    for (void **p = begin; p < end; ++p) { ++n; sum += *(int *)*p; }
    printf("objc_classlist entries: %d (sum %d)\n", n, sum);
    return n == 2 && sum == 3 ? 0 : 1;
}
