# Android Integration Guide for zelda3_assets.dat

This guide shows you how to integrate the cross-platform compatible asset file into your Android port of Zelda3.

## Prerequisites

- Windows PC with Python 3.x installed
- Android project for Zelda3
- Original Zelda 3 (USA) ROM file: `zelda3.sfc`

---

## Step 1: Generate Assets on Windows

### Quick Method (Recommended)

Run the automated build script:

```cmd
build_assets_windows.bat
```

This will:
1. Check for Python and dependencies
2. Test cross-platform compatibility
3. Extract and compile assets
4. Verify the output file
5. Generate `zelda3_assets.dat`

### Manual Method

```cmd
cd assets
python restool.py --extract-from-rom
python diagnose_assets.py ..\zelda3_assets.dat
cd ..
```

### Verify Success

You should see:
```
✓ File appears to be valid!
Endianness: little-endian
SHA256: Match
```

The generated file `zelda3_assets.dat` should be **~18-22 MB**.

---

## Step 2: Choose Android Integration Method

You have three options for how users get the asset file:

### Option A: Bundle in APK (Simplest)

**Best for**: Single-language, small apps, immediate playability

**Pros**: No network needed, instant gameplay
**Cons**: Larger APK size (+20MB)

**Implementation**:

1. Copy asset file to Android project:
   ```
   YourAndroidProject/
   └── app/
       └── src/
           └── main/
               └── assets/
                   └── zelda3_assets.dat  ← Copy here
   ```

2. In your C++ code (JNI):
   ```cpp
   #include <android/asset_manager.h>
   #include <android/asset_manager_jni.h>

   uint8_t* LoadAssetsFromAPK(JNIEnv* env, jobject assetManager, size_t* out_length) {
       AAssetManager* mgr = AAssetManager_fromJava(env, assetManager);
       AAsset* asset = AAssetManager_open(mgr, "zelda3_assets.dat", AASSET_MODE_BUFFER);

       if (!asset) {
           __android_log_print(ANDROID_LOG_ERROR, "Zelda3",
                             "Failed to open zelda3_assets.dat from APK");
           return NULL;
       }

       *out_length = AAsset_getLength(asset);
       uint8_t* buffer = (uint8_t*)malloc(*out_length);
       AAsset_read(asset, buffer, *out_length);
       AAsset_close(asset);

       return buffer;
   }
   ```

3. In your Java/Kotlin code:
   ```java
   public class MainActivity extends Activity {
       static {
           System.loadLibrary("zelda3");
       }

       @Override
       protected void onCreate(Bundle savedInstanceState) {
           super.onCreate(savedInstanceState);

           // Pass AssetManager to native code
           nativeLoadAssets(getAssets());
       }

       private native void nativeLoadAssets(AssetManager assetManager);
   }
   ```

### Option B: Download on First Run

**Best for**: Multi-language support, smaller initial download

**Pros**: Small APK, can update assets independently
**Cons**: Requires network, first-run delay

**Implementation**:

1. Host the file on your server:
   ```
   https://yourserver.com/assets/zelda3_assets_en.dat
   https://yourserver.com/assets/zelda3_assets_de.dat
   https://yourserver.com/assets/zelda3_assets_fr.dat
   ```

2. Download and cache in app:
   ```java
   public class AssetDownloader {
       private static final String ASSET_URL =
           "https://yourserver.com/assets/zelda3_assets_en.dat";
       private static final String ASSET_FILENAME = "zelda3_assets.dat";

       public void downloadAssets(Context context, ProgressCallback callback) {
           File cacheFile = new File(context.getCacheDir(), ASSET_FILENAME);

           // Check if already cached
           if (cacheFile.exists() && verifyAssetFile(cacheFile)) {
               callback.onComplete(cacheFile.getAbsolutePath());
               return;
           }

           // Download
           DownloadManager.Request request = new DownloadManager.Request(Uri.parse(ASSET_URL));
           request.setDestinationInExternalFilesDir(context, null, ASSET_FILENAME);

           DownloadManager dm = (DownloadManager) context.getSystemService(Context.DOWNLOAD_SERVICE);
           long downloadId = dm.enqueue(request);

           // Monitor download progress...
       }

       private boolean verifyAssetFile(File file) {
           // Verify first 16 bytes match "Zelda3_v0     \n\0"
           try (FileInputStream fis = new FileInputStream(file)) {
               byte[] sig = new byte[16];
               fis.read(sig);
               return Arrays.equals(sig, "Zelda3_v0     \n\0".getBytes());
           } catch (IOException e) {
               return false;
           }
       }
   }
   ```

