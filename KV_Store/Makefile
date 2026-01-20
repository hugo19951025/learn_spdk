SPDK_ROOT_DIR := $(abspath $(CURDIR)/../spdk/)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

APP = KVstore

C_SRCS := simple_slab.c spdk_server.c

CXX_SRCS := kv_main.cpp

HEADERS := simple_slab.h spdk_server.h

SPDK_CXX = yes

SPDK_LIB_LIST = $(ALL_MODULES_LIST) event event_bdev sock

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk

# SPDK_ROOT_DIR := $(abspath $(CURDIR)/../spdk/)
# APP = KVstore
# include $(SPDK_ROOT_DIR)/mk/nvme.libtest.mk