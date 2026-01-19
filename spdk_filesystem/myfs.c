#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/log.h"
#include "spdk/bdev.h"
#include "spdk/blob.h"
#include "spdk/blob_bdev.h"
#include "spdk/init.h"
#include "spdk/file.h"
#include "spdk/rpc.h"
#include "spdk_internal/event.h"
#include "spdk/bdev_module.h"
#include "spdk/thread.h"
#include "spdk/jsonrpc.h"
#include "spdk/rpc.h"
#include "spdk/string.h"
#include "../spdk/lib/blob/blobstore.h"
#include "spdk/thread.h"

#include "internal.h"
#include "myfs.h"

#define BDEV_NAME_LENGTH 128
#define ALIGN_4k 0x1000
#define FILENAME_LENGTH 128
#define MAX_FD_COUNT 1024
#define DEFAULT_FD_NUM 3
#define FD_TABLE_SIZE MAX_FD_COUNT / 8
/*
###########################################
define fs file structure
###########################################
*/
struct myfs_blob_context_t {
    char bdev_name[BDEV_NAME_LENGTH];
    struct spdk_blob_store *bs;
    spdk_blob_id blobid;
    struct spdk_blob* blob;

    struct spdk_io_channel* channel;
    struct spdk_thread* cb_thread;

    uint8_t *write_buff;
    uint8_t *read_buff;
    uint64_t io_unit_size;

    bool done;

};

typedef struct myfs_s {
    struct spdk_thread* thread;
    struct spdk_bs_dev *bs_dev;
    struct spdk_blob_store* bs;
    struct spdk_io_channel *channel;
    uint64_t unit_size;

    bool finished;
}myfs_t;

typedef struct myfs_operation_s {
    void (*alloc)(void* arg);
    void (*free)(void* arg);
}myfs_operation_t;


typedef enum myfs_file_status_s {
    FILE_CREATE,
    FILE_OPEN,
    FILE_WRITE,
    FILE_READ,
    FILE_CLOSE,
    FILE_RELEASE
}myfs_file_status_t;

typedef struct myfs_file_s {
    char filename[FILENAME_LENGTH];
    struct myfs_s *fs;
    spdk_blob_id blobid;
    struct spdk_blob* blob;
    int flags;
    int offset;
    int ret;
    int status;
    bool done;
}myfs_file_t;

typedef struct myfs_file_operation_s {
    int (*create)(struct myfs_file_s *file);
    int (*open)(struct myfs_file_s *file);
    int (*write)(struct myfs_file_s *file, void* buf, size_t size);
    int (*read)(struct myfs_file_s *file, void* buf, size_t size);

    off_t (*lseek)(struct myfs_file_s *file, off_t offset, int whence);

    int (*close)(struct myfs_file_s *file);
    int (*release)(struct myfs_file_s *file);

}myfs_file_operation_t;

typedef struct myfs_file_rwctx_s {
     struct myfs_file_s* file;
     void* buf;
     size_t size;
}myfs_file_rwctx_t;

struct myfs_s* fs = NULL;
static const int POLLER_MAX_TIME = 1e8;
static const char* json_file = "/home/hugo/learn_spdk/spdk_filesystem/myfs.json";
static unsigned char fd_table[FD_TABLE_SIZE] = {0};
static struct myfs_file_s* files[MAX_FD_COUNT] = {0};
static bool initilized = false;

/*
#################################
vfs external api
#################################
*/

int myfs_create(const char *filename, int flag);
int myfs_open(const char *filename, int flag);
size_t myfs_write(int fd, void *buf, size_t count);
size_t myfs_read(int fd, void *buf, size_t count);
int myfs_close(int fs);


/*
#################################
spdk internal api
#################################
*/

static int get_fd_from_bitmap(void) {
    int fd = DEFAULT_FD_NUM;
    while(fd < MAX_FD_COUNT) {
        if ((fd_table[fd/8] & (0x1 << (fd % 8))) == 0) {
            fd_table[fd/8] |= (0x1 << (fd % 8));
            return fd;
        }
        fd++;
    }
    return -1;
}

