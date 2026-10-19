/* SPDX-License-Identifier: BSD-3-Clause */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <lib/transfer_list_validate.h>

#include <k3_handoff.h>

static uint32_t read_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int k3_handoff_parse(const void *data, size_t available, uint32_t consumer,
		     struct k3_handoff_config *config)
{
	const uint8_t *buf = data;
	struct k3_handoff_config selected = {0};
	uint32_t used, seen = 0U;
	size_t off;
	bool found = false;

	if (config == NULL || consumer == 0U || consumer > 6U ||
	    transfer_list_validate(data, available) != 0) {
		return -EINVAL;
	}

	used = read_le32(buf + 8U);
	if (buf[6] != 24U || buf[7] != 3U || read_le32(buf + 16U) != 1U) {
		return -EINVAL;
	}

	off = ((size_t)buf[6] + 7U) & ~(size_t)7U;
	while (off < used) {
		const uint8_t *entry = buf + off;
		const uint8_t *payload;
		uint32_t tag, size, id, host, rx, count, roles = 0U;
		uint32_t tx = 0U, threads[3];
		size_t end;

		tag = read_le32(entry) & 0xffffffU;
		size = read_le32(entry + 4U);
		end = off + entry[3] + size;
		off = (end + 7U) & ~(size_t)7U;
		if (tag != K3_HANDOFF_SEC_PROXY_TAG) {
			continue;
		}
		if (size < 4U) {
			return -EINVAL;
		}
		payload = entry + entry[3];
		id = read_le32(payload);
		if (id == 0U) {
			return -EINVAL;
		}
		if (id > 6U) {
			continue;
		}
		if (size < 16U) {
			return -EINVAL;
		}
		if ((seen & (1U << id)) != 0U) {
			return -EINVAL;
		}
		seen |= 1U << id;
		host = read_le32(payload + 4U);
		rx = read_le32(payload + 8U);
		count = read_le32(payload + 12U);
		if (host > UINT8_MAX || rx > UINT16_MAX || count == 0U ||
		    count > 3U || size != 16U + count * 8U) {
			return -EINVAL;
		}
		for (uint32_t i = 0U; i < count; i++) {
			uint32_t role = read_le32(payload + 16U + i * 8U);
			uint32_t thread = read_le32(payload + 20U + i * 8U);

			if (role > 2U || thread > UINT16_MAX || thread == rx ||
			    (roles & (1U << role)) != 0U) {
				return -EINVAL;
			}
			for (uint32_t j = 0U; j < i; j++) {
				if (thread == threads[j]) {
					return -EINVAL;
				}
			}
			threads[i] = thread;
			roles |= 1U << role;
			if (role == 0U || role == 2U) {
				tx = thread;
			}
		}
		if (roles != 1U && roles != 6U) {
			return -EINVAL;
		}
		if (id == consumer) {
			selected.host_id = (uint8_t)host;
			selected.rx_thread = (uint16_t)rx;
			selected.tx_thread = (uint16_t)tx;
			found = true;
		}
	}
	if (!found) {
		return -ENOENT;
	}
	*config = selected;
	return 0;
}
