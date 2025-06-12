/*
 * Texas Instruments Mailbox Driver
 *
 * Copyright (C) 2024-2025 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>
#include <lib/utils.h>
#include <lib/utils_def.h>
#include <ti_sci_transport.h>

#include <platform_def.h>

/*
 * TI_MAILBOX_RX/TX_BASE and the MAILBOX_MAX_MESSAGE_SIZE values are expected
 * to come from platform specific header file ie. platform_def.h
 * MAILBOX_*_START_REGION defines the start of the memory segment used to
 * exchange messages with TIFS.  Each mailbox memory segment is divided into
 * slots of 64 bytes with a total of five slots as described in the TISCI
 * documentation.  The FIFO associated with a channel can hold at most four
 * message pointers.
 */

#define MAILBOX_SLOT_SIZE       U(64)
#define MAILBOX_NUM_SLOTS       U(5)

#define TI_MAILBOX_SYSC		UL(0x10)
#define TI_MAILBOX_MSG		UL(0x40)
#define TI_MAILBOX_FIFO_STATUS	UL(0x80)
#define TI_MAILBOX_MSG_STATUS		UL(0xc0)

/*
 * Function to poll for mailbox rx messages
 * IRQ model is currently not in scope of this driver
 */
static int8_t ti_mailbox_poll_rx_status(void)
{
	uint32_t num_messages_pending = 0U;
	uint32_t retry_count = 100U;

	/*
	 * Keep polling till we get a message for 100 times
	 * with intervals of 10 milliseconds.
	 */
	while (num_messages_pending == 0U) {
		num_messages_pending = mmio_read_32(TI_MAILBOX_RX_BASE + TI_MAILBOX_MSG_STATUS);
		if (retry_count-- == 0U) {
			return -ETIMEDOUT;
		}
		mdelay(10);
	}
	return 0;
}

int ti_sci_transport_clear_rx_thread(enum ti_sci_transport_chan_id id)
{
	/* MSG_STATUS tells us how many pending messages */
	uint32_t try_count = mmio_read_32(TI_MAILBOX_RX_BASE + TI_MAILBOX_MSG_STATUS);

	/* Run the loop till the status register is cleared */
	while (mmio_read_32(TI_MAILBOX_RX_BASE + TI_MAILBOX_MSG_STATUS) != 0U) {
		WARN("Clearing message from mailbox FIFO\n");
		/* The act of reading the mailbox msg itself clears it */
		mmio_read_32(TI_MAILBOX_RX_BASE + TI_MAILBOX_MSG);
		/*
		 * The try_count is kept independent of the value of the status register
		 * because if at any point a new mailbox message arrives while this loop
		 * is in progress, we would want to know that message arrived and not clear
		 * it. We would rather print the error than clear the message thus indicating
		 * that the system is probably in a bad/async state.
		 */
		if (!(try_count--)) {
			ERROR("Could not clear all messages from mailbox FIFO\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

int ti_sci_transport_send(enum ti_sci_transport_chan_id id, const struct ti_sci_msg *msg)
{
	static unsigned int tx_slot;
	uint32_t num_bytes;
	void *dst_ptr;

	assert(msg != NULL);

	num_bytes = msg->len;

	/* Ensure there is room for another pointer in the FIFO */
	if (mmio_read_32(TI_MAILBOX_TX_BASE + TI_MAILBOX_FIFO_STATUS) >= 4U) {
		ERROR("Mailbox FIFO is full!\n");
		return -EBUSY;
	}

	if (num_bytes > MAILBOX_MAX_MESSAGE_SIZE) {
		ERROR("message length %lu > max msg size\n", msg->len);
		return -EINVAL;
	}

	/* Select next slot within the TX memory region */
	dst_ptr = (void *)(MAILBOX_TX_START_REGION +
			   (tx_slot % MAILBOX_NUM_SLOTS) * MAILBOX_SLOT_SIZE);
	tx_slot++;

	/* Copy data to shared memory so that TIFS can read it */
	memmove(dst_ptr, msg->buf, num_bytes);
	flush_dcache_range((uintptr_t)dst_ptr, MAILBOX_SLOT_SIZE);

	mmio_write_32(TI_MAILBOX_TX_BASE + TI_MAILBOX_MSG,
		      (uint32_t)(uintptr_t)dst_ptr);

	return 0;
}

int ti_sci_transport_recv(enum ti_sci_transport_chan_id id, struct ti_sci_msg *msg)
{
	uint32_t num_bytes;
	uint64_t rcv_addr;

	assert(msg != NULL);

	num_bytes = msg->len;

	if (ti_mailbox_poll_rx_status() == -ETIMEDOUT) {
		ERROR("Timeout waiting for receive\n");
		return -ETIMEDOUT;
	}

	rcv_addr = mmio_read_32(TI_MAILBOX_RX_BASE + TI_MAILBOX_MSG);

	if (rcv_addr < MAILBOX_RX_START_REGION ||
	    rcv_addr >= MAILBOX_RX_START_REGION +
			    MAILBOX_NUM_SLOTS * MAILBOX_SLOT_SIZE ||
	    (rcv_addr - MAILBOX_RX_START_REGION) % MAILBOX_SLOT_SIZE) {
		ERROR("message address %lu is not valid\n", rcv_addr);
		return -EINVAL;
	}

	if (num_bytes > MAILBOX_MAX_MESSAGE_SIZE) {
		ERROR("message length %lu > max msg size\n", msg->len);
		return -EINVAL;
	}

	inv_dcache_range(rcv_addr, MAILBOX_SLOT_SIZE);
	memmove(msg->buf, (uint8_t *)rcv_addr, num_bytes);

	return 0;
}