static int set_fd_to_bitmap(int fd) {
    if (fd >= MAX_FD_COUNT) return -1;
    fd_table[fd/8] &= ~(0x1 << (fd % 8));
    return 0;
}

static bool poller(struct spdk_thread* thread, spdk_msg_fn fn, void* ctx, bool* done) {
    
    int poller_count = 0;
    spdk_thread_send_msg(thread, fn, ctx);
    do {
        spdk_thread_poll(thread, 0, 0);
        poller_count++;
    } while (!(*done) && poller_count < POLLER_MAX_TIME);

    if (!(*done) && poller_count >= POLLER_MAX_TIME) {
        SPDK_ERRLOG("work not done but time out\n");
        return false;
    }
    return true;
}


static void myfs_blob_resize_complete(void* arg, int bserrno) {
    SPDK_NOTICELOG("myfs_blob_resize_complete----> bserrno: %d\n", bserrno);
    struct myfs_file_s* myfile = arg;
    myfile->done = true;
}

static void myfs_blob_open_complete(void* arg, struct spdk_blob* blob, int bserrno) {
    struct myfs_file_s* myfile = arg;

    myfile->blob = blob;
    myfile->blobid = blob->id;
    uint64_t numfreed = spdk_bs_free_cluster_count(fs->bs);
    SPDK_NOTICELOG("spdk_blob_open_complete----> numfreed: %lu\n", numfreed);

    // 重新扩展一下大小
    spdk_blob_resize(myfile->blob, numfreed, myfs_blob_resize_complete, myfile);
}

static void myfs_blob_create_complete(void* arg, spdk_blob_id blobid, int bserrno) {
    SPDK_NOTICELOG("spdk_blob_create_complete----> blobid: %lu\n", blobid);
    struct myfs_file_s* myfile = arg;
    myfile->blobid = blobid;
    spdk_bs_open_blob(fs->bs, blobid, myfs_blob_open_complete, myfile);
    return;
}

static void myfs_bs_get_super_complete(void* arg, spdk_blob_id blobid, int bserrno) {
    struct myfs_file_s* myfile = arg;
    if (bserrno) {
        if (bserrno == -ENOENT) {
            spdk_bs_create_blob(fs->bs, myfs_blob_create_complete, myfile);
        }
    } else {
        spdk_bs_open_blob(fs->bs, blobid, myfs_blob_open_complete, myfile);
    }
}

static void myfs_file_write_complete(void* arg, int bserrno) {
    SPDK_NOTICELOG("myfs_file_write_complete---->bserrno: %d\n", bserrno);
    struct myfs_file_s* myfile = arg;
    myfile->done = true;
}

static void myfs_file_read_complete(void* arg, int bserrno) {
    SPDK_NOTICELOG("myfs_file_read_complete---->bserrno: %d\n", bserrno);
    struct myfs_file_s* myfile = arg;
    myfile->done = true;

}

static void myfs_file_release_complete(void* arg, int bserrno) {
    SPDK_NOTICELOG("myfs_file_release_complete---->bserrno: %d\n", bserrno);
    struct myfs_file_s* myfile = arg;
    myfile->done = true;
}

static void myfs_file_close_complete(void* arg, int bserrno) {
    SPDK_NOTICELOG("myfs_file_close_complete---->bserrno: %d\n", bserrno);
    struct myfs_file_s* myfile = arg;
    if (myfile->status == FILE_CLOSE) {
        myfile->done = true;
    } else if (myfile->status == FILE_RELEASE) {
        spdk_bs_delete_blob(fs->bs, myfile->blobid, myfs_file_release_complete, myfile);
    }
}


static void do_create(void* arg) {
    struct myfs_file_s* myfile = arg;
    myfile->status = FILE_CREATE;
    spdk_bs_get_super(fs->bs, myfs_bs_get_super_complete, myfile);
}

static void do_open(void* arg) {
    struct myfs_file_s* myfile = arg;
    if (myfile->blobid == 0) {
        myfile->ret = -1;
        return;
    }
    myfile->status = FILE_OPEN;
    spdk_bs_open_blob(fs->bs, myfile->blobid, myfs_blob_open_complete, myfile);
}

static void do_write(void* arg) {
    struct myfs_file_rwctx_s* ctx = arg;
    size_t count = ctx->size / fs->unit_size + 1;
    ctx->file->status = FILE_WRITE;
    spdk_blob_io_write(ctx->file->blob, fs->channel, ctx->buf, 0, count, myfs_file_write_complete, ctx->file);
}

static void do_read(void* arg) {
    struct myfs_file_rwctx_s* ctx = arg;
    size_t count = ctx->size / fs->unit_size + 1;
    ctx->file->status = FILE_READ;
    spdk_blob_io_read(ctx->file->blob, fs->channel, ctx->buf, 0, count, myfs_file_read_complete, ctx->file);
}

static void do_close(void* arg) {
    struct myfs_file_s* myfile = arg;
    myfile->status = FILE_CLOSE;
    spdk_blob_close(myfile->blob, myfs_file_close_complete, myfile);
}

static void do_release(void* arg) {
    struct myfs_file_s* myfile = arg;
    myfile->status = FILE_RELEASE;
    spdk_blob_close(myfile->blob, myfs_file_close_complete, myfile);
}

/*
#################################
spdk file api
#################################
*/
static int myfs_file_create(struct myfs_file_s *file){
    file->done = false;
    poller(fs->thread, do_create, file, &file->done);
    return file->ret;
}
static int myfs_file_open(struct myfs_file_s *file) {
    file->done = false;
    poller(fs->thread, do_open, file, &file->done);
    return file->ret;
}
static int myfs_file_write(struct myfs_file_s *file, void* buf, size_t size) {
    assert(buf != NULL);
    assert(size > 0);

    struct myfs_file_rwctx_s ctx = {file, buf, size};
    file->done = false;
    poller(fs->thread, do_write, &ctx, &file->done);
    return file->ret;
}
static int myfs_file_read(struct myfs_file_s *file, void* buf, size_t size) {
    assert(buf != NULL);
    assert(size > 0);

    struct myfs_file_rwctx_s ctx = {file, buf, size};
    file->done = false;
    poller(fs->thread, do_read, &ctx, &file->done);
    return file->ret;
}

static off_t myfs_file_lseek(struct myfs_file_s *file, off_t offset, int whence) {
    return file->ret;
}

static int myfs_file_close(struct myfs_file_s *file) {
    file->done = false;
    poller(fs->thread, do_close, file, &file->done);
    return file->ret;
}
static int myfs_file_release(struct myfs_file_s *file) {
    file->done = false;
    poller(fs->thread, do_release, file, &file->done);
    return file->ret;
}

struct myfs_file_operation_s myfs_file_opera = {
    .create = myfs_file_create,
    .open = myfs_file_open,
    .write = myfs_file_write,
    .read = myfs_file_read,
    .lseek = myfs_file_lseek,
    .close = myfs_file_close,
    .release = myfs_file_release
};


static void myfs_bdev_event_callback(enum spdk_bdev_event_type type, struct spdk_bdev* bdev, void* event_ctx) {
    SPDK_NOTICELOG("myfs_bdev_event_callback---->\n");
    return;
}



/*
#################################
deprecated spdk file api
#################################
*/

// static void spdk_blob_close_complete(void* cb_arg, int bserrno) {
//     return;
// }

// static void spdk_bs_delete_blob_complete(void* cb_arg, int bserrno) {
//     return;
// }


// static void myfs_blob_destory(struct myfs_blob_context_t* ctx) {
//     if (ctx->channel) {
//         spdk_bs_free_io_channel(ctx->channel);
//         ctx->channel = NULL;
//     }
//     if (ctx->blob) {
//         spdk_blob_close(ctx->blob, spdk_blob_close_complete, ctx);
//     }

