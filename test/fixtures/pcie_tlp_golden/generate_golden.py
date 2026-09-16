#!/usr/bin/env python3
"""PCIe TLP wire-format golden hex dump generator.

Generates golden .hex files for PCIe TLP verification:
  - 4kb_mwr_32bit.hex: 4KB Memory Write TLP (32-bit address, 3DW header)
  - 64b_mrd_32bit.hex: 64B Memory Read TLP (32-bit address, 3DW header)
  - 64b_cpld.hex:      64B Completion with Data TLP (3DW header)

Each .hex file contains a hex dump of a single TLP as transmitted on the PCIe
wire, one line per 16 bytes (32 hex characters), uppercase, LF-terminated.

Byte ordering: little-endian (PCIe native). All TLPs assume TD=0 (no ECRC),
so the wire format is: [TLP Header] [Data Payload] [LCRC (4 bytes)].

LCRC is PCIe CRC-32: poly=0x04C11DB7 (reflected: 0xEDB88320), init=0xFFFFFFFF,
XOR-out=0xFFFFFFFF, reflected (LSB-first on wire). This is the standard CRC-32
algorithm (PKZIP/Ethernet), matching PCIe Base Spec §2.7 test vectors.

Usage:
    python3 generate_golden.py

Output: 3 .hex files in the same directory as this script.
"""

import struct
import os
import sys

# ── CRC-32 (PCIe spec §2.7) ──────────────────────────────────────────────
# Polynomial: 0x04C11DB7, reflected poly: 0xEDB88320
# Init: 0xFFFFFFFF, XOR-out: 0xFFFFFFFF, Reflected (LSB-first on wire)
# This is the standard CRC-32 algorithm (PKZIP).

def _build_crc32_table():
    """Build the 256-entry CRC-32 lookup table (reflected polynomial)."""
    table = [0] * 256
    poly = 0xEDB88320  # reflected 0x04C11DB7
    for i in range(256):
        crc = i
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ poly
            else:
                crc >>= 1
        table[i] = crc
    return table

_CRC32_TABLE = _build_crc32_table()

def crc32_pcie(data: bytes) -> int:
    """Compute PCIe CRC-32 over `data`.

    Algorithm: CRC-32 with poly=0x04C11DB7 (reflected=0xEDB88320),
    init=0xFFFFFFFF, reflected input, XOR-out=0xFFFFFFFF.
    """
    crc = 0xFFFFFFFF
    for byte in data:
        crc = _CRC32_TABLE[(crc ^ byte) & 0xFF] ^ (crc >> 8)
    return crc ^ 0xFFFFFFFF


def verify_crc32_self_test():
    """Round-trip self-test for CRC-32 implementation.

    Raises AssertionError if test vectors fail.
    """
    # Test vector 1: CRC of empty data = 0
    assert crc32_pcie(b"") == 0, (
        f"CRC(b'') = 0x{crc32_pcie(b''):08X}, expected 0x00000000"
    )
    # Test vector 2: CRC of four zero bytes (PCIe spec §2.7)
    assert crc32_pcie(b"\x00\x00\x00\x00") == 0x2144DF1C, (
        f"CRC(b'\\\\x00'*4) = 0x{crc32_pcie(b'\\x00'*4):08X}, "
        f"expected 0x2144DF1C"
    )
    # Round-trip: append CRC, verify residue = 0x2144DF1C
    # When the CRC of a message is appended (LE) and CRC is recalculated
    # over the extended message, the result is a constant residue.
    # For standard CRC-32 with xor-out=0xFFFFFFFF, the residue is 0x2144DF1C
    # (this is the "magic number" observed when CRC is appended).
    msg = b"\x01\x02\x03\x04"
    crc_val = crc32_pcie(msg)
    extended = msg + struct.pack("<I", crc_val)
    check = crc32_pcie(extended)
    assert check == 0x2144DF1C, (
        f"CRC residue = 0x{check:08X}, expected 0x2144DF1C "
        f"(standard CRC-32 residue)"
    )
    print(f"  [PASS] CRC self-test: crc('')=0x00000000, "
          f"crc(0x00*4)=0x2144DF1C, residue check=0x{check:08X}")


# ── TLP Builders ──────────────────────────────────────────────────────────

