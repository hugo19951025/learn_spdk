
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

#include "spdk/thread.h"

#include "internal.h"


#include <stdio.h>
#include <stdint.h>


#define BDEV_NAME_LENGTH 512
#define ALIGN_4k 0x1000
#define SPDK_RPC_STARTUP	0x1
#define SPDK_RPC_RUNTIME	0x2

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

};

typedef struct myfs_s {
    struct spdk_thread* thread;
}myfs_t;

struct myfs_s* fs = NULL;
static const int POLLER_MAX_TIME = 1e8;
static const char* json_file = "/home/hugo/learn_spdk/spdk_filesystem/myfs.json";

#if 0
struct load_json_config_ctx;
typedef void (*client_resp_handler)(struct load_json_config_ctx *,
				    struct spdk_jsonrpc_client_response *);

#define RPC_SOCKET_PATH_MAX SPDK_SIZEOF_MEMBER(struct sockaddr_un, sun_path)

/* 1s connections timeout */
#define RPC_CLIENT_CONNECT_TIMEOUT_US (1U * 1000U * 1000U)

/*
 * Currently there is no timeout in SPDK for any RPC command. This result that
 * we can't put a hard limit during configuration load as it most likely randomly fail.
 * So just print WARNLOG every 10s. */
#define RPC_CLIENT_REQUEST_TIMEOUT_US (10U * 1000 * 1000)


struct load_json_config_ctx {
	/* Thread used during configuration. */
	struct spdk_thread *thread;
	spdk_subsystem_init_fn cb_fn;
	void *cb_arg;
	bool stop_on_error;

	/* Current subsystem */
	struct spdk_json_val *subsystems; /* "subsystems" array */
	struct spdk_json_val *subsystems_it; /* current subsystem array position in "subsystems" array */

	struct spdk_json_val *subsystem_name; /* current subsystem name */

	/* Current "config" entry we are processing */
	struct spdk_json_val *config; /* "config" array */
	struct spdk_json_val *config_it; /* current config position in "config" array */

	/* Current request id we are sending. */
	uint32_t rpc_request_id;

	/* Whole configuration file read and parsed. */
	size_t json_data_size;
	char *json_data;

	size_t values_cnt;
	struct spdk_json_val *values;

	char rpc_socket_path_temp[RPC_SOCKET_PATH_MAX + 1];

	struct spdk_jsonrpc_client *client_conn;
	struct spdk_poller *client_conn_poller;

	client_resp_handler client_resp_cb;

	/* Timeout for current RPC client action. */
	uint64_t timeout;
};

static int
rpc_client_connect_poller(void *_ctx)
{
	struct load_json_config_ctx *ctx = _ctx;
	int rc;

	rc = spdk_jsonrpc_client_poll(ctx->client_conn, 0);
	if (rc != -ENOTCONN) {
		/* We are connected. Start regular poller and issue first request */
		spdk_poller_unregister(&ctx->client_conn_poller);
		ctx->client_conn_poller = SPDK_POLLER_REGISTER(rpc_client_poller, ctx, 100);
		app_json_config_load_subsystem(ctx);
	} else {
		rc = rpc_client_check_timeout(ctx);
		if (rc) {
			app_json_config_load_done(ctx, rc);
		}

		return SPDK_POLLER_IDLE;
	}

	return SPDK_POLLER_BUSY;
}