//     if (ctx->bs) {
//         spdk_bs_delete_blob(ctx->bs, ctx->blobid, spdk_bs_delete_blob_complete, ctx);
//     }
// }

// static void myfs_blob_read_complete(void* cb_arg, int bserrno) {
//     struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
//     SPDK_NOTICELOG("read complete %s\n", myfs_ctx->read_buff);

//     //myfs_blob_destory(myfs_ctx);
//     myfs_ctx->done = true;
//     //spdk_app_stop(0);
// }

// static void myfs_blob_read(struct myfs_blob_context_t* ctx) {
//     ctx->read_buff = spdk_malloc(ctx->io_unit_size, ALIGN_4k, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
//     memset(ctx->read_buff, 0, ALIGN_4k);

//     if (ctx->read_buff == NULL) {
//         return;
//     }

//     spdk_blob_io_read(ctx->blob, ctx->channel, ctx->read_buff, 0, 1, myfs_blob_read_complete, ctx);
// }

// static void myfs_blob_write_complete(void* cb_arg, int bserrno) {
//     struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
//     SPDK_NOTICELOG("myfs_blob_write_complete---->bserrno: %d\n", bserrno);

//     myfs_blob_read(myfs_ctx);
// }


// static void myfs_blob_write(struct myfs_blob_context_t* ctx) {
//     ctx->write_buff = spdk_malloc(ctx->io_unit_size, ALIGN_4k, NULL, SPDK_ENV_LCORE_ID_ANY, SPDK_MALLOC_DMA);
//     if (ctx->write_buff == NULL) {
//         return;
//     }
//     memset(ctx->write_buff, 'h', ctx->io_unit_size);

//     struct spdk_io_channel* channel = spdk_bs_alloc_io_channel(ctx->bs); // 这里虽然是起名字叫alloc 但是其实在app init里面就已经分配好了 这里都是直接拿来用
//     ctx->channel = channel;

//     if (ctx->channel == NULL) {
//         return;
//     }

//     // 写之前要把blob大小做个resize
//     spdk_blob_io_write(ctx->blob, channel, ctx->write_buff, 0, 1, myfs_blob_write_complete, ctx);
// }

// static void myfs_blob_resize_complete(void* cb_arg, int bserrno) {
//     SPDK_NOTICELOG("myfs_blob_resize_complete----> bserrno: %d\n", bserrno);
//     struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;

//     myfs_blob_write(myfs_ctx);
// }

// static void spdk_blob_open_complete(void* cb_arg, struct spdk_blob* blob, int bserrno) {
//     struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
    

//     myfs_ctx->blob = blob;

//     uint64_t numfreed = spdk_bs_free_cluster_count(myfs_ctx->bs);

//     SPDK_NOTICELOG("spdk_blob_open_complete----> numfreed: %lu\n", numfreed);

//     // 重新扩展一下大小
//     spdk_blob_resize(myfs_ctx->blob, numfreed, myfs_blob_resize_complete, myfs_ctx);

//     //myfs_blob_write(myfs_ctx);
// }


// static void spdk_blob_create_complete(void* cb_arg, spdk_blob_id blobid, int bserrno) {
//     SPDK_NOTICELOG("spdk_blob_create_complete----> blobid: %lu\n", blobid);
//     struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
//     myfs_ctx->blobid = blobid;

//     spdk_bs_open_blob(myfs_ctx->bs, blobid, spdk_blob_open_complete, myfs_ctx);
//     return;
// }

// static void spdk_bs_get_super_complete(void* cb_arg, spdk_blob_id blobid, int bserrno) {
//     struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;

//     if (bserrno) {
//         SPDK_ERRLOG("Super blob get failed, bserrno %d\n", bserrno);
//         if (bserrno == -ENOENT) {
//             spdk_bs_create_blob(myfs_ctx->bs, spdk_blob_create_complete, myfs_ctx);
//         }
//     } else {
//         spdk_bs_open_blob(myfs_ctx->bs, myfs_ctx->blobid, spdk_blob_open_complete, myfs_ctx);
//     }
// }

