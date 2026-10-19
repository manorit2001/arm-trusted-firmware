# SPDX-License-Identifier: BSD-3-Clause
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
LIBTL_PATH ?= $(ROOT)/contrib/libtl
O ?= .
HOSTCC ?= cc
CPPFLAGS += -I$(ROOT)/include -I$(LIBTL_PATH)/include
CFLAGS += -std=gnu11 -Wall -Wextra -Werror

.PHONY: check
check: $(O)/test_validate
	$(O)/test_validate

$(O)/test_validate: $(ROOT)/tests/lib/transfer_list/test_validate.c \
		$(ROOT)/lib/transfer_list/transfer_list_validate.c \
		$(LIBTL_PATH)/src/generic/transfer_list.c \
		$(LIBTL_PATH)/src/generic/logging.c
	@mkdir -p $(@D)
	$(HOSTCC) $(CPPFLAGS) $(CFLAGS) $^ -o $@
