// android_jni.c - JNI interface for Android hot-reload functionality
#include <jni.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <SDL.h>
#include "jni_helpers.h"
#include "config.h"
#include "features.h"
#include "zelda_rtl.h"
#include "snes/ppu.h"
#include "logging.h"

#define LOG_TAG "Zelda3JNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Android-specific global state (desktop has these in main.c)
uint8_t g_paused = 0;

// External function declarations
void ParseConfigFile(const char *filename);
void ZeldaEnableMsu(uint8_t enable);
void ZeldaApuLock(void);
void ZeldaApuUnlock(void);
void SaveLoadSlot(int cmd, int which);

// JNI function to reload audio config without restarting the app
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeReloadAudioConfig(
    JNIEnv* env, jobject obj, jint enable_msu, jint msu_vol, jint disable_low_health_beep) {

    LOGD("nativeReloadAudioConfig called with: enable_msu=%d, msu_volume=%d, disable_beep=%d",
         enable_msu, msu_vol, disable_low_health_beep);

    // Save old values
    uint8_t old_enable_msu = g_config.enable_msu;
    uint8_t old_msuvolume = g_config.msuvolume;
    uint32_t old_features0 = g_config.features0;

    LOGD("Old values: enable_msu=%u, msuvolume=%u, features0=%u", old_enable_msu, old_msuvolume, old_features0);

    // Update config directly (file will be updated by Kotlin side)
    g_config.enable_msu = (uint8_t)enable_msu;
    g_config.msuvolume = (uint8_t)msu_vol;

    // Update DisableLowHealthBeep feature flag
    if (disable_low_health_beep) {
        g_config.features0 |= kFeatures0_DisableLowHealthBeep;  // Set bit
    } else {
        g_config.features0 &= ~kFeatures0_DisableLowHealthBeep;  // Clear bit
    }
    g_wanted_zelda_features = g_config.features0;

    LOGD("New values: enable_msu=%u, msuvolume=%u, features0=%u",
         g_config.enable_msu, g_config.msuvolume, g_config.features0);

    // Apply MSU settings change
    if (old_enable_msu != g_config.enable_msu || old_msuvolume != g_config.msuvolume) {
        LOGD("MSU settings changed, calling ZeldaEnableMsu(%u)", g_config.enable_msu);
        ZeldaApuLock();
        ZeldaEnableMsu(g_config.enable_msu);
        ZeldaApuUnlock();
        LOGD("ZeldaEnableMsu completed");
    } else {
        LOGD("MSU settings unchanged, skipping ZeldaEnableMsu");
    }

    LOGD("Hot-reload complete");
}

// JNI environment and method calling helpers are now in jni_helpers.c

/**
 * Opens an MSU file using Android SAF (Storage Access Framework).
 * Called from audio.c when loading MSU files on Android 13+.
 *
 * @param filename Relative filename like "ALttP-msu-Deluxe-1.pcm" or "alttp_msu-1.opuz"
 * @return File descriptor (>= 0) on success, -1 on failure
 */
int Android_OpenMsuFileDescriptor(const char *filename) {
    return JniHelper_CallStaticIntMethod_1S("openMsuFile", filename);
}

/**
 * Loads an asset file from the APK using Android AssetManager.
 * Called from vulkan.c when loading shader SPIR-V files.
 *
 * @param asset_path Relative path within assets, e.g. "shaders/vert.spv"
 * @param out_size Pointer to store the size of the loaded data
 * @return Pointer to allocated buffer containing asset data, or NULL on failure
 *         Caller must free() the returned buffer.
 */
void* Android_LoadAsset(const char *asset_path, int *out_size) {
    return JniHelper_CallStaticByteArrayMethod_1S("loadAsset", asset_path, out_size);
}

/**
 * Saves the current game state to the specified slot.
 * Called from MainActivity when user selects a save slot.
 *
 * @param slot Save slot number (0-9, where 0 = Quick Save)
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeSaveState(
    JNIEnv* env, jobject obj, jint slot) {
    LOGD("nativeSaveState: slot=%d", slot);
    SaveLoadSlot(0, (int)slot);  // 0 = kSaveLoad_Save
}

/**
 * Loads the game state from the specified slot.
 * Called from MainActivity when user selects a load slot.
 *
 * @param slot Save slot number (0-9, where 0 = Quick Save)
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeLoadState(
    JNIEnv* env, jobject obj, jint slot) {
    LOGD("nativeLoadState: slot=%d", slot);
    SaveLoadSlot(1, (int)slot);  // 1 = kSaveLoad_Load
}

/**
 * Gets the current frame buffer as RGBA data for thumbnail generation.
 * Returns 256x224 RGBA image (229,376 bytes).
 *
 * @return byte array containing RGBA data, or null if not available
 */
