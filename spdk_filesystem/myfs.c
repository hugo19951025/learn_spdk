
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/bdev.h"
#include "spdk/blob.h"
#include "spdk/blob_bdev.h"

#include <stdio.h>
#include <stdint.h>


#define BDEV_NAME_LENGTH 512
#define ALIGN_4k 0x1000

struct myfs_blob_context_t {
    char bdev_name[BDEV_NAME_LENGTH];
    struct spdk_blob_store *bs;
    spdk_blob_id blobid;
    struct spdk_blob* blob;

    struct spdk_io_channel* channel;

    uint8_t *write_buff;
    uint8_t *read_buff;
    uint64_t io_unit_size;

};

static void myfs_bdev_event_callback(enum spdk_bdev_event_type type, struct spdk_bdev* bdev, void* event_ctx) {
    SPDK_NOTICELOG("myfs_bdev_event_callback---->\n");
    return;
}

static void spdk_blob_close_complete(void* cb_arg, int bserrno) {
    return;
}

static void spdk_bs_delete_blob_complete(void* cb_arg, int bserrno) {
    return;
}


static void myfs_blob_destory(struct myfs_blob_context_t* ctx) {
    if (ctx->channel) {
        spdk_bs_free_io_channel(ctx->channel);
        ctx->channel = NULL;
    }
    if (ctx->blob) {
        spdk_blob_close(ctx->blob, spdk_blob_close_complete, ctx);
    }

    if (ctx->bs) {
        spdk_bs_delete_blob(ctx->bs, ctx->blobid, spdk_bs_delete_blob_complete, ctx);
    }
}

static void myfs_blob_read_complete(void* cb_arg, int bserrno) {
    struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
    SPDK_NOTICELOG("read complete %s\n", myfs_ctx->read_buff);

    myfs_blob_destory(myfs_ctx);
    //spdk_app_stop(0);
}

static void myfs_blob_read(struct myfs_blob_context_t* ctx) {
    ctx->read_buff = spdk_malloc(ctx->io_unit_size, ALIGN_4k, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
    memset(ctx->read_buff, 0, ALIGN_4k);

    if (ctx->read_buff == NULL) {
        return;
    }

    spdk_blob_io_read(ctx->blob, ctx->channel, ctx->read_buff, 0, 1, myfs_blob_read_complete, ctx);
}

static void myfs_blob_write_complete(void* cb_arg, int bserrno) {
    struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
    SPDK_NOTICELOG("myfs_blob_write_complete---->bserrno: %d\n", bserrno);

    myfs_blob_read(myfs_ctx);
}


static void myfs_blob_write(struct myfs_blob_context_t* ctx) {
    ctx->write_buff = spdk_malloc(ctx->io_unit_size, ALIGN_4k, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
    if (ctx->write_buff == NULL) {
        return;
    }
    memset(ctx->write_buff, 'h', ctx->io_unit_size);

    struct spdk_io_channel* channel = spdk_bs_alloc_io_channel(ctx->bs); // 这里虽然是起名字叫alloc 但是其实在app init里面就已经分配好了 这里都是直接拿来用
    ctx->channel = channel;

    if (ctx->channel == NULL) {
        return;
    }

    // 写之前要把blob大小做个resize
    spdk_blob_io_write(ctx->blob, channel, ctx->write_buff, 0, 1, myfs_blob_write_complete, ctx);
}

static void myfs_blob_resize_complete(void* cb_arg, int bserrno) {
    SPDK_NOTICELOG("myfs_blob_resize_complete----> bserrno: %d\n", bserrno);
    struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;

    myfs_blob_write(myfs_ctx);
}

static void spdk_blob_open_complete(void* cb_arg, struct spdk_blob* blob, int bserrno) {
    struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
    

    myfs_ctx->blob = blob;

    uint64_t numfreed = spdk_bs_free_cluster_count(myfs_ctx->bs);

    SPDK_NOTICELOG("spdk_blob_open_complete----> numfreed: %lu\n", numfreed);

    // 重新扩展一下大小
    spdk_blob_resize(myfs_ctx->blob, numfreed, myfs_blob_resize_complete, myfs_ctx);

    //myfs_blob_write(myfs_ctx);
}


static void spdk_blob_create_complete(void* cb_arg, spdk_blob_id blobid, int bserrno) {
    SPDK_NOTICELOG("spdk_blob_create_complete----> blobid: %lu\n", blobid);
    struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
    myfs_ctx->blobid = blobid;

    spdk_bs_open_blob(myfs_ctx->bs, blobid, spdk_blob_open_complete, myfs_ctx);
    return;
}

static void spdk_bs_get_super_complete(void* cb_arg, spdk_blob_id blobid, int bserrno) {
    struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;

    if (bserrno) {
        SPDK_ERRLOG("Super blob get failed, bserrno %d\n", bserrno);
        if (bserrno == -ENOENT) {
            spdk_bs_create_blob(myfs_ctx->bs, spdk_blob_create_complete, myfs_ctx);
            SPDK_NOTICELOG("spdk_bs_create_blob\n");
        }
    } else {
        spdk_bs_open_blob(myfs_ctx->bs, myfs_ctx->blobid, spdk_blob_open_complete, myfs_ctx);
    }
}

static void myfs_blob_init_complete(void* cb_arg, struct spdk_blob_store* bs, int bserrno) {
    struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
    myfs_ctx->bs = bs;
    myfs_ctx->io_unit_size = spdk_bs_get_io_unit_size(bs);

    SPDK_NOTICELOG("myfs_blob_init_complete---->io_unit_size: %lu\n", myfs_ctx->io_unit_size);

#if 0
    spdk_bs_create_blob(myfs_ctx->bs, spdk_blob_create_complete, myfs_ctx);
#else
    spdk_bs_get_super(bs, spdk_bs_get_super_complete, myfs_ctx);
#endif
}

static void myfs_blob_start(void* ctx) {
    SPDK_NOTICELOG("Myfs_blob_start\n");

    struct myfs_blob_context_t *myfs_ctx = (struct myfs_blob_context_t *)ctx;
    struct spdk_bs_dev* bs_dev = NULL;
    const char* bdev_name = "Malloc0";
    strcpy(myfs_ctx->bdev_name, bdev_name);

    //‌ 该函数的核心功能是初始化一个类文件系统（blobstore）
    // 它允许应用程序在块设备上管理blob（类似文件的数据单元）
    // 并提供统一接口来操作这些数据单元
    int rc = spdk_bdev_create_bs_dev_ext(bdev_name, myfs_bdev_event_callback, NULL, &bs_dev);
    if (rc != 0) {
        spdk_app_stop(-1);
        return;
    }

    spdk_bs_init(bs_dev, NULL, myfs_blob_init_complete, myfs_ctx);

}

int main (int argc, char* argv[]) {
    struct spdk_app_opts opts = {};

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "myfs_blob";
    opts.json_config_file = argv[1];
    opts.reactor_mask = "0x1";
    opts.no_huge = true;
    opts.mem_size = 512;  

    struct myfs_blob_context_t *myfs_ctx = calloc(1, sizeof(struct myfs_blob_context_t));

    if (myfs_ctx != NULL) {
        spdk_app_start(&opts, myfs_blob_start, myfs_ctx);
    }
    

    return 0;
}