// static void myfs_blob_init_complete(void* cb_arg, struct spdk_blob_store* bs, int bserrno) {
//     struct myfs_blob_context_t* myfs_ctx = (struct myfs_blob_context_t*)cb_arg;
//     myfs_ctx->bs = bs;
//     myfs_ctx->io_unit_size = spdk_bs_get_io_unit_size(bs);

//     SPDK_NOTICELOG("myfs_blob_init_complete---->io_unit_size: %lu\n", myfs_ctx->io_unit_size);

// #if 0
//     spdk_bs_create_blob(myfs_ctx->bs, spdk_blob_create_complete, myfs_ctx);
// #else
//     spdk_bs_get_super(bs, spdk_bs_get_super_complete, myfs_ctx);
// #endif
// }

// static void myfs_blob_start(void* ctx) {
//     SPDK_NOTICELOG("Myfs_blob_start\n");

//     struct myfs_blob_context_t *myfs_ctx = (struct myfs_blob_context_t *)ctx;
//     struct spdk_bs_dev* bs_dev = NULL;
//     const char* bdev_name = "Malloc0";
//     strcpy(myfs_ctx->bdev_name, bdev_name);

//     //‌ 该函数的核心功能是初始化一个类文件系统（blobstore）
//     // 它允许应用程序在块设备上管理blob（类似文件的数据单元）
//     // 并提供统一接口来操作这些数据单元
//     int rc = spdk_bdev_create_bs_dev_ext(bdev_name, myfs_bdev_event_callback, NULL, &bs_dev);
//     if (rc != 0) {
//         exit(1);
//     }

//     spdk_bs_init(bs_dev, NULL, myfs_blob_init_complete, myfs_ctx);

// }

/*
#################################
spdk file system api
#################################
*/

static void json_app_load_done(int rc, void *cb_arg) {
    bool *done = cb_arg;
    *done = true;
}

static void setup_spdk_json_app(void* cb_arg) {
    spdk_subsystem_init_from_json_config_temp(json_file, SPDK_DEFAULT_RPC_ADDR, json_app_load_done, cb_arg, true);
}

static int myfs_spdk_env_init(void) {
    struct spdk_env_opts opts = {};

    spdk_env_opts_init(&opts);
    opts.name = "myfs_blob";
    opts.opts_size = sizeof(opts);
    // opts.json_config_file = argv[1];
    // opts.reactor_mask = "0x1";
    opts.no_huge = true;
    opts.mem_size = 4096;  

    int rc = spdk_env_init(&opts);
    if (rc != 0) {
        SPDK_ERRLOG("Unable to initalize SPDK env\n");
    }

    SPDK_NOTICELOG("SPDK environment initialized\n");

    // set log
    spdk_log_set_print_level(SPDK_LOG_NOTICE);
    spdk_log_set_level(SPDK_LOG_NOTICE);
    spdk_log_open(NULL);


    //set thread
    spdk_thread_lib_init(NULL, 0);
    SPDK_NOTICELOG("Thread library initialized\n");


    fs->thread = spdk_thread_create("myfs", NULL);
    if (!fs->thread) {
        SPDK_ERRLOG("Failed to create SPDK thread\n");
        return -1;
    }
    spdk_set_thread(fs->thread);

    // set poller
    bool done = false;
    poller(fs->thread, setup_spdk_json_app, &done, &done);

    
    return 0;
}

static void myfs_blobstore_init_complete(void* cb_arg, struct spdk_blob_store* bs, int bserrno) {
    struct myfs_s* fs = cb_arg;

    fs->bs = bs;
    fs->unit_size = spdk_bs_get_io_unit_size(bs);
    fs->channel = spdk_bs_alloc_io_channel(bs);
    if (fs->channel == NULL) {
        return;
    }

    fs->finished = true;
}


