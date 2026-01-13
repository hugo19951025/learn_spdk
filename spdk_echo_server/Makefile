SPDK_ROOT_DIR := $(abspath $(CURDIR)/../spdk)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

APP = spdk_server

C_SRCS := spdk_server.c

SPDK_LIB_LIST = $(SOCK_MODULES_LIST)
SPDK_LIB_LIST += event sock

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk
