


#include "spdk/stdinc.h"
#include "spdk/thread.h"
#include "spdk/env.h"
#include "spdk/event.h"
#include "spdk/string.h"

#include "spdk/log.h"
#include "spdk/sock.h"
#include "spdk_server.h"



//
#define ADDR_STR_LEN		INET6_ADDRSTRLEN
#define BUFFER_SIZE			1024
#define MAX_TOKENS          16

// static char *g_host;
// static int g_port;
// static char *g_sock_impl_name;
static bool g_running;


typedef enum {
	KVS_CMD_START = 0,
	KVS_CMD_BSET = KVS_CMD_START,
	KVS_CMD_BGET,
	KVS_CMD_BDEL,
	KVS_CMD_BMOD,
	KVS_CMD_RSET,
	KVS_CMD_RGET,
	KVS_CMD_RDEL,
	KVS_CMD_RMOD,
	KVS_CMD_COUNT
} kvs_cmd_t;

const char *commands[] = {
	"BSET", "BGET", "BDEL", "BMOD",
	"RSET", "RGET", "RDEL", "RMOD",
};



static int kvs_split_tokens(char** tokens, char* msg) {
	int count = 0;
	char *token = strtok(msg, " ");
	while (token != NULL) {
		tokens[count++] = token;
		token = strtok(NULL, " ");
	}

	return count;
}

static int kvs_proto_parser(char *msg, char **tokens, int count) {
	if (msg == NULL || tokens == NULL || count <= 0) return -1;
	int cmd = 0;
	for(cmd = 0; cmd < KVS_CMD_COUNT; cmd++) {
		if (strcmp(tokens[0], commands[cmd]) == 0) {
			break;
		}
	}
	switch (cmd) {
		case KVS_CMD_BSET:
			break;
		case KVS_CMD_BGET:
			break;
		case KVS_CMD_BDEL:
			break;
		case KVS_CMD_BMOD:
			break;
		case KVS_CMD_RSET:
			break;
		case KVS_CMD_RGET:
			break;
		case KVS_CMD_RDEL:
			break;
		case KVS_CMD_RMOD:
			break;
	}
	return 0;
}

static int kvs_proto_process(char *msg, ssize_t len) {
	char *tokens[MAX_TOKENS] = { 0 };
	int count = kvs_split_tokens(tokens, msg);
	for (int i = 0; i < count; i++) {
		printf("token %d : %s\n", i, tokens[i]);
	}
	return kvs_proto_parser(msg, tokens, count);
}

/*
#############
spdk network
#############
*/

struct server_context_t {

	char *host;
	int port;
	char *sock_impl_name;

	int bytes_in;
	int bytes_out;
	
	struct spdk_sock *sock;
	struct spdk_sock_group *group;

};

// printf();
// debug

static void spdk_server_shutdown_callback(void) {

	g_running = false;

}

// -H 0.0.0.0 -P 8888 
// static int spdk_server_app_parse(int ch, char *arg) {

// 	printf("spdk_server_app_parse: %s\n", arg);
// 	switch (ch) {

// 	case 'H':
// 		g_host = arg;
// 		break;

// 	case 'P':
// 		g_port = spdk_strtol(arg, 10);
// 		if (g_port < 0) {
// 			SPDK_ERRLOG("Invalid port ID\n");
// 			return g_port;
// 		}
// 		break;

// 	case 'N':
// 		g_sock_impl_name = arg; //-N posix or -N uring
// 		break;

// 	default:
// 		return -EINVAL;
// 	}

// 	return 0;
// }


// help
// static void spdk_server_app_usage(void) {

// 	printf("-H host_addr \n");
// 	printf("-P host_port \n");
// 	printf("-N sock_impl \n");

// }

static void spdk_server_callback(void *arg, struct spdk_sock_group *group, struct spdk_sock *sock) {

	struct server_context_t *ctx = arg;
	char buf[BUFFER_SIZE] = { 0 };
	struct iovec iov;
	
	ssize_t n =  spdk_sock_recv(sock, buf, sizeof(buf));
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			SPDK_ERRLOG("spdk_sock_recv failed, errno %d: %s",
				errno, spdk_strerror(errno));
			return ;
		}
		
		SPDK_ERRLOG("spdk_sock_recv failed, errno %d: %s",
				errno, spdk_strerror(errno));
	} else if (n == 0) {

		SPDK_NOTICELOG("Connection closed\n");
		spdk_sock_group_remove_sock(group, sock);
		spdk_sock_close(&sock);

		return ;

	} else { 
		printf("ret: %ld, recv: %s\n", n, buf);
		kvs_proto_process(buf, n);
		ctx->bytes_in += n;

		iov.iov_base = buf;
		iov.iov_len = n;

		int n = spdk_sock_writev(sock, &iov, 1);
		if (n > 0) {
			ctx->bytes_out += n;
		}
		return ;
	}  

	// assert(0);
	
	return ;
}


