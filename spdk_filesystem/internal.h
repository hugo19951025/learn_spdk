#ifndef INTERNAL_H
#define INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#define SPDK_DEFAULT_RPC_ADDR "/var/tmp/spdk.sock"



typedef void (*spdk_subsystem_init_fn)(int rc, void *ctx);

/**
 * Like spdk_subsystem_init, but additionally configure each subsystem using the provided JSON config
 * file. This will automatically start a JSON RPC server and then stop it.
 *
 * \param json_config_file Path to a JSON config file.
 * \param rpc_addr Path to a unix domain socket to send configuration RPCs to.
 * \param cb_fn Function called when the process is complete.
 * \param cb_arg User context passed to cb_fn.
 * \param stop_on_error Whether to stop initialization if one of the JSON RPCs fails.
 */
void spdk_subsystem_init_from_json_config_temp(const char *json_config_file, const char *rpc_addr,
		spdk_subsystem_init_fn cb_fn, void *cb_arg,
		bool stop_on_error);


#ifdef __cplusplus
}
#endif

#endif
