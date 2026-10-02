/*
 * Texas Instruments System Control Interface Driver
 *   Based on Linux and U-Boot implementation
 *
 * Copyright (C) 2018-2025 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <platform_def.h>
#include <lib/bakery_lock.h>

#include <common/debug.h>
#include <ti_sci_transport.h>

#include "ti_sci_protocol.h"
#include "ti_sci.h"

#if USE_COHERENT_MEM
__section(".tzfw_coherent_mem")
#endif
static uint8_t message_sequence;

DEFINE_BAKERY_LOCK(ti_sci_xfer_lock);

/**
 * struct ti_sci_xfer - Structure representing a message flow
 * @tx_message:	Transmit message
 * @rx_message:	Receive message
 */
struct ti_sci_xfer {
	struct ti_sci_msg tx_message;
	struct ti_sci_msg rx_message;
};

/**
 * ti_sci_setup_one_xfer() - Setup one message type
 *
 * @msg_type:	Message type
 * @msg_flags:	Flag to set for the message
 * @tx_buf:	Buffer to be sent to mailbox channel
 * @tx_message_size: transmit message size
 * @rx_buf:	Buffer to be received from mailbox channel
 * @rx_message_size: receive message size
 *
 * Helper function which is used by various command functions that are
 * exposed to clients of this driver for allocating a message traffic event.
 *
 * Return: 0 if all goes well, else appropriate error message
 */
static int ti_sci_setup_one_xfer(uint16_t msg_type, uint32_t msg_flags,
				 void *tx_buf,
				 size_t tx_message_size,
				 void *rx_buf,
				 size_t rx_message_size,
				 struct ti_sci_xfer *xfer)
{
	struct ti_sci_msg_hdr *hdr;

	/* Ensure we have sane transfer sizes */
	if (rx_message_size > TI_SCI_MAX_MESSAGE_SIZE ||
	    tx_message_size > TI_SCI_MAX_MESSAGE_SIZE ||
	    tx_message_size < sizeof(*hdr))
		return -ERANGE;

	hdr = (struct ti_sci_msg_hdr *)tx_buf;

	/* TODO: Calculate checksum */
	hdr->sec_hdr.checksum = 0;
	hdr->seq = ++message_sequence;
	hdr->type = msg_type;
	hdr->host = TI_SCI_HOST_ID;
	hdr->flags = msg_flags;
	/* Request a response if rx_message_size is non-zero */
	if (rx_message_size != 0U) {
		hdr->flags |= TI_SCI_FLAG_REQ_ACK_ON_PROCESSED;
	}

	xfer->tx_message.buf = tx_buf;
	xfer->tx_message.len = tx_message_size;

	xfer->rx_message.buf = rx_buf;
	xfer->rx_message.len = rx_message_size;

	return 0;
}

/**
 * ti_sci_get_response() - Receive response from mailbox channel
 *
 * @xfer:	Transfer to initiate and wait for response
 * @chan:	Channel to receive the response
 *
 * Return: 0 if all goes well, else appropriate error message
 */
static int ti_sci_get_response(struct ti_sci_msg *msg,
			       enum ti_sci_transport_chan_id chan)
{
	struct ti_sci_msg_hdr *hdr;
	unsigned int retry = 5;
	int ret;

	for (; retry > 0; retry--) {
		/* Receive the response */
		ret = ti_sci_transport_recv(chan, msg);
		if (ret) {
			ERROR("Message receive failed (%d)\n", ret);
			return ret;
		}

		/* msg is updated by Secure Proxy driver */
		hdr = (struct ti_sci_msg_hdr *)msg->buf;

		/* Sanity check for message response */
		if (hdr->seq == message_sequence)
			break;
		else
			WARN("Message with sequence ID %u is not expected\n", hdr->seq);
	}
	if (!retry) {
		ERROR("Timed out waiting for message\n");
		return -EINVAL;
	}

	if (msg->len > TI_SCI_MAX_MESSAGE_SIZE) {
		ERROR("Unable to handle %lu xfer (max %d)\n",
		      msg->len, TI_SCI_MAX_MESSAGE_SIZE);
		return -EINVAL;
	}

	if (!(hdr->flags & TI_SCI_FLAG_RESP_GENERIC_ACK))
		return -ENODEV;

	/* TODO: Verify checksum */
	(void)hdr->sec_hdr.checksum;

	return 0;
}

/**
 * ti_sci_do_xfer() - Do one transfer
 *
 * @xfer:	Transfer to initiate and wait for response
 *
 * Return: 0 if all goes well, else appropriate error message
 */