JNIEXPORT jbyteArray JNICALL Java_com_dishii_zelda3_MainActivity_nativeGetScreenshotRGBA(
    JNIEnv* env, jobject obj) {

    uint8_t *buffer = NULL;
    int width = 0, height = 0, pitch = 0;

    // Get frame buffer from PPU
    PpuGetFrameBuffer(g_zenv.ppu, &buffer, &width, &height, &pitch);

    if (!buffer || width == 0 || height == 0) {
        LOGD("nativeGetScreenshotRGBA: No frame buffer available");
        return NULL;
    }

    LOGD("nativeGetScreenshotRGBA: width=%d, height=%d, pitch=%d", width, height, pitch);

    // Calculate total size: width * height * 4 bytes per pixel (RGBA)
    int totalSize = width * height * 4;

    // Create Java byte array
    jbyteArray result = (*env)->NewByteArray(env, totalSize);
    if (!result) {
        LOGD("nativeGetScreenshotRGBA: Failed to allocate byte array");
        return NULL;
    }

    // Copy pixel data row by row (to handle pitch) and convert BGRA to RGBA
    jbyte *resultData = (*env)->GetByteArrayElements(env, result, NULL);
    if (!resultData) {
        LOGD("nativeGetScreenshotRGBA: Failed to get byte array elements");
        (*env)->DeleteLocalRef(env, result);
        return NULL;
    }

    // PPU outputs BGRA format, but Android Bitmap expects RGBA
    // We need to swap B and R channels
    // Also need to account for extraLeftRight offset in the buffer
    int extraLeftRight = g_zenv.ppu->extraLeftRight;
    LOGD("nativeGetScreenshotRGBA: extraLeftRight=%d", extraLeftRight);

    for (int y = 0; y < height; y++) {
        // Skip extraLeftRight pixels on the left
        uint8_t *src = buffer + (y * pitch) + (extraLeftRight * 4);
        uint8_t *dst = (uint8_t*)resultData + (y * width * 4);

        for (int x = 0; x < width; x++) {
            dst[x * 4 + 0] = src[x * 4 + 2];  // R from B
            dst[x * 4 + 1] = src[x * 4 + 1];  // G stays G
            dst[x * 4 + 2] = src[x * 4 + 0];  // B from R
            dst[x * 4 + 3] = 0xFF;             // Alpha (PPU outputs 0, we want opaque)
        }
    }

    (*env)->ReleaseByteArrayElements(env, result, resultData, 0);

    LOGD("nativeGetScreenshotRGBA: Returning %d bytes (converted BGRA->RGBA)", totalSize);
    return result;
}

/**
 * Toggles the game pause state.
 * Called from MainActivity when user selects "Pause" from nav drawer.
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeTogglePause(
    JNIEnv* env, jobject obj) {
    extern uint8 g_paused;
    g_paused = !g_paused;
    LOGD("nativeTogglePause: g_paused=%d", g_paused);
}

/**
 * Gets the current pause state.
 * Called from MainActivity to query pause status.
 *
 * @return true if game is paused, false otherwise
 */
JNIEXPORT jboolean JNICALL Java_com_dishii_zelda3_MainActivity_nativeIsPaused(
    JNIEnv* env, jobject obj) {
    extern uint8 g_paused;
    return (jboolean)g_paused;
}

/**
 * Binds a gamepad button (with optional modifiers) to a command.
 * Called from MainActivity's controller settings dialog.
 *
 * @param buttonName Name of button (e.g., "DpadUp", "A", "Start")
 * @param modifierNames Array of modifier button names (e.g., ["Start", "Select"]), can be null
 * @param commandId Command ID (e.g., 1 for Up, 2 for Down, etc.)
 * @return true if binding succeeded, false otherwise
 */