static void
spdk_subsystem_init_from_json_config(const char *json_config_file, const char *rpc_addr,
				     spdk_subsystem_init_fn cb_fn, void *cb_arg,
				     bool stop_on_error)
{
	struct load_json_config_ctx *ctx = calloc(1, sizeof(*ctx));
	int rc;

	assert(cb_fn);
	if (!ctx) {
		cb_fn(-ENOMEM, cb_arg);
		return;
	}

	ctx->cb_fn = cb_fn;
	ctx->cb_arg = cb_arg;
	ctx->stop_on_error = stop_on_error;
	ctx->thread = spdk_get_thread();

	rc = app_json_config_read(json_config_file, ctx);
	if (rc) {
		goto fail;
	}

	/* Capture subsystems array */
	rc = spdk_json_find_array(ctx->values, "subsystems", NULL, &ctx->subsystems);
	switch (rc) {
	case 0:
		/* Get first subsystem */
		ctx->subsystems_it = spdk_json_array_first(ctx->subsystems);
		if (ctx->subsystems_it == NULL) {
			SPDK_NOTICELOG("'subsystems' configuration is empty\n");
		}
		break;
	case -EPROTOTYPE:
		SPDK_ERRLOG("Invalid JSON configuration: not enclosed in {}.\n");
		goto fail;
	case -ENOENT:
		SPDK_WARNLOG("No 'subsystems' key JSON configuration file.\n");
		break;
	case -EDOM:
		SPDK_ERRLOG("Invalid JSON configuration: 'subsystems' should be an array.\n");
		goto fail;
	default:
		SPDK_ERRLOG("Failed to parse JSON configuration.\n");
		goto fail;
	}

	/* If rpc_addr is not an Unix socket use default address as prefix. */
	if (rpc_addr == NULL || rpc_addr[0] != '/') {
		rpc_addr = SPDK_DEFAULT_RPC_ADDR;
	}

	/* FIXME: rpc client should use socketpair() instead of this temporary socket nonsense */
	rc = snprintf(ctx->rpc_socket_path_temp, sizeof(ctx->rpc_socket_path_temp), "%s.%d_config",
		      rpc_addr, getpid());
	if (rc >= (int)sizeof(ctx->rpc_socket_path_temp)) {
		SPDK_ERRLOG("Socket name create failed\n");
		goto fail;
	}

	rc = spdk_rpc_initialize(ctx->rpc_socket_path_temp, NULL);
	if (rc) {
		goto fail;
	}

	ctx->client_conn = spdk_jsonrpc_client_connect(ctx->rpc_socket_path_temp, AF_UNIX);
	if (ctx->client_conn == NULL) {
		SPDK_ERRLOG("Failed to connect to '%s'\n", ctx->rpc_socket_path_temp);
		goto fail;
	}

	rpc_client_set_timeout(ctx, RPC_CLIENT_CONNECT_TIMEOUT_US);
	ctx->client_conn_poller = SPDK_POLLER_REGISTER(rpc_client_connect_poller, ctx, 100);
	return;

fail:
	app_json_config_load_done(ctx, -EINVAL);
}


#endif


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

static bool poller(struct spdk_thread* thread, spdk_msg_fn fn, void* ctx, bool* done) {
    
    int poller_count = 0;
    spdk_thread_send_msg(thread, fn, ctx);
    do {
        spdk_thread_poll(thread, 0, 0);
    } while (!(*done) && poller_count < POLLER_MAX_TIME);

    if (!(*done) && poller_count >= POLLER_MAX_TIME) {
        SPDK_ERRLOG("work not done but time out\n");
        return false;
    }
    return true;
}

static void json_app_load_done(int rc, void *cb_arg) {
    bool *done = cb_arg;
    *done = true;
}

static void setup_spdk_json_app(void* cb_arg) {
    SPDK_NOTICELOG("setup_spdk_json_app--->start\n");
    spdk_subsystem_init_from_json_config_temp(json_file, SPDK_DEFAULT_RPC_ADDR, json_app_load_done, cb_arg, true);
    SPDK_NOTICELOG("setup_spdk_json_app--->end\n");
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
    struct myfs_s myfs;
    fs = &myfs;
    myfs_spdk_env_init();
    SPDK_NOTICELOG("spdk env init--->\n");


    struct myfs_blob_context_t *myfs_ctx = calloc(1, sizeof(struct myfs_blob_context_t));
    if (myfs_ctx != NULL) {
        bool finished = false;
        poller(fs->thread, myfs_blob_start, myfs_ctx, &finished);
    }

    SPDK_NOTICELOG("spdk myfs_blob_start--->\n");
    return 0;
}


#endif