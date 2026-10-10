package com.dishii.zelda3

import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.documentfile.provider.DocumentFile
import com.dishii.zelda3.overlay.InputOverlay
import com.dishii.zelda3.overlay.model.OverlayLayout
import com.dishii.zelda3.ui.menu.MenuItem
import com.dishii.zelda3.ui.menu.MenuTab
import com.dishii.zelda3.ui.menu.PauseMenuView
import com.dishii.zelda3.ui.menu.SaveSlot
import com.dishii.zelda3.util.ConfigManager
import com.dishii.zelda3.util.showToast
import com.dishii.zelda3.util.stylePositiveAsDestructive
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.libsdl.app.SDLActivity
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.util.concurrent.Executors

/**
 * Main SDL game activity.
 * Extends SDLActivity for native game integration and manages UI overlay/navigation.
 */
class MainActivity : SDLActivity() {
    private companion object {
        const val TAG = "Zelda3MainActivity"
        const val PREFS_NAME = "app_prefs"
        const val PREF_ZELDA3_FOLDER_PATH = "zelda3_folder_path"
        const val PREF_ZELDA3_FOLDER_URI = "zelda3_folder_uri"
        const val BUFFER_SIZE = 8192
        const val REQUEST_SHADER_FILE = 9001

        /**
         * Helper to get MainActivity instance from SDLActivity context.
         */
        private fun getActivityInstance(): MainActivity? = SDLActivity.getContext() as? MainActivity

        /**
         * Opens an external file using SAF and returns a file descriptor.
         * Called from native C code via JNI (from Platform_OpenFile on Android).
         *
         * Path format: "subdir/filename" (e.g., "MSU/track-1.pcm", "shaders/crt.glsl")
         *
         * @param path Path with subdirectory and filename
         * @param mode File mode - "r" for read, "w" for write
         * @return File descriptor (>= 0) on success, -1 on failure
         */
        @JvmStatic
        fun openExternalFile(
            path: String,
            mode: String,
        ): Int {
            Log.d(TAG, "openExternalFile: path='$path', mode='$mode'")
            // Split path into subdir and filename
            val slashIndex = path.indexOf('/')
            if (slashIndex < 0) {
                Log.e(TAG, "openExternalFile: Invalid path format (no slash): $path")
                return -1
            }
            val subdir = path.substring(0, slashIndex)
            val filename = path.substring(slashIndex + 1)
            // Convert C file mode to Android ContentResolver mode
            // C modes: "r", "rb", "w", "wb", "r+", "w+", etc.
            // Android modes: "r", "w", "rw", "rwt"
            val androidMode =
                when {
                    mode.contains('w') && mode.contains('r') -> "rw"
                    mode.contains('w') -> "w"
                    mode.contains('r') -> "r"
                    else -> "r"
                }
            val createIfMissing = mode.contains('w')
            Log.d(TAG, "openExternalFile: subdir='$subdir', filename='$filename', androidMode='$androidMode', create=$createIfMissing")
            return getActivityInstance()?.openFileInSubdir(subdir, filename, androidMode, createIfMissing) ?: -1
        }

        /**
         * Loads an asset file from the APK.
         * Called from native C code via JNI (e.g., for loading shader files).
         *
         * @param assetPath Relative path within assets, e.g. "shaders/vert.spv"
         * @return ByteArray containing asset data, or null on failure
         */
        @JvmStatic
        fun loadAsset(assetPath: String): ByteArray? {
            Log.d(TAG, "loadAsset called from JNI: path='$assetPath'")

            // Get singleton MainActivity instance (SDLActivity stores this)
            val activity = SDLActivity.getContext() as? MainActivity
            if (activity == null) {
                Log.e(TAG, "loadAsset: Failed to get MainActivity instance")
                return null
            }

            return try {
                val inputStream = activity.assets.open(assetPath)
                val data = inputStream.readBytes()
                inputStream.close()
                Log.d(TAG, "loadAsset: Successfully loaded $assetPath (${data.size} bytes)")
                data
            } catch (e: IOException) {
                Log.e(TAG, "loadAsset: IOException while loading $assetPath", e)
                null
            } catch (e: Exception) {
                Log.e(TAG, "loadAsset: Exception while loading $assetPath", e)
                null
            }
        }

        /**
         * Show a Toast notification from native C code.
         * Called from JNI on any thread, posts to main thread.
         *
         * @param message Message to display in Toast
         */
        @JvmStatic
        fun showToast(message: String) {
            Log.d(TAG, "showToast called from JNI: message='$message'")

            // Get singleton MainActivity instance
            val activity = SDLActivity.getContext() as? MainActivity
            if (activity == null) {
                Log.e(TAG, "showToast: Failed to get MainActivity instance")
                return
            }

            // Post to main thread (JNI calls happen on native threads)
            activity.runOnUiThread {
                Toast.makeText(activity, message, Toast.LENGTH_LONG).show()
                Log.d(TAG, "showToast: Displayed toast")
            }
        }

        /**
         * Update renderer setting in zelda3.ini from native C code.
         * Called from JNI when falling back from Vulkan to OpenGL ES.
         *
         * @param renderer Renderer name ("SDL", "OpenGL ES", "Vulkan", etc.)
         */
        @OptIn(DelicateCoroutinesApi::class)
        @JvmStatic
        fun updateRendererSetting(renderer: String) {
            Log.d(TAG, "updateRendererSetting called from JNI: renderer='$renderer'")

            // Get singleton MainActivity instance
            val activity = SDLActivity.getContext() as? MainActivity
            if (activity == null) {
                Log.e(TAG, "updateRendererSetting: Failed to get MainActivity instance")
                return
            }

            // Launch coroutine to update config (fire-and-forget)
            GlobalScope.launch {
                activity.updateRendererSetting(renderer)
            }
        }

        /**
         * Opens a save file for reading using SAF.
         * Called from native C code via JNI.
         *
         * @param filename Filename like "sram.dat" or "save0.sav"
         * @return File descriptor (>= 0) on success, -1 on failure
         */
        @JvmStatic
        fun openSaveFileRead(filename: String): Int {
            Log.d(TAG, "openSaveFileRead: filename='$filename'")
            return getActivityInstance()?.openFileInSubdir("saves", filename, "r") ?: -1
        }

        /**
         * Opens a save file for writing using SAF. Creates file if it doesn't exist.
         * Called from native C code via JNI.
         *
         * @param filename Filename like "sram.dat" or "save0.sav"
         * @return File descriptor (>= 0) on success, -1 on failure
         */
        @JvmStatic
        fun openSaveFileWrite(filename: String): Int {
            Log.d(TAG, "openSaveFileWrite: filename='$filename'")
            return getActivityInstance()?.openFileInSubdir("saves", filename, "wt", createIfMissing = true) ?: -1
        }

        /**
         * Renames a save file in external storage.
         * Used for creating backup files (sram.dat -> sram.bak).
         * Called from native C code via JNI.
         *
         * @param oldName Current filename
         * @param newName New filename
         * @return true on success, false on failure
         */
        @JvmStatic
        fun renameSaveFile(
            oldName: String,
            newName: String,
        ): Boolean {
            Log.d(TAG, "renameSaveFile called from JNI: '$oldName' -> '$newName'")

            val activity = SDLActivity.getContext() as? MainActivity
            if (activity == null) {
                Log.e(TAG, "renameSaveFile: Failed to get MainActivity instance")
                return false
            }

            val uriString = activity.zelda3FolderUri
            if (uriString == null) {
                Log.e(TAG, "renameSaveFile: No SAF Uri stored")
                return false
            }

            return try {
                val treeUri = Uri.parse(uriString)
                val rootDir = DocumentFile.fromTreeUri(activity, treeUri)
                if (rootDir == null) {
                    Log.e(TAG, "renameSaveFile: Failed to get DocumentFile from tree Uri")
                    return false
                }

                val savesDir = rootDir.findFile("saves")
                if (savesDir == null || !savesDir.isDirectory) {
                    Log.e(TAG, "renameSaveFile: saves directory not found")
                    return false
                }

                val oldFile = savesDir.findFile(oldName)
                if (oldFile == null || !oldFile.isFile) {
                    Log.d(TAG, "renameSaveFile: Source file not found: $oldName")
                    return false
                }

                // Delete existing file with new name if it exists
                val existingNewFile = savesDir.findFile(newName)
                existingNewFile?.delete()

                // Rename the file
                val success = oldFile.renameTo(newName)
                Log.d(TAG, "renameSaveFile: Result: $success")
                success
            } catch (e: Exception) {
                Log.e(TAG, "renameSaveFile: Exception", e)
                false
            }
        }

        /**
         * Checks if a save file exists in external storage.
         * Called from native C code via JNI.
         *
         * @param filename Filename to check
         * @return true if file exists, false otherwise
         */
        @JvmStatic
        fun saveFileExists(filename: String): Boolean {
            Log.d(TAG, "saveFileExists called from JNI: filename='$filename'")

            val activity = SDLActivity.getContext() as? MainActivity
            if (activity == null) {
                Log.e(TAG, "saveFileExists: Failed to get MainActivity instance")
                return false
            }

            val uriString = activity.zelda3FolderUri
            if (uriString == null) {
                Log.e(TAG, "saveFileExists: No SAF Uri stored")
                return false
            }

            return try {
                val treeUri = Uri.parse(uriString)
                val rootDir = DocumentFile.fromTreeUri(activity, treeUri)
                if (rootDir == null) {
                    Log.e(TAG, "saveFileExists: Failed to get DocumentFile from tree Uri")
                    return false
                }

                val savesDir = rootDir.findFile("saves")
                if (savesDir == null || !savesDir.isDirectory) {
                    Log.d(TAG, "saveFileExists: saves directory not found")
                    return false
                }

                val file = savesDir.findFile(filename)
                val exists = file != null && file.isFile
                Log.d(TAG, "saveFileExists: '$filename' exists=$exists")
                exists
            } catch (e: Exception) {
                Log.e(TAG, "saveFileExists: Exception", e)
                false
            }
        }

        /**
         * Deletes a save file in external storage.
         * Called from native C code via JNI.
         *
         * @param filename Filename to delete
         * @return true on success, false on failure
         */
        @JvmStatic
        fun deleteSaveFile(filename: String): Boolean {
            Log.d(TAG, "deleteSaveFile called from JNI: filename='$filename'")

            val activity = SDLActivity.getContext() as? MainActivity
            if (activity == null) {
                Log.e(TAG, "deleteSaveFile: Failed to get MainActivity instance")
                return false
            }

            val uriString = activity.zelda3FolderUri
            if (uriString == null) {
                Log.e(TAG, "deleteSaveFile: No SAF Uri stored")
                return false
            }

            return try {
                val treeUri = Uri.parse(uriString)
                val rootDir = DocumentFile.fromTreeUri(activity, treeUri)
                if (rootDir == null) {
                    Log.e(TAG, "deleteSaveFile: Failed to get DocumentFile from tree Uri")
                    return false
                }

                val savesDir = rootDir.findFile("saves")
                if (savesDir == null || !savesDir.isDirectory) {
                    Log.e(TAG, "deleteSaveFile: saves directory not found")
                    return false
                }

                val file = savesDir.findFile(filename)
                if (file == null || !file.isFile) {
                    Log.d(TAG, "deleteSaveFile: File not found: $filename")
                    return false
                }

                val success = file.delete()
                Log.d(TAG, "deleteSaveFile: Result: $success")
                success
            } catch (e: Exception) {
                Log.e(TAG, "deleteSaveFile: Exception", e)
                false
            }
        }
    }