JNIEXPORT jboolean JNICALL Java_com_dishii_zelda3_MainActivity_nativeBindGamepadButton(
    JNIEnv* env, jobject obj, jstring buttonName, jobjectArray modifierNames, jint commandId) {

    // Convert button name to C string
    const char *btnStr = (*env)->GetStringUTFChars(env, buttonName, NULL);
    if (!btnStr) {
        LOGD("nativeBindGamepadButton: Failed to get button name");
        return JNI_FALSE;
    }

    // Parse button name to button ID
    const char *btnPtr = btnStr;
    int button = ParseGamepadButtonName(&btnPtr);
    (*env)->ReleaseStringUTFChars(env, buttonName, btnStr);

    if (button == -1) {  // kGamepadBtn_Invalid
        LOGD("nativeBindGamepadButton: Invalid button name");
        return JNI_FALSE;
    }

    // Parse modifiers (if any)
    uint32 modifiers = 0;
    if (modifierNames != NULL) {
        jsize modCount = (*env)->GetArrayLength(env, modifierNames);
        for (jsize i = 0; i < modCount; i++) {
            jstring modStr = (jstring)(*env)->GetObjectArrayElement(env, modifierNames, i);
            const char *modCStr = (*env)->GetStringUTFChars(env, modStr, NULL);
            if (modCStr) {
                const char *modPtr = modCStr;
                int modBtn = ParseGamepadButtonName(&modPtr);
                if (modBtn != -1) {
                    modifiers |= (1 << modBtn);
                }
                (*env)->ReleaseStringUTFChars(env, modStr, modCStr);
            }
            (*env)->DeleteLocalRef(env, modStr);
        }
    }

    // Validate command ID (should be in range 0 to kKeys_Total)
    int cmd = (int)commandId;
    if (cmd < 0 || cmd >= 127) {  // kKeys_Total (from config.h:37)
        LOGD("nativeBindGamepadButton: Invalid command ID %d", cmd);
        return JNI_FALSE;
    }

    // Add binding
    GamepadMap_Add(button, modifiers, (uint16)cmd);
    LOGD("nativeBindGamepadButton: Bound button %d (modifiers=%u) to cmd %d", button, modifiers, cmd);
    return JNI_TRUE;
}

/**
 * Unbinds a specific gamepad button+modifier combination.
 * Called from MainActivity's controller settings dialog.
 *
 * @param buttonName Name of button to unbind
 * @param modifierNames Array of modifier button names, can be null
 * @return true if unbinding succeeded, false otherwise
 */
JNIEXPORT jboolean JNICALL Java_com_dishii_zelda3_MainActivity_nativeUnbindGamepadButton(
    JNIEnv* env, jobject obj, jstring buttonName, jobjectArray modifierNames) {

    // TODO: Implement unbind logic (requires modifying config.c to add GamepadMap_Remove)
    LOGD("nativeUnbindGamepadButton: Not yet implemented");
    return JNI_FALSE;
}

/**
 * Clears all gamepad bindings.
 * Called from MainActivity's controller settings dialog.
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeClearGamepadBindings(
    JNIEnv* env, jobject obj) {
    GamepadMap_Clear();
    LOGD("nativeClearGamepadBindings: Cleared all gamepad bindings");
}

/**
 * Gets all current gamepad bindings as a JSON string.
 * Called from MainActivity to populate controller settings dialog.
 *
 * @return JSON string with format: [{"commandName":"Up","binding":"DpadUp"},...]
 */
