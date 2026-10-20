/* SPDX-License-Identifier: BSD-3-Clause */

#include <lib/firmware_handoff.h>
#include <lib/transfer_list_validate.h>
#include <lib/utils_def.h>
#include <plat/common/platform.h>

#pragma weak plat_get_transfer_list

void *plat_get_transfer_list(size_t *available)
{
	*available = 0U;
	return NULL;
}

struct transfer_list_header *firmware_handoff_get(size_t *available)
{
	size_t bound = 0U;
	void *tl = plat_get_transfer_list(&bound);

	if (available == NULL || transfer_list_validate(tl, bound) != 0) {
		return NULL;
	}
	*available = bound;
	return tl;
}