    /**
     * Opens a file in a subdirectory of the Zelda3 folder using SAF.
     * Shared helper for MSU and save file access.
     *
     * @param subdir Subdirectory name ("MSU" or "saves")
     * @param filename File to open
     * @param mode "r" for read, "wt" for write+truncate
     * @param createIfMissing If true, create file/directory if they don't exist
     * @return File descriptor (>= 0) on success, -1 on failure
     */
    private fun openFileInSubdir(
        subdir: String,
        filename: String,
        mode: String,
        createIfMissing: Boolean = false,
    ): Int {
        val uriString =
            zelda3FolderUri ?: run {
                Log.e(TAG, "openFileInSubdir: No SAF Uri stored")
                return -1
            }

        return try {
            val treeUri = Uri.parse(uriString)
            val rootDir =
                DocumentFile.fromTreeUri(this, treeUri) ?: run {
                    Log.e(TAG, "openFileInSubdir: Failed to get DocumentFile from tree Uri")
                    return -1
                }

            // Case-insensitive file finder (Android filesystems may be case-sensitive)
            fun DocumentFile.findFileCaseInsensitive(name: String): DocumentFile? {
                // Try exact match first
                findFile(name)?.let { return it }
                // Fall back to case-insensitive search
                val lowerName = name.lowercase()
                return listFiles().firstOrNull { it.name?.lowercase() == lowerName }
            }

            // Start from the base subdirectory
            var currentDir = rootDir.findFileCaseInsensitive(subdir)
            if (currentDir == null) {
                if (createIfMissing) {
                    currentDir = rootDir.createDirectory(subdir) ?: run {
                        Log.e(TAG, "openFileInSubdir: Failed to create $subdir directory")
                        return -1
                    }
                } else {
                    Log.e(TAG, "openFileInSubdir: $subdir directory not found")
                    return -1
                }
            }

            // Handle nested paths: split "subdir1/subdir2/file.ext" by "/"
            val pathParts = filename.split("/")

            // Navigate through intermediate directories
            var targetDir: DocumentFile = currentDir
            for (i in 0 until pathParts.size - 1) {
                val dirName = pathParts[i]
                var nextDir = targetDir.findFileCaseInsensitive(dirName)
                if (nextDir == null) {
                    if (createIfMissing) {
                        nextDir = targetDir.createDirectory(dirName) ?: run {
                            Log.e(TAG, "openFileInSubdir: Failed to create $dirName directory")
                            return -1
                        }
                    } else {
                        Log.d(TAG, "openFileInSubdir: Directory not found: $dirName in path $subdir/$filename")
                        return -1
                    }
                }
                targetDir = nextDir
            }

            // Find or create the final file
            val finalFileName = pathParts.last()
            var file = targetDir.findFileCaseInsensitive(finalFileName)
            if (file == null) {
                if (createIfMissing) {
                    file = targetDir.createFile("application/octet-stream", finalFileName) ?: run {
                        Log.e(TAG, "openFileInSubdir: Failed to create $finalFileName")
                        return -1
                    }
                } else {
                    Log.d(TAG, "openFileInSubdir: File not found: $subdir/$filename")
                    return -1
                }
            }

            val pfd =
                contentResolver.openFileDescriptor(file.uri, mode) ?: run {
                    Log.e(TAG, "openFileInSubdir: Failed to open file descriptor")
                    return -1
                }

            val fd = pfd.detachFd()
            Log.d(TAG, "openFileInSubdir: $subdir/$filename -> fd=$fd")
            fd
        } catch (e: Exception) {
            Log.e(TAG, "openFileInSubdir: Exception for $subdir/$filename", e)
            -1
        }
    }

    // JNI function for hot-reloading audio config (pass settings directly, don't rely on file)
    private external fun nativeReloadAudioConfig(
        enableMsu: Int,
        msuVolume: Int,
        disableLowHealthBeep: Int,
    )

    // JNI functions for save/load state
    private external fun nativeSaveState(slot: Int)

    private external fun nativeLoadState(slot: Int)

    private external fun nativeGetScreenshotRGBA(): ByteArray?

    // JNI functions for pause control
    private external fun nativeTogglePause()

    private external fun nativeIsPaused(): Boolean

    // JNI functions for controller binding
    private external fun nativeBindGamepadButton(
        buttonName: String,
        modifierNames: Array<String>?,
        commandId: Int,
    ): Boolean

    private external fun nativeUnbindGamepadButton(
        buttonName: String,
        modifierNames: Array<String>?,
    ): Boolean

    private external fun nativeClearGamepadBindings()

    private external fun nativeGetGamepadBindings(): String

    private external fun nativeApplyDefaultGamepadBindings()

    private external fun nativeGetButtonForCommand(cmdId: Int): String?

    // JNI function for language settings
    private external fun nativeGetAvailableLanguages(path: String): Array<String>?

    // Language display name mapping (keep in sync with kLanguageNames in launcher-tauri/frontend/js/main.js)
    private val languageDisplayNames =
        mapOf(
            "us" to "English (US)",
            "en" to "English (EU)",
            "de" to "German",
            "fr" to "French",
            "fr-c" to "French (Canada)",
            "es" to "Spanish",
            "nl" to "Dutch",
            "pl" to "Polish",
            "pt" to "Portuguese (Brazil)",
            "pt-cbt" to "Portuguese (Brazil, CBT)",
            "it" to "Italian",
            "gl" to "Galician",
            "ca" to "Catalan",
            "ga" to "Irish",
            "sv" to "Swedish",
            "en-redux" to "English (Redux)",
            "en-kal" to "English (Kaleidoscope)",
            "en-gn" to "English (Gender Neutral)",
        )

    private fun getLanguageDisplayName(code: String): String = languageDisplayNames[code] ?: code.uppercase()

    // Older asset files and configs use the pre-rename codes; the game accepts both
    // (ZeldaSetLanguage), so normalize to the current names for display and saving
    private val renamedLanguageCodes = mapOf("redux" to "en-redux", "retrans-kal" to "en-kal")

    private fun normalizeLanguageCode(code: String): String = renamedLanguageCodes[code] ?: code

    // Data class for MSU settings
    private data class MsuSettings(
        val enableMsu: Boolean,
        val volume: Int,
        val resumeMsu: Boolean,
        val audioFreq: Int,
    )

    // Data class for controller binding
    private data class ControllerBinding(
        // C command ID (e.g., kKeys_Controls + 0 for Up)
        val commandId: Int,
        val commandDisplayName: String,
        // null if not bound, otherwise "DpadUp" or "Start + Select"
        val currentBinding: String?,
        // "GAME CONTROLS", "SAVE STATE", etc.
        val category: String,
    )

    private lateinit var overlayView: View
    private lateinit var inputOverlay: InputOverlay
    private lateinit var pauseMenu: PauseMenuView
    private lateinit var doneControlConfigButton: Button
    private var currentDialog: AlertDialog? = null

    // Pause menu state
    private var wasPausedBeforeMenu = false
    private var resumeAfterEdit = false
    private var availableLanguages: List<String> = emptyList()
    private var startupAudioFreq: Int? = null
    private val configExecutor = Executors.newSingleThreadExecutor()
    private val configDispatcher = configExecutor.asCoroutineDispatcher()

    // Start+Select opens the pause menu: gamepad buttons, or N+B from keyboard and touch overlay
    private val menuComboPartners =
        mapOf(
            KeyEvent.KEYCODE_BUTTON_START to KeyEvent.KEYCODE_BUTTON_SELECT,
            KeyEvent.KEYCODE_BUTTON_SELECT to KeyEvent.KEYCODE_BUTTON_START,
            KeyEvent.KEYCODE_N to KeyEvent.KEYCODE_B,
            KeyEvent.KEYCODE_B to KeyEvent.KEYCODE_N,
        )
    private val heldComboKeys = mutableSetOf<Int>()

    // Bind dialog state management
    private var isBindDialogOpen = false
    private val detectedButtons = mutableSetOf<Int>()
    private var capturedButtons = mutableSetOf<Int>() // Captured when first button is released
    private var bindDialogChipGroup: com.google.android.material.chip.ChipGroup? = null
    private var bindDialogTimer: TextView? = null
    private var currentBindingCommandId: Int? = null // C command ID
    private var currentBindingCommandDisplay: String? = null

    private val prefs: SharedPreferences by lazy {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
    }

    private var zelda3FolderPath: String?
        get() = prefs.getString(PREF_ZELDA3_FOLDER_PATH, null)
        set(value) = prefs.edit().putString(PREF_ZELDA3_FOLDER_PATH, value).apply()

    private var zelda3FolderUri: String?
        get() = prefs.getString(PREF_ZELDA3_FOLDER_URI, null)
        set(value) = prefs.edit().putString(PREF_ZELDA3_FOLDER_URI, value).apply()

    // Shader file picker callback
    private var pendingShaderCallback: ((android.net.Uri?) -> Unit)? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Force landscape (both orientations) - manifest alone doesn't work when orientation is in configChanges
        requestedOrientation = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE

        // Setup initial files and directories (compatibility with older flow)
        if (isExternalStorageWritable()) {
            getExternalFilesDir(null)?.let { externalDir ->
                setupInitialFiles(externalDir)
            }
        }

        // Check for save file migration from internal to external storage
        migrateSavesToExternalStorage()

