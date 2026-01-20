
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "simple_slab.h"

// #define ALIGN_4k 0x1000
// #define KV_PAGE_SIZE ALIGN_4k

// typedef struct slab_s {
//     int block_size;
//     int free_count;

//     char* first_free;
//     char* mem;
// }slab_t;

int init_slab(slab_t *s, int block_size) {
    if (!s) return -1;

    s->block_size = block_size;
    s->free_count = KV_PAGE_SIZE / block_size;
    s->first_free = (char*)malloc(KV_PAGE_SIZE);
    if (!s->first_free) return -1;
    s->mem = s->first_free;
    char* ptr = s->first_free;
    for (int i = 0; i < s->free_count; i++) {
        *(char**)ptr = ptr + block_size;
        ptr += block_size;
    }
    *(char**)ptr = NULL;
    return 0;
}

void delete_slab(slab_t *s) {
    if (!s) return;
    free(s->mem);
}

void* alloc_slab(slab_t *s) {
    if (!s || s->free_count == 0) {
        return NULL;
    }
    void *ptr = s->first_free;
    s->first_free = *(char**)ptr;
    s->free_count--;
    return ptr;
}

void free_slab(slab_t *s, void *ptr) {
    *(char**)ptr = s->first_free;
    s->first_free = (char*)ptr;
    s->free_count++;
}


// int main() {
//     slab_t s;
//     init_slab(&s, 16);
//     void *p1 = alloc_slab(&s);
//     printf("alloc slab: %p\n", p1);

//     void *p2 = alloc_slab(&s);
//     printf("alloc slab: %p\n", p2);

//     void *p3 = alloc_slab(&s);
//     printf("alloc slab: %p\n", p3);

//     void *p4 = alloc_slab(&s);
//     printf("alloc slab: %p\n", p4);

//     free_slab(&s, p2);
//     printf("free slab: %p\n", p2);

//     void *p5 = alloc_slab(&s);
//     printf("alloc slab: %p\n", p5);

//     free_slab(&s, p4);
//     printf("free slab: %p\n", p4);

//     void *p6 = alloc_slab(&s);
//     printf("alloc slab: %p\n", p6);
//     return 0;
// }