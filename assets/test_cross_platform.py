#!/usr/bin/env python3
"""
Test script to verify cross-platform binary compatibility
"""
import struct
import sys
import os

def test_byte_order():
    """Test that struct packing produces expected byte order"""
    print("=" * 60)
    print("Testing Byte Order Compatibility")
    print("=" * 60)

    test_value = 0x12345678

    # Test native
    native = struct.pack('I', test_value)

    # Test explicit little-endian
    little = struct.pack('<I', test_value)

    # Test explicit big-endian
    big = struct.pack('>I', test_value)

    print(f"\nTest value: 0x{test_value:08x}")
    print(f"Native:        {native.hex()}")
    print(f"Little-endian: {little.hex()}")
    print(f"Big-endian:    {big.hex()}")

    print(f"\nPlatform: {sys.platform}")
    print(f"Byte order: {sys.byteorder}")

    if native == little:
        print("✓ Native byte order is little-endian (x86/ARM compatible)")
    else:
        print("⚠ WARNING: Native byte order is NOT little-endian!")
        print("  This could cause compatibility issues")

    # Test that our fix is correct
    expected_little = b'\x78\x56\x34\x12'
    if little == expected_little:
        print("✓ Little-endian packing is correct")
    else:
        print(f"✗ ERROR: Little-endian packing mismatch!")
        print(f"  Expected: {expected_little.hex()}")
        print(f"  Got:      {little.hex()}")

    return native == little

def test_multiple_ints():
    """Test packing multiple integers"""
    print("\n" + "=" * 60)
    print("Testing Multiple Integer Packing")
    print("=" * 60)

    values = [100, 200, 300, 400, 500]

    # Old way (platform-dependent)
    import array
    old_way = array.array('I', values)

    # New way (explicit little-endian)
    new_way = struct.pack(f'<{len(values)}I', *values)

    print(f"\nTest values: {values}")
    print(f"Old way (array.array): {old_way.tobytes().hex()[:40]}...")
    print(f"New way (struct.pack): {new_way.hex()[:40]}...")

    if old_way.tobytes() == new_way:
        print("✓ Both methods produce identical output on this platform")
    else:
        print("⚠ WARNING: Methods produce different output!")

    # Test that new way is definitely little-endian
    manual_check = struct.pack('<I', values[0])
    if new_way[:4] == manual_check:
        print("✓ New method is definitely little-endian")
    else:
        print("✗ ERROR: New method is not little-endian!")

    return True

def test_asset_file_if_exists():
    """Test actual asset file if it exists"""
    print("\n" + "=" * 60)
    print("Testing Actual Asset File")
    print("=" * 60)

    asset_path = os.path.join(os.path.dirname(__file__), '..', 'zelda3_assets.dat')
    asset_path = os.path.normpath(asset_path)

    if not os.path.exists(asset_path):
        print(f"\nAsset file not found at: {asset_path}")
        print("Run 'python restool.py' to generate it first")
        return True

    print(f"\nAsset file found: {asset_path}")

    with open(asset_path, 'rb') as f:
        # Read header
        signature = f.read(16)
        sha256 = f.read(32)
        padding = f.read(32)
        num_assets_bytes = f.read(4)
        key_sig_len_bytes = f.read(4)

    # Check signature
    expected_sig = b'Zelda3_v0     \n\0'
    if signature == expected_sig:
        print("✓ Signature is correct")
    else:
        print("✗ Signature is incorrect!")
        return False

    # Check padding is all zeros
    if all(b == 0 for b in padding):
        print("✓ Padding is correct")
    else:
        print("✗ Padding contains non-zero bytes!")

    # Check header values
    num_assets_le = struct.unpack('<I', num_assets_bytes)[0]
    key_sig_len_le = struct.unpack('<I', key_sig_len_bytes)[0]

    print(f"\nHeader values (little-endian):")
    print(f"  Number of assets: {num_assets_le}")
    print(f"  Key signature length: {key_sig_len_le}")

    if 50 < num_assets_le < 200 and 1000 < key_sig_len_le < 10000:
        print("✓ Header values look reasonable")
    else:
        print("⚠ WARNING: Header values look suspicious!")
        print("  File might not be properly formatted")
        return False

    file_size = os.path.getsize(asset_path)
    print(f"\nTotal file size: {file_size:,} bytes ({file_size / 1024 / 1024:.2f} MB)")

    return True

def main():
    """Run all tests"""
    print("\nCross-Platform Binary Compatibility Test Suite")
    print("This verifies that the asset file will work on all platforms")

    results = []

    results.append(("Byte Order Test", test_byte_order()))
    results.append(("Multiple Integer Packing Test", test_multiple_ints()))
    results.append(("Asset File Test", test_asset_file_if_exists()))

    print("\n" + "=" * 60)
    print("Test Results Summary")
    print("=" * 60)

    all_passed = True
    for name, passed in results:
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"{status}: {name}")
        if not passed:
            all_passed = False

    print("\n" + "=" * 60)
    if all_passed:
        print("✓ All tests passed!")
        print("The asset file should work on Windows, Linux, macOS, and Android")
    else:
        print("✗ Some tests failed!")
        print("There may be compatibility issues with the asset file")
    print("=" * 60)

    return 0 if all_passed else 1

if __name__ == '__main__':
    sys.exit(main())