3. Load from cache in C++:
   ```cpp
   // Get cache path from Java, then load normally
   uint8_t* LoadAssetsFromCache(const char* cache_path, size_t* out_length) {
       FILE* f = fopen(cache_path, "rb");
       if (!f) return NULL;

       fseek(f, 0, SEEK_END);
       *out_length = ftell(f);
       fseek(f, 0, SEEK_SET);

       uint8_t* buffer = (uint8_t*)malloc(*out_length);
       fread(buffer, 1, *out_length, f);
       fclose(f);

       return buffer;
   }
   ```

### Option C: Google Play Asset Packs

**Best for**: Very large assets, professional distribution

**Pros**: Handled by Google Play, efficient delivery
**Cons**: Only works with Google Play, more complex setup

See: https://developer.android.com/guide/playcore/asset-delivery

---

## Step 3: Update Your Build Configuration

### CMakeLists.txt (if using CMake)

```cmake
cmake_minimum_required(VERSION 3.10)
project(zelda3)

# Add your source files
add_library(zelda3 SHARED
    src/main.c
    src/zelda_rtl.c
    # ... all your other C files
)

# Link against Android libraries
target_link_libraries(zelda3
    android
    log
    # ... other libraries
)

# If bundling assets in APK, no special handling needed
# The asset will be accessible via AssetManager
```

### build.gradle (app level)

```gradle
android {
    compileSdk 33

    defaultConfig {
        applicationId "com.yourname.zelda3"
        minSdk 21
        targetSdk 33

        // If bundling in APK, Android will compress large files
        // You may want to prevent compression:
        aaptOptions {
            noCompress "dat"  // Don't compress .dat files
        }
    }

    // ... rest of your config
}
```

---

## Step 4: Test on Android

### Test Checklist:

1. **Asset Loading**
   ```
   ✓ Asset file loads successfully
   ✓ File signature is correct
   ✓ No crashes during loading
   ✓ Asset count matches expected value
   ```

2. **Game Functionality**
   ```
   ✓ Graphics display correctly
   ✓ Text/dialogue appears
   ✓ Music plays
   ✓ Maps/dungeons load
   ✓ No corrupted sprites
   ```

3. **Cross-Device Testing**
   ```
   ✓ Works on ARM32 devices
   ✓ Works on ARM64 devices
   ✓ Works on x86 emulator (if needed)
   ✓ Works on different Android versions (5.0+)
   ```

### Debugging Tips

If assets don't load:

1. **Check file location**:
   ```java
   // Log where you're looking for the file
   Log.d("Zelda3", "Asset path: " + assetPath);
   ```

2. **Verify file signature**:
   ```cpp
   // In C++ before parsing
   if (memcmp(data, "Zelda3_v0     \n\0", 16) != 0) {
       __android_log_print(ANDROID_LOG_ERROR, "Zelda3",
                         "Invalid asset signature!");
   }
   ```

3. **Check endianness**:
   ```cpp
   uint32_t test = *(uint32_t*)(data + 80);
   __android_log_print(ANDROID_LOG_DEBUG, "Zelda3",
                     "Asset count: %u (should be 100-150)", test);
   ```

4. **Compare with desktop**:
   - Generate asset file on Windows
   - Load in desktop version (should work)
   - Copy same file to Android
   - If desktop works but Android doesn't, issue is in Android loading code

---

## Step 5: Add Language Support (Optional)

### Generate Multi-Language Assets

On Windows:

