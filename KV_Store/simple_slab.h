#ifdef __cplusplus
extern "C" {
#endif

#define ALIGN_4k 0x1000
#define KV_PAGE_SIZE ALIGN_4k

typedef struct slab_s {
    int block_size;
    int free_count;

    char* first_free;
    char* mem;
}slab_t;

int init_slab(slab_t *s, int block_size);
void delete_slab(slab_t *s);
void* alloc_slab(slab_t *s);
void free_slab(slab_t *s, void *ptr);

#ifdef __cplusplus
}
#endif