static int ti_sci_do_xfer(struct ti_sci_xfer *xfer)
{
	struct ti_sci_msg *tx_msg = &xfer->tx_message;
	struct ti_sci_msg *rx_msg = &xfer->rx_message;
	int ret;

	bakery_lock_get(&ti_sci_xfer_lock);

	/* Clear any spurious messages in receive queue */
	ret = ti_sci_transport_clear_rx_thread(RX_SECURE_TRANSPORT_CHANNEL_ID);
	if (ret) {
		ERROR("Could not clear response queue (%d)\n", ret);
		goto unlock;
	}

	/* Send the message */
	ret = ti_sci_transport_send(TX_SECURE_TRANSPORT_CHANNEL_ID, tx_msg);
	if (ret) {
		ERROR("Message sending failed (%d)\n", ret);
		goto unlock;
	}

	/* Get the response if requested */
	if (rx_msg->len != 0U) {
		ret = ti_sci_get_response(rx_msg, RX_SECURE_TRANSPORT_CHANNEL_ID);
		if (ret != 0U) {
			ERROR("Failed to get response (%d)\n", ret);
			goto unlock;
		}
	}

unlock:
	bakery_lock_release(&ti_sci_xfer_lock);

	return ret;
}

/**
 * ti_sci_get_revision() - Get the revision of the SCI entity
 *
 * Updates the SCI information in the internal data structure.
 *
 * @version: Structure containing the version info
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_get_revision(struct ti_sci_msg_version *version)
{
	struct ti_sci_msg_resp_version rev_info;
	struct ti_sci_msg_hdr hdr;
	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TI_SCI_MSG_VERSION, 0x0,
				    &hdr, sizeof(hdr),
				    &rev_info, sizeof(rev_info),
				    &xfer);
	if (ret) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	ret = ti_sci_do_xfer(&xfer);
	if (ret) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	memcpy(version->firmware_description, rev_info.firmware_description,
		sizeof(rev_info.firmware_description));
	version->abi_major = rev_info.abi_major;
	version->abi_minor = rev_info.abi_minor;
	version->firmware_revision = rev_info.firmware_revision;
	version->sub_version = rev_info.sub_version;
	version->patch_version = rev_info.patch_version;

	return 0;
}

/**
 * ti_sci_query_fw_caps() - Get the FW/SoC capabilities
 * @handle:		Pointer to TI SCI handle
 * @fw_caps:		Each bit in fw_caps indicating one FW/SOC capability
 *
 * Return: 0 if all went well, else returns appropriate error value.
 */
int ti_sci_query_fw_caps(uint64_t *fw_caps)
{
	struct ti_sci_msg_hdr req;
	struct ti_sci_msg_resp_query_fw_caps resp;

	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TI_SCI_MSG_QUERY_FW_CAPS, 0,
				    &req, sizeof(req),
				    &resp, sizeof(resp),
				    &xfer);
	if (ret != 0U) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	ret = ti_sci_do_xfer(&xfer);
	if (ret != 0U) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	if (fw_caps)
		*fw_caps = resp.fw_caps;

	return 0;
}

/**
 * ti_sci_device_set_state() - Set device state
 *
 * @id:		Device identifier
 * @flags:	flags to setup for the device
 * @state:	State to move the device to
 *
 * Return: 0 if all goes well, else appropriate error message
 */
static int ti_sci_device_set_state(uint32_t id, uint32_t flags, uint8_t state)
{
	struct ti_sci_msg_req_set_device_state req;
	struct ti_sci_msg_hdr resp;

	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TI_SCI_MSG_SET_DEVICE_STATE, flags,
				    &req, sizeof(req),
				    &resp, sizeof(resp),
				    &xfer);
	if (ret) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	req.id = id;
	req.state = state;

	ret = ti_sci_do_xfer(&xfer);
	if (ret) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * ti_sci_device_get() - Request for device managed by TISCI
 *
 * @id:		Device Identifier
 *
 * Request for the device - NOTE: the client MUST maintain integrity of
 * usage count by balancing get_device with put_device. No refcounting is
 * managed by driver for that purpose.
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_device_get(uint32_t id)
{
	return ti_sci_device_set_state(id, 0, MSG_DEVICE_SW_STATE_ON);
}

