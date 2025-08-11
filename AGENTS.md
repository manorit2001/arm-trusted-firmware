# Developer Notes

- The clock table `plat/ti/k3/board/am62l/scmi/scmi_clk_data.h` must not contain duplicate entries. Duplicate SCMI clock entries can cause the device to hang after `DEV_INIT`.
- For MMCSD0/1/2 clock groups, list parent clock entries before the MUX entries in this order:
  1. `*_IO_CLK_I_PARENT_*` entries
  2. `*_IO_CLK_I` MUX
  3. `*_VBUS_CLK`
  4. `*_XIN_CLK_PARENT_*` entries
  5. `*_XIN_CLK` MUX
  6. `*_IO_CLK_O` output
- Building for K3 platforms requires an AArch64 cross-compiler. A quick smoke test can be run with `CROSS_COMPILE=aarch64-linux-gnu- make PLAT=k3 bl31`.
- `make checkpatch` requires the external `checkpatch.pl` script; set `CHECKPATCH` to the script path if available.
