/* SPDX-License-Identifier: BSD-3-Clause */

/* The platform supplies a capacity and reserves the corresponding memory. */
__TRANSFER_LIST_START__ = ALIGN(__BL31_END__, PAGE_SIZE);
__TRANSFER_LIST_END__ = __TRANSFER_LIST_START__ + PLAT_TRANSFER_LIST_SIZE;
ASSERT(__TRANSFER_LIST_END__ <= BL31_LIMIT,
	"No room for the transfer list within BL31_LIMIT")