/**
 * ti_sci_device_put_no_wait() - Release a device without requesting or waiting
 *				 for a response.
 *
 * @id:		Device Identifier
 *
 * Request for the device - NOTE: the client MUST maintain integrity of
 * usage count by balancing get_device with put_device. No refcounting is
 * managed by driver for that purpose.
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_device_put_no_wait(uint32_t id)
{
	struct ti_sci_msg_req_set_device_state req;
	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TI_SCI_MSG_SET_DEVICE_STATE, 0,
				    &req, sizeof(req),
				    NULL, 0,
				    &xfer);
	if (ret != 0U) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	req.id = id;
	req.state = MSG_DEVICE_SW_STATE_AUTO_OFF;

	ret = ti_sci_do_xfer(&xfer);
	if (ret != 0U) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * ti_sci_core_reboot() - Command to request system reset
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_core_reboot(void)
{
	struct ti_sci_msg_req_reboot req;
	struct ti_sci_msg_hdr resp;

	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TI_SCI_MSG_SYS_RESET, 0,
				    &req, sizeof(req),
				    &resp, sizeof(resp),
				    &xfer);
	if (ret) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}
	req.domain = TI_SCI_DOMAIN_FULL_SOC_RESET;

	ret = ti_sci_do_xfer(&xfer);
	if (ret) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * ti_sci_proc_request() - Request a physical processor control
 *
 * @proc_id:	Processor ID this request is for
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_proc_request(uint8_t proc_id)
{
	struct ti_sci_msg_req_proc_request req;
	struct ti_sci_msg_hdr resp;

	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TISCI_MSG_PROC_REQUEST, 0,
				    &req, sizeof(req),
				    &resp, sizeof(resp),
				    &xfer);
	if (ret) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	req.processor_id = proc_id;

	ret = ti_sci_do_xfer(&xfer);
	if (ret) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * ti_sci_proc_set_boot_cfg() - Set the processor boot configuration flags
 *
 * @proc_id:		Processor ID this request is for
 * @config_flags_set:	Configuration flags to be set
 * @config_flags_clear:	Configuration flags to be cleared
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_proc_set_boot_cfg(uint8_t proc_id, uint64_t bootvector,
			     uint32_t config_flags_set,
			     uint32_t config_flags_clear)
{
	struct ti_sci_msg_req_set_proc_boot_config req;
	struct ti_sci_msg_hdr resp;

	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TISCI_MSG_SET_PROC_BOOT_CONFIG, 0,
				    &req, sizeof(req),
				    &resp, sizeof(resp),
				    &xfer);
	if (ret) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	req.processor_id = proc_id;
	req.bootvector_low = bootvector & TISCI_ADDR_LOW_MASK;
	req.bootvector_high = (bootvector & TISCI_ADDR_HIGH_MASK) >>
				TISCI_ADDR_HIGH_SHIFT;
	req.config_flags_set = config_flags_set;
	req.config_flags_clear = config_flags_clear;

	ret = ti_sci_do_xfer(&xfer);
	if (ret) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * ti_sci_proc_set_boot_ctrl() - Set the processor boot control flags
 *
 * @proc_id:			Processor ID this request is for
 * @control_flags_set:		Control flags to be set
 * @control_flags_clear:	Control flags to be cleared
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_proc_set_boot_ctrl(uint8_t proc_id, uint32_t control_flags_set,
			      uint32_t control_flags_clear)
{
	struct ti_sci_msg_req_set_proc_boot_ctrl req;
	struct ti_sci_msg_hdr resp;

	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TISCI_MSG_SET_PROC_BOOT_CTRL, 0,
				    &req, sizeof(req),
				    &resp, sizeof(resp),
				    &xfer);
	if (ret) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	req.processor_id = proc_id;
	req.control_flags_set = control_flags_set;
	req.control_flags_clear = control_flags_clear;

	ret = ti_sci_do_xfer(&xfer);
	if (ret) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * ti_sci_proc_set_boot_ctrl_no_wait() - Set the processor boot control flags
 *					 without requesting or waiting for a
 *					 response.
 *
 * @proc_id:			Processor ID this request is for
 * @control_flags_set:		Control flags to be set
 * @control_flags_clear:	Control flags to be cleared
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_proc_set_boot_ctrl_no_wait(uint8_t proc_id,
				      uint32_t control_flags_set,
				      uint32_t control_flags_clear)
{
	struct ti_sci_msg_req_set_proc_boot_ctrl req;
	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TISCI_MSG_SET_PROC_BOOT_CTRL, 0,
				    &req, sizeof(req),
				    NULL, 0,
				    &xfer);
	if (ret != 0U) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	req.processor_id = proc_id;
	req.control_flags_set = control_flags_set;
	req.control_flags_clear = control_flags_clear;

	ret = ti_sci_do_xfer(&xfer);
	if (ret != 0U) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * ti_sci_proc_wait_boot_status_no_wait() - Wait for a processor boot status
 *					    without requesting or waiting for
 *					    a response.
 *
 * @proc_id:			Processor ID this request is for
 * @num_wait_iterations		Total number of iterations we will check before
 *				we will timeout and give up
 * @num_match_iterations	How many iterations should we have continued
 *				status to account for status bits glitching.
 *				This is to make sure that match occurs for
 *				consecutive checks. This implies that the
 *				worst case should consider that the stable
 *				time should at the worst be num_wait_iterations
 *				num_match_iterations to prevent timeout.
 * @delay_per_iteration_us	Specifies how long to wait (in micro seconds)
 *				between each status checks. This is the minimum
 *				duration, and overhead of register reads and
 *				checks are on top of this and can vary based on
 *				varied conditions.
 * @delay_before_iterations_us	Specifies how long to wait (in micro seconds)
 *				before the very first check in the first
 *				iteration of status check loop. This is the
 *				minimum duration, and overhead of register
 *				reads and checks are.
 * @status_flags_1_set_all_wait	If non-zero, Specifies that all bits of the
 *				status matching this field requested MUST be 1.
 * @status_flags_1_set_any_wait	If non-zero, Specifies that at least one of the
 *				bits matching this field requested MUST be 1.
 * @status_flags_1_clr_all_wait	If non-zero, Specifies that all bits of the
 *				status matching this field requested MUST be 0.
 * @status_flags_1_clr_any_wait	If non-zero, Specifies that at least one of the
 *				bits matching this field requested MUST be 0.
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_proc_wait_boot_status_no_wait(uint8_t proc_id,
					 uint8_t num_wait_iterations,
					 uint8_t num_match_iterations,
					 uint8_t delay_per_iteration_us,
					 uint8_t delay_before_iterations_us,
					 uint32_t status_flags_1_set_all_wait,
					 uint32_t status_flags_1_set_any_wait,
					 uint32_t status_flags_1_clr_all_wait,
					 uint32_t status_flags_1_clr_any_wait)
{
	struct ti_sci_msg_req_wait_proc_boot_status req;
	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TISCI_MSG_WAIT_PROC_BOOT_STATUS, 0,
				    &req, sizeof(req),
				    NULL, 0,
				    &xfer);
	if (ret != 0U) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	req.processor_id = proc_id;
	req.num_wait_iterations = num_wait_iterations;
	req.num_match_iterations = num_match_iterations;
	req.delay_per_iteration_us = delay_per_iteration_us;
	req.delay_before_iterations_us = delay_before_iterations_us;
	req.status_flags_1_set_all_wait = status_flags_1_set_all_wait;
	req.status_flags_1_set_any_wait = status_flags_1_set_any_wait;
	req.status_flags_1_clr_all_wait = status_flags_1_clr_all_wait;
	req.status_flags_1_clr_any_wait = status_flags_1_clr_any_wait;

	ret = ti_sci_do_xfer(&xfer);
	if (ret != 0U) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * ti_sci_enter_sleep - Command to initiate system transition into suspend.
 *
 * @proc_id: Processor ID.
 * @mode: Low power mode to enter.
 * @core_resume_addr: Address that core should be
 *		      resumed from after low power transition.
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_enter_sleep(uint8_t proc_id,
		       uint8_t mode,
		       uint64_t core_resume_addr)
{
	struct ti_sci_msg_req_enter_sleep req;
	struct ti_sci_xfer xfer;
	int ret;

	ret = ti_sci_setup_one_xfer(TI_SCI_MSG_ENTER_SLEEP, 0,
				    &req, sizeof(req),
				    NULL, 0,
				    &xfer);
	if (ret != 0U) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	req.processor_id = proc_id;
	req.mode = mode;
	req.core_resume_lo = core_resume_addr & TISCI_ADDR_LOW_MASK;
	req.core_resume_hi = (core_resume_addr & TISCI_ADDR_HIGH_MASK) >>
			     TISCI_ADDR_HIGH_SHIFT;

	ret = ti_sci_do_xfer(&xfer);
	if (ret != 0U) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

/**
 * ti_sci_lpm_get_next_sys_mode() - Get next LPM system mode
 *
 * @next_mode:	pointer to a variable that will store the next mode
 *
 * Return: 0 if all goes well, else appropriate error message
 */
int ti_sci_lpm_get_next_sys_mode(uint8_t *next_mode)
{
	struct ti_sci_msg_req_lpm_get_next_sys_mode req;
	struct ti_sci_msg_resp_lpm_get_next_sys_mode resp;
	struct ti_sci_xfer xfer;
	int ret;

	if (next_mode == NULL) {
		return -EINVAL;
	}

	ret = ti_sci_setup_one_xfer(TI_SCI_MSG_LPM_GET_NEXT_SYS_MODE, 0,
				    &req, sizeof(req),
				    &resp, sizeof(resp),
				    &xfer);
	if (ret != 0) {
		ERROR("Message alloc failed (%d)\n", ret);
		return ret;
	}

	ret = ti_sci_do_xfer(&xfer);
	if (ret != 0) {
		ERROR("Transfer send failed (%d)\n", ret);
		return ret;
	}

	*next_mode = resp.mode;

	return 0;
}
