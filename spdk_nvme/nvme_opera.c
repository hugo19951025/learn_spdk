
#include "spdk/env.h"
#include "spdk/nvme.h"
#include "spdk/stdinc.h"
#include "spdk/vmd.h"
#include "spdk/nvme_spec.h"

#include <stdio.h>
#include <stdint.h>


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

static int nvme_opera_write(struct spdk_nvme_ctrlr* ctrlr, struct spdk_nvme_ns* ns) {
    //spdk采用异步读写 分配一个队列组 包含提交队列 和 回收队列
    struct spdk_nvme_qpair *qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
    if (qpair == NULL) return -1;

    size_t sz;
    // wmem 是 写空间 叫 write buf 更好
    char* wmem = spdk_nvme_ctrlr_map_cmb(ctrlr, &sz); // 先尝试分配4k内存页 使用mmap的方式来映射
    if (wmem == NULL) {
        // mmap 映射失败 可以自主分配空间 使用spdk内置的malloc函数
        wmem = spdk_zmalloc(0x1000, 0x1000, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
    }

    strcpy(wmem, "The test for spdk, 555, 1025, KKK");

    int finished = 0;
    int rc = spdk_nvme_ns_cmd_write(ns, qpair, wmem, 0, 1, nvme_opera_write_cb, &finished, 0);
    if (rc != 0) {
        printf("starting write I/O failed\n");
        exit(1);
    }

    while (!finished) {
        spdk_nvme_qpair_process_completions(qpair, 0);
    }
    spdk_nvme_ctrlr_free_io_qpair(qpair);
    printf("nvme_opera_write successful\n\n\n");
    return 0;
}

static int nvme_opera_read(struct spdk_nvme_ctrlr* ctrlr, struct spdk_nvme_ns* ns) {
    //spdk采用异步读写 分配一个队列组 包含提交队列 和 回收队列
    struct spdk_nvme_qpair *qpair = spdk_nvme_ctrlr_alloc_io_qpair(ctrlr, NULL, 0);
    if (qpair == NULL) return -1;

    size_t sz;
    // rmem 是 写空间 叫 read buf 更好
    char* rmem = spdk_nvme_ctrlr_map_cmb(ctrlr, &sz); // 先尝试分配4k内存页 使用mmap的方式来映射
    if (rmem == NULL) {
        // mmap 映射失败 可以自主分配空间 使用spdk内置的malloc函数
        rmem = spdk_zmalloc(0x1000, 0x1000, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
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
    printf("nvme_opera_write successful: %s\n\n\n", rmem);
    return 0;
    return 0;
}


static void nvme_opera_attach_callback(void* cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr* ctrlr, const struct spdk_nvme_ctrlr_opts* opts) {
    printf("nvme_opera_attach_callback\n");
    //spdk_nvme_ctrlr_get_data(ctrlr);
    uint32_t num_ns = spdk_nvme_ctrlr_get_num_ns(ctrlr);
    printf("name space num: %u\n", num_ns);

    int32_t nsid = 0;
    nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr);
    while (nsid != 0) {
        struct spdk_nvme_ns* ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
        if (ns == NULL) continue;
        
        uint64_t sz = spdk_nvme_ns_get_size(ns);
        printf("nsid: %u name space size: %juGB\n\n\n", nsid, sz / (1024 * 1024 * 1024));

        nvme_opera_write(ctrlr, ns);
        nvme_opera_read(ctrlr, ns);


        nsid = spdk_nvme_ctrlr_get_next_active_ns(ctrlr, nsid);
    }
}

static bool nvme_opera_probe_callback(void* cb_ctx, const struct spdk_nvme_transport_id *trid, struct spdk_nvme_ctrlr_opts* opts) {
    printf("nvme_opera_probe_callback\n");
    return true;
}

static void nvme_opera_remove_callback(void *cb_ctx, struct spdk_nvme_ctrlr *ctrlr) {
    printf("nvme_opera_remove_callback\n");
}


int main(int argc, char* argv[]) {
    struct spdk_env_opts opts;
    memset(&opts, 0, sizeof(opts));
    
    opts.name = "nvme_opera";
    opts.core_mask = "0x1";
    opts.opts_size = sizeof(struct spdk_env_opts);

    spdk_env_opts_init(&opts);

    int res = spdk_env_init(&opts);
    printf("spdk_env_init res: %d\n", res);

    // Enumerate VMD devices and hook them into the spdk pci subsystem
    spdk_vmd_init();

    spdk_nvme_probe(NULL, NULL, nvme_opera_probe_callback, nvme_opera_attach_callback, nvme_opera_remove_callback);
    spdk_vmd_fini();
    return 0;
}