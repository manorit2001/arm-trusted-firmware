/* SPDX-License-Identifier: BSD-3-Clause */

#include <errno.h>
#include <stdint.h>

#include <lib/transfer_list_validate.h>

static uint32_t read_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int transfer_list_validate(const void *data, size_t available)
{
	const uint8_t *buf = data;
	uint32_t used, total;
	size_t off;
	uint8_t checksum = 0U;

	if (buf == NULL || ((uintptr_t)buf & 7U) != 0U || available < 24U) {
		return -EINVAL;
	}
	used = read_le32(buf + 8U);
	total = read_le32(buf + 12U);
	if (read_le32(buf) != 0x4a0fb10bU || buf[5] != 2U ||
	    buf[6] < 24U || buf[7] < 3U || buf[7] >= sizeof(uintptr_t) * 8U ||
	    used < buf[6] ||
	    used > total || total > available ||
	    (total & 7U) != 0U || (read_le32(buf + 16U) & ~1U) != 0U ||
	    read_le32(buf + 20U) != 0U) {
		return -EINVAL;
	}
	if ((read_le32(buf + 16U) & 1U) != 0U) {
		for (size_t i = 0U; i < used; i++) {
			checksum += buf[i];
		}
		if (checksum != 0U) {
			return -EINVAL;
		}
	}

	off = ((size_t)buf[6] + 7U) & ~(size_t)7U;
	while (off < used) {
		const uint8_t *entry = buf + off;
		uint32_t size;

		if (used - off < 8U) {
			return -EINVAL;
		}
		size = read_le32(entry + 4U);
		if (entry[3] < 8U || entry[3] > used - off ||
		    size > used - off - entry[3]) {
			return -EINVAL;
		}
		off += entry[3] + size;
		if (off == used) {
			return 0;
		}
		/* Check padding before rounding, including on 32-bit hosts. */
		if (((8U - (off & 7U)) & 7U) > used - off) {
			return -EINVAL;
		}
		off = (off + 7U) & ~(size_t)7U;
	}
	return off == used ? 0 : -EINVAL;
}
