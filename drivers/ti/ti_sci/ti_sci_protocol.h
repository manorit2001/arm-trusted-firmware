/*
 * Texas Instruments System Control Interface (TISCI) Protocol
 *
 * Communication protocol with TI SCI hardware
 * The system works in a message response protocol
 * See: http://processors.wiki.ti.com/index.php/TISCI for details
 *
 * Copyright (C) 2018-2025 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TI_SCI_PROTOCOL_H
#define TI_SCI_PROTOCOL_H

#include <stdint.h>

/* Generic Messages */
#define TI_SCI_MSG_VERSION		0x0002
#define TI_SCI_MSG_SYS_RESET		0x0005
#define TI_SCI_MSG_QUERY_FW_CAPS	0x0022

/* Device requests */
#define TI_SCI_MSG_SET_DEVICE_STATE	0x0200

/* Low Power Mode Requests */
#define TI_SCI_MSG_ENTER_SLEEP		0x0301
#define TI_SCI_MSG_LPM_GET_NEXT_SYS_MODE 0x030d

/* Clock requests */

/* Processor Control Messages */
#define TISCI_MSG_PROC_REQUEST		0xc000
#define TISCI_MSG_SET_PROC_BOOT_CONFIG	0xc100
#define TISCI_MSG_SET_PROC_BOOT_CTRL	0xc101
#define TISCI_MSG_WAIT_PROC_BOOT_STATUS	0xc401

/**
 * struct ti_sci_secure_msg_hdr - Header that prefixes all TISCI messages sent
 *				  via secure transport.
 * @checksum:	crc16 checksum for the entire message
 * @reserved:	Reserved for future use.
 */
struct ti_sci_secure_msg_hdr {
	uint16_t checksum;
	uint16_t reserved;
} __packed;

/**
 * struct ti_sci_msg_hdr - Generic Message Header for All messages and responses
 * @type:	Type of messages: One of TI_SCI_MSG* values
 * @host:	Host of the message
 * @seq:	Message identifier indicating a transfer sequence
 * @flags:	Flag for the message
 */
struct ti_sci_msg_hdr {
	struct ti_sci_secure_msg_hdr sec_hdr;
	uint16_t type;
	uint8_t host;
	uint8_t seq;
#define TI_SCI_MSG_FLAG(val)			(1 << (val))
#define TI_SCI_FLAG_REQ_GENERIC_NORESPONSE	0x0
#define TI_SCI_FLAG_REQ_ACK_ON_RECEIVED		TI_SCI_MSG_FLAG(0)
#define TI_SCI_FLAG_REQ_ACK_ON_PROCESSED	TI_SCI_MSG_FLAG(1)
#define TI_SCI_FLAG_RESP_GENERIC_NACK		0x0
#define TI_SCI_FLAG_RESP_GENERIC_ACK		TI_SCI_MSG_FLAG(1)
	/* Additional Flags */
	uint32_t flags;
} __packed;

/**
 * struct ti_sci_msg_version_req - Request for firmware version information
 * @hdr:	Generic header
 *
 * Request for TI_SCI_MSG_VERSION
 */
struct ti_sci_msg_req_version {
	struct ti_sci_msg_hdr hdr;
} __packed;

/**
 * struct ti_sci_msg_resp_version - Response for firmware version information
 * @hdr:		Generic header
 * @firmware_description: String describing the firmware
 * @firmware_revision:	Firmware revision
 * @abi_major:		Major version of the ABI that firmware supports
 * @abi_minor:		Minor version of the ABI that firmware supports
 * @sub_version:	Sub-version number of the firmware
 * @patch_version:	Patch-version number of the firmware.
 *
 * In general, ABI version changes follow the rule that minor version increments
 * are backward compatible. Major revision changes in ABI may not be
 * backward compatible.
 *
 * Response to request TI_SCI_MSG_VERSION
 */
struct ti_sci_msg_resp_version {
	struct ti_sci_msg_hdr hdr;
#define FIRMWARE_DESCRIPTION_LENGTH 32
	char firmware_description[FIRMWARE_DESCRIPTION_LENGTH];
	uint16_t firmware_revision;
	uint8_t abi_major;
	uint8_t abi_minor;
	uint8_t sub_version;
	uint8_t patch_version;
} __packed;

