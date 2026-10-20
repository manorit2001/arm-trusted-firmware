/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef FIRMWARE_HANDOFF_H
#define FIRMWARE_HANDOFF_H

#include <stddef.h>

struct transfer_list_header;

/* Discover and validate a list using the platform's independently known bound. */
struct transfer_list_header *firmware_handoff_get(size_t *available);

#endif /* FIRMWARE_HANDOFF_H */