JNIEXPORT jstring JNICALL Java_com_dishii_zelda3_MainActivity_nativeGetGamepadBindings(
    JNIEnv* env, jobject obj) {

    // Command IDs to check for bindings
    // Covers: Controls (1-12), Save (33-42), Load (13-22), Pause (121), Turbo (123), DisplayPerf (125), Cheats (113-116)
    static const int cmdIds[] = {
        1,2,3,4,5,6,7,8,9,10,11,12,  // Controls
        33,34,35,36,37,38,39,40,41,42,  // Save
        13,14,15,16,17,18,19,20,21,22,  // Load
        121,  // Pause
        123,  // Turbo
        125,  // DisplayPerf
        113,114,115,116  // Cheats
    };

    JniJsonBuilder *arr = JniJson_CreateArray();
    if (!arr) {
        LOGD("nativeGetGamepadBindings: Failed to create JSON array");
        return (*env)->NewStringUTF(env, "[]");
    }

    for (int i = 0; i < sizeof(cmdIds)/sizeof(cmdIds[0]); i++) {
        int cmdId = cmdIds[i];
        uint32 modifiers = 0;
        int button = GamepadMap_GetBindingForCommand(cmdId, &modifiers);

        if (button != -1) {
            const char *cmdName = FindCmdName(cmdId);
            if (!cmdName) continue;

            // Build button combo string (e.g., "L2" or "L2+R3")
            char buttonCombo[128];
            const char *btnName = JniHelper_GetButtonName(button);
            if (!btnName) continue;
            snprintf(buttonCombo, sizeof(buttonCombo), "%s", btnName);

            // Add modifiers
            for (int mod = 0; mod < JNI_GAMEPAD_BUTTON_COUNT; mod++) {
                if (modifiers & (1 << mod)) {
                    const char *modName = JniHelper_GetButtonName(mod);
                    if (modName) {
                        char temp[128];
                        snprintf(temp, sizeof(temp), "%s+%s", buttonCombo, modName);
                        snprintf(buttonCombo, sizeof(buttonCombo), "%s", temp);
                    }
                }
            }

            // Add binding entry to JSON array
            JniJsonBuilder *obj = JniJson_CreateObject();
            if (obj) {
                JniJson_AddString(obj, "commandName", cmdName);
                JniJson_AddString(obj, "binding", buttonCombo);
                JniJson_AddObject(arr, obj);
            }
        }
    }

    char *json = JniJson_Finalize(arr);
    if (!json) {
        LOGD("nativeGetGamepadBindings: Failed to finalize JSON");
        return (*env)->NewStringUTF(env, "[]");
    }

    LOGD("nativeGetGamepadBindings: Returning JSON: %s", json);
    jstring result = (*env)->NewStringUTF(env, json);
    free(json);
    return result;
}

/**
 * Applies default gamepad bindings.
 * Called from MainActivity when "Reset to defaults" button is clicked,
 * or automatically when a controller is detected with no existing bindings.
 *
 * Applies 13 default bindings:
 * - 12 standard SNES controls (DpadUp/Down/Left/Right, Back/Start, B/A/Y/X, L1/R1)
 * - 1 Turbo binding (L3 → Turbo)
 */
JNIEXPORT void JNICALL Java_com_dishii_zelda3_MainActivity_nativeApplyDefaultGamepadBindings(
    JNIEnv* env, jobject obj) {

    // Apply 12 standard controls
    // Order: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R
    for (int i = 0; i < 12; i++) {
        GamepadMap_Add(kDefaultGamepadCmds[i], 0, kKeys_Controls + i);
    }

    // Apply Turbo binding (L3 → Turbo)
    GamepadMap_Add(kGamepadBtn_L3, 0, kKeys_Turbo);

    LOGD("nativeApplyDefaultGamepadBindings: Applied 13 default bindings");
}

/**
 * Gets the gamepad button name for a specific command.
 * Returns the button name (e.g., "DpadUp", "A", "L1") or combo (e.g., "L2+R3") or null if not bound.
 *
 * @param cmdId The command ID (e.g., kKeys_Controls+0 for Up)
 * @return Button name string or null
 */
JNIEXPORT jstring JNICALL Java_com_dishii_zelda3_MainActivity_nativeGetButtonForCommand(
    JNIEnv* env, jobject obj, jint cmdId) {

    uint32 modifiers = 0;
    int button = GamepadMap_GetBindingForCommand(cmdId, &modifiers);

    if (button == -1) {
        LOGD("nativeGetButtonForCommand: cmd=%d -> not bound", cmdId);
        return NULL;
    }

    const char *btnName = JniHelper_GetButtonName(button);
    if (!btnName) {
        LOGD("nativeGetButtonForCommand: cmd=%d -> invalid button %d", cmdId, button);
        return NULL;
    }

    // Build the button combo string
    static char result[256];
    snprintf(result, sizeof(result), "%s", btnName);

    // Add modifier buttons (if any)
    if (modifiers) {
        for (int mod_btn = 0; mod_btn < JNI_GAMEPAD_BUTTON_COUNT; mod_btn++) {
            if (modifiers & (1 << mod_btn)) {
                const char *modName = JniHelper_GetButtonName(mod_btn);
                if (modName) {
                    strncat(result, "+", sizeof(result) - strlen(result) - 1);
                    strncat(result, modName, sizeof(result) - strlen(result) - 1);
                }
            }
        }
    }

    LOGD("nativeGetButtonForCommand: cmd=%d -> button=%s", cmdId, result);
    return (*env)->NewStringUTF(env, result);
}