/**
 * struct ti_sci_msg_req_reboot - Reboot the SoC
 * @hdr:	Generic Header
 * @domain:	Domain to be reset, 0 for full SoC reboot
 *
 * Request type is TI_SCI_MSG_SYS_RESET, responded with a generic
 * ACK/NACK message.
 */
struct ti_sci_msg_req_reboot {
	struct ti_sci_msg_hdr hdr;
#define TI_SCI_DOMAIN_FULL_SOC_RESET	0x0
	uint8_t domain;
} __packed;

/**
 * struct ti_sci_msg_resp_query_fw_caps - Response for query firmware caps
 * @hdr:	Generic header
 * @fw_caps:	Each bit in fw_caps indicating one FW/SOC capability
 *		MSG_FLAG_CAPS_GENERIC: Generic capability (LPM not supported)
 *		MSG_FLAG_CAPS_LPM_DEEP_SLEEP: Deep Sleep LPM
 *		MSG_FLAG_CAPS_LPM_MCU_ONLY: MCU only LPM
 *		MSG_FLAG_CAPS_LPM_STANDBY: Standby LPM
 *		MSG_FLAG_CAPS_LPM_PARTIAL_IO: Partial IO in LPM
 *		MSG_FLAG_CAPS_LPM_DM_MANAGED: LPM can be managed by DM
 *
 * Response to a generic message with message type TI_SCI_MSG_QUERY_FW_CAPS
 * providing currently available SOC/firmware capabilities. SoC that don't
 * support low power modes return only MSG_FLAG_CAPS_GENERIC capability.
 */
struct ti_sci_msg_resp_query_fw_caps {
	struct ti_sci_msg_hdr hdr;
#define MSG_FLAG_CAPS_GENERIC		TI_SCI_MSG_FLAG(0)
#define MSG_FLAG_CAPS_LPM_DEEP_SLEEP	TI_SCI_MSG_FLAG(1)
#define MSG_FLAG_CAPS_LPM_MCU_ONLY	TI_SCI_MSG_FLAG(2)
#define MSG_FLAG_CAPS_LPM_STANDBY	TI_SCI_MSG_FLAG(3)
#define MSG_FLAG_CAPS_LPM_PARTIAL_IO	TI_SCI_MSG_FLAG(4)
#define MSG_FLAG_CAPS_LPM_DM_MANAGED	TI_SCI_MSG_FLAG(5)
	uint64_t fw_caps;
} __packed;

/**
 * struct ti_sci_msg_req_set_device_state - Set the desired state of the device
 * @hdr:		Generic header
 * @id:	Indicates which device to modify
 * @reserved: Reserved space in message, must be 0 for backward compatibility
 * @state: The desired state of the device.
 *
 * Certain flags can also be set to alter the device state:
 * + MSG_FLAG_DEVICE_WAKE_ENABLED - Configure the device to be a wake source.
 * The meaning of this flag will vary slightly from device to device and from
 * SoC to SoC but it generally allows the device to wake the SoC out of deep
 * suspend states.
 * + MSG_FLAG_DEVICE_RESET_ISO - Enable reset isolation for this device.
 * + MSG_FLAG_DEVICE_EXCLUSIVE - Claim this device exclusively. When passed
 * with STATE_RETENTION or STATE_ON, it will claim the device exclusively.
 * If another host already has this device set to STATE_RETENTION or STATE_ON,
 * the message will fail. Once successful, other hosts attempting to set
 * STATE_RETENTION or STATE_ON will fail.
 *
 * Request type is TI_SCI_MSG_SET_DEVICE_STATE, responded with a generic
 * ACK/NACK message.
 */
struct ti_sci_msg_req_set_device_state {
	/* Additional hdr->flags options */
#define MSG_FLAG_DEVICE_WAKE_ENABLED	TI_SCI_MSG_FLAG(8)
#define MSG_FLAG_DEVICE_RESET_ISO	TI_SCI_MSG_FLAG(9)
#define MSG_FLAG_DEVICE_EXCLUSIVE	TI_SCI_MSG_FLAG(10)
	struct ti_sci_msg_hdr hdr;
	uint32_t id;
	uint32_t reserved;

#define MSG_DEVICE_SW_STATE_AUTO_OFF	0
#define MSG_DEVICE_SW_STATE_RETENTION	1
#define MSG_DEVICE_SW_STATE_ON		2
	uint8_t state;
} __packed;

