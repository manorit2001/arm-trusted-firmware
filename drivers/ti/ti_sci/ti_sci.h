/*
 * Texas Instruments System Control Interface API
 *   Based on Linux and U-Boot implementation
 *
 * Copyright (C) 2018-2025 Texas Instruments Incorporated - https://www.ti.com/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TI_SCI_H
#define TI_SCI_H

#include <stdint.h>
#include <stdbool.h>

/**
 * User exported structures.
 *
 * The structures in ti_sci_protocol.h are used by the internal drivers.
 * These are the structures that are exported for outside use and populated
 * by the internal drivers.
 *
 * struct ti_sci_msg_version - Structure containing version info
 *
 * @firmware_description: String describing the firmware
 * @firmware_revision:	Firmware revision
 * @abi_major:		Major version of the ABI that firmware supports
 * @abi_minor:		Minor version of the ABI that firmware supports
 * @sub_version:	Sub-version number of the firmware
 * @patch_version:	Patch-version number of the firmware.
 */
struct ti_sci_msg_version {
#define FIRMWARE_DESCRIPTION_LENGTH 32
	char firmware_description[FIRMWARE_DESCRIPTION_LENGTH];
	uint16_t firmware_revision;
	uint8_t abi_major;
	uint8_t abi_minor;
	uint8_t sub_version;
	uint8_t patch_version;
};

/**
 * General Message
 *
 * ti_sci_get_revision - Get the revision of the SCI entity
 *			@version: Structure containing the version info
 *
 **/
int ti_sci_get_revision(struct ti_sci_msg_version *version);

/**
 * Device control operations
 *
 * - ti_sci_device_get - Request a device managed by TISCI
 * - ti_sci_device_put_no_wait - Release a device without waiting for a response
 *
 * @id: Device identifier
 *
 * Return: 0 for a successful request, otherwise an error code.
 */
int ti_sci_device_get(uint32_t id);
int ti_sci_device_put_no_wait(uint32_t id);

/**
 * Core control operations
 *
 * - ti_sci_core_reboot() - Command to request system reset
 * - ti_sci_query_fw_caps() - Get the FW/SoC capabilities
 *              @fw_caps: Each bit in fw_caps indicating one FW/SOC capability
 *
 * Return: 0 if all went well, else returns appropriate error value.
 */
int ti_sci_core_reboot(void);
int ti_sci_query_fw_caps(uint64_t *fw_caps);

/**
 * Processor control operations
 *
 * @proc_id: Processor identifier
 *
 * Return: 0 for a successful request, otherwise an error code.
 */
int ti_sci_proc_request(uint8_t proc_id);
int ti_sci_proc_set_boot_cfg(uint8_t proc_id, uint64_t bootvector,
			     uint32_t config_flags_set,
			     uint32_t config_flags_clear);
int ti_sci_proc_set_boot_ctrl(uint8_t proc_id, uint32_t control_flags_set,
			      uint32_t control_flags_clear);
int ti_sci_proc_set_boot_ctrl_no_wait(uint8_t proc_id,
				      uint32_t control_flags_set,
				      uint32_t control_flags_clear);
int ti_sci_proc_wait_boot_status_no_wait(uint8_t proc_id,
					 uint8_t num_wait_iterations,
					 uint8_t num_match_iterations,
					 uint8_t delay_per_iteration_us,
					 uint8_t delay_before_iterations_us,
					 uint32_t status_flags_1_set_all_wait,
					 uint32_t status_flags_1_set_any_wait,
					 uint32_t status_flags_1_clr_all_wait,
					 uint32_t status_flags_1_clr_any_wait);

/**
 * System Low Power Operations
 *
 * - ti_sci_enter_sleep - Command to initiate system transition into suspend.
 *		@proc_id: Processor ID.
 *		@mode: Low power mode to enter.
 *		@core_resume_addr: Address that core should be resumed from
 *				   after low power transition.
 * - ti_sci_lpm_get_next_sys_mode - Get next LPM system mode
 *
 * @next_mode:	pointer to a variable that will store the next mode
 *
 * Return: 0 if all goes well, else appropriate error message
 *
 * NOTE: for all these functions, the following are generic in nature:
 * Returns 0 for successful request, else returns corresponding error message.
 */
int ti_sci_enter_sleep(uint8_t proc_id,
		       uint8_t mode,
		       uint64_t core_resume_addr);
int ti_sci_lpm_get_next_sys_mode(uint8_t *next_mode);

#endif /* TI_SCI_H */