// Show a Toast notification to the user
void Android_ShowToast(const char* message) {
    JniHelper_CallStaticVoidMethod_1S("showToast", message);
}

// Update renderer setting in zelda3.ini
void Android_UpdateRendererConfig(const char *renderer) {
    JniHelper_CallStaticVoidMethod_1S("updateRendererSetting", renderer);
}

// ============================================================================
// Restool JNI Wrappers (for ROM selection and asset extraction)
// ============================================================================

#include "restool/restool_lib.h"

/**
 * Identifies a ROM file and returns language information as JSON.
 * Called from RomSelectionActivity when user selects a ROM file.
 *
 * @param romPath Path to the ROM file
 * @return JSON string: {"lang_code":"us","lang_name":"USA","valid":true} or null on error
 */
JNIEXPORT jstring JNICALL Java_com_dishii_zelda3_RomSelectionActivity_nativeIdentifyRom(
    JNIEnv* env, jobject obj, jstring jromPath) {

    const char *rom_path = (*env)->GetStringUTFChars(env, jromPath, NULL);
    if (!rom_path) {
        LOGD("nativeIdentifyRom: Failed to get ROM path string");
        return NULL;
    }

    LOGD("nativeIdentifyRom: Identifying ROM at: %s", rom_path);

    RestoolRomInfo info;
    bool success = Restool_IdentifyRom(rom_path, &info);

    (*env)->ReleaseStringUTFChars(env, jromPath, rom_path);

    if (!success) {
        LOGD("nativeIdentifyRom: Failed to identify ROM");
        return NULL;
    }

    // Return JSON string with ROM info
    char json[256];
    snprintf(json, sizeof(json),
             "{\"lang_code\":\"%s\",\"lang_name\":\"%s\",\"valid\":%s}",
             info.lang_code, info.lang_name, info.valid ? "true" : "false");

    LOGD("nativeIdentifyRom: Result: %s", json);
    return (*env)->NewStringUTF(env, json);
}

/**
 * Extracts dialogue from a ROM file to a text file.
 * Called from RomSelectionActivity for each language ROM.
 *
 * @param romPath Path to the ROM file
 * @param outputDir Directory to write dialogue_{lang}.txt
 * @return RESTOOL_OK (0) on success, error code on failure
 */
JNIEXPORT jint JNICALL Java_com_dishii_zelda3_RomSelectionActivity_nativeExtractDialogue(
    JNIEnv* env, jobject obj, jstring jromPath, jstring joutputDir) {

    const char *rom_path = (*env)->GetStringUTFChars(env, jromPath, NULL);
    const char *output_dir = (*env)->GetStringUTFChars(env, joutputDir, NULL);

    if (!rom_path || !output_dir) {
        if (rom_path) (*env)->ReleaseStringUTFChars(env, jromPath, rom_path);
        if (output_dir) (*env)->ReleaseStringUTFChars(env, joutputDir, output_dir);
        LOGD("nativeExtractDialogue: Failed to get string parameters");
        return RESTOOL_ERR_ROM_LOAD;
    }

    LOGD("nativeExtractDialogue: rom=%s, output_dir=%s", rom_path, output_dir);

    int result = Restool_ExtractDialogue(rom_path, output_dir);

    (*env)->ReleaseStringUTFChars(env, jromPath, rom_path);
    (*env)->ReleaseStringUTFChars(env, joutputDir, output_dir);

    LOGD("nativeExtractDialogue: result=%d", result);
    return result;
}

/**
 * Compiles assets from US ROM with optional language dialogues.
 * Called from RomSelectionActivity after all dialogues are extracted.
 *
 * @param usRomPath Path to the US ROM file
 * @param outputPath Path to output zelda3_assets.dat
 * @param languages Comma-separated language codes (e.g., "de,fr") or null for US only
 * @param dialogueDir Directory containing dialogue_{lang}.txt files (or null)
 * @return RESTOOL_OK (0) on success, error code on failure
 */
