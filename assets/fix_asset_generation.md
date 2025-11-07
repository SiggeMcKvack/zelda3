# Fix for Cross-Platform zelda3_assets.dat Generation

## Problem

The Windows-generated `zelda3_assets.dat` file may not work on Android due to platform-specific binary format issues.

## Root Causes

### 1. Implicit Byte Order in array.array

**Location**: `compile_resources.py:803`

```python
encoded_sizes = array.array('I', [len(i) for i in all_data])
```

The format code `'I'` creates an unsigned int array with **platform-native** byte order. While both Windows x86 and Android ARM are little-endian, this is implicit and not portable.

### 2. Implicit Byte Order in struct.pack

**Location**: `compile_resources.py:801`

```python
hdr = assets_sig + b'\x00' * 32 + struct.pack('II', len(all_data), len(key_sig))
```

The format `'II'` uses platform-native byte order. Should use `'<II'` for explicit little-endian.

### 3. Potential Path Issues

**Location**: `compile_resources.py:812`

```python
open('../zelda3_assets.dat', 'wb').write(file_data)
```

Uses relative path which might resolve differently on different platforms.

## Solution

### Fix 1: Explicit Little-Endian for struct.pack

```python
# OLD (line 801):
hdr = assets_sig + b'\x00' * 32 + struct.pack('II', len(all_data), len(key_sig))

# NEW:
hdr = assets_sig + b'\x00' * 32 + struct.pack('<II', len(all_data), len(key_sig))
```

The `<` prefix forces little-endian byte order.

### Fix 2: Use struct.pack for Size Array

```python
# OLD (line 803):
encoded_sizes = array.array('I', [len(i) for i in all_data])

# NEW:
encoded_sizes = struct.pack(f'<{len(all_data)}I', *[len(i) for i in all_data])
```

This explicitly packs the sizes as little-endian uint32 values.

### Fix 3: Absolute Output Path

```python
# OLD (line 812):
open('../zelda3_assets.dat', 'wb').write(file_data)

# NEW:
import os
output_path = os.path.join(os.path.dirname(__file__), '..', 'zelda3_assets.dat')
output_path = os.path.normpath(output_path)
with open(output_path, 'wb') as f:
    f.write(file_data)
```

## Complete Patched Function

```python
def write_assets_to_file(print_header = False):
  import os  # Add at top of function

  key_sig = b''
  all_data = []
  if print_header:
    print('''#pragma once
#include "types.h"

enum {
  kNumberOfAssets = %d
};
extern const uint8 *g_asset_ptrs[kNumberOfAssets];
extern uint32 g_asset_sizes[kNumberOfAssets];
extern MemBlk FindInAssetArray(int asset, int idx);
''' % len(assets))

  for i, (k, (tp, data)) in enumerate(assets.items()):
    if print_header:
      if tp == 'packed':
        print('#define %s(idx) FindInAssetArray(%d, idx)' % (k, i))
      else:
        print('#define %s ((%s*)g_asset_ptrs[%d])' % (k, tp, i))
        print('#define %s_SIZE (g_asset_sizes[%d])' % (k, i))
    key_sig += k.encode('utf8') + b'\0'
    all_data.append(data)

  assets_sig = b'Zelda3_v0     \n\0' + hashlib.sha256(key_sig).digest()

  if print_header:
    print('#define kAssets_Sig %s' % ", ".join((str(a) for a in assets_sig)))

  # FIX: Use explicit little-endian byte order
  hdr = assets_sig + b'\x00' * 32 + struct.pack('<II', len(all_data), len(key_sig))

  # FIX: Use struct.pack instead of array.array for explicit byte order
  encoded_sizes = struct.pack(f'<{len(all_data)}I', *[len(i) for i in all_data])

  file_data = hdr + encoded_sizes + key_sig

  for v in all_data:
    while len(file_data) & 3:
      file_data += b'\0'
    file_data += v

  # FIX: Use absolute path with proper path joining
  output_path = os.path.join(os.path.dirname(__file__), '..', 'zelda3_assets.dat')
  output_path = os.path.normpath(output_path)

  with open(output_path, 'wb') as f:
    f.write(file_data)

  print(f"Wrote {len(file_data):,} bytes to {output_path}")
```

## How to Test

### 1. Run Diagnostic Tool

```bash
cd zelda3/assets
python diagnose_assets.py ../zelda3_assets.dat
```

This will check:
- File signature
- Byte order (little vs big endian)
- SHA256 hash
- Asset count and sizes
- Overall file structure

### 2. Generate Fresh Assets on Windows

```bash
cd zelda3/assets
python restool.py --extract-from-rom
```

### 3. Test on Android

Copy the generated `zelda3_assets.dat` to your Android device and test loading.

### 4. Compare Before/After

```bash
# Generate with old code
python restool.py
mv ../zelda3_assets.dat ../zelda3_assets_old.dat

# Apply fix and regenerate
# (apply the patches above)
python restool.py
mv ../zelda3_assets.dat ../zelda3_assets_new.dat

# Compare
python diagnose_assets.py ../zelda3_assets_old.dat > old_diag.txt
python diagnose_assets.py ../zelda3_assets_new.dat > new_diag.txt
diff old_diag.txt new_diag.txt
```

## Alternative: Check C++ Loading Code

If the fix above doesn't work, the issue might be in how the C++ code reads the file on Android. Check:

### Android-Specific File Loading

The C++ code in `src/main.c:813` uses:
```c
uint8 *data = ReadWholeFile("zelda3_assets.dat", &length);
```

Make sure `ReadWholeFile` on Android:
1. Opens file in binary mode
2. Looks in the correct location (APK assets vs external storage)
3. Doesn't modify the data during reading

### Check Asset Path on Android

On Android, you might need to:
1. Extract assets from APK to cache directory first
2. Use Android NDK asset APIs
3. Use JNI to access app resources

Example Android-specific code:
```c
#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

uint8* ReadWholeFileAndroid(AAssetManager* assetManager,
                            const char* filename,
                            size_t* length) {
  AAsset* asset = AAssetManager_open(assetManager, filename, AASSET_MODE_BUFFER);
  if (!asset) return NULL;

  *length = AAsset_getLength(asset);
  uint8* buffer = malloc(*length);
  AAsset_read(asset, buffer, *length);
  AAsset_close(asset);

  return buffer;
}
#endif
```

## Quick Test Script

Create `test_asset_compatibility.py`:

```python
#!/usr/bin/env python3
import struct

def test_endianness():
    # Test uint32 packing
    value = 0x12345678

    # Platform native
    native = struct.pack('I', value)

    # Explicit little-endian
    little = struct.pack('<I', value)

    # Explicit big-endian
    big = struct.pack('>I', value)

    print(f"Value: 0x{value:08x}")
    print(f"Native: {native.hex()}")
    print(f"Little: {little.hex()}")
    print(f"Big:    {big.hex()}")
    print(f"Native == Little: {native == little}")

if __name__ == '__main__':
    test_endianness()
```

On Windows x86-64 and Android ARM, you should see:
```
Value: 0x12345678
Native: 78563412
Little: 78563412
Big:    12345678
Native == Little: True
```

If `Native != Little`, that's your issue!

## Summary

**Most likely issue**: Implicit byte order in binary packing
**Fix**: Add explicit little-endian format codes (`<I` instead of `I`)
**Test**: Run diagnostic tool to verify file format
**Fallback**: Check Android-specific file loading code

Apply the fixes above and regenerate the asset file. The diagnostic tool will help confirm the file is correctly formatted.
