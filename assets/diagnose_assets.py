#!/usr/bin/env python3
"""
Diagnostic tool to check zelda3_assets.dat file compatibility
"""
import struct
import hashlib
import sys

def diagnose_asset_file(filename):
    """Diagnose potential issues with asset file format"""

    print(f"Diagnosing: {filename}")
    print("=" * 60)

    try:
        with open(filename, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"ERROR: File '{filename}' not found!")
        return False

    print(f"File size: {len(data):,} bytes")

    # Check signature
    if len(data) < 48:
        print("ERROR: File too small (< 48 bytes)")
        return False

    signature = data[0:16]
    expected_sig = b'Zelda3_v0     \n\0'

    print(f"\nSignature: {signature}")
    print(f"Expected:  {expected_sig}")
    print(f"Match: {signature == expected_sig}")

    # Check SHA256
    sha256 = data[16:48]
    print(f"\nSHA256 hash present: {len(sha256)} bytes")
    print(f"SHA256 hex: {sha256.hex()[:32]}...")

    # Check padding
    padding = data[48:80]
    print(f"\nPadding (should be 32 zero bytes): {all(b == 0 for b in padding)}")
    print(f"Padding bytes: {len(padding)}")

    # Check header values at offset 80
    if len(data) < 88:
        print("ERROR: File too small for header")
        return False

    # Try both endianness
    num_assets_le = struct.unpack('<I', data[80:84])[0]  # Little-endian
    key_sig_len_le = struct.unpack('<I', data[84:88])[0]

    num_assets_be = struct.unpack('>I', data[80:84])[0]  # Big-endian
    key_sig_len_be = struct.unpack('>I', data[84:88])[0]

    print(f"\nHeader at offset 80-87:")
    print(f"  Little-endian: num_assets={num_assets_le}, key_sig_len={key_sig_len_le}")
    print(f"  Big-endian:    num_assets={num_assets_be}, key_sig_len={key_sig_len_be}")

    # Determine which makes sense
    if 50 < num_assets_le < 200 and 1000 < key_sig_len_le < 10000:
        print(f"  → Little-endian appears correct")
        num_assets, key_sig_len = num_assets_le, key_sig_len_le
        endian = 'little'
    elif 50 < num_assets_be < 200 and 1000 < key_sig_len_be < 10000:
        print(f"  → Big-endian appears correct")
        num_assets, key_sig_len = num_assets_be, key_sig_len_be
        endian = 'big'
    else:
        print(f"  → WARNING: Neither endianness looks correct!")
        num_assets, key_sig_len = num_assets_le, key_sig_len_le
        endian = 'unknown'

    # Check size table
    size_table_offset = 88
    size_table_end = size_table_offset + num_assets * 4

    if len(data) < size_table_end:
        print(f"\nERROR: File too small for size table (need {size_table_end} bytes)")
        return False

    print(f"\nSize table:")
    print(f"  Offset: {size_table_offset}")
    print(f"  Count: {num_assets} assets")
    print(f"  End: {size_table_end}")

    # Read first few sizes
    fmt = '<' if endian == 'little' else '>'
    sizes = []
    for i in range(min(5, num_assets)):
        offset = size_table_offset + i * 4
        size = struct.unpack(f'{fmt}I', data[offset:offset+4])[0]
        sizes.append(size)

    print(f"  First 5 asset sizes: {sizes}")

    # Check key signature
    key_sig_offset = size_table_end
    key_sig_end = key_sig_offset + key_sig_len

    if len(data) < key_sig_end:
        print(f"\nERROR: File too small for key signatures")
        return False

    print(f"\nKey signatures:")
    print(f"  Offset: {key_sig_offset}")
    print(f"  Length: {key_sig_len}")
    print(f"  End: {key_sig_end}")

    key_sig_data = data[key_sig_offset:key_sig_end]
    keys = key_sig_data.split(b'\0')
    print(f"  Number of keys: {len(keys) - 1}")  # -1 for trailing null
    print(f"  First 5 keys: {[k.decode('utf8', errors='replace') for k in keys[:5]]}")

    # Verify SHA256
    computed_sha = hashlib.sha256(key_sig_data).digest()
    print(f"\nSHA256 verification:")
    print(f"  Stored:   {sha256.hex()[:32]}...")
    print(f"  Computed: {computed_sha.hex()[:32]}...")
    print(f"  Match: {sha256 == computed_sha}")

    # Check asset data
    asset_data_offset = key_sig_end
    print(f"\nAsset data starts at: {asset_data_offset}")
    print(f"Remaining data: {len(data) - asset_data_offset:,} bytes")

    # Check alignment
    total_expected = 0
    for i in range(num_assets):
        offset = size_table_offset + i * 4
        size = struct.unpack(f'{fmt}I', data[offset:offset+4])[0]
        total_expected += size
        # Add padding to 4-byte boundary
        if size & 3:
            total_expected += 4 - (size & 3)

    print(f"Total asset data expected: {total_expected:,} bytes")
    print(f"Total asset data actual: {len(data) - asset_data_offset:,} bytes")
    print(f"Match: {total_expected == len(data) - asset_data_offset}")

    print("\n" + "=" * 60)
    if endian == 'little' and sha256 == computed_sha:
        print("✓ File appears to be valid!")
        return True
    else:
        print("✗ File has issues!")
        return False

if __name__ == '__main__':
    filename = sys.argv[1] if len(sys.argv) > 1 else '../zelda3_assets.dat'
    diagnose_asset_file(filename)
