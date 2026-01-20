#include <spdk/env.h>
#include <spdk/nvme.h>
#include <spdk/queue.h>

#define ALIGN_4k 0x1000
#define NVME_CONTROLLER_NAME_LENGTH 5112

struct spdk_nvme_transport_id gtrid;
struct spdk_nvme_ctrlr *gctrlr = NULL;
struct spdk_nvme_ns* gns = NULL;
struct spdk_nvme_qpair* gqpair = NULL;

struct ctrlr_entry {
    struct spdk_nvme_ctrlr *ctrlr;
    struct spdk_nvme_qpair *qpair;
    TAILQ_ENTRY(ctrlr_entry) link;
    char name[NVME_CONTROLLER_NAME_LENGTH];
};

struct namespace_entry
{
    struct spdk_nvme_ns *ns;
    TAILQ_ENTRY(namespace_entry) link;
    struct ctrlr_entry* ctrlr;
    int nsid;
};

TAILQ_HEAD(, ctrlr_entry) g_ctrlr = TAILQ_HEAD_INITIALIZER(g_ctrlr);
TAILQ_HEAD(, namespace_entry) g_ns = TAILQ_HEAD_INITIALIZER(g_ns); 



static bool nvme_probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr_opts *opts) {
    printf("nvme_probe_cb: %s\n", trid->traddr);
    return true;
}

static void nvme_attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr *ctrlr, const struct spdk_nvme_ctrlr_opts *opts) {
    // printf("nvme_attach_cb: %s\n", trid->traddr);
    // gctrlr = ctrlr;
    
    // gns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);

    int nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr);
    gns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
    gctrlr = ctrlr;
    struct ctrlr_entry *entry = (struct ctrlr_entry*)malloc(sizeof(struct ctrlr_entry));
    struct spdk_nvme_qpair *qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
    if (!entry) {
        exit(1);
    }

    entry->ctrlr = ctrlr;
    entry->qpair = qpair;
    
    snprintf(entry->name, sizeof(entry->name), "%s", trid->traddr);
    TAILQ_INSERT_TAIL(&g_ctrlr, entry, link);

    while (nsid != 0) {
        struct spdk_nvme_ns* ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
        if (ns == NULL) {
            continue;
        }

        struct namespace_entry* ns_entry = (struct namespace_entry*)malloc(sizeof(struct namespace_entry));
        if (ns_entry == NULL) {
            exit(1);
        }
        ns_entry->ns = ns;
        ns_entry->ctrlr = entry;
        ns_entry->nsid = nsid;

        TAILQ_INSERT_TAIL(&g_ns, ns_entry, link);

        nsid = spdk_nvme_ctrlr_get_next_active_ns(ctrlr, nsid);
    }
}

static void nvme_remove_cb(void* cb_ctx, struct spdk_nvme_ctrlr* ctrlr) {
    printf("nvme_remove_cb\n");
}



static void nvme_opera_write_cb(void* ctx, const struct spdk_nvme_cpl* cpl) {
    int *finished = (int*)ctx;
    *finished = 1;
    printf("nvme_opera_write cb finished\n");
}

static void nvme_opera_read_cb(void* ctx, const struct spdk_nvme_cpl* cpl) {
    int *finished = (int*)ctx;
    *finished = 1;
    printf("nvme_opera_read cb finished\n");
}

static int nvme_opera_read(struct spdk_nvme_ctrlr* ctrlr, struct spdk_nvme_ns* ns) {
    //spdk采用异步读写 分配一个队列组 包含提交队列 和 回收队列
    struct spdk_nvme_qpair *qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
    if (qpair == NULL) return -1;

    size_t sz;
    // rmem 是 写空间 叫 read buf 更好
    char* rmem = (char*)spdk_nvme_ctrlr_map_cmb(ctrlr, &sz); // 先尝试分配4k内存页 使用mmap的方式来映射
    if (rmem == NULL) {
        // mmap 映射失败 可以自主分配空间 使用spdk内置的malloc函数
        rmem = (char*)spdk_zmalloc(0x1000, 0x1000, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
    }

    int finished = 0;
    int rc = spdk_nvme_ns_cmd_read(ns, qpair, rmem, 0, 1, nvme_opera_read_cb, &finished, 0);
    if (rc != 0) {
        printf("starting read I/O failed\n");
        exit(1);
    }

    while (!finished) {
        spdk_nvme_qpair_process_completions(qpair, 0);
    }

    spdk_nvme_ctrlr_free_io_qpair(qpair);
    printf("nvme_opera_read successful: %s\n", rmem);
    return 0;
}


int main(int argc, char* argv[]) {
    struct spdk_env_opts opts = {0};
    memset(&opts, 0, sizeof(opts));

    opts.name = "kvstore";
    opts.core_mask = "0x1";
    opts.opts_size = sizeof(struct spdk_env_opts);
    //opts.no_huge = true;
    spdk_env_opts_init(&opts);


    spdk_env_init(&opts);

    memset(&gtrid, 0, sizeof(gtrid));
    spdk_nvme_trid_populate_transport(&gtrid, SPDK_NVME_TRANSPORT_PCIE);
    snprintf(gtrid.subnqn, sizeof(gtrid.subnqn), "%s", SPDK_NVMF_DISCOVERY_NQN);

    spdk_nvme_probe(&gtrid, NULL, nvme_probe_cb, nvme_attach_cb, nvme_remove_cb);

    struct spdk_nvme_qpair* qpair = spdk_nvme_ctrlr_alloc_io_qpair(gctrlr, NULL, 0);
    gqpair = qpair;
    if (qpair == NULL) return -1;

    size_t sz = 0;
    int mmap_io =  1;
    void* buf = spdk_nvme_ctrlr_map_cmb(gctrlr, &sz); // 先尝试分配4k内存页 使用mmap的方式来映射
    if (buf == NULL || sz == 0) {
        printf("switch spdk_zmalloc\n");
        // mmap 映射失败 可以自主分配空间 使用spdk内置的malloc函数
        buf = spdk_zmalloc(ALIGN_4k, ALIGN_4k, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
        sz = ALIGN_4k;
        mmap_io = 0;
    }
    if (buf == NULL) return -1;
    memset(buf, 0, sz);
    snprintf((char*)buf, ALIGN_4k, "The test for spdk, 555, 1025, KKK");
    int finished = 0;
    int rc = spdk_nvme_ns_cmd_write(gns, qpair, buf, 0, 1, nvme_opera_write_cb, &finished, 0);
    if (rc != 0) {
        printf("starting write I/O failed\n");
        exit(1);
    }
    while (!finished) {
        spdk_nvme_qpair_process_completions(qpair, 0);
    }
    spdk_nvme_ctrlr_free_io_qpair(qpair);
    nvme_opera_read(gctrlr, gns);
    printf("main mmap_io %d\n", mmap_io);
    return 0;
}