// 
static int spdk_server_accept(void *arg) {

	struct server_context_t *ctx = arg;
	char saddr[ADDR_STR_LEN], caddr[ADDR_STR_LEN];
	uint16_t sport, cport;
	int count = 0;

	// printf("spdk_server_accept\n");
	if (!g_running) {
		//...
	} 

	while (1) {
		// accept
		struct spdk_sock *client_sock = spdk_sock_accept(ctx->sock);
		if (client_sock == NULL)	{
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				
			}
			break;
		}

		//getpeeraddr();
		int rc = spdk_sock_getaddr(client_sock, saddr, sizeof(saddr), &sport, 
				caddr, sizeof(caddr), &cport);
		if (rc < 0) {

			SPDK_ERRLOG("Cannot get connection address\n");
			spdk_sock_close(&ctx->sock);
			return SPDK_POLLER_IDLE;

		}

		rc = spdk_sock_group_add_sock(ctx->group, client_sock, 
			spdk_server_callback, ctx);
		if (rc < 0) {

			SPDK_ERRLOG("Cannot get connection address\n");
			spdk_sock_close(&client_sock);
			return SPDK_POLLER_IDLE;

		}

		count ++;

	}

	return count > 0 ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}


static int spdk_server_group_poll(void *arg) {

	struct server_context_t *ctx = arg;

	int rc = spdk_sock_group_poll(ctx->group);
	if (rc < 0) {
		SPDK_ERRLOG("Failed to poll sock_group = %p\n", ctx->group);
	}
	return rc > 0 ? SPDK_POLLER_BUSY : SPDK_POLLER_IDLE;
}


// spdk sock 
static int spdk_server_listen(struct server_context_t *ctx) {

	ctx->sock = spdk_sock_listen(ctx->host, ctx->port, ctx->sock_impl_name);
	if (ctx->sock == NULL) {
		SPDK_ERRLOG("Cannot create server socket");
		return -1;
	}

	ctx->group = spdk_sock_group_create(NULL); //epoll

	g_running = true;

	SPDK_POLLER_REGISTER(spdk_server_accept, ctx, 2000 * 1000);
	SPDK_POLLER_REGISTER(spdk_server_group_poll, ctx, 0);

	printf("spdk_server_listen\n");

	return 0;
}


static void sdpk_server_start(void *arg) {

	struct server_context_t *ctx = arg;
	
	printf("sdpk_server_start\n");
	int rc = spdk_server_listen(ctx);
	if (rc) {
		spdk_app_stop(-1);
	}

	return ;
}

#if 0
int main(int argc, char *argv[]) {

	struct spdk_app_opts opts = {};

	spdk_app_opts_init(&opts, sizeof(opts));
	opts.name = "spdk_server";
	opts.shutdown_cb = spdk_server_shutdown_callback;
    opts.reactor_mask = "0x1";  // 使用第一个核
    opts.mem_size = 512;        // 512MB内存
    opts.no_huge = true;

	printf("spdk_app_parse_args\n");
	spdk_app_parse_args(argc, argv, &opts, "H:P:N:SVzZ", NULL,
		spdk_server_app_parse, spdk_server_app_usage);

	printf("spdk_app_parse_args 11\n");
	struct server_context_t server_context = {};
	// server_context.host = g_host;
	// server_context.port = g_port;
	// server_context.sock_impl_name = g_sock_impl_name;
	
	server_context.host = "0.0.0.0";
	server_context.port = 8888;
	server_context.sock_impl_name = "posix";
	printf("host: %s, port: %d, impl_name: %s\n", g_host, g_port, g_sock_impl_name);
	//sdpk_server_start(&server_context);

	int rc = spdk_app_start(&opts, sdpk_server_start, &server_context); // ?
	if (rc) {
		SPDK_ERRLOG("Error starting application\n");
	}

	spdk_app_fini();
}
#elif 1
void start_server(void) {
	struct spdk_app_opts opts = {};

	spdk_app_opts_init(&opts, sizeof(opts));
	opts.name = "spdk_server";
	opts.shutdown_cb = spdk_server_shutdown_callback;
    opts.reactor_mask = "0x1";  // 使用第一个核
    opts.mem_size = 512;        // 512MB内存
    opts.no_huge = true;

	struct server_context_t server_context = {};
	
	server_context.host = "0.0.0.0";
	server_context.port = 8888;
	server_context.sock_impl_name = "posix";
	// printf("host: %s, port: %d, impl_name: %s\n", g_host, g_port, g_sock_impl_name);

	int rc = spdk_app_start(&opts, sdpk_server_start, &server_context); // ?
	if (rc) {
		SPDK_ERRLOG("Error starting application\n");
	}

	spdk_app_fini();
	return;	
}

#endif


