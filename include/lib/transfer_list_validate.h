/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef TRANSFER_LIST_VALIDATE_H
#define TRANSFER_LIST_VALIDATE_H

#include <stddef.h>

/* Validate the complete list against an independently known memory extent. */
int transfer_list_validate(const void *data, size_t available);

#endif /* TRANSFER_LIST_VALIDATE_H */