        inflateOverlay()
        setupPauseMenu()
    }

    @Suppress("DEPRECATION")
    override fun onActivityResult(
        requestCode: Int,
        resultCode: Int,
        data: Intent?,
    ) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_SHADER_FILE) {
            if (resultCode == RESULT_OK) {
                pendingShaderCallback?.invoke(data?.data)
            } else {
                pendingShaderCallback?.invoke(null)
            }
            pendingShaderCallback = null
        }
    }

    /**
     * Sets up initial files and directories.
     * This duplicates some LauncherActivity logic for backward compatibility.
     */
    private fun setupInitialFiles(externalDir: File) {
        val configFile = File(externalDir, "zelda3.ini")
        val datNotice = File(externalDir, "PLACE zelda3_assets.dat HERE")
        val savesFolder = File(externalDir, "saves")
        val savesRefFolder = File(savesFolder, "ref")

        savesFolder.mkdirs()
        savesRefFolder.mkdirs()

        try {
            AssetCopyUtil.copyAssetsToExternal(
                this,
                "saves/ref",
                savesRefFolder.absolutePath,
            )

            datNotice.createNewFile()

            if (configFile.createNewFile()) {
                assets.open("zelda3.ini").use { input ->
                    configFile.outputStream().use { output ->
                        input.copyTo(output, BUFFER_SIZE)
                    }
                }
            }
        } catch (e: IOException) {
            Log.e(TAG, "Failed to create config files", e)
        }
    }

    // Enum for conflict resolution dialog
    private enum class ConflictResolution { ASK, OVERWRITE_ALL, SKIP_ALL }

    /**
     * Migrates save files from internal storage (getExternalFilesDir) to external storage (SAF).
     * Shows conflict resolution dialog if files exist in both locations.
     */
    private fun migrateSavesToExternalStorage() {
        val internalDir = getExternalFilesDir(null) ?: return
        val internalSavesDir = File(internalDir, "saves")

        // Check if internal saves exist
        if (!internalSavesDir.exists() || !internalSavesDir.isDirectory) {
            Log.d(TAG, "migrateSavesToExternalStorage: No internal saves directory")
            return
        }

        // Check if already migrated (backup exists)
        val backupDir = File(internalDir, "saves.backup")
        if (backupDir.exists()) {
            Log.d(TAG, "migrateSavesToExternalStorage: Already migrated (saves.backup exists)")
            return
        }

        // Check if SAF folder is configured
        val uriString = zelda3FolderUri
        if (uriString == null) {
            Log.d(TAG, "migrateSavesToExternalStorage: No SAF Uri configured")
            return
        }

        val treeUri = Uri.parse(uriString)
        val rootDir = DocumentFile.fromTreeUri(this, treeUri)
        if (rootDir == null) {
            Log.e(TAG, "migrateSavesToExternalStorage: Failed to get DocumentFile from tree Uri")
            return
        }

        // Get or create external saves directory
        var externalSavesDir = rootDir.findFile("saves")
        if (externalSavesDir == null) {
            externalSavesDir = rootDir.createDirectory("saves")
            if (externalSavesDir == null) {
                Log.e(TAG, "migrateSavesToExternalStorage: Failed to create external saves directory")
                return
            }
        }

        // Identify all files to migrate (everything in saves/ except ref/ subdirectory)
        val existingInternalFiles =
            internalSavesDir
                .listFiles()
                ?.filter { it.isFile } // Only files, not directories (excludes ref/)
                ?.map { it.name }
                ?: emptyList()

        if (existingInternalFiles.isEmpty()) {
            Log.d(TAG, "migrateSavesToExternalStorage: No save files to migrate")
            return
        }

        // Check for conflicts
        val conflicts =
            existingInternalFiles.filter { filename ->
                externalSavesDir.findFile(filename) != null
            }

        Log.d(TAG, "migrateSavesToExternalStorage: Found ${existingInternalFiles.size} files, ${conflicts.size} conflicts")

        if (conflicts.isNotEmpty()) {
            // Show conflict resolution dialog
            showMigrationConflictDialog(existingInternalFiles, conflicts, internalSavesDir, externalSavesDir)
        } else {
            // No conflicts - migrate directly
            performMigration(existingInternalFiles, emptySet(), internalSavesDir, externalSavesDir)
        }
    }

    /**
     * Shows conflict resolution dialog for each conflicting file.
     */
    private fun showMigrationConflictDialog(
        filesToMigrate: List<String>,
        conflicts: List<String>,
        internalSavesDir: File,
        externalSavesDir: DocumentFile,
    ) {
        var resolution = ConflictResolution.ASK
        val skipFiles = mutableSetOf<String>()

        fun processNextConflict(index: Int) {
            if (index >= conflicts.size || resolution != ConflictResolution.ASK) {
                // Done with conflicts, perform migration
                val filesToSkip =
                    when (resolution) {
                        ConflictResolution.SKIP_ALL -> conflicts.toSet()
                        ConflictResolution.OVERWRITE_ALL -> emptySet()
                        ConflictResolution.ASK -> skipFiles
                    }
                performMigration(filesToMigrate, filesToSkip, internalSavesDir, externalSavesDir)
                return
            }

            val filename = conflicts[index]
            val remainingCount = conflicts.size - index

            // Use custom layout for 4-button dialog
            val dialogView = layoutInflater.inflate(android.R.layout.simple_list_item_1, null)

            AlertDialog
                .Builder(this)
                .setTitle("Save File Conflict ($remainingCount remaining)")
                .setMessage("'$filename' already exists in external storage.\n\nOverwrite with internal version?")
                .setPositiveButton("Overwrite") { _, _ ->
                    // Don't add to skip, will be overwritten
                    processNextConflict(index + 1)
                }.setNegativeButton("Skip") { _, _ ->
                    skipFiles.add(filename)
                    processNextConflict(index + 1)
                }.setNeutralButton("Overwrite All") { _, _ ->
                    resolution = ConflictResolution.OVERWRITE_ALL
                    processNextConflict(index + 1)
                }.setCancelable(false)
                .create()
                .apply {
                    setOnShowListener {
                        // Add "Skip All" button manually (AlertDialog only supports 3 buttons natively)
                        // We'll use a custom approach: add it as a 4th option in the message
                    }
                    // For simplicity, we'll use a different approach: show Skip All in neutral position alternately
                    // Instead, let's add Skip All via a list dialog approach
                }.show()
        }

        // If there are multiple conflicts, offer bulk options first
        if (conflicts.size > 1) {
            val options =
                arrayOf(
                    "Review each file individually",
                    "Overwrite all ${conflicts.size} files",
                    "Skip all ${conflicts.size} files",
                )

            AlertDialog
                .Builder(this)
                .setTitle("Multiple Save File Conflicts")
                .setMessage("${conflicts.size} save files already exist in external storage.")
                .setItems(options) { _, which ->
                    when (which) {
                        0 -> {
                            processNextConflict(0)
                        }

                        // Review individually
                        1 -> {
                            resolution = ConflictResolution.OVERWRITE_ALL
                            performMigration(filesToMigrate, emptySet(), internalSavesDir, externalSavesDir)
                        }

                        2 -> {
                            resolution = ConflictResolution.SKIP_ALL
                            performMigration(filesToMigrate, conflicts.toSet(), internalSavesDir, externalSavesDir)
                        }
                    }
                }.setCancelable(false)
                .show()
        } else {
            // Single conflict - show individual dialog
            processNextConflict(0)
        }
    }

    /**
     * Performs the actual migration of save files.
     */
    @OptIn(DelicateCoroutinesApi::class)
    private fun performMigration(
        filesToMigrate: List<String>,
        filesToSkip: Set<String>,
        internalSavesDir: File,
        externalSavesDir: DocumentFile,
    ) {
        GlobalScope.launch {
            var migratedCount = 0
            var skippedCount = 0
            var errorCount = 0

            withContext(Dispatchers.IO) {
                for (filename in filesToMigrate) {
                    if (filename in filesToSkip) {
                        skippedCount++
                        continue
                    }

                    val srcFile = File(internalSavesDir, filename)
                    if (!srcFile.exists()) continue

                    try {
                        // Delete existing file in destination if overwriting
                        externalSavesDir.findFile(filename)?.delete()

                        // Create destination file
                        val destFile = externalSavesDir.createFile("application/octet-stream", filename)
                        if (destFile == null) {
                            Log.e(TAG, "performMigration: Failed to create $filename")
                            errorCount++
                            continue
                        }

                        // Copy contents
                        contentResolver.openOutputStream(destFile.uri)?.use { output ->
                            srcFile.inputStream().use { input ->
                                input.copyTo(output, BUFFER_SIZE)
                            }
                        }

                        migratedCount++
                        Log.d(TAG, "performMigration: Migrated $filename")
                    } catch (e: Exception) {
                        Log.e(TAG, "performMigration: Error migrating $filename", e)
                        errorCount++
                    }
                }

                // Rename internal saves/ to saves.backup/
                if (migratedCount > 0 || skippedCount > 0) {
                    val backupDir = File(internalSavesDir.parentFile, "saves.backup")
                    if (internalSavesDir.renameTo(backupDir)) {
                        Log.d(TAG, "performMigration: Renamed internal saves to saves.backup")
                    } else {
                        Log.e(TAG, "performMigration: Failed to rename internal saves to backup")
                    }
                }
            }

            // Show result Toast on UI thread
            val message =
                when {
                    errorCount > 0 -> "Migrated $migratedCount saves, $errorCount errors"
                    skippedCount > 0 -> "Migrated $migratedCount saves, skipped $skippedCount"
                    migratedCount > 0 -> "Migrated $migratedCount save files to external storage"
                    else -> return@launch // Nothing to report
                }

            withContext(Dispatchers.Main) {
                Toast.makeText(this@MainActivity, message, Toast.LENGTH_LONG).show()
            }
            Log.i(TAG, "performMigration: $message")
        }
    }

    /**
     * Inflates the overlay layout (InputOverlay with the pause menu above it).
     * CRITICAL: Must preserve exact layout structure for SDL compatibility.
     */
    fun inflateOverlay() {
        val inflater: LayoutInflater = layoutInflater
        overlayView = inflater.inflate(R.layout.layout, null)

        // Add overlay to SDLActivity's mLayout (ensures proper z-ordering above SDL surface)
        mLayout?.let { layout ->
            val params =
                android.widget.RelativeLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT,
                )
            layout.addView(overlayView, params)
            overlayView.bringToFront()
            layout.requestLayout()
            layout.invalidate()
        } ?: run {
            Log.e(TAG, "mLayout is null, falling back to content root")
            val rootView = window.decorView.findViewById<ViewGroup>(android.R.id.content)
            rootView.addView(overlayView)
        }

        pauseMenu = overlayView.findViewById(R.id.pause_menu)

        val inputContainer = overlayView.findViewById<ViewGroup>(R.id.input_container)

        // Create and add InputOverlay
        inputOverlay = InputOverlay(this)
        val overlayParams =
            android.widget.FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
            )
        inputContainer.addView(inputOverlay, 0, overlayParams)
        Log.d(TAG, "InputOverlay added to FrameLayout")

        // Setup done button for edit mode
        doneControlConfigButton = overlayView.findViewById(R.id.done_control_config)
        doneControlConfigButton.setOnClickListener { exitEditMode() }
        Log.d(TAG, "Done button initialized with onClick listener")
    }

    // ========== Pause Menu ==========

    /**
     * Wires the pause menu: opening pauses the game, closing resumes it (unless it was already
     * paused) and offers a restart if a restart-only setting changed.
     */
    private fun setupPauseMenu() {
        pauseMenu.onOpened = {
            wasPausedBeforeMenu = nativeIsPaused()
            if (!wasPausedBeforeMenu) nativeTogglePause()
            inputOverlay.setMenuOpen(true)
        }
        pauseMenu.onClosed = { restartNeeded ->
            inputOverlay.setMenuOpen(false)
            if (!wasPausedBeforeMenu && nativeIsPaused()) nativeTogglePause()
            mSurface?.requestFocus()
            if (restartNeeded) showRestartDialog()
        }
        setMenuTabs()

        // The language row only appears when more than one dialogue language is available
        Thread {
            val externalDir = getExternalFilesDir(null)?.absolutePath ?: ""
            val languages = nativeGetAvailableLanguages(externalDir) ?: return@Thread
            Log.d(TAG, "setupPauseMenu: availableLanguages = ${languages.joinToString()}")
            runOnUiThread { availableLanguages = languages.map { normalizeLanguageCode(it) }.distinct() }
        }.start()
    }

    private fun setMenuTabs() {
        val hasTouchscreen = packageManager.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)
        pauseMenu.setTabs(
            listOfNotNull(
                MenuTab("Game") { gamePage() },
                MenuTab("Gameplay") { gameplayPage() },
                MenuTab("Graphics") { graphicsPage() },
                MenuTab("Audio") { audioPage() },
                MenuTab("Controls") { controlsPage() },
                if (hasTouchscreen) MenuTab("Touch") { overlayPage() } else null,
                MenuTab("System") { systemPage() },
            ),
        )
    }

    /** Writes zelda3.ini off the main thread, strictly in the order changes were made. */
    private fun persist(block: suspend () -> Unit) {
        configExecutor.execute { kotlinx.coroutines.runBlocking { block() } }
    }

    /** Reads zelda3.ini after every pending [persist] has landed. */
    private suspend fun <T> readConfig(block: suspend () -> T): T = withContext(configDispatcher) { kotlinx.coroutines.runBlocking { block() } }

    /** A toggle backed by a boolean zelda3.ini key. Most of these only apply after a restart. */
    private suspend fun configToggle(
        label: String,
        section: String,
        key: String,
        default: Boolean,
        restart: Boolean = true,
    ): MenuItem.Toggle {
        var value = readConfig { ConfigManager.readBool(this, section, key, default) }
        return MenuItem.Toggle(
            label,
            restart = restart,
            get = { value },
            set = {
                value = it
                persist { ConfigManager.writeBool(this@MainActivity, section, key, it) }
            },
        )
    }

    private fun gamePage(): List<MenuItem> =
        listOf(
            MenuItem.Action("Resume") { pauseMenu.close() },
            MenuItem.Action("Save state", opensPage = true) { pauseMenu.push("Save state") { slotsPage(save = true) } },
            MenuItem.Action("Load state", opensPage = true) { pauseMenu.push("Load state") { slotsPage(save = false) } },
            MenuItem.Action("Exit game", destructive = true) {
                finish()
                android.os.Process.killProcess(android.os.Process.myPid())
            },
        )

    private suspend fun slotsPage(save: Boolean): List<MenuItem> {
        val slots = withContext(Dispatchers.IO) { loadSaveSlots() }
        return listOf(
            MenuItem.Info({ if (save) "Choose a slot to save to" else "Choose a slot to load" }),
            MenuItem.Slots(slots) { slot ->
                if (save) {
                    nativeSaveState(slot.slot)
                    captureSaveThumbnail(slot.slot)
                    showToast("Saved to ${slot.name}")
                    pauseMenu.close()
                } else if (slot.exists) {
                    nativeLoadState(slot.slot)
                    pauseMenu.close()
                } else {
                    showToast("Save slot is empty")
                }
            },
        )
    }

    private suspend fun gameplayPage(): List<MenuItem> {
        val swordLevels = listOf("Disabled", "Level 1 (Wooden)", "Level 2 (Master)", "Level 3 (Tempered)", "Level 4 (Golden)")
        var breakPots = readConfig { ConfigManager.readInt(this, "Features", "BreakPotsWithSword", 0).coerceIn(0, 4) }
        return listOf(
            MenuItem.Header("Save"),
            configToggle("Autosave on quit", "General", "Autosave", false),
            MenuItem.Header("Controls"),
            configToggle("Item switching with L/R", "Features", "ItemSwitchLR", false),
            configToggle("Limit L/R to first 4 items", "Features", "ItemSwitchLRLimit", false),
            configToggle("Turn while dashing", "Features", "TurnWhileDashing", false),
            MenuItem.Header("Gameplay"),
            configToggle("Skip intro on keypress", "Features", "SkipIntroOnKeypress", false),
            configToggle("Mirror warps to Dark World", "Features", "MirrorToDarkworld", false),
            configToggle("Collect items with sword", "Features", "CollectItemsWithSword", false),
            MenuItem.Choice(
                "Break pots with sword",
                swordLevels,
                restart = true,
                get = { breakPots },
                set = {
                    breakPots = it
                    persist { ConfigManager.writeInt(this@MainActivity, "Features", "BreakPotsWithSword", it) }
                },
            ),
            configToggle("More active bombs (4)", "Features", "MoreActiveBombs", false),
            configToggle("Carry more rupees (9999)", "Features", "CarryMoreRupees", false),
            configToggle("Cancel bird travel with X", "Features", "CancelBirdTravel", false),
            MenuItem.Header("Interface"),
            configToggle("Show max items in yellow", "Features", "ShowMaxItemsInYellow", false),
            MenuItem.Header("Bug fixes"),
            configToggle("Miscellaneous bug fixes", "Features", "MiscBugFixes", false),
            configToggle("Gameplay bug fixes", "Features", "GameChangingBugFixes", false),
            MenuItem.Header("Experimental"),
            configToggle("Pokemode", "Features", "Pokemode", false),
            configToggle("Princess Zelda helps", "Features", "PrincessZeldaHelps", false),
        )
    }

    private suspend fun graphicsPage(): List<MenuItem> {
        val renderers = listOf("SDL", "OpenGL ES", "Vulkan")
        val aspectRatios = listOf("8:7", "4:3", "3:2", "16:9", "16:10", "18:9")

        // Older configs say "OpenGL"
        val storedRenderer = readConfig { readRendererSetting() }.let { if (it == "OpenGL") "OpenGL ES" else it }
        var renderer = renderers.indexOf(storedRenderer).coerceAtLeast(0)
        val aspect = readConfig { readExtendedAspectRatio() }
        var aspectIndex = aspectRatios.indexOf(aspect.aspectRatio).let { if (it < 0) aspectRatios.indexOf("16:9") else it }
        var extendY = aspect.extendY
        var shader = readConfig { ConfigManager.readString(this, "Graphics", "Shader", "") }

        val isVulkan = { renderer == renderers.indexOf("Vulkan") }
        val saveAspect = { persist { writeExtendedAspectRatio(aspectRatios[aspectIndex], extendY) } }
        val saveShader = { path: String ->
            shader = path
            persist { ConfigManager.writeString(this@MainActivity, "Graphics", "Shader", path) }
            pauseMenu.markRestartNeeded()
        }

        return listOf(
            MenuItem.Header("Renderer"),
            MenuItem.Choice(
                "Renderer",
                renderers,
                restart = true,
                get = { renderer },
                set = {
                    renderer = it
                    persist { updateRendererSetting(renderers[it]) }
                },
            ),
            MenuItem.Action(
                "Shader",
                value = { shader.substringAfterLast("/").ifEmpty { "None" } },
                visible = isVulkan,
            ) {
                pickShader { path ->
                    saveShader(path)
                    pauseMenu.refresh()
                }
            },
            MenuItem.Action("Clear shader", visible = { isVulkan() && shader.isNotEmpty() }) { saveShader("") },
            MenuItem.Header("Display"),
            MenuItem.Choice(
                "Aspect ratio",
                aspectRatios,
                restart = true,
                get = { aspectIndex },
                set = {
                    aspectIndex = it
                    saveAspect()
                },
            ),
            MenuItem.Toggle(
                "Extended height (240 lines)",
                restart = true,
                get = { extendY },
                set = {
                    extendY = it
                    saveAspect()
                },
            ),
            configToggle("Disable frame delay (60hz only)", "General", "DisableFrameDelay", false),
            configToggle("Optimized SNES PPU renderer", "Graphics", "NewRenderer", true),
            configToggle("High resolution world map", "Graphics", "EnhancedMode7", true),
            configToggle("Dim flashing effects", "Graphics", "DimFlashes", false),
        )
    }

    private fun pickShader(onPicked: (String) -> Unit) {
        pendingShaderCallback = { uri ->
            if (uri != null) {
                val relativePath = extractShaderRelativePath(uri)
                if (relativePath != null) {
                    onPicked(relativePath)
                } else {
                    showToast("Invalid shader location. Place shaders in your zelda3 folder under shaders/")
                }
            }
        }
        val intent =
            Intent(Intent.ACTION_GET_CONTENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
            }
        @Suppress("DEPRECATION")
        startActivityForResult(Intent.createChooser(intent, "Select shader preset (.slangp)"), REQUEST_SHADER_FILE)
    }

    private suspend fun audioPage(): List<MenuItem> {
        var disableBeep = readConfig { readLowHealthBeepSetting() }
        val settings = readConfig { readMsuSettings() }
        // The game keeps running at the rate it started with; changing it needs a restart
        val runningAudioFreq = startupAudioFreq ?: settings.audioFreq.also { startupAudioFreq = it }
        val msuScan = scanMsuFiles()
        val msuInfo = detectMsuInfo()
        val isDeluxe = msuScan.maxTrackNumber > 47 // The original game has tracks 1-47

        var enableMsu = settings.enableMsu
        var volume = settings.volume
        var resumeMsu = settings.resumeMsu

        val apply = {
            applyAudioSettings(disableBeep, enableMsu, volume, resumeMsu, msuInfo, isDeluxe, runningAudioFreq)
        }

        val msuSummary =
            if (msuScan.fileCount > 0) {
                val formatName =
                    when (msuInfo?.format) {
                        "MSU1" -> if (isDeluxe) "PCM Deluxe" else "PCM"
                        "OPUZ" -> if (isDeluxe) "Opuz Deluxe" else "Opuz"
                        else -> "Unknown"
                    }
                "${msuScan.fileCount} MSU track${if (msuScan.fileCount != 1) "s" else ""} detected ($formatName)"
            } else {
                "No MSU files detected"
            }

        return listOf(
            MenuItem.Header("Sound"),
            MenuItem.Toggle(
                "Disable low health beep",
                get = { disableBeep },
                set = {
                    disableBeep = it
                    apply()
                },
            ),
            MenuItem.Header("MSU-1 soundtrack"),
            MenuItem.Info({ msuSummary }),
            MenuItem.Toggle(
                "Enable MSU",
                enabled = { msuScan.fileCount > 0 },
                get = { enableMsu },
                set = {
                    enableMsu = it
                    apply()
                },
            ),
            MenuItem.Toggle(
                "Resume MSU",
                enabled = { enableMsu },
                get = { resumeMsu },
                set = {
                    resumeMsu = it
                    apply()
                },
            ),
            MenuItem.Slider(
                "MSU volume",
                min = 0,
                max = 100,
                step = 5,
                enabled = { enableMsu },
                get = { volume },
                set = {
                    volume = it
                    apply()
                },
            ),
        )
    }

    /**
     * Saves audio settings and hot-reloads them in the running game. Switching between PCM and
     * Opuz changes the output sample rate, which only takes effect after a restart.
     */
    private fun applyAudioSettings(
        disableBeep: Boolean,
        enableMsu: Boolean,
        volume: Int,
        resumeMsu: Boolean,
        msuInfo: MsuDetectionResult?,
        isDeluxe: Boolean,
        runningAudioFreq: Int,
    ) {
        val format = if (enableMsu) msuInfo?.format else null
        val audioFreq =
            when (format) {
                "MSU1" -> 44100
                "OPUZ" -> 48000
                else -> runningAudioFreq
            }
        // kMsuEnabled_Msu=1, kMsuEnabled_MsuDeluxe=2, kMsuEnabled_Opuz=4
        var msuFlags = 0
        if (enableMsu) {
            msuFlags = 1
            if (isDeluxe) msuFlags = msuFlags or 2
            if (format == "OPUZ") msuFlags = msuFlags or 4
        }

        persist {
            updateLowHealthBeepSetting(disableBeep)
            updateMsuSettings(enableMsu, volume, resumeMsu, audioFreq, if (enableMsu) msuInfo else null, isDeluxe)
            runOnUiThread {
                if (audioFreq != runningAudioFreq) {
                    pauseMenu.markRestartNeeded()
                } else {
                    nativeReloadAudioConfig(msuFlags, volume, if (disableBeep) 1 else 0)
                }
            }
        }
    }

    private fun controlsPage(): List<MenuItem> {
        val items = mutableListOf<MenuItem>(MenuItem.Info({ "Select a command, then press the button or combination to bind" }))
        var category: String? = null
        for (binding in getAllBindableCommands()) {
            if (binding.category != category) {
                category = binding.category
                items += MenuItem.Header(binding.category)
            }
            items +=
                MenuItem.Action(binding.commandDisplayName, value = { binding.currentBinding ?: "Not bound" }) {
                    showBindButtonDialog(binding.commandId, binding.commandDisplayName)
                }
        }
        items += MenuItem.Header("Defaults")
        items += MenuItem.Action("Reset to defaults", destructive = true) { confirmResetBindings() }
        return items
    }

    private fun confirmResetBindings() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Reset to defaults?")
            .setMessage("This will reset controller inputs to default.")
            .setPositiveButton("Reset to default") { _, _ ->
                nativeClearGamepadBindings()
                nativeApplyDefaultGamepadBindings()
                saveGamepadBindingsToConfig()
                showToast("Default bindings restored")
                pauseMenu.reload()
            }.setNegativeButton("Cancel", null)
            .show()
            .stylePositiveAsDestructive()
    }

    private fun overlayPage(): List<MenuItem> {
        val enabled = { inputOverlay.getOverlayEnabled() }
        return listOf(
            MenuItem.Toggle(
                "Show touch controls",
                get = enabled,
                set = { inputOverlay.setOverlayEnabled(it) },
            ),
            MenuItem.Action("Edit layout", enabled = enabled) { editOverlayLayout() },
            MenuItem.Action("Visible buttons", opensPage = true, enabled = enabled) {
                pauseMenu.push("Visible buttons") { overlayButtonsPage() }
            },
            // Scale -50..100 is a (scale + 50) / 100 multiplier on the bitmaps; show it relative to
            // the default (0 = 100%) and keep a floor so the controls never shrink to nothing
            MenuItem.Slider(
                "Size",
                min = -40,
                max = 100,
                step = 5,
                format = { "${(it + 50) * 2}%" },
                enabled = enabled,
                get = { inputOverlay.getOverlayScale() },
                set = { inputOverlay.setOverlayScale(it) },
            ),
            MenuItem.Slider(
                "Opacity",
                min = 0,
                max = 100,
                step = 5,
                enabled = enabled,
                get = { inputOverlay.getOverlayOpacity() },
                set = { inputOverlay.setOverlayOpacity(it) },
            ),
            MenuItem.Toggle(
                "D-pad slide",
                enabled = enabled,
                get = { inputOverlay.getDpadSlideEnabled() },
                set = { inputOverlay.setDpadSlide(it) },
            ),
            MenuItem.Toggle(
                "Haptic feedback",
                enabled = enabled,
                get = { inputOverlay.getHapticFeedbackEnabled() },
                set = { inputOverlay.setHapticFeedback(it) },
            ),
            MenuItem.Action("Reset touch controls", destructive = true, enabled = enabled) { resetOverlay() },
        )
    }

    private fun overlayButtonsPage(): List<MenuItem> {
        // Same order as OverlayControl, which setControlVisibility() indexes into
        val labels = listOf("Button A", "Button B", "Button X", "Button Y", "Button L", "Button R", "Start", "Select", "D-pad")
        val visibility = inputOverlay.getControlVisibility()
        return labels.mapIndexed { index, label ->
            MenuItem.Toggle(
                label,
                get = { visibility[index] },
                set = {
                    visibility[index] = it
                    inputOverlay.setControlVisibility(index, it)
                },
            )
        }
    }

    private suspend fun systemPage(): List<MenuItem> {
        val items = mutableListOf<MenuItem>()
        val languages = availableLanguages
        if (languages.size > 1) {
            // US first, then alphabetically by display name
            val sorted = languages.sortedWith(compareBy({ it != "us" }, { getLanguageDisplayName(it) }))
            var current = sorted.indexOf(normalizeLanguageCode(readConfig { readLanguageSetting() })).coerceAtLeast(0)
            items += MenuItem.Header("Language")
            items +=
                MenuItem.Choice(
                    "Dialogue language",
                    sorted.map { getLanguageDisplayName(it) },
                    restart = true,
                    get = { current },
                    set = {
                        current = it
                        persist { updateLanguageSetting(sorted[it]) }
                    },
                )
        }
        items += MenuItem.Header("Assets")
        items += MenuItem.Action("Delete assets file", destructive = true) { showResetAssetsDialog() }
        return items
    }

    /** Leaves the menu with the game still paused so the touch layout can be edited. */
    private fun editOverlayLayout() {
        resumeAfterEdit = !wasPausedBeforeMenu
        wasPausedBeforeMenu = true // stops onClosed from resuming
        pauseMenu.close()
        enterEditMode()
    }

    private fun enterEditMode() {
        inputOverlay.setIsInEditMode(true)
        doneControlConfigButton.visibility = View.VISIBLE
        showToast("Edit mode: Drag to move, tap to scale", Toast.LENGTH_LONG)
    }

    private fun exitEditMode() {
        doneControlConfigButton.visibility = View.GONE
        inputOverlay.setIsInEditMode(false)
        showToast("Changes saved")
        if (resumeAfterEdit && nativeIsPaused()) nativeTogglePause()
        resumeAfterEdit = false
    }

    private fun resetOverlay() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Reset touch controls")
            .setMessage("Reset all button positions, scales, and settings to defaults?")
            .setPositiveButton("Reset") { _, _ ->
                inputOverlay.resetOverlay()
                showToast("Touch controls reset to defaults")
                pauseMenu.reload()
            }.setNegativeButton("Cancel", null)
            .show()
    }

    private fun showResetAssetsDialog() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Delete Assets File")
            .setMessage(
                "This will delete zelda3_assets.dat and restart the app.\n\nYou will need to select the assets file again. Your saves and config will NOT be deleted.",
            ).setPositiveButton("Delete file") { _, _ ->
                val assetsFile = File(getExternalFilesDir(null), "zelda3_assets.dat")
                when {
                    !assetsFile.exists() -> {
                        showToast("Assets file not found")
                    }

                    assetsFile.delete() -> {
                        showToast("Assets file deleted. Restarting...")
                        restartApp()
                    }

                    else -> {
                        showToast("Failed to delete assets file", Toast.LENGTH_LONG)
                    }
                }
            }.setNegativeButton("Cancel", null)
            .show()
            .styleConfirmButton(R.color.md_theme_error, R.color.md_theme_onError, R.drawable.ic_delete)
    }

    private fun showRestartDialog() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Restart Required")
            .setMessage("Some of the settings you changed take effect after restarting the game.")
            .setNegativeButton("Later", null)
            .setPositiveButton("Restart app") { _, _ -> restartApp() }
            .show()
            .styleConfirmButton(R.color.md_theme_primary, R.color.md_theme_onPrimary, R.drawable.ic_restart)
    }

    private fun restartApp() {
        val intent =
            Intent(this, LauncherActivity::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
            }
        startActivity(intent)
        finishAndRemoveTask()
        System.exit(0)
    }

    /** Fills the positive button so the confirming action stands out (optionally with an icon). */
    private fun AlertDialog.styleConfirmButton(
        background: Int,
        foreground: Int,
        icon: Int = 0,
    ) {
        getButton(AlertDialog.BUTTON_POSITIVE)?.apply {
            val onColor = resources.getColorStateList(foreground, theme)
            backgroundTintList = resources.getColorStateList(background, theme)
            setTextColor(onColor)
            if (icon != 0) {
                setCompoundDrawablesWithIntrinsicBounds(icon, 0, 0, 0)
                compoundDrawablePadding = resources.getDimensionPixelSize(android.R.dimen.app_icon_size) / 4
                setCompoundDrawableTintList(onColor)
            }
        }
    }

    /**
     * Extracts a relative shader path from a content URI.
     * Looks for "shaders/" or "Shaders/" in the path and returns everything from there.
     */
    private fun extractShaderRelativePath(uri: android.net.Uri): String? {
        val path = uri.path ?: return null
        Log.d(TAG, "extractShaderRelativePath: uri.path = $path")

        // Look for "shaders/" or "Shaders/" in the path (case-insensitive search)
        val lowerPath = path.lowercase()
        val shadersIndex = lowerPath.indexOf("shaders/")
        if (shadersIndex >= 0) {
            // Return the path preserving original case
            return path.substring(shadersIndex)
        }

        // Fallback: try to get filename only (let native code resolve path)
        val filename = path.substringAfterLast("/")
        if (filename.endsWith(".slangp") || filename.endsWith(".slang")) {
            Log.w(TAG, "extractShaderRelativePath: Could not find 'shaders/' in path, returning filename only: $filename")
            return filename
        }

        return null
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)

        // Landscape is forced in onCreate - this should only fire for landscape changes
        // (e.g. 180° flip) or non-orientation config changes (locale, keyboard, etc.)
        val orientation = newConfig.orientation
        if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
            inputOverlay.setLayoutOrientation(OverlayLayout.Landscape)
        }
        // Portrait is locked out - shouldn't reach here

        // Dismiss dialog on config change to fix layout issues
        currentDialog?.let {
            if (it.isShowing) {
                it.dismiss()
            }
        }
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val keyCode = event.keyCode
        val isDown = event.action == KeyEvent.ACTION_DOWN

        // Bind dialog input interception - capture gamepad buttons for binding
        if (isBindDialogOpen && isGamepadButton(keyCode)) {
            handleBindDialogInput(event)
            return true // Consume event to block SDL
        }

        if (trackMenuCombo(keyCode, isDown, event.repeatCount)) return true

        if (pauseMenu.isOpen) {
            // Keys held when the menu opened (e.g. Start of Start+Select) must still be released
            // in the game, or they stay stuck after resuming
            if (event.action == KeyEvent.ACTION_UP) handleKeyEvent(null, keyCode, event, null)
            return pauseMenu.handleKey(event) || super.dispatchKeyEvent(event)
        }

        // CRITICAL: Intercept back button BEFORE SDL processes it
        if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_BUTTON_MODE) {
            if (isDown && event.repeatCount == 0) pauseMenu.open()
            return true // Consume event - don't let SDL process it
        }

        return super.dispatchKeyEvent(event)
    }

    // Controllers like the AYN Thor report the D-pad as hat axes. Android turns unhandled hat/stick
    // motion into D-pad keys for the focused view, as long as it doesn't reach the game surface.
    override fun dispatchGenericMotionEvent(event: android.view.MotionEvent): Boolean {
        if (pauseMenu.handleMotion(event)) return true
        return super.dispatchGenericMotionEvent(event)
    }

    /**
     * Start+Select toggles the pause menu, from a gamepad, a keyboard (N+B, the zelda3.ini
     * defaults) or the touch overlay. Returns true when the combo fired and the key is consumed.
     */
    private fun trackMenuCombo(
        keyCode: Int,
        isDown: Boolean,
        repeatCount: Int = 0,
    ): Boolean {
        val partner = menuComboPartners[keyCode] ?: return false
        if (!isDown) {
            heldComboKeys.remove(keyCode)
            return false
        }
        heldComboKeys.add(keyCode)
        if (repeatCount > 0 || partner !in heldComboKeys) return false
        if (pauseMenu.isOpen) pauseMenu.close() else pauseMenu.open()
        return true
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        if (pauseMenu.isOpen) {
            pauseMenu.handleKey(KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK))
        } else {
            pauseMenu.open()
        }
    }

    // CRITICAL: Public wrappers for SDL key events (called from InputOverlay)
    // These methods MUST remain for JNI integration
    fun sendKeyDown(keycode: Int) {
        if (trackMenuCombo(keycode, isDown = true)) return
        onNativeKeyDown(keycode)
    }

    fun sendKeyUp(keycode: Int) {
        trackMenuCombo(keycode, isDown = false)
        onNativeKeyUp(keycode)
    }

    private fun isExternalStorageWritable(): Boolean = Environment.getExternalStorageState() == Environment.MEDIA_MOUNTED

    // === MSU Audio Management Methods (using coroutines for I/O) ===

    // Data class for MSU scan result (file count and max track number)
    private data class MsuScanResult(
        val fileCount: Int,
        val maxTrackNumber: Int,
    )

    /**
     * Scans MSU directory, counts valid MSU files, and finds max track number.
     */
    private suspend fun scanMsuFiles(): MsuScanResult =
        withContext(Dispatchers.IO) {
            val uriString =
                zelda3FolderUri ?: run {
                    Log.e(TAG, "scanMsuFiles: No SAF Uri stored")
                    return@withContext MsuScanResult(0, 0)
                }

            val treeUri = Uri.parse(uriString)
            Log.d(TAG, "scanMsuFiles: Using SAF Uri = $uriString")

            val rootDir =
                DocumentFile.fromTreeUri(this@MainActivity, treeUri) ?: run {
                    Log.e(TAG, "scanMsuFiles: Failed to get DocumentFile from tree Uri")
                    return@withContext MsuScanResult(0, 0)
                }

            val msuDir =
                rootDir.findFile("MSU") ?: run {
                    Log.e(TAG, "scanMsuFiles: MSU directory not found")
                    return@withContext MsuScanResult(0, 0)
                }

            if (!msuDir.isDirectory) {
                Log.e(TAG, "scanMsuFiles: MSU is not a directory")
                return@withContext MsuScanResult(0, 0)
            }

            val files = msuDir.listFiles()
            Log.d(TAG, "scanMsuFiles: DocumentFile.listFiles() returned ${files.size} files")

            // Pattern to match MSU files and capture track number
            val pattern = Regex(".*?(\\d+)\\.(pcm|opuz)$", RegexOption.IGNORE_CASE)
            var count = 0
            var maxTrack = 0

            for (file in files) {
                val fileName = file.name ?: continue
                val match = pattern.find(fileName)
                if (match != null) {
                    count++
                    val trackNum = match.groupValues[1].toIntOrNull() ?: 0
                    if (trackNum > maxTrack) maxTrack = trackNum
                    if (files.size <= 5 || count <= 5) {
                        Log.d(TAG, "scanMsuFiles: $fileName track=$trackNum")
                    }
                }
            }

            Log.d(TAG, "scanMsuFiles: Total files matched = $count, maxTrack = $maxTrack")
            MsuScanResult(count, maxTrack)
        }

    // Data class for MSU format detection result
    private data class MsuDetectionResult(
        // "MSU1" or "OPUZ"
        val format: String,
        // e.g. "ALttP-msu-Deluxe-"
        val filePrefix: String,
    )

    /**
     * Detects MSU format and filename prefix from first valid file.
     * @return MsuDetectionResult with format and prefix, or null if no files found
     */
    private suspend fun detectMsuInfo(): MsuDetectionResult? =
        withContext(Dispatchers.IO) {
            val uriString =
                zelda3FolderUri ?: run {
                    Log.e(TAG, "detectMsuInfo: No SAF Uri stored")
                    return@withContext null
                }

            val treeUri = Uri.parse(uriString)
            Log.d(TAG, "detectMsuInfo: Using SAF Uri = $uriString")

            val rootDir =
                DocumentFile.fromTreeUri(this@MainActivity, treeUri) ?: run {
                    Log.e(TAG, "detectMsuInfo: Failed to get DocumentFile from tree Uri")
                    return@withContext null
                }

            val msuDir =
                rootDir.findFile("MSU") ?: run {
                    Log.e(TAG, "detectMsuInfo: MSU directory not found")
                    return@withContext null
                }

            val files = msuDir.listFiles()
            val pattern = Regex("alttp[_-]msu[_-](?:deluxe[_-])?\\d+\\.(pcm|opuz)", RegexOption.IGNORE_CASE)

            for (file in files) {
                val fileName = file.name ?: continue
                if (fileName.matches(pattern)) {
                    Log.d(TAG, "detectMsuInfo: Checking $fileName")
                    try {
                        contentResolver.openInputStream(file.uri)?.use { fis ->
                            val header = ByteArray(4)
                            if (fis.read(header) == 4) {
                                val format: String? =
                                    when {
                                        // Check for MSU1 magic (0x4D535531)
                                        header[0] == 0x4D.toByte() && header[1] == 0x53.toByte() &&
                                            header[2] == 0x55.toByte() && header[3] == 0x31.toByte() -> {
                                            Log.d(TAG, "detectMsuInfo: Detected MSU1 (PCM) format")
                                            "MSU1"
                                        }

                                        // Check for OPUZ magic (0x4F50555A)
                                        header[0] == 0x4F.toByte() && header[1] == 0x50.toByte() &&
                                            header[2] == 0x55.toByte() && header[3] == 0x5A.toByte() -> {
                                            Log.d(TAG, "detectMsuInfo: Detected OPUZ (Opus) format")
                                            "OPUZ"
                                        }

                                        else -> {
                                            null
                                        }
                                    }

                                if (format != null) {
                                    // Extract prefix from filename (remove track number and extension)
                                    // e.g. "ALttP-msu-Deluxe-1.pcm" -> "ALttP-msu-Deluxe-"
                                    val prefix = fileName.replaceFirst(Regex("\\d+\\.(pcm|opuz)$", RegexOption.IGNORE_CASE), "")
                                    Log.d(TAG, "detectMsuInfo: Detected prefix='$prefix'")
                                    // Return just the prefix - Platform_OpenFile prepends "MSU/" on Android
                                    return@withContext MsuDetectionResult(format, prefix)
                                }
                            }
                        }
                    } catch (e: IOException) {
                        Log.e(TAG, "Error detecting MSU info for $fileName", e)
                    }
                }
            }
            Log.d(TAG, "detectMsuInfo: No valid format detected")
            null
        }

    /**
     * Reads MSU settings from zelda3.ini [Sound] section.
     */
    private suspend fun readMsuSettings(): MsuSettings =
        withContext(Dispatchers.IO) {
            val externalDir = getExternalFilesDir(null) ?: return@withContext MsuSettings(false, 100, true, 44100)
            val configFile = File(externalDir, "zelda3.ini")

            if (!configFile.exists()) return@withContext MsuSettings(false, 100, true, 44100)

            var enableMsu = false
            var volume = 100
            var resumeMsu = true
            var audioFreq = 44100

            try {
                var inSoundSection = false

                configFile.readLines().forEach { line ->
                    val trimmed = line.trim()

                    when {
                        trimmed == "[Sound]" -> {
                            inSoundSection = true
                        }

                        trimmed.startsWith("[") -> {
                            inSoundSection = false
                        }

                        inSoundSection -> {
                            when {
                                trimmed.startsWith("EnableMSU") -> {
                                    val value = trimmed.substringAfter("=").trim()
                                    enableMsu = !(value.equals("false", ignoreCase = true) || value == "0")
                                    Log.d(TAG, "readMsuSettings: EnableMSU = $value -> $enableMsu")
                                }

                                trimmed.startsWith("MSUVolume") -> {
                                    val value = trimmed.substringAfter("=").trim().removeSuffix("%")
                                    volume = value.toIntOrNull() ?: 100
                                    Log.d(TAG, "readMsuSettings: MSUVolume = $volume")
                                }

                                trimmed.startsWith("ResumeMSU") -> {
                                    val value = trimmed.substringAfter("=").trim()
                                    resumeMsu = value.toIntOrNull() == 1
                                    Log.d(TAG, "readMsuSettings: ResumeMSU = $resumeMsu")
                                }

                                trimmed.startsWith("AudioFreq") -> {
                                    val value = trimmed.substringAfter("=").trim()
                                    audioFreq = value.toIntOrNull() ?: 44100
                                    Log.d(TAG, "readMsuSettings: AudioFreq = $audioFreq")
                                }
                            }
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error reading MSU settings", e)
            }

            val settings = MsuSettings(enableMsu, volume, resumeMsu, audioFreq)
            Log.d(TAG, "readMsuSettings: Final settings: $settings")
            settings
        }

    /**
     * Updates MSU settings in zelda3.ini [Sound] section.
     * [msuInfo] and [isDeluxe] come from [detectMsuInfo]/[scanMsuFiles], done once by the caller.
     */
    private suspend fun updateMsuSettings(
        enableMsu: Boolean,
        volume: Int,
        resumeMsu: Boolean,
        audioFreq: Int,
        msuInfo: MsuDetectionResult?,
        isDeluxe: Boolean,
    ): Boolean =
        withContext(Dispatchers.IO) {
            val externalDir = getExternalFilesDir(null) ?: return@withContext false
            val configFile = File(externalDir, "zelda3.ini")

            if (!configFile.exists()) return@withContext false

            try {
                val detectedFormat = msuInfo?.format
                val detectedPath = msuInfo?.filePrefix

                if (enableMsu && detectedPath != null) {
                    Log.d(TAG, "updateMsuSettings: Will update MSUPath to: $detectedPath, isDeluxe=$isDeluxe")
                }

                val lines = configFile.readLines()
                var inSoundSection = false

                val updatedLines =
                    lines.map { line ->
                        val trimmed = line.trim()

                        when {
                            trimmed == "[Sound]" -> {
                                inSoundSection = true
                                line
                            }

                            trimmed.startsWith("[") && trimmed.endsWith("]") -> {
                                inSoundSection = false
                                line
                            }

                            inSoundSection && trimmed.startsWith("EnableMSU") -> {
                                val newValue =
                                    if (enableMsu) {
                                        when {
                                            detectedFormat == "OPUZ" && isDeluxe -> "EnableMSU = deluxe-opuz"
                                            detectedFormat == "OPUZ" -> "EnableMSU = opuz"
                                            isDeluxe -> "EnableMSU = deluxe"
                                            else -> "EnableMSU = true"
                                        }
                                    } else {
                                        "EnableMSU = false"
                                    }
                                Log.d(TAG, "Updated config line: $newValue (format: $detectedFormat, isDeluxe: $isDeluxe)")
                                newValue
                            }

                            inSoundSection && trimmed.startsWith("MSUPath") -> {
                                // Update MSUPath to match actual file naming pattern
                                val newValue =
                                    if (enableMsu && detectedPath != null) {
                                        "MSUPath = $detectedPath"
                                    } else {
                                        line // Keep existing value if disabled or detection failed
                                    }
                                if (newValue != line) {
                                    Log.d(TAG, "Updated config line: $newValue")
                                }
                                newValue
                            }

                            inSoundSection && trimmed.startsWith("MSUVolume") -> {
                                val newValue = "MSUVolume = $volume%"
                                Log.d(TAG, "Updated config line: $newValue")
                                newValue
                            }

                            inSoundSection && trimmed.startsWith("ResumeMSU") -> {
                                val newValue = "ResumeMSU = ${if (resumeMsu) 1 else 0}"
                                Log.d(TAG, "Updated config line: $newValue")
                                newValue
                            }

                            inSoundSection && audioFreq > 0 && trimmed.startsWith("AudioFreq") -> {
                                val newValue = "AudioFreq = $audioFreq"
                                Log.d(TAG, "Updated config line: $newValue")
                                newValue
                            }

                            else -> {
                                line
                            }
                        }
                    }

                // Write config with explicit flush and sync to ensure data is on disk before JNI reads it
                FileOutputStream(configFile).use { fos ->
                    fos.write(updatedLines.joinToString("\n").toByteArray())
                    fos.flush()
                    fos.fd.sync() // Force OS to flush buffers to disk
                }
                Log.d(TAG, "Successfully wrote config to: ${configFile.absolutePath}")
                Log.d(TAG, "MSU settings updated: EnableMSU=$enableMsu, Volume=$volume, ResumeMSU=$resumeMsu, AudioFreq=$audioFreq")
                true
            } catch (e: IOException) {
                Log.e(TAG, "Error updating MSU settings", e)
                false
            }
        }

    /**
     * Reads DisableLowHealthBeep setting from zelda3.ini [Features] section.
     */
    private suspend fun readLowHealthBeepSetting(): Boolean = ConfigManager.readBool(this, "Features", "DisableLowHealthBeep", false)

    /**
     * Updates DisableLowHealthBeep setting in zelda3.ini [Features] section.
     */
    private suspend fun updateLowHealthBeepSetting(disable: Boolean) {
        ConfigManager.writeBool(this, "Features", "DisableLowHealthBeep", disable)
    }

    /**
     * Reads Language setting from zelda3.ini [General] section.
     */
    private suspend fun readLanguageSetting(): String =
        withContext(Dispatchers.IO) {
            val externalDir = getExternalFilesDir(null) ?: return@withContext "us"
            val configFile = File(externalDir, "zelda3.ini")

            if (!configFile.exists()) return@withContext "us"

            try {
                configFile.readLines().forEach { line ->
                    val trimmed = line.trim()
                    if (trimmed.startsWith("Language", ignoreCase = true) && trimmed.contains("=")) {
                        val value = trimmed.substringAfter("=").trim()
                        if (value.isNotEmpty()) {
                            Log.d(TAG, "readLanguageSetting: Found Language = $value")
                            return@withContext value
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error reading language setting", e)
            }

            "us" // Default
        }

    /**
     * Updates Language setting in zelda3.ini.
     */
    private suspend fun updateLanguageSetting(language: String) =
        withContext(Dispatchers.IO) {
            val externalDir = getExternalFilesDir(null) ?: return@withContext
            val configFile = File(externalDir, "zelda3.ini")

            try {
                val lines = if (configFile.exists()) configFile.readLines().toMutableList() else mutableListOf()

                // Find and update or add Language setting
                var found = false
                for (i in lines.indices) {
                    if (lines[i].trim().startsWith("Language", ignoreCase = true) && lines[i].contains("=")) {
                        lines[i] = "Language = $language"
                        found = true
                        Log.d(TAG, "updateLanguageSetting: Updated existing Language to $language")
                        break
                    }
                }

                if (!found) {
                    // Add under [General] section if exists, otherwise at the end
                    val generalIndex = lines.indexOfFirst { it.trim() == "[General]" }
                    if (generalIndex >= 0) {
                        lines.add(generalIndex + 1, "Language = $language")
                        Log.d(TAG, "updateLanguageSetting: Added Language under [General]")
                    } else {
                        lines.add("Language = $language")
                        Log.d(TAG, "updateLanguageSetting: Added Language at end of file")
                    }
                }

                configFile.writeText(lines.joinToString("\n"))
                Log.d(TAG, "Language setting saved: $language")
            } catch (e: Exception) {
                Log.e(TAG, "Error updating language setting", e)
            }
        }

    /**
     * Reads OutputMethod setting from zelda3.ini [Graphics] section.
     */
    private suspend fun readRendererSetting(): String =
        withContext(Dispatchers.IO) {
            val externalDir = getExternalFilesDir(null) ?: return@withContext "SDL"
            val configFile = File(externalDir, "zelda3.ini")

            Log.d(TAG, "readRendererSetting: Reading from ${configFile.absolutePath}")
            Log.d(TAG, "readRendererSetting: File exists = ${configFile.exists()}")

            if (!configFile.exists()) return@withContext "SDL"

            var outputMethod = "SDL"

            try {
                var inGraphicsSection = false
                val allLines = configFile.readLines()
                Log.d(TAG, "readRendererSetting: Total lines in config = ${allLines.size}")

                allLines.forEachIndexed { index, line ->
                    val trimmed = line.trim()

                    when {
                        trimmed == "[Graphics]" -> {
                            inGraphicsSection = true
                            Log.d(TAG, "readRendererSetting: Found [Graphics] section at line $index")
                        }

                        trimmed.startsWith("[") && trimmed.endsWith("]") -> {
                            if (inGraphicsSection) {
                                Log.d(TAG, "readRendererSetting: Exited [Graphics] section at line $index")
                            }
                            inGraphicsSection = false
                        }

                        inGraphicsSection && trimmed.startsWith("OutputMethod") -> {
                            val value = trimmed.substringAfter("=").trim()
                            outputMethod = value
                            Log.d(TAG, "readRendererSetting: Found OutputMethod at line $index = '$value'")
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error reading renderer setting", e)
            }

            Log.d(TAG, "readRendererSetting: Final value = '$outputMethod'")
            outputMethod
        }

    /**
     * Updates OutputMethod setting in zelda3.ini [Graphics] section.
     */
    private suspend fun updateRendererSetting(renderer: String) =
        withContext(Dispatchers.IO) {
            val externalDir = getExternalFilesDir(null) ?: return@withContext
            val configFile = File(externalDir, "zelda3.ini")

            Log.d(TAG, "updateRendererSetting: Updating to '$renderer'")
            Log.d(TAG, "updateRendererSetting: File path = ${configFile.absolutePath}")

            if (!configFile.exists()) {
                Log.e(TAG, "updateRendererSetting: Config file does not exist!")
                return@withContext
            }

            try {
                val lines = configFile.readLines()
                var inGraphicsSection = false
                var settingUpdated = false
                var lineNumber = 0

                val updatedLines =
                    lines.map { line ->
                        lineNumber++
                        val trimmed = line.trim()

                        when {
                            trimmed == "[Graphics]" -> {
                                inGraphicsSection = true
                                Log.d(TAG, "updateRendererSetting: Found [Graphics] section at line $lineNumber")
                                line
                            }

                            trimmed.startsWith("[") && trimmed.endsWith("]") -> {
                                inGraphicsSection = false
                                line
                            }

                            inGraphicsSection && trimmed.startsWith("OutputMethod") -> {
                                settingUpdated = true
                                val newValue = "OutputMethod = $renderer"
                                Log.d(TAG, "updateRendererSetting: Updated line $lineNumber: '$trimmed' -> '$newValue'")
                                newValue
                            }

                            else -> {
                                line
                            }
                        }
                    }

                if (!settingUpdated) {
                    Log.e(TAG, "updateRendererSetting: OutputMethod not found in [Graphics] section!")
                    Log.e(TAG, "updateRendererSetting: Dumping first 60 lines of config:")
                    lines.take(60).forEachIndexed { idx, line ->
                        Log.e(TAG, "  Line ${idx + 1}: $line")
                    }
                    return@withContext
                }

                configFile.writeText(updatedLines.joinToString("\n"))
                Log.d(TAG, "updateRendererSetting: Successfully wrote config file")
                Log.d(TAG, "updateRendererSetting: Verifying write...")

                // Verify the write
                val verifyValue =
                    configFile
                        .readLines()
                        .find { it.trim().startsWith("OutputMethod") }
                        ?.substringAfter("=")
                        ?.trim()
                Log.d(TAG, "updateRendererSetting: Verification read: '$verifyValue'")
            } catch (e: Exception) {
                Log.e(TAG, "Error updating renderer setting: ${e::class.simpleName}: ${e.message}", e)
            }
        }

    /**
     * Data class for ExtendedAspectRatio settings.
     */
    data class AspectRatioSettings(
        val aspectRatio: String = "16:9",
        val extendY: Boolean = true,
    )

    /**
     * Reads ExtendedAspectRatio setting from zelda3.ini [General] section.
     * Parses composite format like "extend_y, 16:9" or just "16:9".
     */
    private suspend fun readExtendedAspectRatio(): AspectRatioSettings =
        withContext(Dispatchers.IO) {
            val externalDir = getExternalFilesDir(null) ?: return@withContext AspectRatioSettings()
            val configFile = File(externalDir, "zelda3.ini")

            if (!configFile.exists()) return@withContext AspectRatioSettings()

            var aspectRatio = "16:9"
            var extendY = true

            try {
                var inGeneralSection = false
                configFile.readLines().forEach { line ->
                    val trimmed = line.trim()

                    when {
                        trimmed == "[General]" -> {
                            inGeneralSection = true
                        }

                        trimmed.startsWith("[") && trimmed.endsWith("]") -> {
                            inGeneralSection = false
                        }

                        inGeneralSection && trimmed.startsWith("ExtendedAspectRatio", ignoreCase = true) -> {
                            val value = trimmed.substringAfter("=").trim()
                            Log.d(TAG, "readExtendedAspectRatio: raw value = '$value'")

                            // Parse composite value like "extend_y, 16:9" or just "16:9"
                            extendY = value.contains("extend_y", ignoreCase = true)

                            // Extract aspect ratio (look for pattern like "16:9", "4:3", etc.)
                            val ratioPattern = Regex("""\d+:\d+""")
                            val match = ratioPattern.find(value)
                            if (match != null) {
                                aspectRatio = match.value
                            } else if (value.isEmpty() || value == "0") {
                                aspectRatio = "4:3"
                                extendY = false
                            }

                            Log.d(TAG, "readExtendedAspectRatio: parsed aspectRatio=$aspectRatio, extendY=$extendY")
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error reading ExtendedAspectRatio setting", e)
            }

            AspectRatioSettings(aspectRatio, extendY)
        }

    /**
     * Writes ExtendedAspectRatio setting to zelda3.ini [General] section.
     * Writes composite format like "extend_y, 16:9" or just "16:9".
     */
    private suspend fun writeExtendedAspectRatio(
        aspectRatio: String,
        extendY: Boolean,
    ) = withContext(Dispatchers.IO) {
        val externalDir = getExternalFilesDir(null) ?: return@withContext
        val configFile = File(externalDir, "zelda3.ini")

        if (!configFile.exists()) {
            Log.e(TAG, "writeExtendedAspectRatio: Config file does not exist!")
            return@withContext
        }

        // Build the composite value
        val newValue =
            if (extendY) {
                "extend_y, $aspectRatio"
            } else {
                aspectRatio
            }

        Log.d(TAG, "writeExtendedAspectRatio: Writing value '$newValue'")

        try {
            val lines = configFile.readLines()
            var inGeneralSection = false
            var settingUpdated = false

            val updatedLines =
                lines.map { line ->
                    val trimmed = line.trim()

                    when {
                        trimmed == "[General]" -> {
                            inGeneralSection = true
                            line
                        }

                        trimmed.startsWith("[") && trimmed.endsWith("]") -> {
                            inGeneralSection = false
                            line
                        }

                        inGeneralSection && trimmed.startsWith("ExtendedAspectRatio", ignoreCase = true) -> {
                            settingUpdated = true
                            "ExtendedAspectRatio = $newValue"
                        }

                        else -> {
                            line
                        }
                    }
                }

            if (!settingUpdated) {
                Log.w(TAG, "writeExtendedAspectRatio: ExtendedAspectRatio not found, adding to [General] section")
                // Add the setting to [General] section
                val newLines = mutableListOf<String>()
                var addedSetting = false
                lines.forEach { line ->
                    newLines.add(line)
                    if (line.trim() == "[General]" && !addedSetting) {
                        newLines.add("ExtendedAspectRatio = $newValue")
                        addedSetting = true
                    }
                }
                configFile.writeText(newLines.joinToString("\n"))
            } else {
                configFile.writeText(updatedLines.joinToString("\n"))
            }

            Log.d(TAG, "writeExtendedAspectRatio: Successfully wrote config file")
        } catch (e: Exception) {
            Log.e(TAG, "Error writing ExtendedAspectRatio setting", e)
        }
    }

    // ========== Save/Load State Functions ==========

    /**
     * Gets the SAF DocumentFile for the saves directory.
     * Returns null if SAF is not configured.
     */
    private fun getSafSavesDir(): DocumentFile? {
        val uriString = zelda3FolderUri ?: return null
        val treeUri = Uri.parse(uriString)
        val rootDir = DocumentFile.fromTreeUri(this, treeUri) ?: return null
        return rootDir.findFile("saves")
    }

    /** Lists all save slots with a single SAF directory listing (findFile per slot is slow). */
    private fun loadSaveSlots(): List<SaveSlot> {
        val files = getSafSavesDir()?.listFiles()?.associateBy { it.name } ?: emptyMap()
        return (0..9).map { slot ->
            val thumbnail =
                files["save$slot.png"]?.let { file ->
                    try {
                        contentResolver.openInputStream(file.uri)?.use { android.graphics.BitmapFactory.decodeStream(it) }
                    } catch (e: Exception) {
                        Log.e(TAG, "loadSaveSlots: Error loading thumbnail for slot $slot", e)
                        null
                    }
                }
            SaveSlot(slot, files["save$slot.sav"]?.isFile == true, thumbnail)
        }
    }

    /**
     * Saves a thumbnail for a save slot to SAF external storage.
     */
    private fun captureSaveThumbnail(slot: Int) {
        try {
            // Get screenshot from C code
            val rgbaData =
                nativeGetScreenshotRGBA() ?: run {
                    Log.w(TAG, "captureSaveThumbnail: No screenshot data available")
                    return
                }

            // Convert to Bitmap (256×224 RGBA)
            val bitmap = Bitmap.createBitmap(256, 224, Bitmap.Config.ARGB_8888)
            bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(rgbaData))

            // Get or create saves directory in SAF
            val uriString =
                zelda3FolderUri ?: run {
                    Log.e(TAG, "captureSaveThumbnail: No SAF Uri configured")
                    return
                }
            val treeUri = Uri.parse(uriString)
            val rootDir =
                DocumentFile.fromTreeUri(this, treeUri) ?: run {
                    Log.e(TAG, "captureSaveThumbnail: Failed to get DocumentFile")
                    return
                }
            var savesDir = rootDir.findFile("saves")
            if (savesDir == null) {
                savesDir = rootDir.createDirectory("saves") ?: run {
                    Log.e(TAG, "captureSaveThumbnail: Failed to create saves directory")
                    return
                }
            }

            // Delete existing thumbnail if present
            savesDir.findFile("save$slot.png")?.delete()

            // Create new thumbnail file
            val thumbnailFile =
                savesDir.createFile("image/png", "save$slot.png") ?: run {
                    Log.e(TAG, "captureSaveThumbnail: Failed to create thumbnail file")
                    return
                }

            // Write PNG data
            contentResolver.openOutputStream(thumbnailFile.uri)?.use { out ->
                bitmap.compress(Bitmap.CompressFormat.PNG, 100, out)
            }

            Log.d(TAG, "captureSaveThumbnail: Saved thumbnail for slot $slot")
        } catch (e: Exception) {
            Log.e(TAG, "captureSaveThumbnail: Error saving thumbnail for slot $slot", e)
        }
    }

    // ========== Controller Bindings ==========

    private fun getAllBindableCommands(): List<ControllerBinding> {
        // Command ID constants from config.h (calculated from enum)
        // Enum starts at kKeys_Null=0, kKeys_Controls=1, then sequential
        val keysControls = 1
        val keysLoad = 13
        val keysSave = 33
        val keysCheatLife = 113
        val keysCheatKeys = 114
        val keysCheatEquipment = 115
        val keysCheatWalkThroughWalls = 116
        // Was 121 (kKeys_Pause), correct value is 123
        val keysTurbo = 123
        val keysDisplayPerf = 125

        // Helper to get binding or null
        fun getBinding(cmdId: Int): String? = nativeGetButtonForCommand(cmdId)

        return listOf(
            // GAME CONTROLS (12)
            // Order: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R
            ControllerBinding(keysControls + 0, "Up", getBinding(keysControls + 0), "GAME CONTROLS"),
            ControllerBinding(keysControls + 1, "Down", getBinding(keysControls + 1), "GAME CONTROLS"),
            ControllerBinding(keysControls + 2, "Left", getBinding(keysControls + 2), "GAME CONTROLS"),
            ControllerBinding(keysControls + 3, "Right", getBinding(keysControls + 3), "GAME CONTROLS"),
            ControllerBinding(keysControls + 4, "Select", getBinding(keysControls + 4), "GAME CONTROLS"),
            ControllerBinding(keysControls + 5, "Start", getBinding(keysControls + 5), "GAME CONTROLS"),
            ControllerBinding(keysControls + 6, "A", getBinding(keysControls + 6), "GAME CONTROLS"),
            ControllerBinding(keysControls + 7, "B", getBinding(keysControls + 7), "GAME CONTROLS"),
            ControllerBinding(keysControls + 8, "X", getBinding(keysControls + 8), "GAME CONTROLS"),
            ControllerBinding(keysControls + 9, "Y", getBinding(keysControls + 9), "GAME CONTROLS"),
            ControllerBinding(keysControls + 10, "L", getBinding(keysControls + 10), "GAME CONTROLS"),
            ControllerBinding(keysControls + 11, "R", getBinding(keysControls + 11), "GAME CONTROLS"),
            // SAVE STATE (20)
            ControllerBinding(keysSave + 0, "Save (Quick Save)", getBinding(keysSave + 0), "SAVE STATE"),
            ControllerBinding(keysSave + 1, "Save 1", getBinding(keysSave + 1), "SAVE STATE"),
            ControllerBinding(keysSave + 2, "Save 2", getBinding(keysSave + 2), "SAVE STATE"),
            ControllerBinding(keysSave + 3, "Save 3", getBinding(keysSave + 3), "SAVE STATE"),
            ControllerBinding(keysSave + 4, "Save 4", getBinding(keysSave + 4), "SAVE STATE"),
            ControllerBinding(keysSave + 5, "Save 5", getBinding(keysSave + 5), "SAVE STATE"),
            ControllerBinding(keysSave + 6, "Save 6", getBinding(keysSave + 6), "SAVE STATE"),
            ControllerBinding(keysSave + 7, "Save 7", getBinding(keysSave + 7), "SAVE STATE"),
            ControllerBinding(keysSave + 8, "Save 8", getBinding(keysSave + 8), "SAVE STATE"),
            ControllerBinding(keysSave + 9, "Save 9", getBinding(keysSave + 9), "SAVE STATE"),
            ControllerBinding(keysLoad + 0, "Load (Quick Save)", getBinding(keysLoad + 0), "SAVE STATE"),
            ControllerBinding(keysLoad + 1, "Load 1", getBinding(keysLoad + 1), "SAVE STATE"),
            ControllerBinding(keysLoad + 2, "Load 2", getBinding(keysLoad + 2), "SAVE STATE"),
            ControllerBinding(keysLoad + 3, "Load 3", getBinding(keysLoad + 3), "SAVE STATE"),
            ControllerBinding(keysLoad + 4, "Load 4", getBinding(keysLoad + 4), "SAVE STATE"),
            ControllerBinding(keysLoad + 5, "Load 5", getBinding(keysLoad + 5), "SAVE STATE"),
            ControllerBinding(keysLoad + 6, "Load 6", getBinding(keysLoad + 6), "SAVE STATE"),
            ControllerBinding(keysLoad + 7, "Load 7", getBinding(keysLoad + 7), "SAVE STATE"),
            ControllerBinding(keysLoad + 8, "Load 8", getBinding(keysLoad + 8), "SAVE STATE"),
            ControllerBinding(keysLoad + 9, "Load 9", getBinding(keysLoad + 9), "SAVE STATE"),
            // GAMEPLAY & SYSTEM (2)
            ControllerBinding(keysTurbo, "Turbo", getBinding(keysTurbo), "GAMEPLAY"),
            ControllerBinding(keysDisplayPerf, "Display Performance", getBinding(keysDisplayPerf), "SYSTEM"),
            // CHEATS (4)
            ControllerBinding(keysCheatLife, "Cheat: Full Health", getBinding(keysCheatLife), "CHEATS"),
            ControllerBinding(keysCheatKeys, "Cheat: Get Keys", getBinding(keysCheatKeys), "CHEATS"),
            ControllerBinding(keysCheatEquipment, "Cheat: Get Equipment", getBinding(keysCheatEquipment), "CHEATS"),
            ControllerBinding(
                keysCheatWalkThroughWalls,
                "Cheat: Walk Through Walls (toggle)",
                getBinding(keysCheatWalkThroughWalls),
                "CHEATS",
            ),
        )
    }

    // Gamepad button detection helpers
    private fun isGamepadButton(keyCode: Int): Boolean =
        keyCode in
            listOf(
                KeyEvent.KEYCODE_BUTTON_A,
                KeyEvent.KEYCODE_BUTTON_B,
                KeyEvent.KEYCODE_BUTTON_X,
                KeyEvent.KEYCODE_BUTTON_Y,
                KeyEvent.KEYCODE_BUTTON_START,
                KeyEvent.KEYCODE_BUTTON_SELECT,
                KeyEvent.KEYCODE_BUTTON_L1,
                KeyEvent.KEYCODE_BUTTON_R1,
                KeyEvent.KEYCODE_BUTTON_L2,
                KeyEvent.KEYCODE_BUTTON_R2,
                KeyEvent.KEYCODE_BUTTON_THUMBL, // L3
                KeyEvent.KEYCODE_BUTTON_THUMBR, // R3
                KeyEvent.KEYCODE_DPAD_UP,
                KeyEvent.KEYCODE_DPAD_DOWN,
                KeyEvent.KEYCODE_DPAD_LEFT,
                KeyEvent.KEYCODE_DPAD_RIGHT,
            )

    private fun getButtonName(keyCode: Int): String =
        when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> "A"
            KeyEvent.KEYCODE_BUTTON_B -> "B"
            KeyEvent.KEYCODE_BUTTON_X -> "X"
            KeyEvent.KEYCODE_BUTTON_Y -> "Y"
            KeyEvent.KEYCODE_BUTTON_START -> "Start"
            KeyEvent.KEYCODE_BUTTON_SELECT -> "Back"
            KeyEvent.KEYCODE_BUTTON_L1 -> "L1"
            KeyEvent.KEYCODE_BUTTON_R1 -> "R1"
            KeyEvent.KEYCODE_BUTTON_L2 -> "L2"
            KeyEvent.KEYCODE_BUTTON_R2 -> "R2"
            KeyEvent.KEYCODE_BUTTON_THUMBL -> "L3"
            KeyEvent.KEYCODE_BUTTON_THUMBR -> "R3"
            KeyEvent.KEYCODE_DPAD_UP -> "DpadUp"
            KeyEvent.KEYCODE_DPAD_DOWN -> "DpadDown"
            KeyEvent.KEYCODE_DPAD_LEFT -> "DpadLeft"
            KeyEvent.KEYCODE_DPAD_RIGHT -> "DpadRight"
            else -> "Unknown"
        }

    private fun handleBindDialogInput(event: KeyEvent) {
        val keyCode = event.keyCode
        Log.d(TAG, "handleBindDialogInput: action=${event.action}, keyCode=$keyCode, button=${getButtonName(keyCode)}")
        Log.d(TAG, "handleBindDialogInput: detectedButtons before=$detectedButtons")

        when (event.action) {
            KeyEvent.ACTION_DOWN -> {
                if (!detectedButtons.contains(keyCode)) {
                    detectedButtons.add(keyCode)
                    addButtonChip(keyCode)
                    Log.d(TAG, "handleBindDialogInput: Added button $keyCode, set now=$detectedButtons")

                    // Show timer message if this is the first button
                    if (detectedButtons.size == 1) {
                        bindDialogTimer?.text = "Release all buttons to confirm"
                        bindDialogTimer?.visibility = View.VISIBLE
                    }
                } else {
                    Log.d(TAG, "handleBindDialogInput: Button $keyCode already in set (duplicate press)")
                }
            }

            KeyEvent.ACTION_UP -> {
                Log.d(TAG, "handleBindDialogInput: ACTION_UP for $keyCode, detectedButtons=$detectedButtons")
                if (detectedButtons.contains(keyCode)) {
                    // Capture the full button set on FIRST release
                    if (capturedButtons.isEmpty()) {
                        capturedButtons.addAll(detectedButtons)
                        Log.d(TAG, "handleBindDialogInput: Captured buttons=$capturedButtons")
                    }

                    detectedButtons.remove(keyCode)
                    Log.d(TAG, "handleBindDialogInput: Removed $keyCode, remaining=$detectedButtons")

                    // All buttons released - finalize binding
                    if (detectedButtons.isEmpty()) {
                        Log.d(TAG, "handleBindDialogInput: All buttons released, calling finalizeBind()")
                        finalizeBind()
                    }
                } else {
                    Log.d(TAG, "handleBindDialogInput: ACTION_UP for $keyCode but not in set!")
                }
            }
        }
    }

    private fun addButtonChip(keyCode: Int) {
        val chipGroup = bindDialogChipGroup ?: return
        val chip =
            com.google.android.material.chip.Chip(this).apply {
                text = getButtonName(keyCode)
                isCloseIconVisible = true
                setOnCloseIconClickListener {
                    detectedButtons.remove(keyCode)
                    chipGroup.removeView(this)
                }
            }
        chipGroup.addView(chip)
    }

    private fun finalizeBind() {
        if (capturedButtons.isEmpty()) {
            showToast("No buttons detected")
            return
        }

        // Convert captured buttons to list of button names (preserves insertion order)
        // LinkedHashSet preserves order: first pressed → last pressed
        val buttonNames = capturedButtons.map { getButtonName(it) }
        val bindingString = buttonNames.joinToString("+")

        val commandId =
            currentBindingCommandId ?: run {
                showToast("Error: No command selected")
                return
            }

        Log.d(TAG, "finalizeBind: Binding commandId=$commandId to '$bindingString'")

        // Save to config via JNI
        // The LAST pressed button is the primary (action trigger)
        // All other buttons are modifiers (must be held when pressing primary)
        // Example: User holds L2, then presses L3 → primary=L3, modifiers=[L2]
        val primaryButton = buttonNames.last()
        val modifiers =
            if (buttonNames.size > 1) {
                buttonNames.dropLast(1).toTypedArray()
            } else {
                null
            }

        Log.d(
            TAG,
            "finalizeBind: Calling JNI with primaryButton='$primaryButton', modifiers=${modifiers?.contentToString()}, commandId=$commandId",
        )
        val success = nativeBindGamepadButton(primaryButton, modifiers, commandId)
        if (success) {
            showToast("Bound $currentBindingCommandDisplay to $bindingString")
            Log.d(TAG, "finalizeBind: Successfully saved binding")
        } else {
            showToast("Error: Failed to save binding")
            Log.e(TAG, "finalizeBind: JNI binding failed for commandId=$commandId")
        }

        // Close bind dialog and show the new binding in the menu
        currentDialog?.dismiss()
        cleanupBindDialogState()
        pauseMenu.reload()

        // Persist bindings to zelda3.ini
        saveGamepadBindingsToConfig()
    }

    /**
     * Saves all current gamepad bindings to zelda3.ini file.
     * Reads all bindings from C code via JNI and writes them to [GamepadMap] section.
     */
    private fun saveGamepadBindingsToConfig() {
        try {
            val configFile = File(getExternalFilesDir(null), "zelda3.ini")
            if (!configFile.exists()) {
                Log.e(TAG, "saveGamepadBindingsToConfig: zelda3.ini not found")
                return
            }

            // Read entire config file
            val lines = configFile.readLines().toMutableList()

            // Find [GamepadMap] section
            val gamePadMapIndex = lines.indexOfFirst { it.trim().equals("[GamepadMap]", ignoreCase = true) }
            if (gamePadMapIndex == -1) {
                Log.e(TAG, "saveGamepadBindingsToConfig: [GamepadMap] section not found")
                return
            }

            // Find next section or end of file
            val nextSectionIndex =
                lines
                    .subList(gamePadMapIndex + 1, lines.size)
                    .indexOfFirst { it.trim().startsWith("[") }
                    .let { if (it == -1) lines.size else gamePadMapIndex + 1 + it }

            // Get bindings from native code
            val bindingsJson = nativeGetGamepadBindings()
            Log.d(TAG, "saveGamepadBindingsToConfig: Got bindings JSON: $bindingsJson")
            if (bindingsJson.isEmpty() || bindingsJson == "[]") {
                Log.d(TAG, "saveGamepadBindingsToConfig: No bindings to save")
                return
            }

            // Control names returned by C code, in order (indices 0-11)
            val controlNames = listOf("Up", "Down", "Left", "Right", "Select", "Start", "B", "A", "Y", "X", "L", "R")

            // Build updates map from native bindings
            val updates = mutableMapOf<String, String>()
            val nativeBindings = parseGamepadBindingsJson(bindingsJson)

            // Group control/load/save bindings into arrays, others go directly to updates
            val controlsBindings = arrayOfNulls<String>(12)
            val loadBindings = arrayOfNulls<String>(10)
            val saveBindings = arrayOfNulls<String>(10)

            for ((cmdName, buttonCombo) in nativeBindings) {
                val controlIndex = controlNames.indexOf(cmdName)
                when {
                    controlIndex != -1 -> {
                        controlsBindings[controlIndex] = buttonCombo
                    }

                    cmdName.startsWith("Load") && cmdName.length > 4 -> {
                        cmdName.substring(4).toIntOrNull()?.let { slot ->
                            if (slot in 0..9) loadBindings[slot] = buttonCombo
                        }
                    }

                    cmdName.startsWith("Save") && cmdName.length > 4 -> {
                        cmdName.substring(4).toIntOrNull()?.let { slot ->
                            if (slot in 0..9) saveBindings[slot] = buttonCombo
                        }
                    }

                    else -> {
                        updates[cmdName] = buttonCombo
                    }
                }
            }

            // Convert arrays to comma-separated strings for updates
            if (controlsBindings.any { it != null }) {
                updates["Controls"] = controlsBindings.map { it ?: "" }.joinToString(", ")
            }
            if (loadBindings.any { it != null }) {
                updates["Load"] = loadBindings.map { it ?: "" }.joinToString(", ")
            }
            if (saveBindings.any { it != null }) {
                updates["Save"] = saveBindings.map { it ?: "" }.joinToString(", ")
            }

            // Update existing lines IN-PLACE
            val updatedKeys = mutableSetOf<String>()
            for (i in gamePadMapIndex + 1 until nextSectionIndex) {
                val trimmed = lines[i].trim()
                if (trimmed.contains("=") && !trimmed.startsWith("#")) {
                    val key = trimmed.substringBefore("=").trim()
                    updates[key]?.let { newValue ->
                        lines[i] = "$key = $newValue"
                        updatedKeys.add(key)
                    }
                }
            }

            // Add new bindings that didn't exist (before next section or end)
            val newKeys = updates.keys - updatedKeys
            var insertIdx = nextSectionIndex
            for (key in newKeys) {
                lines.add(insertIdx, "$key = ${updates[key]}")
                insertIdx++
            }

            // Write back to file
            configFile.writeText(lines.joinToString("\n"))
            Log.d(TAG, "saveGamepadBindingsToConfig: Successfully saved bindings")
        } catch (e: Exception) {
            Log.e(TAG, "saveGamepadBindingsToConfig: Error saving bindings", e)
        }
    }

    /**
     * Parses JSON bindings from nativeGetGamepadBindings().
     * Example JSON: [{"commandName":"Up","binding":"DpadUp"},{"commandName":"Turbo","binding":"L3+Start"}]
     *
     * @return Map of command name to button combo string
     */
    private fun parseGamepadBindingsJson(json: String): Map<String, String> {
        val bindings = mutableMapOf<String, String>()
        try {
            // Simple JSON parsing (avoiding external dependencies)
            val items = json.trim('[', ']').split("},")
            for (item in items) {
                val cleaned = item.trim('{', '}', ' ')
                val pairs = cleaned.split(",")
                var cmdName: String? = null
                var binding: String? = null

                for (pair in pairs) {
                    val (key, value) = pair.split(":").map { it.trim('"', ' ') }
                    when (key) {
                        "commandName" -> cmdName = value
                        "binding" -> binding = value
                    }
                }

                if (cmdName != null && binding != null && binding.isNotEmpty()) {
                    bindings[cmdName] = binding
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "parseGamepadBindingsJson: Error parsing JSON", e)
        }
        return bindings
    }

    private fun cleanupBindDialogState() {
        detectedButtons.clear()
        capturedButtons.clear()
        bindDialogChipGroup = null
        bindDialogTimer = null
        currentBindingCommandId = null
        currentBindingCommandDisplay = null
        isBindDialogOpen = false
    }

    private fun showBindButtonDialog(
        commandId: Int,
        commandDisplayName: String,
    ) {
        val dialogView = layoutInflater.inflate(R.layout.dialog_bind_button, null)
        val textInstruction = dialogView.findViewById<TextView>(R.id.text_instruction)
        val chipGroup = dialogView.findViewById<com.google.android.material.chip.ChipGroup>(R.id.chip_group_buttons)
        val textTimer = dialogView.findViewById<TextView>(R.id.text_timer)

        // Initialize bind dialog state
        isBindDialogOpen = true
        currentBindingCommandId = commandId
        currentBindingCommandDisplay = commandDisplayName
        bindDialogChipGroup = chipGroup
        bindDialogTimer = textTimer
        detectedButtons.clear()

        textInstruction.text = "Press button(s) on your controller..."

        val dialog =
            MaterialAlertDialogBuilder(this)
                .setTitle("Bind: $commandDisplayName")
                .setView(dialogView)
                .setNegativeButton("Cancel") { _, _ ->
                    cleanupBindDialogState()
                }.setOnDismissListener {
                    cleanupBindDialogState()
                }.create()

        currentDialog = dialog
        dialog.show()

        // Request focus on dialog view and force it to stay focused (Eden's approach)
        dialogView.requestFocus()
        dialogView.setOnFocusChangeListener { v, hasFocus ->
            if (!hasFocus) v.requestFocus()
        }

        // Set key listener on dialog to intercept gamepad buttons
        dialog.setOnKeyListener { _, keyCode, keyEvent ->
            Log.d(TAG, "Dialog key listener: keyCode=$keyCode, action=${keyEvent.action}, button=${getButtonName(keyCode)}")
            if (isGamepadButton(keyCode)) {
                handleBindDialogInput(keyEvent)
                true // Consume event
            } else {
                false // Let dialog handle non-gamepad keys (back button)
            }
        }
    }

    /**
     * Override SDL's orientation method to force landscape mode.
     * SDL calls this from native code and tries to change orientation dynamically.
     * We override it to always enforce landscape, ignoring SDL's requests.
     */
    override fun setOrientationBis(
        w: Int,
        h: Int,
        resizable: Boolean,
        hint: String?,
    ) {
        // Always force sensor landscape orientation (allows 180° rotation, blocks portrait)
        Log.d(TAG, "setOrientationBis called with w=$w, h=$h, resizable=$resizable, hint=$hint - forcing SENSOR_LANDSCAPE")
        requestedOrientation = android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
    }
}
