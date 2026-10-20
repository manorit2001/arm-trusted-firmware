Texas Instruments K3
====================

Trusted Firmware-A (TF-A) implements the EL3 firmware layer for Texas Instruments K3 SoCs.

Boot Flow
---------

::

   R5(U-Boot) --> TF-A BL31 --> BL32(OP-TEE) --> TF-A BL31 --> BL33(U-Boot) --> Linux
                                                       \
                                                   Optional direct to Linux boot
                                                           \
                                                           --> BL33(Linux)

Texas Instruments K3 SoCs contain an R5 processor used as the boot master, it
loads the needed images for A53 startup, because of this we do not need BL1 or
BL2 TF-A stages.

Build Instructions
------------------

https://github.com/ARM-software/arm-trusted-firmware.git

TF-A:

.. code:: shell

    make CROSS_COMPILE=aarch64-linux-gnu- PLAT=k3 SPD=opteed all

OP-TEE:

.. code:: shell

    make ARCH=arm CROSS_COMPILE64=aarch64-linux-gnu- PLATFORM=k3 CFG_ARM64_core=y all

R5 U-Boot:

.. code:: shell

    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- am65x_evm_r5_defconfig
    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- SYSFW=<path to SYSFW>

A53 U-Boot:

.. code:: shell

    make ARCH=arm CROSS_COMPILE=aarch64-linux-gnu- am65x_evm_a53_defconfig
    make ARCH=arm CROSS_COMPILE=aarch64-linux-gnu- ATF=<path> TEE=<path>

Deploy Images
-------------

.. code:: shell

    cp tiboot3.bin tispl.bin u-boot.img /sdcard/boot/

Experimental secure-proxy firmware handoff
------------------------------------------

``K3_SEC_PROXY_HANDOFF=1`` enables configuration of the TI-SCI host ID and
secure-proxy request/response threads from a Transfer List v2. It is disabled
by default; existing binaries without this feature cannot gain it by repackaging.
This experimental ABI uses private tag ``0xfff000`` and requires a tag allocation
and an agreed TI payload ABI before production upstream use. Example host/thread
values are not resource assignments: they must agree with the actual SYSFW
security, ownership and resource-management configuration.

Build the feature once, retaining both the binary and its matching ELF::

    git submodule update --init --depth 1 contrib/libtl
    make PLAT=k3 TARGET_BOARD=generic SPD=opteed K3_SEC_PROXY_HANDOFF=1 \
        CROSS_COMPILE=aarch64-linux-gnu- bl31

The reusable linker fragment ``include/lib/transfer_list.ld.h`` exports
``__TRANSFER_LIST_START__`` and ``__TRANSFER_LIST_END__``. K3 selects a 4096-byte
capacity and asserts that the entire region fits within ``BL31_LIMIT``. The start
follows all BL31 runtime allocations, including stacks and BSS, not merely the
last byte in ``bl31.bin``. The existing BL31 memory mapping covers this region.
Separate NOBITS/RW layouts are not supported by this K3 integration.

U-Boot Binman uses those symbols from the matching ELF to pad the BL31 binary
and append a generated list. Configuration-only changes then require rebuilding
and repackaging U-Boot, not rebuilding or modifying BL31. The loader must load
and reserve the complete combined image. On authenticated devices the complete
combined image must be signed; a transfer-list checksum is not authentication.
The trust boundary follows the
:doc:`Firmware Handoff threat model </threat_model/firmware_threat_model/threat_model_firmware_handoff>`.

Framework and platform responsibilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``firmware_handoff_get()`` in ``lib/transfer_list/firmware_handoff.c`` calls the
weak ``plat_get_transfer_list(size_t *available)`` discovery hook and performs
bounded validation before exposing a list. The default hook reports no list.
A platform override must provide an independently trusted accessible extent,
never a capacity read from the unvalidated list itself. K3's override uses the
image-relative linker symbol and its compiled capacity. Other platforms can
provide a fixed address or a previously saved register argument instead.

The reusable validator checks the full container and each entry's bounds before
consumers inspect their payloads. TI-specific parsing, host/thread selection and
initialization remain in ``plat/ti/common/`` and ``drivers/ti/``. K3 calls the
framework from early platform setup, before the first TI-SCI operation.

Payload and onward handoff
~~~~~~~~~~~~~~~~~~~~~~~~~~

Each TI entry contains little-endian 32-bit consumer ID, host ID, RX thread,
TX count, then TX role/thread pairs. Consumer IDs are R5 SPL (1), TF-A (2),
OP-TEE (3), AArch64 SPL (4), U-Boot (5) and Linux (6). TX roles are request (0),
high priority (1) and low priority (2). A consumer has either one request role
or both priority roles. The current single-request transports use the request
or low-priority thread; this feature does not add priority scheduling.

The BL31 list contains secure consumers only. With OPTEED, TF-A adds a final
FDT entry and supplies the standard AArch64 handoff arguments: x0 is the FDT,
x1 is the convention signature/version, x2 is zero and x3 is the secure list.
OP-TEE must enable its corresponding K3 consumer. The list is writable so
OP-TEE can expand its final FDT, and it must remain inaccessible to non-secure
software. It is not passed to BL33.

U-Boot builds stage-local lists from the same YAML source. The ROM-loaded R5
image contains only its own record; downstream records need not occupy early
SRAM before DDR initialization. AArch64 SPL and U-Boot use their own records.
U-Boot translates the Linux record into existing TI-SCI device-tree properties;
no Linux transfer-list consumer is required. See U-Boot's
``doc/board/ti/k3-handoff.rst`` for the YAML schema and packaging instructions.

Host parser checks can be run without a target::

    make -C tests/lib/transfer_list O=/tmp/tfa-tl-tests check
    python3 tests/plat/ti/test_handoff.py -v

These checks and platform builds do not establish hardware resource ownership,
authentication policy, or successful boot. Validate those on the target before
deploying a new configuration.