static void myfs_alloc(void* arg) {
    struct myfs_s* fs = arg;
    struct spdk_bs_dev* bs_dev = NULL;
    const char* bdev_name = "Malloc0";

    int rc = spdk_bdev_create_bs_dev_ext(bdev_name, myfs_bdev_event_callback, NULL, &bs_dev);
    if (rc != 0) {
        exit(1);
    }
    fs->bs_dev = bs_dev;
    
    spdk_bs_init(bs_dev, NULL, myfs_blobstore_init_complete, fs);
    
}

static void myfs_blobstore_destory_complete(void* arg, int bserrno) {
    struct myfs_s* fs = arg;
    fs->finished = true;
}

static void myfs_free(void* arg) {
    struct myfs_s* fs = arg;
    //channel
    spdk_bs_free_io_channel(fs->channel);
    //bs
    spdk_bs_destroy(fs->bs, myfs_blobstore_destory_complete, fs);
    //bs_dev

    SPDK_NOTICELOG("myfs_free--->\n");
}

struct myfs_operation_s myfs_opera = {
    .alloc = myfs_alloc,
    .free = myfs_free
};

/*
#################################
vfs external api
#################################
*/

int myfs_create(const char *filename, int flag) {
    int fd = get_fd_from_bitmap();
    struct myfs_file_s* file = (struct myfs_file_s*)malloc(sizeof(struct myfs_file_s));
    memset(file, 0, sizeof(struct myfs_file_s));
    file->fs = fs;
    myfs_file_create(file);
    files[fd] = file;
    return fd;
}

int myfs_open(const char *filename, int flag) {
    int fd = get_fd_from_bitmap();
    struct myfs_file_s* file = (struct myfs_file_s*)malloc(sizeof(struct myfs_file_s));
    memset(file, 0, sizeof(struct myfs_file_s));
    file->fs = fs;
    myfs_file_open(file);
    files[fd] = file;
    return fd;
}

size_t myfs_write(int fd, void *buf, size_t count) {
    struct myfs_file_s* file = files[fd];
    myfs_file_write(file, buf, count);
    return file->ret;
}

size_t myfs_read(int fd, void *buf, size_t count) {
    struct myfs_file_s* file = files[fd];
    myfs_file_read(file, buf, count);
    return file->ret;
}

int myfs_close(int fd) {
    struct myfs_file_s* file = files[fd];
    myfs_file_close(file);
    set_fd_to_bitmap(fd);
    return file->ret;
}

static int myfs_setup(void) {
    struct myfs_s* myfs = (struct myfs_s*)malloc(sizeof(struct myfs_s));
    assert(myfs != NULL);
    memset(myfs, 0, sizeof(struct myfs_s));
    fs = myfs;
    myfs_spdk_env_init();
    SPDK_NOTICELOG("spdk env init--->\n");

    myfs->finished = false;
    poller(fs->thread, myfs_alloc, myfs, &(myfs->finished));
    return 0;
}

static int myfs_destory(void) {
    fs->finished = false;
    poller(fs->thread, myfs_free, fs, &(fs->finished));
    free(fs);
    fs = NULL;
    return 0;
}

/*
#################################
posix api
#################################
*/

// hook
typedef int (*creat_t)(const char* pathname, mode_t mode);
creat_t creat_f = NULL;

typedef int (*open_t)(const char* pathname, int flags);
open_t open_f = NULL;

typedef ssize_t (*read_t)(int fd, void* buf, size_t count);
read_t read_f = NULL;

typedef ssize_t (*write_t)(int fd, const void* buf, size_t count);
write_t write_f = NULL;

typedef off_t (*lseek_t)(int fd, off_t offset, int whence);
lseek_t lseek_f = NULL;

typedef int (*close_t)(int fd);
close_t close_f = NULL;

int creat(const char* pathname, mode_t mode) {
    int ret = 0;
    if (!initilized) {
        if (fs == NULL) {
            myfs_setup();
        }
        initilized = true;
        ret = myfs_create(pathname, mode);
    } else {
        ret = syscall(__NR_creat, pathname, mode);
    }

    return ret;
}