JNIEXPORT jint JNICALL Java_com_dishii_zelda3_RomSelectionActivity_nativeCompileAssets(
    JNIEnv* env, jobject obj,
    jstring jusRomPath, jstring joutputPath, jstring jlanguages, jstring jdialogueDir) {

    const char *us_rom_path = (*env)->GetStringUTFChars(env, jusRomPath, NULL);
    const char *output_path = (*env)->GetStringUTFChars(env, joutputPath, NULL);
    const char *languages = jlanguages ? (*env)->GetStringUTFChars(env, jlanguages, NULL) : NULL;
    const char *dialogue_dir = jdialogueDir ? (*env)->GetStringUTFChars(env, jdialogueDir, NULL) : NULL;

    if (!us_rom_path || !output_path) {
        if (us_rom_path) (*env)->ReleaseStringUTFChars(env, jusRomPath, us_rom_path);
        if (output_path) (*env)->ReleaseStringUTFChars(env, joutputPath, output_path);
        if (languages) (*env)->ReleaseStringUTFChars(env, jlanguages, languages);
        if (dialogue_dir) (*env)->ReleaseStringUTFChars(env, jdialogueDir, dialogue_dir);
        LOGD("nativeCompileAssets: Failed to get required string parameters");
        return RESTOOL_ERR_ROM_LOAD;
    }

    LOGD("nativeCompileAssets: us_rom=%s, output=%s, langs=%s, dialogue_dir=%s",
         us_rom_path, output_path, languages ? languages : "us", dialogue_dir ? dialogue_dir : "(null)");

    // Enable INFO-level logging for asset compilation progress
    SetLogLevel(LOG_INFO);
    int result = Restool_CompileAssets(us_rom_path, output_path, languages, dialogue_dir);

    (*env)->ReleaseStringUTFChars(env, jusRomPath, us_rom_path);
    (*env)->ReleaseStringUTFChars(env, joutputPath, output_path);
    if (languages) (*env)->ReleaseStringUTFChars(env, jlanguages, languages);
    if (dialogue_dir) (*env)->ReleaseStringUTFChars(env, jdialogueDir, dialogue_dir);

    LOGD("nativeCompileAssets: result=%d", result);
    return result;
}

// ============================================================================
// Language Settings JNI (for Audio Options)
// ============================================================================

#include "launcher/dat_reader.h"

/**
 * Gets the available languages from the zelda3_assets.dat file.
 * Called from MainActivity to determine if Language option should be shown
 * in Audio Options dialog.
 *
 * @param path Directory containing zelda3_assets.dat
 * @return Array of language codes (e.g., ["us", "de", "fr"]) or null if error/single language
 */
JNIEXPORT jobjectArray JNICALL Java_com_dishii_zelda3_MainActivity_nativeGetAvailableLanguages(
    JNIEnv* env, jobject obj, jstring jpath) {

    const char *path = (*env)->GetStringUTFChars(env, jpath, NULL);
    if (!path) {
        LOGD("nativeGetAvailableLanguages: Failed to get path string");
        return NULL;
    }

    LOGD("nativeGetAvailableLanguages: Checking path: %s", path);

    char languages[16][16];
    int count = DatReader_GetLanguages(path, languages, 16);

    (*env)->ReleaseStringUTFChars(env, jpath, path);

    if (count <= 0) {
        LOGD("nativeGetAvailableLanguages: No languages found or error");
        return NULL;
    }

    LOGD("nativeGetAvailableLanguages: Found %d language(s)", count);

    // Create String array
    jclass stringClass = (*env)->FindClass(env, "java/lang/String");
    if (!stringClass) {
        LOGD("nativeGetAvailableLanguages: Failed to find String class");
        return NULL;
    }

    jobjectArray result = (*env)->NewObjectArray(env, count, stringClass, NULL);
    if (!result) {
        LOGD("nativeGetAvailableLanguages: Failed to create array");
        (*env)->DeleteLocalRef(env, stringClass);
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        jstring langStr = (*env)->NewStringUTF(env, languages[i]);
        if (langStr) {
            (*env)->SetObjectArrayElement(env, result, i, langStr);
            (*env)->DeleteLocalRef(env, langStr);
            LOGD("nativeGetAvailableLanguages: Added language[%d] = '%s'", i, languages[i]);
        }
    }

    (*env)->DeleteLocalRef(env, stringClass);

    return result;
}

// ============================================================================
// Save File JNI Functions (for external storage via SAF)
// ============================================================================

