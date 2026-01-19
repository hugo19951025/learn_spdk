
SPDK_ROOT_DIR := $(abspath $(CURDIR)/../spdk/)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

APP = myfs

CFLAGS += -g

C_SRCS := myfs.c json_config.c
HEADERS := internal.h myfs.h

SPDK_LIB_LIST = $(ALL_MODULES_LIST) event event_bdev

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk