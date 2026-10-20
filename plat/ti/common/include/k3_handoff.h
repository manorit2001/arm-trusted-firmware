/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef K3_HANDOFF_H
#define K3_HANDOFF_H

#include <stddef.h>
#include <stdint.h>

/* Experimental tag: allocate a Firmware Handoff tag before upstreaming. */
#define K3_HANDOFF_SEC_PROXY_TAG	0xfff000U
#define K3_HANDOFF_TF_A		2U
#define K3_HANDOFF_OPTEE		3U

struct k3_handoff_config {
	uint8_t host_id;
	uint16_t tx_thread;
	uint16_t rx_thread;
};

int k3_handoff_parse(const void *data, size_t available, uint32_t consumer,
		     struct k3_handoff_config *config);
void k3_handoff_init(void);
struct entry_point_info;
void k3_handoff_prepare_optee(struct entry_point_info *ep);

#endif /* K3_HANDOFF_H */