int open(const char* pathname, int flags, ...) {
    int ret = 0;
    if (initilized) {
        if (fs == NULL) {
            myfs_setup();
        }
        ret = myfs_open(pathname, flags);
    } else {
        ret = syscall(__NR_open, pathname, flags);
    }
    return ret;
}

ssize_t read(int fd, void* buf, size_t count) {
    ssize_t ret = 0;
    struct myfs_file_s* file = files[fd];
    if (initilized && file) {
        ret = myfs_read(fd, buf, count);
    } else {
        ret = syscall(__NR_read, fd, buf, count);
    }
    return ret;
}

ssize_t write(int fd, const void* buf, size_t count) {
    ssize_t ret = 0;
    struct myfs_file_s* file = files[fd];
    if (initilized && file) {
        ret = myfs_write(fd, buf, count);
    } else {
        ret = syscall(__NR_write, fd, buf, count);
    }
    return ret;
}

off_t lseek(int fd, off_t offset, int whence) {
    return 0;
}

int close(int fd) {
    int ret = 0;
    struct myfs_file_s* file = files[fd];
    if (initilized && file) {
        ret = myfs_close(fd);
    } else {
        ret = syscall(__NR_close, fd);
    }
    return ret;
}

/*
#################################
main and test
#################################
*/

#if 0
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
#elif 1
int main(int argc, char* argv[]) {
    // struct myfs_s myfs;
    // fs = &myfs;
    // myfs_spdk_env_init();
    // SPDK_NOTICELOG("spdk env init--->\n");

#if 0
    struct myfs_blob_context_t *myfs_ctx = calloc(1, sizeof(struct myfs_blob_context_t));
    if (myfs_ctx != NULL) {
        myfs_ctx->done = false;
        poller(fs->thread, myfs_blob_start, myfs_ctx, &myfs_ctx->done);
    }
#elif 0
    for (int i = 0; i < 1; i++) {
        myfs.finished = false;
        poller(fs->thread, myfs_alloc, &myfs, &myfs.finished);

        struct myfs_file_s file = {0};
        memset(&file, 0, sizeof(struct myfs_file_s));
        file.fs = fs;

        myfs_file_create(&file);

        char buffer[1024] = {0};
        memset(buffer, 'A', 1024);
        myfs_file_write(&file, buffer, 1024);

        memset(buffer, 0, 1024);
        myfs_file_read(&file, buffer, 1024);
        SPDK_NOTICELOG("myfs_file_read---> \n%s\n", buffer);

        myfs_file_release(&file);

        myfs.finished = false;
        poller(fs->thread, myfs_free, &myfs, &myfs.finished);
        SPDK_NOTICELOG("end i: %d--->\n", i);
    }
#elif 0
    myfs.finished = false;
    poller(fs->thread, myfs_alloc, &myfs, &myfs.finished);

    int fd = myfs_create("mytest.txt", O_CREAT);
    //fd = myfs_open("mytest.txt", O_RDWR);

    char buffer[1024] = {0};
    memset(buffer, 'A', 1024);
    myfs_write(fd, buffer, 1024);

    //myfs_lseek(fd, 0, SEEK_SET);

    memset(buffer, 0, 1024);
    myfs_read(fd, buffer, 1024);
    SPDK_NOTICELOG("myfs_file_read---> \n%s\n", buffer);

    myfs_close(fd);

    myfs.finished = false;
    poller(fs->thread, myfs_free, &myfs, &myfs.finished);   

#elif 1
    int fd = creat("mytest.txt", 0777);

    char buffer[129] = {0};
    memset(buffer, 'H', 128);
    int ret = write(fd, buffer, 128);
    printf("ret = %d\n", ret);

    memset(buffer, 0, 128);
    ret = read(fd, buffer, 128);
    printf("ret = %d\n", ret);

    printf("%s\n", buffer);

    close(fd);
#endif
    return 0;
}


#endif