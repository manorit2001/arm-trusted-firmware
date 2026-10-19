/* SPDX-License-Identifier: BSD-3-Clause */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <logging.h>
#include <transfer_list.h>
#include <lib/transfer_list_validate.h>

int main(void)
{
	_Alignas(8) uint8_t data[256];
	const uint8_t payload[9] = { 1, 2, 3 };
	struct transfer_list_header *tl;

	libtl_register_logger(NULL);
	for (size_t len = 1; len <= sizeof(payload); len++) {
		tl = transfer_list_init(data, sizeof(data));
		assert(tl != NULL);
		assert(transfer_list_add(tl, 2, len, payload) != NULL);
		assert(tl->size == sizeof(*tl) +
		       sizeof(struct transfer_list_entry) + len);
		assert(transfer_list_validate(tl, sizeof(data)) == 0);
		assert(transfer_list_validate(tl, sizeof(data) - 1) == -EINVAL);
		assert(transfer_list_add(tl, 3, len, payload) != NULL);
		assert(transfer_list_validate(tl, sizeof(data)) == 0);
		tl->size = (tl->size + 7U) & ~7U;
		transfer_list_update_checksum(tl);
		assert(transfer_list_validate(tl, sizeof(data)) == 0);
		tl->checksum++;
		assert(transfer_list_validate(tl, sizeof(data)) == -EINVAL);
	}
	return 0;
}