def build_mwr_tlp(req_id: int, tag: int, addr32: int, data: bytes) -> bytes:
    """Build a 32-bit address Memory Write TLP (3DW header).

    Args:
        req_id: 16-bit Requester ID (e.g., 0x0100 for bus=01 dev=0 fn=0).
        tag:    8-bit Tag.
        addr32: 32-bit address.
        data:   Payload bytes (must be 4-byte aligned).

    Returns:
        Complete TLP bytes: [Header (12)] [Payload (N)] [LCRC (4)].
        TD=0, so no ECRC.
    """
    # Validate payload length
    assert len(data) % 4 == 0, "Payload must be 4-byte aligned"
    payload_dw = len(data) // 4
    length = payload_dw & 0x3FF  # 10-bit Length field; 0 means 1024

    # DW0: [Fmt=010][Type=00000] | R | TC=000 | R | Attr=000 | AT=00 | ... | Length
    # Fmt=010: 3DW header with data; Type=00000: Memory
    # For 3DW Memory Write: byte0 = (Fmt<<5)|Type = (0b010<<5)|0b00000 = 0x40
    # byte1 has TC, Attr, AT, TD, EP bits (all 0 for default case)
    # byte2 = Length[7:0]; byte3 bits[1:0] = Length[9:8]
    dw0 = 0x40000000 | (length & 0x3FF)
    if payload_dw == 0:
        # Length=0 means 1024 DW (4096 bytes)
        dw0 = 0x40000000 | 0x000
    elif payload_dw == 1024:
        # For 1024 DW, PCIe encodes Length=0
        dw0 = 0x40000000 | 0x000
    else:
        dw0 = 0x40000000 | (length & 0x3FF)

    header = struct.pack("<I", dw0)  # DW0 little-endian

    # DW1: [Requester ID (16)] [Tag (8)] [Last DW BE (4)] [First DW BE (4)]
    # Requester ID = bus(8) | dev(5) | func(3) as a 16-bit value
    # For all bytes valid: Last BE=0xF, First BE=0xF
    dw1_16 = req_id & 0xFFFF
    be_val = 0xFF  # Last BE=0xF, First BE=0xF
    dw1_bytes = struct.pack("<H", dw1_16) + struct.pack("B", tag & 0xFF) + struct.pack("B", be_val)
    header += dw1_bytes

    # DW2: [Address (32)]
    dw2_bytes = struct.pack("<I", addr32 & 0xFFFFFFFF)
    header += dw2_bytes

    # Assemble: Header + Payload
    tlp = header + data

    # LCRC covers header + payload (TD=0 → no ECRC)
    lcrc = struct.pack("<I", crc32_pcie(tlp))

    return tlp + lcrc


def build_mrd_tlp(req_id: int, tag: int, addr32: int, len_dw: int) -> bytes:
    """Build a 32-bit address Memory Read TLP (3DW header, no data).

    Args:
        req_id: 16-bit Requester ID.
        tag:    8-bit Tag.
        addr32: 32-bit address.
        len_dw: Read length in DWORDs (1..1024; 0 means 1024).

    Returns:
        Complete TLP bytes: [Header (12)] [LCRC (4)].
    """
    assert 1 <= len_dw <= 1024, "Read length must be 1..1024 DW"
    length = len_dw if len_dw < 1024 else 0

    # DW0: Fmt=000 (3DW no data), Type=00000 (Memory)
    # byte0 = (0b000 << 5) | 0b00000 = 0x00
    dw0 = 0x00000000 | (length & 0x3FF)

    header = struct.pack("<I", dw0)

    # DW1: [Req ID][Tag][Last BE][First BE]
    dw1_16 = req_id & 0xFFFF
    be_val = 0xFF  # All bytes enabled
    header += struct.pack("<H", dw1_16) + struct.pack("B", tag & 0xFF) + struct.pack("B", be_val)

    # DW2: [Address (32)]
    header += struct.pack("<I", addr32 & 0xFFFFFFFF)

    # LCRC (TD=0 → no ECRC)
    lcrc = struct.pack("<I", crc32_pcie(header))
    return header + lcrc


def build_cpld_tlp(completer_id: int, requester_id: int, tag: int,
                   byte_count: int, data: bytes) -> bytes:
    """Build a Completion with Data TLP (3DW header).

    Args:
        completer_id: 16-bit Completer ID (e.g., 0x0200).
        requester_id: 16-bit Requester ID (echoed from request).
        tag:          8-bit Tag (echoed from request).
        byte_count:   Byte Count (12-bit, number of bytes completed).
        data:         Payload bytes (must be 4-byte aligned).

    Returns:
        Complete TLP bytes: [Header (12)] [Payload (N)] [LCRC (4)].
    """
    assert len(data) % 4 == 0, "Payload must be 4-byte aligned"
    payload_dw = len(data) // 4
    length = payload_dw if payload_dw < 1024 else 0

    # DW0: Fmt=010 (3DW with data), Type=01010 (Completion)
    # byte0 = (0b010 << 5) | 0b01010 = 0b010_01010 = 0x4A
    dw0 = 0x4A000000 | (length & 0x3FF)
    header = struct.pack("<I", dw0)

    # DW1: [Completer ID (16)] [BCM(1)|R(1)|Status(3)|BC_high(3)] [Byte Count low(8)]
    # Status=000 (Successful Completion), BCM=0
    # Byte Count is 12 bits: byte_count & 0xFFF
    byte_count = min(byte_count, 0xFFF)
    bc_high = (byte_count >> 8) & 0x07
    bc_low = byte_count & 0xFF
    # byte 6 = BCM(1)|R(1)|Status(3)|bc_high(3)
    byte4_5 = struct.pack("<H", completer_id & 0xFFFF)
    byte6 = (0 << 7) | (0 << 6) | (0 << 3) | bc_high  # BCM=0,R=0,Status=000,bc_high
    byte6_packed = struct.pack("B", byte6)
    byte7_packed = struct.pack("B", bc_low)
    header += byte4_5 + byte6_packed + byte7_packed

    # DW2: [Requester ID (16)] [Tag (8)] [Lower Address (8)]
    # Lower Address byte = 0x00 for aligned completions
    dw2_req_h, dw2_req_l = (requester_id >> 8) & 0xFF, requester_id & 0xFF
    # In the spec: bits 31:24 = Requester ID[7:0], bits 23:16 = Requester ID[15:8]
    # In LE bytes: byte8=LowerAddr, byte9=Tag, byte10=ReqID[15:8], byte11=ReqID[7:0]
    byte8 = 0x00  # Lower Address (aligned)
    byte9 = tag & 0xFF
    byte10 = dw2_req_h  # Requester ID high byte → bits 23:16 of DW
    byte11 = dw2_req_l  # Requester ID low byte → bits 31:24 of DW
    header += struct.pack("BBBB", byte8, byte9, byte10, byte11)

    # Header + Payload
    tlp = header + data

    # LCRC
    lcrc = struct.pack("<I", crc32_pcie(tlp))
    return tlp + lcrc