```cmd
cd assets

REM Extract dialogue from different ROM versions
python restool.py --extract-dialogue --rom zelda3_de.sfc
python restool.py --extract-dialogue --rom zelda3_fr.sfc
python restool.py --extract-dialogue --rom zelda3_es.sfc

REM Build with multiple languages
python restool.py --languages de,fr,es
```

### In Android App

```java
public class LanguageSelector {
    public static String getAssetFilename(String languageCode) {
        switch (languageCode) {
            case "de": return "zelda3_assets_de.dat";
            case "fr": return "zelda3_assets_fr.dat";
            case "es": return "zelda3_assets_es.dat";
            default:   return "zelda3_assets.dat";  // English
        }
    }

    public static void downloadLanguageAssets(Context context, String lang) {
        String filename = getAssetFilename(lang);
        String url = "https://yourserver.com/assets/" + filename;
        // Download...
    }
}
```

---

## Complete Example: Simple Android Activity

```java
package com.yourname.zelda3;

import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

public class MainActivity extends Activity {
    private static final String TAG = "Zelda3";

    static {
        System.loadLibrary("zelda3");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Method 1: Load from APK assets
        if (loadAssetsFromAPK()) {
            Log.i(TAG, "Assets loaded successfully from APK");
            startGame();
        } else {
            Log.e(TAG, "Failed to load assets");
            showError("Failed to load game assets");
        }
    }

    private boolean loadAssetsFromAPK() {
        try {
            AssetManager assetManager = getAssets();
            return nativeLoadAssets(assetManager);
        } catch (Exception e) {
            Log.e(TAG, "Exception loading assets", e);
            return false;
        }
    }

    private void startGame() {
        // Initialize SDL or your rendering system
        nativeStartGame();
    }

    private void showError(String message) {
        // Show error dialog to user
    }

    // Native methods
    private native boolean nativeLoadAssets(AssetManager assetManager);
    private native void nativeStartGame();
}
```

---

## Troubleshooting

### "Asset file not found"

**Solution**:
- Verify file is in `app/src/main/assets/`
- Check filename exactly matches (case-sensitive)
- Rebuild project to re-package APK

### "Invalid asset signature"

**Solution**:
- Regenerate asset file with fixed `restool.py`
- Run diagnostic: `python diagnose_assets.py zelda3_assets.dat`
- Make sure you're using the latest version from this branch

### "Asset count is wrong"

**Solution**:
- File may be corrupted during transfer
- Check file size matches (~18-22 MB)
- Regenerate asset file
- Verify SHA256 checksum

### "Graphics corrupted on Android"

**Solution**:
- Asset file is loading but may have byte-order issues
- Run test: `python test_cross_platform.py`
- Verify you're using fixed version of `compile_resources.py`
- Check that you're not modifying the file during transfer (e.g., line ending conversion)

### "Works on emulator but not real device"

**Solution**:
- Emulators are x86, real devices are ARM
- This suggests a byte-order issue
- Verify asset file was built with explicit little-endian format
- Check compiler flags in native code

---

## Performance Tips

1. **Don't keep asset file in RAM**: The C++ code should load assets on-demand
2. **Use mmap for large files**: Instead of reading entire file
   ```cpp
   int fd = open(path, O_RDONLY);
   void* mapped = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
   ```
3. **Cache parsed assets**: Parse once, keep structured data in memory
4. **Compress HTTP downloads**: Use gzip if downloading from server

---

## Security Notes

1. **Verify asset integrity**: Check SHA256 hash of downloaded files
2. **Use HTTPS**: When downloading from server
3. **Handle corrupted files**: Graceful error messages, not crashes
4. **ROM legality**: Don't bundle ROM, only extracted game data

---

## Summary

✅ **Generate** assets on Windows with `build_assets_windows.bat`
✅ **Verify** with `diagnose_assets.py`
✅ **Choose** integration method (APK bundle recommended)
✅ **Copy** `zelda3_assets.dat` to `app/src/main/assets/`
✅ **Load** using Android AssetManager
✅ **Test** on real device

The cross-platform fix ensures the asset file works identically on Windows, Linux, macOS, and Android with no modifications needed!
