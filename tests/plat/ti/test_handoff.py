#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Host checks for the bounded TL validator and TI payload reader."""

import ctypes
import errno
from pathlib import Path
import random
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
TAG = 0xFFF000


def entry(consumer=2, host=11, rx=4, tx=((0, 3),), tag=TAG):
    payload = struct.pack("<IIII", consumer, host, rx, len(tx))
    payload += b"".join(struct.pack("<II", *pair) for pair in tx)
    data = struct.pack("<II", tag | (8 << 24), len(payload)) + payload
    return data + bytes(-len(data) % 8)


def checksum(data):
    data = bytearray(data)
    data[4] = 0
    data[4] = -sum(data) % 256
    return bytes(data)


def transfer_list(*entries, capacity=None):
    body = b"".join(entries)
    used = 24 + len(body)
    capacity = capacity or used
    header = struct.pack("<IBBBBIIII", 0x4A0FB10B, 0, 2, 24, 3,
                         used, capacity, 1, 0)
    return checksum(header + body + bytes(capacity - used))


class Config(ctypes.Structure):
    _fields_ = [("host", ctypes.c_uint8), ("tx", ctypes.c_uint16),
                ("rx", ctypes.c_uint16)]


class HandoffTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="k3-handoff-")
        library = Path(cls.temp.name) / "handoff.so"
        subprocess.run([
            "cc", "-shared", "-fPIC", "-std=c99", "-Wall", "-Wextra",
            "-Werror", "-I" + str(ROOT / "include"),
            "-I" + str(ROOT / "plat/ti/common/include"),
            str(ROOT / "lib/transfer_list/transfer_list_validate.c"),
            str(ROOT / "plat/ti/common/k3_handoff_parse.c"),
            "-o", str(library),
        ], check=True)
        cls.lib = ctypes.CDLL(str(library))
        cls.lib.transfer_list_validate.argtypes = [ctypes.c_void_p,
                                                  ctypes.c_size_t]
        cls.lib.k3_handoff_parse.argtypes = [ctypes.c_void_p, ctypes.c_size_t,
                                            ctypes.c_uint32,
                                            ctypes.POINTER(Config)]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def parse(self, data, consumer=2, available=None):
        buf = ctypes.create_string_buffer(data)
        config = Config(99, 99, 99)
        ret = self.lib.k3_handoff_parse(buf, len(data) if available is None
                                        else available, consumer,
                                        ctypes.byref(config))
        return ret, (config.host, config.tx, config.rx)

    def test_decode(self):
        self.assertEqual(self.parse(transfer_list(entry())), (0, (11, 3, 4)))

    def test_priority_roles_not_order(self):
        blob = transfer_list(entry(tx=((2, 7), (1, 8))))
        self.assertEqual(self.parse(blob), (0, (11, 7, 4)))

    def test_select_consumer_and_skip_unknown_tag(self):
        blob = transfer_list(entry(tag=123), entry(consumer=3), entry())
        self.assertEqual(self.parse(blob), (0, (11, 3, 4)))

    def test_missing_consumer(self):
        self.assertEqual(self.parse(transfer_list(entry(consumer=3)))[0],
                         -errno.ENOENT)

    def test_capacity_is_bounded_before_checksum(self):
        blob = transfer_list(entry(), capacity=4096)
        self.assertEqual(self.parse(blob)[0], 0)
        self.assertNotEqual(self.parse(blob, available=64)[0], 0)

    def test_truncations(self):
        blob = transfer_list(entry())
        for length in range(len(blob)):
            with self.subTest(length=length):
                self.assertNotEqual(self.parse(blob[:length])[0], 0)

    def test_checksum(self):
        blob = bytearray(transfer_list(entry()))
        blob[-1] ^= 1
        self.assertNotEqual(self.parse(bytes(blob))[0], 0)

    def test_bad_payloads(self):
        invalid = [entry(host=256), entry(rx=65536), entry(tx=()),
                   entry(tx=((0, 4),)), entry(tx=((0, 65536),)),
                   entry(tx=((0, 3), (0, 5))), entry(tx=((1, 3),)),
                   entry(tx=((0, 3), (2, 5))),
                   entry(tx=((1, 3), (2, 3))), entry(tx=((3, 3),))]
        for payload in invalid:
            with self.subTest(payload=payload.hex()):
                self.assertNotEqual(self.parse(transfer_list(payload))[0], 0)

    def test_duplicate_consumer(self):
        self.assertNotEqual(self.parse(transfer_list(entry(), entry()))[0], 0)

    def test_consumer_payload_bounds(self):
        unknown = struct.pack("<IIII", TAG | (8 << 24), 4, 99, 0)
        self.assertEqual(self.parse(transfer_list(entry(), unknown))[0], 0)
        reserved = struct.pack("<IIII", TAG | (8 << 24), 4, 0, 0)
        self.assertNotEqual(self.parse(transfer_list(entry(), reserved))[0], 0)
        extra = bytearray(entry() + bytes(8))
        struct.pack_into("<I", extra, 4, 32)
        self.assertNotEqual(self.parse(transfer_list(extra))[0], 0)

    def test_generic_container_policy_is_not_ti_policy(self):
        blob = bytearray(transfer_list(entry()))
        blob[7] = 4
        struct.pack_into("<I", blob, 16, 0)
        buf = ctypes.create_string_buffer(bytes(blob))
        self.assertEqual(self.lib.transfer_list_validate(buf, len(blob)), 0)
        self.assertNotEqual(self.parse(bytes(blob))[0], 0)
        buf = ctypes.create_string_buffer(b"_" + bytes(blob))
        self.assertNotEqual(self.lib.transfer_list_validate(
            ctypes.byref(buf, 1), len(blob)), 0)

    def test_bad_entry_lengths(self):
        for offset, value in [(27, 0), (27, 255), (28, 0xFFFFFFFF),
                              (44, 0xFFFFFFFF)]:
            blob = bytearray(transfer_list(entry()))
            if offset == 27:
                blob[offset] = value
            else:
                struct.pack_into("<I", blob, offset, value)
            self.assertNotEqual(self.parse(checksum(blob))[0], 0)

    def test_bad_headers(self):
        for offset, value in [(5, 0), (5, 1), (5, 3), (6, 0), (6, 255),
                              (7, 2), (7, 255)]:
            blob = bytearray(transfer_list(entry()))
            blob[offset] = value
            self.assertNotEqual(self.parse(checksum(blob))[0], 0)

    def test_failures_do_not_publish_partial_config(self):
        blob = transfer_list(entry(), entry(consumer=3, host=256))
        self.assertEqual(self.parse(blob)[1], (99, 99, 99))

    def test_random_inputs(self):
        rng = random.Random(0)
        for _ in range(1000):
            data = rng.randbytes(rng.randrange(200))
            self.assertNotEqual(self.parse(data)[0], 0)


if __name__ == "__main__":
    unittest.main()