# ── Hex Dump ──────────────────────────────────────────────────────────────

def dump_hex_file(tlp_bytes: bytes, path: str):
    """Write a hex dump of `tlp_bytes`, 16 bytes per line (32 hex chars).

    Args:
        tlp_bytes: The TLP bytes to dump.
        path:      Output file path.
    """
    lines = []
    for i in range(0, len(tlp_bytes), 16):
        chunk = tlp_bytes[i:i + 16]
        hex_str = chunk.hex().upper()
        lines.append(hex_str)
    content = "\n".join(lines) + "\n"
    with open(path, "w") as f:
        f.write(content)
    actual_bytes = len(tlp_bytes)
    hex_chars = len(content.strip().replace("\n", ""))
    print(f"  [OK] {os.path.basename(path)}: {actual_bytes} bytes, "
          f"{hex_chars} hex chars")


# ── Main ──────────────────────────────────────────────────────────────────

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))

    print("=" * 60)
    print("PCIe TLP Golden Fixture Generator")
    print("=" * 60)
    print()

    # ── CRC self-test ──
    print("[1/4] CRC-32 self-test...")
    verify_crc32_self_test()
    print()

    # ── Generate 4KB MWr TLP ──
    # 4KB = 1024 DW → Length=0 (0 means 1024 DW in PCIe Length field)
    print("[2/4] Generating 4KB MWr (32-bit address)...")
    mwr_data = b"\xAA\x55" * 2048  # 4096 bytes = 4KB
    assert len(mwr_data) == 4096
    mwr_tlp = build_mwr_tlp(
        req_id=0x0100,
        tag=0x01,
        addr32=0x10000000,
        data=mwr_data,
    )
    # Expected size: header(12) + payload(4096) + LCRC(4) = 4112 bytes
    assert len(mwr_tlp) == 4112, (
        f"MWr TLP size {len(mwr_tlp)}, expected 4112"
    )
    dump_hex_file(mwr_tlp, os.path.join(script_dir, "4kb_mwr_32bit.hex"))
    print()

    # ── Generate 64B MRd TLP ──
    print("[3/4] Generating 64B MRd (32-bit address)...")
    mrd_tlp = build_mrd_tlp(
        req_id=0x0100,
        tag=0x02,
        addr32=0x10001000,
        len_dw=16,  # 16 DW = 64 bytes
    )
    # Expected size: header(12) + LCRC(4) = 16 bytes
    assert len(mrd_tlp) == 16, (
        f"MRd TLP size {len(mrd_tlp)}, expected 16"
    )
    dump_hex_file(mrd_tlp, os.path.join(script_dir, "64b_mrd_32bit.hex"))
    print()

    # ── Generate 64B CplD TLP ──
    print("[4/4] Generating 64B CplD...")
    cpld_data = b"\xDE\xAD\xBE\xEF" * 16  # 64 bytes
    assert len(cpld_data) == 64
    cpld_tlp = build_cpld_tlp(
        completer_id=0x0200,
        requester_id=0x0100,
        tag=0x02,
        byte_count=64,
        data=cpld_data,
    )
    # Expected size: header(12) + payload(64) + LCRC(4) = 80 bytes
    assert len(cpld_tlp) == 80, (
        f"CplD TLP size {len(cpld_tlp)}, expected 80"
    )
    dump_hex_file(cpld_tlp, os.path.join(script_dir, "64b_cpld.hex"))
    print()

    # ── Final Summary ──
    print("=" * 60)
    print("All golden files generated successfully.")
    print(f"  Directory: {script_dir}")
    print(f"  Files:")
    print(f"    - 4kb_mwr_32bit.hex  ({len(mwr_tlp)} bytes)")
    print(f"    - 64b_mrd_32bit.hex  ({len(mrd_tlp)} bytes)")
    print(f"    - 64b_cpld.hex       ({len(cpld_tlp)} bytes)")
    print("=" * 60)


if __name__ == "__main__":
    main()