/**
 * Opens a save file for reading using Android SAF (Storage Access Framework).
 * Called from zelda_rtl.c when loading save files on Android.
 *
 * @param filename Relative filename like "sram.dat" or "save0.sav"
 * @return File descriptor (>= 0) on success, -1 on failure
 */
int Android_OpenSaveFileRead(const char *filename) {
    return JniHelper_CallStaticIntMethod_1S("openSaveFileRead", filename);
}

/**
 * Opens a save file for writing using Android SAF (Storage Access Framework).
 * Creates the file if it doesn't exist.
 * Called from zelda_rtl.c when saving on Android.
 *
 * @param filename Relative filename like "sram.dat" or "save0.sav"
 * @return File descriptor (>= 0) on success, -1 on failure
 */
int Android_OpenSaveFileWrite(const char *filename) {
    return JniHelper_CallStaticIntMethod_1S("openSaveFileWrite", filename);
}

/**
 * Renames a save file in external storage using Android SAF.
 * Used for creating backup files (sram.dat -> sram.bak).
 *
 * @param old_name Current filename
 * @param new_name New filename
 * @return 1 on success, 0 on failure
 */
int Android_RenameSaveFile(const char *old_name, const char *new_name) {
    return JniHelper_CallStaticBoolMethod_2S("renameSaveFile", old_name, new_name) ? 1 : 0;
}

/**
 * Checks if a save file exists in external storage.
 *
 * @param filename Relative filename to check
 * @return 1 if file exists, 0 if not
 */
int Android_SaveFileExists(const char *filename) {
    return JniHelper_CallStaticBoolMethod_1S("saveFileExists", filename) ? 1 : 0;
}

/**
 * Deletes a save file in external storage using Android SAF.
 * Used for removing old backup files before renaming.
 *
 * @param filename Filename to delete
 * @return 1 on success, 0 on failure
 */
int Android_DeleteSaveFile(const char *filename) {
    return JniHelper_CallStaticBoolMethod_1S("deleteSaveFile", filename) ? 1 : 0;
}

// ============================================================================
// Platform Save File API Implementation (Android)
// These implement the unified platform.h API using Android SAF via JNI
// ============================================================================

#include "platform.h"
#include <unistd.h>

struct PlatformSaveFile {
    int fd;       // File descriptor from Android SAF
    FILE *fp;     // FILE* wrapper via fdopen
};

const char *Platform_GetSaveDirectory(void) {
    // Android SAF handles paths internally - return empty string
    return "";
}

PlatformSaveFile *Platform_OpenSaveFile(const char *filename, bool for_writing) {
    int fd;
    if (for_writing) {
        fd = Android_OpenSaveFileWrite(filename);
    } else {
        fd = Android_OpenSaveFileRead(filename);
    }

    if (fd < 0)
        return NULL;

    FILE *fp = fdopen(fd, for_writing ? "wb" : "rb");
    if (!fp) {
        close(fd);
        return NULL;
    }

    PlatformSaveFile *sf = (PlatformSaveFile *)malloc(sizeof(PlatformSaveFile));
    if (!sf) {
        fclose(fp);  // fclose also closes the fd
        return NULL;
    }
    sf->fd = fd;
    sf->fp = fp;
    return sf;
}

size_t Platform_ReadSaveFile(void *ptr, size_t size, size_t count, PlatformSaveFile *file) {
    if (!file || !file->fp)
        return 0;
    return fread(ptr, size, count, file->fp);
}

size_t Platform_WriteSaveFile(const void *ptr, size_t size, size_t count, PlatformSaveFile *file) {
    if (!file || !file->fp)
        return 0;
    return fwrite(ptr, size, count, file->fp);
}

int Platform_CloseSaveFile(PlatformSaveFile *file) {
    if (!file)
        return -1;
    int result = 0;
    if (file->fp)
        result = fclose(file->fp);  // fclose also closes the fd
    free(file);
    return result;
}

FILE *Platform_GetSaveFileHandle(PlatformSaveFile *file) {
    if (!file)
        return NULL;
    return file->fp;
}

bool Platform_SaveFileExists(const char *filename) {
    return Android_SaveFileExists(filename) != 0;
}

bool Platform_DeleteSaveFile(const char *filename) {
    return Android_DeleteSaveFile(filename) != 0;
}

bool Platform_RenameSaveFile(const char *old_name, const char *new_name) {
    return Android_RenameSaveFile(old_name, new_name) != 0;
}