#define TISCI_ADDR_LOW_MASK		0x00000000ffffffff
#define TISCI_ADDR_HIGH_MASK		0xffffffff00000000
#define TISCI_ADDR_HIGH_SHIFT		32

/**
 * struct ti_sci_msg_req_proc_request - Request a processor
 *
 * @hdr:		Generic Header
 * @processor_id:	ID of processor
 *
 * Request type is TISCI_MSG_PROC_REQUEST, response is a generic ACK/NACK
 * message.
 */
struct ti_sci_msg_req_proc_request {
	struct ti_sci_msg_hdr hdr;
	uint8_t processor_id;
} __packed;

/* A53 Config Flags */
#define PROC_BOOT_CFG_FLAG_ARMV8_DBG_EN         0x00000001
#define PROC_BOOT_CFG_FLAG_ARMV8_DBG_NIDEN      0x00000002
#define PROC_BOOT_CFG_FLAG_ARMV8_DBG_SPIDEN     0x00000004
#define PROC_BOOT_CFG_FLAG_ARMV8_DBG_SPNIDEN    0x00000008
#define PROC_BOOT_CFG_FLAG_ARMV8_AARCH32        0x00000100

/* R5 Config Flags */
#define PROC_BOOT_CFG_FLAG_R5_DBG_EN            0x00000001
#define PROC_BOOT_CFG_FLAG_R5_DBG_NIDEN         0x00000002
#define PROC_BOOT_CFG_FLAG_R5_LOCKSTEP          0x00000100
#define PROC_BOOT_CFG_FLAG_R5_TEINIT            0x00000200
#define PROC_BOOT_CFG_FLAG_R5_NMFI_EN           0x00000400
#define PROC_BOOT_CFG_FLAG_R5_TCM_RSTBASE       0x00000800
#define PROC_BOOT_CFG_FLAG_R5_BTCM_EN           0x00001000
#define PROC_BOOT_CFG_FLAG_R5_ATCM_EN           0x00002000

/**
 * struct ti_sci_msg_req_set_proc_boot_config - Set Processor boot configuration
 * @hdr:		Generic Header
 * @processor_id:	ID of processor
 * @bootvector_low:	Lower 32bit (Little Endian) of boot vector
 * @bootvector_high:	Higher 32bit (Little Endian) of boot vector
 * @config_flags_set:	Optional Processor specific Config Flags to set.
 *			Setting a bit here implies required bit sets to 1.
 * @config_flags_clear:	Optional Processor specific Config Flags to clear.
 *			Setting a bit here implies required bit gets cleared.
 *
 * Request type is TISCI_MSG_SET_PROC_BOOT_CONFIG, response is a generic
 * ACK/NACK message.
 */
struct ti_sci_msg_req_set_proc_boot_config {
	struct ti_sci_msg_hdr hdr;
	uint8_t processor_id;
	uint32_t bootvector_low;
	uint32_t bootvector_high;
	uint32_t config_flags_set;
	uint32_t config_flags_clear;
} __packed;

/* ARMV8 Control Flags */
#define PROC_BOOT_CTRL_FLAG_ARMV8_ACINACTM      0x00000001
#define PROC_BOOT_CTRL_FLAG_ARMV8_AINACTS       0x00000002
#define PROC_BOOT_CTRL_FLAG_ARMV8_L2FLUSHREQ    0x00000100

/* R5 Control Flags */
#define PROC_BOOT_CTRL_FLAG_R5_CORE_HALT        0x00000001

/**
 * struct ti_sci_msg_req_set_proc_boot_ctrl - Set Processor boot control flags
 * @hdr:		Generic Header
 * @processor_id:	ID of processor
 * @config_flags_set:	Optional Processor specific Config Flags to set.
 *			Setting a bit here implies required bit sets to 1.
 * @config_flags_clear:	Optional Processor specific Config Flags to clear.
 *			Setting a bit here implies required bit gets cleared.
 *
 * Request type is TISCI_MSG_SET_PROC_BOOT_CTRL, response is a generic ACK/NACK
 * message.
 */
