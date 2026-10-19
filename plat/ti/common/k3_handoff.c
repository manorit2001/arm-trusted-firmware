/* SPDX-License-Identifier: BSD-3-Clause */

#include <common/bl_common.h>
#include <common/debug.h>
#include <lib/firmware_handoff.h>
#include <libfdt.h>
#include <plat/common/platform.h>
#include <ti_sci.h>
#include <ti_sci_transport.h>
#include <transfer_list.h>

#include <k3_handoff.h>

extern uint8_t __TRANSFER_LIST_START__[];

void *plat_get_transfer_list(size_t *available)
{
	*available = PLAT_TRANSFER_LIST_SIZE;
	return __TRANSFER_LIST_START__;
}

void k3_handoff_init(void)
{
	struct k3_handoff_config config;
	size_t available;
	struct transfer_list_header *tl = firmware_handoff_get(&available);

	if (tl == NULL || k3_handoff_parse(tl, available,
			     K3_HANDOFF_TF_A, &config) != 0 ||
	    ti_sci_transport_configure(config.tx_thread, config.rx_thread) != 0) {
		ERROR("Invalid K3 secure-proxy handoff configuration\n");
		panic();
	}
	ti_sci_set_host_id(config.host_id);
}

void k3_handoff_prepare_optee(struct entry_point_info *ep)
{
	size_t available;
	struct transfer_list_header *tl = firmware_handoff_get(&available);
	struct transfer_list_entry *te;
	struct k3_handoff_config config;
	uint8_t dt[128] __aligned(8) = {0};

	if (tl == NULL || k3_handoff_parse(tl, available, K3_HANDOFF_OPTEE,
			     &config) != 0 ||
	    transfer_list_check_header(tl) != TL_OPS_ALL ||
	    transfer_list_find(tl, TL_TAG_FDT) != NULL ||
	    fdt_create_empty_tree(dt, sizeof(dt)) != 0) {
		ERROR("Invalid K3 OP-TEE handoff configuration\n");
		panic();
	}

	/* OP-TEE expands this final FDT entry into the list's remaining space. */
	te = transfer_list_add(tl, TL_TAG_FDT, sizeof(dt), dt);
	if (te == NULL) {
		ERROR("No room for OP-TEE handoff FDT\n");
		panic();
	}
	if (transfer_list_set_handoff_args(tl, ep) == NULL) {
		ERROR("Cannot prepare OP-TEE handoff arguments\n");
		panic();
	}
}