struct ti_sci_msg_req_set_proc_boot_ctrl {
	struct ti_sci_msg_hdr hdr;
	uint8_t processor_id;
	uint32_t control_flags_set;
	uint32_t control_flags_clear;
} __packed;

/* ARMv8 Status Flags */
#define PROC_BOOT_STATUS_FLAG_ARMV8_WFE			0x00000001
#define PROC_BOOT_STATUS_FLAG_ARMV8_WFI			0x00000002
#define PROC_BOOT_STATUS_FLAG_ARMV8_L2F_DONE		0x00000010
#define PROC_BOOT_STATUS_FLAG_ARMV8_STANDBYWFIL2	0x00000020

/* R5 Status Flags */
#define PROC_BOOT_STATUS_FLAG_R5_WFE			0x00000001
#define PROC_BOOT_STATUS_FLAG_R5_WFI			0x00000002
#define PROC_BOOT_STATUS_FLAG_R5_CLK_GATED		0x00000004
#define PROC_BOOT_STATUS_FLAG_R5_LOCKSTEP_PERMITTED	0x00000100

/**
 * struct ti_sci_msg_req_wait_proc_boot_status - Wait for a processor boot status
 * @hdr:			Generic Header
 * @processor_id:		ID of processor
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
 * Request type is TISCI_MSG_WAIT_PROC_BOOT_STATUS, response is appropriate
 * message, or NACK in case of inability to satisfy request.
 */
struct ti_sci_msg_req_wait_proc_boot_status {
	struct ti_sci_msg_hdr hdr;
	uint8_t processor_id;
	uint8_t num_wait_iterations;
	uint8_t num_match_iterations;
	uint8_t delay_per_iteration_us;
	uint8_t delay_before_iterations_us;
	uint32_t status_flags_1_set_all_wait;
	uint32_t status_flags_1_set_any_wait;
	uint32_t status_flags_1_clr_all_wait;
	uint32_t status_flags_1_clr_any_wait;
} __packed;

/**
 * struct ti_sci_msg_req_enter_sleep - Request for TI_SCI_MSG_ENTER_SLEEP.
 *
 * @hdr		    Generic Header
 * @mode	    Low power mode to enter.
 * @proc_id	    Processor id to be restored.
 * @core_resume_lo  Low 32-bits of physical pointer to address for core
 *		    to begin execution upon resume.
 * @core_resume_hi  High 32-bits of physical pointer to address for core
 *		    to begin execution upon resume.
 *
 * This message is to be sent after TI_SCI_MSG_PREPARE_SLEEP is sent from OS
 * and is what actually triggers entry into the specified low power mode.
 */
struct ti_sci_msg_req_enter_sleep {
	struct ti_sci_msg_hdr hdr;
#define MSG_VALUE_SLEEP_MODE_DEEP_SLEEP 0x0
	uint8_t mode;
	uint8_t processor_id;
	uint32_t core_resume_lo;
	uint32_t core_resume_hi;
} __packed;

/**
 * struct ti_sci_msg_req_lpm_get_next_sys_mode - Request for TI_SCI_MSG_LPM_GET_NEXT_SYS_MODE.
 *
 * @hdr Generic Header
 *
 * This message is used to enquire DM for selected system wide low power mode.
 */
struct ti_sci_msg_req_lpm_get_next_sys_mode {
	struct ti_sci_msg_hdr hdr;
} __packed;

/**
 * struct ti_sci_msg_resp_lpm_get_next_sys_mode - Response for TI_SCI_MSG_LPM_GET_NEXT_SYS_MODE.
 *
 * @hdr Generic Header
 * @mode The selected system wide low power mode.
 *
 * Note: If the mode selection is not yet locked, this API returns "not selected" mode.
 */
struct ti_sci_msg_resp_lpm_get_next_sys_mode {
	struct ti_sci_msg_hdr hdr;
	uint8_t mode;
} __packed;

#endif /* TI_SCI_PROTOCOL_H */
