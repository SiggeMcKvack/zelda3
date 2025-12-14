package com.dishii.zelda3

import android.content.Intent
import android.content.SharedPreferences
import android.content.res.Configuration
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.os.CountDownTimer
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.ParcelFileDescriptor
import android.util.Log
import android.view.Gravity
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.AutoCompleteTextView
import android.widget.Button
import android.widget.ImageView
import android.widget.Spinner
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.core.view.GravityCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.documentfile.provider.DocumentFile
import androidx.drawerlayout.widget.DrawerLayout
import androidx.lifecycle.lifecycleScope
import com.dishii.zelda3.overlay.InputOverlay
import com.dishii.zelda3.overlay.model.OverlayLayout
import com.dishii.zelda3.util.ConfigManager
import com.dishii.zelda3.util.showToast
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.navigation.NavigationView
import com.google.android.material.slider.Slider
import com.google.android.material.switchmaterial.SwitchMaterial
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.libsdl.app.SDLActivity
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.nio.ByteBuffer

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
            kotlinx.coroutines.GlobalScope.launch {
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

    // Language display name mapping (matches launcher_ui.c get_language_display_name)
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
            "sv" to "Swedish",
            "redux" to "English (Redux)",
            "retrans-kal" to "English (Kaleidoscope)",
        )

    private fun getLanguageDisplayName(code: String): String = languageDisplayNames[code] ?: code.uppercase()

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

    private lateinit var drawerLayout: DrawerLayout
    private lateinit var overlayView: View
    private lateinit var inputOverlay: InputOverlay
    private lateinit var doneControlConfigButton: Button
    private var currentDialog: AlertDialog? = null

    // Track Start+Select combo for opening drawer (for gamepad users)
    private var isStartPressed = false
    private var isSelectPressed = false

    // Bind dialog state management
    private var isBindDialogOpen = false
    private val detectedButtons = mutableSetOf<Int>()
    private var capturedButtons = mutableSetOf<Int>() // Captured when first button is released
    private var bindDialogChipGroup: com.google.android.material.chip.ChipGroup? = null
    private var bindDialogTimer: TextView? = null
    private var currentBindingCommandId: Int? = null // C command ID
    private var currentBindingCommandDisplay: String? = null

    // Controller settings dialog state
    private var controllerSettingsDialog: AlertDialog? = null // Keep reference to settings dialog
    private var controllerSettingsRecyclerView: androidx.recyclerview.widget.RecyclerView? = null
    private var controllerBindingAdapter: ControllerBindingAdapter? = null
    private var savedScrollPosition: Int = 0

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
        setupDrawer()
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
    private fun performMigration(
        filesToMigrate: List<String>,
        filesToSkip: Set<String>,
        internalSavesDir: File,
        externalSavesDir: DocumentFile,
    ) {
        kotlinx.coroutines.GlobalScope.launch {
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
     * Inflates the overlay layout (DrawerLayout with InputOverlay).
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

        drawerLayout = overlayView as DrawerLayout

        // Get FrameLayout (main content area) from DrawerLayout
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

    /**
     * Sets up navigation drawer with menu item listeners and drawer open/close behavior.
     */
    private fun setupDrawer() {
        Log.d(TAG, "setupDrawer() called")
        val navigationView = drawerLayout.findViewById<NavigationView>(R.id.nav_view)
        Log.d(TAG, "NavigationView found: ${navigationView != null}")

        // Handle display cutouts and system bars (push drawer content below camera)
        navigationView?.let { navView ->
            ViewCompat.setOnApplyWindowInsetsListener(navView) { v, insets ->
                val bars =
                    insets.getInsets(
                        WindowInsetsCompat.Type.systemBars() or
                            WindowInsetsCompat.Type.displayCutout(),
                    )

                v.updatePadding(
                    left = bars.left,
                    top = bars.top,
                    right = bars.right,
                    bottom = bars.bottom,
                )

                WindowInsetsCompat.CONSUMED
            }
        }

        // Drawer listener for touch event routing
        drawerLayout.addDrawerListener(
            object : DrawerLayout.DrawerListener {
                override fun onDrawerOpened(drawerView: View) {
                    Log.d(TAG, "onDrawerOpened - setting inputOverlay drawerOpen=true")
                    inputOverlay.setDrawerOpen(true)
                }

                override fun onDrawerClosed(drawerView: View) {
                    Log.d(TAG, "onDrawerClosed - setting inputOverlay drawerOpen=false")
                    inputOverlay.setDrawerOpen(false)
                }

                override fun onDrawerSlide(
                    drawerView: View,
                    slideOffset: Float,
                ) {}

                override fun onDrawerStateChanged(newState: Int) {}
            },
        )

        // Check for multiple languages and show Language menu item if available
        Thread {
            val externalDir = getExternalFilesDir(null)?.absolutePath ?: ""
            val availableLanguages = nativeGetAvailableLanguages(externalDir)
            Log.d(
                TAG,
                "setupDrawer: availableLanguages = ${availableLanguages?.joinToString() ?: "null"}, count = ${availableLanguages?.size ?: 0}",
            )

            if (availableLanguages != null && availableLanguages.size > 1) {
                runOnUiThread {
                    val languageMenuItem = navigationView?.menu?.findItem(R.id.nav_language)
                    languageMenuItem?.isVisible = true
                    Log.d(TAG, "setupDrawer: Language menu item set to visible")
                }
            }
        }.start()

        navigationView?.setNavigationItemSelectedListener { item ->
            val id = item.itemId
            Log.d(TAG, "Menu item selected with ID: $id")

            when (id) {
                R.id.nav_game -> {
                    Log.d(TAG, "Return to game selected - closing drawer")
                    drawerLayout.closeDrawers()
                }

                R.id.nav_pause -> {
                    Log.d(TAG, "Pause button selected - toggling pause")
                    nativeTogglePause()
                    Log.d(TAG, "Game pause state: ${nativeIsPaused()}")
                }

                R.id.nav_save_state -> {
                    Log.d(TAG, "Save state selected - closing drawer and showing dialog")
                    drawerLayout.closeDrawers()
                    Handler(Looper.getMainLooper()).postDelayed({ showSaveStateDialog() }, 300)
                }

                R.id.nav_load_state -> {
                    Log.d(TAG, "Load state selected - closing drawer and showing dialog")
                    drawerLayout.closeDrawers()
                    Handler(Looper.getMainLooper()).postDelayed({ showLoadStateDialog() }, 300)
                }

                R.id.nav_gameplay_settings -> {
                    Log.d(TAG, "Gameplay settings selected - closing drawer and showing dialog")
                    drawerLayout.closeDrawers()
                    Handler(Looper.getMainLooper()).postDelayed({ showGameplaySettingsDialog() }, 300)
                }

                R.id.nav_controller_settings -> {
                    Log.d(TAG, "Controller settings selected - closing drawer and showing dialog")
                    drawerLayout.closeDrawers()
                    Handler(Looper.getMainLooper()).postDelayed({ showControllerSettingsDialog() }, 300)
                }

                R.id.nav_graphics -> {
                    Log.d(TAG, "Graphics options selected - closing drawer (game stays paused)")
                    drawerLayout.closeDrawers()
                    Handler(Looper.getMainLooper()).postDelayed({ showGraphicsOptionsDialog() }, 300)
                }

                R.id.nav_audio -> {
                    Log.d(TAG, "Audio options selected - closing drawer (game stays paused)")
                    drawerLayout.closeDrawers()
                    Handler(Looper.getMainLooper()).postDelayed({ showAudioOptionsDialog() }, 300)
                }

                R.id.nav_language -> {
                    Log.d(TAG, "Language selected - closing drawer and showing dialog")
                    drawerLayout.closeDrawers()
                    Handler(Looper.getMainLooper()).postDelayed({ showLanguageDialog() }, 300)
                }

                R.id.nav_overlay_options -> {
                    Log.d(TAG, "Overlay options selected - closing drawer (game stays paused)")
                    drawerLayout.closeDrawers()
                    Handler(Looper.getMainLooper()).postDelayed({ showOverlayOptionsDialog() }, 300)
                }

                R.id.nav_reset_assets -> {
                    Log.d(TAG, "Reset assets selected - closing drawer and scheduling dialog")
                    drawerLayout.closeDrawers()
                    Handler(Looper.getMainLooper()).postDelayed({
                        Log.d(TAG, "Delayed runnable executing - about to show reset assets dialog")
                        showResetAssetsDialog()
                    }, 300)
                }

                R.id.nav_exit -> {
                    Log.d(TAG, "Exit selected - finishing activity and killing process")
                    finish()
                    android.os.Process.killProcess(android.os.Process.myPid())
                }
            }
            true
        } ?: run {
            Log.e(TAG, "NavigationView is NULL! Cannot set item selected listener")
        }

        Log.d(TAG, "setupDrawer() completed")
    }

    /**
     * Shows overlay options dialog with master enable/disable toggle and settings.
     */
    private fun showOverlayOptionsDialog() {
        Log.d(TAG, "showOverlayOptionsDialog() called")

        try {
            val dialogView = layoutInflater.inflate(R.layout.dialog_overlay_options, null)

            // Get views
            val switchEnableOverlay = dialogView.findViewById<SwitchMaterial>(R.id.switch_enable_overlay)
            val optionEditOverlay = dialogView.findViewById<TextView>(R.id.option_edit_overlay)
            val optionAdjustControls = dialogView.findViewById<TextView>(R.id.option_adjust_controls)
            val optionToggleControls = dialogView.findViewById<TextView>(R.id.option_toggle_controls)
            val optionResetOverlay = dialogView.findViewById<TextView>(R.id.option_reset_overlay)
            val switchDpadSlide = dialogView.findViewById<SwitchMaterial>(R.id.switch_dpad_slide)
            val switchHapticFeedback = dialogView.findViewById<SwitchMaterial>(R.id.switch_haptic_feedback)

            // Set initial states
            switchEnableOverlay.isChecked = inputOverlay.getOverlayEnabled()
            switchDpadSlide.isChecked = inputOverlay.getDpadSlideEnabled()
            switchHapticFeedback.isChecked = inputOverlay.getHapticFeedbackEnabled()

            // Helper to update UI based on overlay enabled state
            val updateOptionsState = {
                val enabled = switchEnableOverlay.isChecked
                val alpha = if (enabled) 1.0f else 0.38f

                optionEditOverlay.isEnabled = enabled
                optionAdjustControls.isEnabled = enabled
                optionToggleControls.isEnabled = enabled
                optionResetOverlay.isEnabled = enabled
                switchDpadSlide.isEnabled = enabled
                switchHapticFeedback.isEnabled = enabled

                optionEditOverlay.alpha = alpha
                optionAdjustControls.alpha = alpha
                optionToggleControls.alpha = alpha
                optionResetOverlay.alpha = alpha
                switchDpadSlide.alpha = alpha
                switchHapticFeedback.alpha = alpha
            }

            updateOptionsState()

            // Create dialog
            val dialog =
                MaterialAlertDialogBuilder(this)
                    .setTitle("Overlay options")
                    .setView(dialogView)
                    .setNegativeButton("Done") { _, _ ->
                        drawerLayout.openDrawer(Gravity.START)
                    }.create()

            // Click handlers for options
            optionEditOverlay.setOnClickListener {
                dialog.dismiss()
                enterEditMode()
                drawerLayout.openDrawer(Gravity.START)
            }

            optionAdjustControls.setOnClickListener {
                dialog.dismiss()
                showAdjustControlsDialog()
            }

            optionToggleControls.setOnClickListener {
                dialog.dismiss()
                showToggleControlsDialog()
            }

            optionResetOverlay.setOnClickListener {
                dialog.dismiss()
                resetOverlay()
                drawerLayout.openDrawer(Gravity.START)
            }

            // Switch listeners (don't close dialog)
            switchEnableOverlay.setOnCheckedChangeListener { _, checked ->
                inputOverlay.setOverlayEnabled(checked)
                updateOptionsState()
            }

            switchDpadSlide.setOnCheckedChangeListener { _, checked ->
                inputOverlay.setDpadSlide(checked)
            }

            switchHapticFeedback.setOnCheckedChangeListener { _, checked ->
                inputOverlay.setHapticFeedback(checked)
            }

            Log.d(TAG, "Showing dialog")
            dialog.show()
            Log.d(TAG, "Dialog shown successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error showing overlay options dialog", e)
            showToast("Error: ${e.message}", Toast.LENGTH_LONG)
        }
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
    }

    private fun showAdjustControlsDialog() {
        val dialogView = layoutInflater.inflate(R.layout.dialog_overlay_adjust, null)

        val scaleSlider = dialogView.findViewById<Slider>(R.id.input_scale_slider)
        val scaleValue = dialogView.findViewById<TextView>(R.id.input_scale_value)
        val opacitySlider = dialogView.findViewById<Slider>(R.id.input_opacity_slider)
        val opacityValue = dialogView.findViewById<TextView>(R.id.input_opacity_value)

        scaleSlider.value = 0f
        opacitySlider.value = 100f
        scaleValue.text = "100%"
        opacityValue.text = "100%"

        scaleSlider.addOnChangeListener { _, value, _ ->
            val percent = (((value + 50) / 150) * 100).toInt()
            scaleValue.text = "$percent%"
            inputOverlay.setOverlayScale(value.toInt())
        }

        opacitySlider.addOnChangeListener { _, value, _ ->
            opacityValue.text = "${value.toInt()}%"
            inputOverlay.setOverlayOpacity(value.toInt())
        }

        currentDialog =
            MaterialAlertDialogBuilder(this)
                .setTitle("Adjust controls")
                .setView(dialogView)
                .setPositiveButton("Close") { _, _ ->
                    showOverlayOptionsDialog()
                }.show()
    }

    private fun showToggleControlsDialog() {
        val dialogView = layoutInflater.inflate(R.layout.dialog_toggle_controls, null)

        // Get all switches
        val switches =
            listOf(
                dialogView.findViewById<SwitchMaterial>(R.id.switch_button_a),
                dialogView.findViewById<SwitchMaterial>(R.id.switch_button_b),
                dialogView.findViewById<SwitchMaterial>(R.id.switch_button_x),
                dialogView.findViewById<SwitchMaterial>(R.id.switch_button_y),
                dialogView.findViewById<SwitchMaterial>(R.id.switch_button_l),
                dialogView.findViewById<SwitchMaterial>(R.id.switch_button_r),
                dialogView.findViewById<SwitchMaterial>(R.id.switch_button_start),
                dialogView.findViewById<SwitchMaterial>(R.id.switch_button_select),
                dialogView.findViewById<SwitchMaterial>(R.id.switch_dpad),
            )

        // Get current visibility states
        val visibility = inputOverlay.getControlVisibility()

        // Set initial states and listeners
        switches.forEachIndexed { index, switch ->
            switch.isChecked = visibility[index]
            switch.setOnCheckedChangeListener { _, checked ->
                inputOverlay.setControlVisibility(index, checked)
            }
        }

        MaterialAlertDialogBuilder(this)
            .setTitle("Toggle controls")
            .setView(dialogView)
            .setPositiveButton("Done") { _, _ ->
                showOverlayOptionsDialog()
            }.show()
    }

    private fun resetOverlay() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Reset overlay")
            .setMessage("Reset all button positions, scales, and settings to defaults?")
            .setPositiveButton("Reset") { _, _ ->
                inputOverlay.resetOverlay()
                showToast("Overlay reset to defaults")
            }.setNegativeButton("Cancel", null)
            .show()
    }

    private fun showResetAssetsDialog() {
        Log.d(TAG, "showResetAssetsDialog() called")
        try {
            Log.d(TAG, "Creating delete assets dialog")
            val dialog =
                MaterialAlertDialogBuilder(this)
                    .setTitle("Delete Assets File")
                    .setMessage(
                        "This will delete zelda3_assets.dat and restart the app.\n\nYou will need to select the assets file again. Your saves and config will NOT be deleted.",
                    ).setPositiveButton("Delete file") { _, _ ->
                        getExternalFilesDir(null)?.let { externalDir ->
                            val assetsFile = File(externalDir, "zelda3_assets.dat")
                            if (assetsFile.exists()) {
                                if (assetsFile.delete()) {
                                    showToast("Assets file deleted. Restarting...")
                                    val intent =
                                        Intent(this, LauncherActivity::class.java).apply {
                                            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                                        }
                                    startActivity(intent)
                                    finishAndRemoveTask()
                                    System.exit(0)
                                } else {
                                    showToast("Failed to delete assets file", Toast.LENGTH_LONG)
                                }
                            } else {
                                showToast("Assets file not found")
                            }
                        }
                    }.setNegativeButton("Cancel", null)
                    .show()

            // Style positive button as filled red with trash icon
            dialog.getButton(AlertDialog.BUTTON_POSITIVE)?.apply {
                // Make button filled with error color (red for destructive action)
                val errorColor = resources.getColorStateList(R.color.md_theme_error, theme)
                val onErrorColor = resources.getColorStateList(R.color.md_theme_onError, theme)
                backgroundTintList = errorColor
                setTextColor(onErrorColor)

                // Add trash icon with matching color
                setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_delete, 0, 0, 0)
                compoundDrawablePadding = resources.getDimensionPixelSize(android.R.dimen.app_icon_size) / 4
                setCompoundDrawableTintList(onErrorColor)
            }

            Log.d(TAG, "Delete assets dialog shown successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error showing delete assets dialog", e)
            showToast("Error: ${e.message}", Toast.LENGTH_LONG)
        }
    }

    /**
     * Shows audio options dialog for MSU management and low health beep toggle.
     */
    private fun showAudioOptionsDialog() {
        Log.d(TAG, "showAudioOptionsDialog() called")

        try {
            val dialogView = layoutInflater.inflate(R.layout.dialog_audio_options, null)

            // Get views
            val textMsuInfo = dialogView.findViewById<TextView>(R.id.text_msu_info)
            val textRestartWarning = dialogView.findViewById<TextView>(R.id.text_restart_warning)
            val switchDisableLowHealthBeep = dialogView.findViewById<SwitchMaterial>(R.id.switch_disable_low_health_beep)
            val switchEnableMsu = dialogView.findViewById<SwitchMaterial>(R.id.switch_enable_msu)
            val switchResumeMsu = dialogView.findViewById<SwitchMaterial>(R.id.switch_resume_msu)
            val sliderMsuVolume = dialogView.findViewById<Slider>(R.id.slider_msu_volume)
            val textVolumeValue = dialogView.findViewById<TextView>(R.id.text_volume_value)

            // Read current settings in background thread
            Thread {
                val disableLowHealthBeep = kotlinx.coroutines.runBlocking { readLowHealthBeepSetting() }
                val settings = kotlinx.coroutines.runBlocking { readMsuSettings() }
                val msuScanResult = kotlinx.coroutines.runBlocking { scanMsuFiles() }
                val format = kotlinx.coroutines.runBlocking { detectMsuFormat() }

                // Store originals for restart detection
                val originalDisableLowHealthBeep = disableLowHealthBeep
                val originalSettings = settings.copy()

                Log.d(TAG, "showAudioOptionsDialog: settings from readMsuSettings: $settings")
                Log.d(TAG, "showAudioOptionsDialog: msuScanResult = $msuScanResult, format = $format")

                // Update UI on main thread
                runOnUiThread {
                    // Update MSU info text
                    textMsuInfo.text =
                        if (msuScanResult.fileCount > 0) {
                            // Detect Deluxe if max track > 47 (original game has tracks 1-47)
                            val isDeluxe = msuScanResult.maxTrackNumber > 47
                            val formatName =
                                when {
                                    format == "MSU1" && isDeluxe -> "PCM Deluxe"
                                    format == "MSU1" -> "PCM"
                                    format == "OPUZ" && isDeluxe -> "Opuz Deluxe"
                                    format == "OPUZ" -> "Opuz"
                                    else -> "Unknown"
                                }
                            "${msuScanResult.fileCount} MSU track${if (msuScanResult.fileCount != 1) "s" else ""} detected ($formatName)"
                        } else {
                            "No MSU files detected"
                        }

                    // Set initial states
                    switchDisableLowHealthBeep.isChecked = disableLowHealthBeep
                    switchEnableMsu.isChecked = settings.enableMsu
                    switchEnableMsu.isEnabled = msuScanResult.fileCount > 0
                    switchResumeMsu.isChecked = settings.resumeMsu
                    sliderMsuVolume.value = settings.volume.toFloat()
                    textVolumeValue.text = "${settings.volume}%"

                    // Volume slider listener
                    sliderMsuVolume.addOnChangeListener { _, value, _ ->
                        textVolumeValue.text = "${value.toInt()}%"
                    }

                    // Create dialog
                    val dialog =
                        MaterialAlertDialogBuilder(this@MainActivity)
                            .setTitle("Audio options")
                            .setView(dialogView)
                            .setNegativeButton("Done") { _, _ ->
                                Thread {
                                    // Save settings
                                    val newDisableLowHealthBeep = switchDisableLowHealthBeep.isChecked
                                    kotlinx.coroutines.runBlocking { updateLowHealthBeepSetting(newDisableLowHealthBeep) }

                                    val newEnableMsu = switchEnableMsu.isChecked
                                    val newVolume = sliderMsuVolume.value.toInt()
                                    val newResumeMsu = switchResumeMsu.isChecked

                                    // Determine AudioFreq and MSU flags based on format
                                    var newAudioFreq = settings.audioFreq
                                    var currentFormat: String? = null
                                    var msuFlags = 0 // 0=disabled, 1=PCM, 3=PCM Deluxe, 5=Opuz, 7=Opuz Deluxe
                                    if (newEnableMsu) {
                                        currentFormat = kotlinx.coroutines.runBlocking { detectMsuFormat() }
                                        val msuScan = kotlinx.coroutines.runBlocking { scanMsuFiles() }
                                        val isDeluxe = msuScan.maxTrackNumber > 47
                                        val isOpuz = currentFormat == "OPUZ"

                                        currentFormat?.let {
                                            newAudioFreq = if (it == "MSU1") 44100 else 48000
                                        }

                                        // Build flags: kMsuEnabled_Msu=1, kMsuEnabled_MsuDeluxe=2, kMsuEnabled_Opuz=4
                                        msuFlags = 1 // kMsuEnabled_Msu
                                        if (isDeluxe) msuFlags = msuFlags or 2 // kMsuEnabled_MsuDeluxe
                                        if (isOpuz) msuFlags = msuFlags or 4 // kMsuEnabled_Opuz

                                        Log.d(TAG, "MSU flags: $msuFlags (isDeluxe=$isDeluxe, isOpuz=$isOpuz)")
                                    }

                                    kotlinx.coroutines.runBlocking {
                                        updateMsuSettings(newEnableMsu, newVolume, newResumeMsu, newAudioFreq, currentFormat)
                                    }

                                    // Check if AudioFreq changed (requires restart)
                                    val audioFreqChanged = (originalSettings.audioFreq != newAudioFreq)

                                    // Check if hot-reloadable settings changed
                                    val hotReloadableChanged =
                                        (originalDisableLowHealthBeep != newDisableLowHealthBeep) ||
                                            (originalSettings.enableMsu != newEnableMsu) ||
                                            (originalSettings.volume != newVolume) ||
                                            (originalSettings.resumeMsu != newResumeMsu)

                                    // Debug logging
                                    Log.d(TAG, "=== HOT RELOAD DEBUG ===")
                                    Log.d(
                                        TAG,
                                        "Original: enableMsu=${originalSettings.enableMsu}, volume=${originalSettings.volume}, resumeMsu=${originalSettings.resumeMsu}, audioFreq=${originalSettings.audioFreq}, beep=$originalDisableLowHealthBeep",
                                    )
                                    Log.d(
                                        TAG,
                                        "New: enableMsu=$newEnableMsu, volume=$newVolume, resumeMsu=$newResumeMsu, audioFreq=$newAudioFreq, msuFlags=$msuFlags, beep=$newDisableLowHealthBeep",
                                    )
                                    Log.d(TAG, "audioFreqChanged=$audioFreqChanged, hotReloadableChanged=$hotReloadableChanged")

                                    runOnUiThread {
                                        if (hotReloadableChanged && !audioFreqChanged) {
                                            // Hot-reload settings immediately (no restart needed)
                                            // Pass MSU flags directly (not just 0/1)
                                            Log.d(TAG, ">>> CALLING HOT RELOAD with msuFlags=$msuFlags <<<")
                                            nativeReloadAudioConfig(
                                                enableMsu = msuFlags,
                                                msuVolume = newVolume,
                                                disableLowHealthBeep = if (newDisableLowHealthBeep) 1 else 0,
                                            )
                                            Log.d(TAG, "Audio settings hot-reloaded successfully")
                                        } else if (audioFreqChanged) {
                                            // AudioFreq change requires full restart
                                            Log.d(TAG, ">>> AudioFreq changed, showing restart dialog <<<")
                                            showRestartRequiredDialog()
                                        } else {
                                            Log.d(TAG, ">>> No changes detected, skipping reload <<<")
                                        }

                                        // Return to drawer
                                        drawerLayout.openDrawer(Gravity.START)
                                    }
                                }.start()
                            }.create()

                    Log.d(TAG, "Showing audio options dialog")
                    dialog.show()
                    Log.d(TAG, "Audio options dialog shown successfully")
                }
            }.start()
        } catch (e: Exception) {
            Log.e(TAG, "Error showing audio options dialog", e)
            showToast("Error: ${e.message}", Toast.LENGTH_LONG)
        }
    }

    private fun showRestartRequiredDialog() {
        val dialog =
            MaterialAlertDialogBuilder(this)
                .setTitle("Restart Required")
                .setMessage("Audio settings have changed. Please restart the game for changes to take effect.")
                .setNegativeButton("Later", null)
                .setPositiveButton("Restart app") { _, _ ->
                    // Restart the app
                    val intent =
                        Intent(this, LauncherActivity::class.java).apply {
                            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                        }
                    startActivity(intent)
                    finishAndRemoveTask()
                    System.exit(0)
                }.show()

        // Style positive button as filled with icon
        dialog.getButton(AlertDialog.BUTTON_POSITIVE)?.apply {
            // Make button filled (solid primary color background)
            val primaryColor = resources.getColorStateList(R.color.md_theme_primary, theme)
            val onPrimaryColor = resources.getColorStateList(R.color.md_theme_onPrimary, theme)
            backgroundTintList = primaryColor
            setTextColor(onPrimaryColor)

            // Add restart icon with matching color
            setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_restart, 0, 0, 0)
            compoundDrawablePadding = resources.getDimensionPixelSize(android.R.dimen.app_icon_size) / 4
            setCompoundDrawableTintList(onPrimaryColor)
        }
    }

    /**
     * Shows language selection dialog with radio buttons.
     */
    private fun showLanguageSelectionDialog(
        availableLanguages: Array<String>,
        currentLanguage: String,
        onLanguageSelected: (String) -> Unit,
    ) {
        val dialogView = layoutInflater.inflate(R.layout.dialog_language_selection, null)
        val radioGroup = dialogView.findViewById<android.widget.RadioGroup>(R.id.radio_group_languages)

        // Sort languages: US first, then alphabetically by display name
        val sortedLanguages =
            availableLanguages.sortedWith(
                compareBy(
                    { it != "us" }, // US comes first (false < true)
                    { getLanguageDisplayName(it) }, // Then alphabetically
                ),
            )

        // Create radio buttons
        sortedLanguages.forEach { langCode ->
            val radioButton =
                android.widget.RadioButton(this).apply {
                    id = View.generateViewId()
                    text = getLanguageDisplayName(langCode)
                    tag = langCode
                    isChecked = (langCode == currentLanguage)
                    val horizontalPadding = (24 * resources.displayMetrics.density).toInt()
                    val verticalPadding = (16 * resources.displayMetrics.density).toInt()
                    setPadding(horizontalPadding, verticalPadding, horizontalPadding, verticalPadding)
                    textSize = 16f
                }
            radioGroup.addView(radioButton)
        }

        val dialog =
            MaterialAlertDialogBuilder(this)
                .setTitle(R.string.language)
                .setView(dialogView)
                .setNegativeButton(android.R.string.cancel, null)
                .setPositiveButton(R.string.done) { _, _ ->
                    val selectedId = radioGroup.checkedRadioButtonId
                    if (selectedId != -1) {
                        val selectedButton = radioGroup.findViewById<android.widget.RadioButton>(selectedId)
                        val selectedLanguage = selectedButton.tag as String
                        if (selectedLanguage != currentLanguage) {
                            onLanguageSelected(selectedLanguage)
                        }
                    }
                }.create()

        dialog.show()

        // Style Done button as filled
        dialog.getButton(AlertDialog.BUTTON_POSITIVE)?.apply {
            val primaryColor = resources.getColorStateList(R.color.md_theme_primary, theme)
            val onPrimaryColor = resources.getColorStateList(R.color.md_theme_onPrimary, theme)
            backgroundTintList = primaryColor
            setTextColor(onPrimaryColor)
        }
    }

    /**
     * Shows language dialog from navigation drawer.
     * Fetches available languages and current setting, then shows selection dialog.
     */
    private fun showLanguageDialog() {
        Log.d(TAG, "showLanguageDialog() called")

        Thread {
            val externalDir = getExternalFilesDir(null)?.absolutePath ?: ""
            val availableLanguages = nativeGetAvailableLanguages(externalDir)
            val currentLanguage = kotlinx.coroutines.runBlocking { readLanguageSetting() }

            Log.d(TAG, "showLanguageDialog: availableLanguages = ${availableLanguages?.joinToString() ?: "null"}")
            Log.d(TAG, "showLanguageDialog: currentLanguage = $currentLanguage")

            if (availableLanguages == null || availableLanguages.size <= 1) {
                runOnUiThread {
                    Toast.makeText(this, "Only one language available", Toast.LENGTH_SHORT).show()
                }
                return@Thread
            }

            runOnUiThread {
                showLanguageSelectionDialog(availableLanguages, currentLanguage) { newLanguage ->
                    // Save setting and show restart dialog
                    Thread {
                        kotlinx.coroutines.runBlocking { updateLanguageSetting(newLanguage) }
                        runOnUiThread {
                            showRestartRequiredDialog()
                        }
                    }.start()
                }
            }
        }.start()
    }

    /**
     * Shows graphics options dialog for renderer selection.
     */
    private fun showGraphicsOptionsDialog() {
        Log.d(TAG, "showGraphicsOptionsDialog() called")

        try {
            val dialogView = layoutInflater.inflate(R.layout.dialog_graphics_options, null)

            // Get views - Renderer
            val textRestartWarning = dialogView.findViewById<TextView>(R.id.text_restart_warning)
            val toggleRenderer = dialogView.findViewById<com.google.android.material.button.MaterialButtonToggleGroup>(R.id.toggle_renderer)
            val buttonSdl = dialogView.findViewById<com.google.android.material.button.MaterialButton>(R.id.button_renderer_sdl)
            val buttonOpenGlEs = dialogView.findViewById<com.google.android.material.button.MaterialButton>(R.id.button_renderer_opengl_es)
            val buttonVulkan = dialogView.findViewById<com.google.android.material.button.MaterialButton>(R.id.button_renderer_vulkan)
            val textRendererInfo = dialogView.findViewById<TextView>(R.id.text_renderer_info)

            // Get views - Aspect ratio dropdown
            val dropdownAspectRatio = dialogView.findViewById<AutoCompleteTextView>(R.id.dropdown_aspect_ratio)

            // Get views - Display toggles
            val switchExtendY = dialogView.findViewById<SwitchMaterial>(R.id.switch_extend_y)
            val switchDisableFrameDelay = dialogView.findViewById<SwitchMaterial>(R.id.switch_disable_frame_delay)
            val switchNewRenderer = dialogView.findViewById<SwitchMaterial>(R.id.switch_new_renderer)
            val switchEnhancedMode7 = dialogView.findViewById<SwitchMaterial>(R.id.switch_enhanced_mode7)
            val switchDimFlashes = dialogView.findViewById<SwitchMaterial>(R.id.switch_dim_flashes)

            // Get views - Shader
            val layoutShaderSection = dialogView.findViewById<android.widget.LinearLayout>(R.id.layout_shader_section)
            val textShaderPath = dialogView.findViewById<TextView>(R.id.text_shader_path)
            val buttonShaderBrowse = dialogView.findViewById<com.google.android.material.button.MaterialButton>(R.id.button_shader_browse)
            val buttonShaderClear = dialogView.findViewById<com.google.android.material.button.MaterialButton>(R.id.button_shader_clear)

            // Aspect ratio options
            val aspectRatios = arrayOf("8:7", "4:3", "3:2", "16:9", "16:10", "18:9")
            val adapter = ArrayAdapter(this, R.layout.list_item, aspectRatios)
            dropdownAspectRatio.setAdapter(adapter)

            // Read all settings in background thread
            Thread {
                val currentRenderer = kotlinx.coroutines.runBlocking { readRendererSetting() }
                val originalRenderer = currentRenderer

                // Read aspect ratio settings
                val aspectSettings = kotlinx.coroutines.runBlocking { readExtendedAspectRatio() }
                val originalAspectRatio = aspectSettings.aspectRatio
                val originalExtendY = aspectSettings.extendY

                // Read boolean settings using ConfigManager
                val originalDisableFrameDelay =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "General", "DisableFrameDelay", false)
                    }
                val originalNewRenderer =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Graphics", "NewRenderer", true)
                    }
                val originalEnhancedMode7 =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Graphics", "EnhancedMode7", true)
                    }
                val originalDimFlashes =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Graphics", "DimFlashes", false)
                    }
                val originalShader =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readString(this@MainActivity, "Graphics", "Shader", "")
                    }
                var selectedShader = originalShader

                Log.d(
                    TAG,
                    "showGraphicsOptionsDialog: renderer=$currentRenderer, aspectRatio=$originalAspectRatio, extendY=$originalExtendY, shader=$originalShader",
                )
                Log.d(
                    TAG,
                    "showGraphicsOptionsDialog: disableFrameDelay=$originalDisableFrameDelay, newRenderer=$originalNewRenderer, enhancedMode7=$originalEnhancedMode7, dimFlashes=$originalDimFlashes",
                )

                // Update UI on main thread
                runOnUiThread {
                    // Update info text based on selection
                    val updateInfoText = { renderer: String ->
                        textRendererInfo.text =
                            when (renderer) {
                                "SDL" -> "SDL: Software or hardware accelerated rendering (default)"
                                "OpenGL ES" -> "OpenGL ES: Hardware accelerated 3D graphics API"
                                "Vulkan" -> "Vulkan: Next-generation graphics API (requires compatible device)"
                                else -> ""
                            }
                    }

                    // Shader UI helpers
                    val updateShaderUI = { path: String ->
                        if (path.isNotEmpty()) {
                            textShaderPath.text = path.substringAfterLast("/")
                            buttonShaderClear.visibility = android.view.View.VISIBLE
                        } else {
                            textShaderPath.text = "None"
                            buttonShaderClear.visibility = android.view.View.GONE
                        }
                    }

                    val updateShaderVisibility = { renderer: String ->
                        layoutShaderSection.visibility = if (renderer == "Vulkan") android.view.View.VISIBLE else android.view.View.GONE
                    }

                    // Shader browse button
                    buttonShaderBrowse.setOnClickListener {
                        pendingShaderCallback = { uri ->
                            if (uri != null) {
                                val relativePath = extractShaderRelativePath(uri)
                                if (relativePath != null) {
                                    selectedShader = relativePath
                                    runOnUiThread { updateShaderUI(selectedShader) }
                                } else {
                                    runOnUiThread {
                                        showToast("Invalid shader location. Place shaders in your zelda3 folder under shaders/")
                                    }
                                }
                            }
                        }
                        val intent =
                            android.content.Intent(android.content.Intent.ACTION_GET_CONTENT).apply {
                                addCategory(android.content.Intent.CATEGORY_OPENABLE)
                                type = "*/*"
                            }
                        @Suppress("DEPRECATION")
                        startActivityForResult(
                            android.content.Intent.createChooser(intent, "Select shader preset (.slangp)"),
                            REQUEST_SHADER_FILE,
                        )
                    }

                    // Shader clear button
                    buttonShaderClear.setOnClickListener {
                        selectedShader = ""
                        updateShaderUI(selectedShader)
                    }

                    // Toggle listener - add BEFORE setting initial selection
                    toggleRenderer.addOnButtonCheckedListener { _, checkedId, isChecked ->
                        if (isChecked) {
                            val rendererName =
                                when (checkedId) {
                                    R.id.button_renderer_sdl -> "SDL"
                                    R.id.button_renderer_opengl_es -> "OpenGL ES"
                                    R.id.button_renderer_vulkan -> "Vulkan"
                                    else -> "SDL"
                                }
                            Log.d(TAG, "showGraphicsOptionsDialog: Button checked changed to $rendererName")
                            updateInfoText(rendererName)
                            updateShaderVisibility(rendererName)
                        }
                    }

                    // Set initial checked button (handle both "OpenGL" and "OpenGL ES")
                    val checkedButtonId =
                        when (currentRenderer) {
                            "SDL" -> {
                                R.id.button_renderer_sdl
                            }

                            "OpenGL", "OpenGL ES" -> {
                                R.id.button_renderer_opengl_es
                            }

                            "Vulkan" -> {
                                R.id.button_renderer_vulkan
                            }

                            else -> {
                                Log.w(TAG, "Unknown renderer value: '$currentRenderer', defaulting to SDL")
                                R.id.button_renderer_sdl
                            }
                        }
                    Log.d(
                        TAG,
                        "showGraphicsOptionsDialog: Setting initial selection to button ID $checkedButtonId for renderer '$currentRenderer'",
                    )
                    toggleRenderer.check(checkedButtonId)
                    updateInfoText(currentRenderer)

                    // Set initial aspect ratio - find matching value or default to 16:9
                    val initialAspectRatio =
                        if (aspectRatios.contains(originalAspectRatio)) {
                            originalAspectRatio
                        } else {
                            "16:9"
                        }
                    dropdownAspectRatio.setText(initialAspectRatio, false)

                    // Set initial toggle states
                    switchExtendY.isChecked = originalExtendY
                    switchDisableFrameDelay.isChecked = originalDisableFrameDelay
                    switchNewRenderer.isChecked = originalNewRenderer
                    switchEnhancedMode7.isChecked = originalEnhancedMode7
                    switchDimFlashes.isChecked = originalDimFlashes

                    // Set initial shader state
                    updateShaderUI(selectedShader)
                    updateShaderVisibility(currentRenderer)

                    // Create dialog
                    val dialog =
                        MaterialAlertDialogBuilder(this@MainActivity)
                            .setTitle("Graphics options")
                            .setView(dialogView)
                            .setNegativeButton("Done") { _, _ ->
                                // Get selected renderer on UI thread
                                val selectedRenderer =
                                    when (toggleRenderer.checkedButtonId) {
                                        R.id.button_renderer_sdl -> {
                                            "SDL"
                                        }

                                        R.id.button_renderer_opengl_es -> {
                                            "OpenGL ES"
                                        }

                                        R.id.button_renderer_vulkan -> {
                                            "Vulkan"
                                        }

                                        else -> {
                                            Log.e(TAG, "Unknown button ID: ${toggleRenderer.checkedButtonId}, defaulting to SDL")
                                            "SDL"
                                        }
                                    }

                                // Get all new values
                                val selectedAspectRatio = dropdownAspectRatio.text.toString()
                                val selectedExtendY = switchExtendY.isChecked
                                val selectedDisableFrameDelay = switchDisableFrameDelay.isChecked
                                val selectedNewRenderer = switchNewRenderer.isChecked
                                val selectedEnhancedMode7 = switchEnhancedMode7.isChecked
                                val selectedDimFlashes = switchDimFlashes.isChecked

                                Log.d(TAG, "showGraphicsOptionsDialog: Done clicked")
                                Log.d(
                                    TAG,
                                    "showGraphicsOptionsDialog: renderer=$selectedRenderer, aspectRatio=$selectedAspectRatio, extendY=$selectedExtendY",
                                )

                                Thread {
                                    // Check if any setting changed
                                    val normalizedOriginal = if (originalRenderer == "OpenGL") "OpenGL ES" else originalRenderer
                                    val rendererChanged = selectedRenderer != normalizedOriginal
                                    val aspectRatioChanged = selectedAspectRatio != originalAspectRatio
                                    val extendYChanged = selectedExtendY != originalExtendY
                                    val disableFrameDelayChanged = selectedDisableFrameDelay != originalDisableFrameDelay
                                    val newRendererChanged = selectedNewRenderer != originalNewRenderer
                                    val enhancedMode7Changed = selectedEnhancedMode7 != originalEnhancedMode7
                                    val dimFlashesChanged = selectedDimFlashes != originalDimFlashes
                                    val shaderChanged = selectedShader != originalShader

                                    val anyChanged =
                                        rendererChanged || aspectRatioChanged || extendYChanged ||
                                            disableFrameDelayChanged || newRendererChanged ||
                                            enhancedMode7Changed || dimFlashesChanged || shaderChanged

                                    Log.d(TAG, "showGraphicsOptionsDialog: anyChanged=$anyChanged")

                                    if (anyChanged) {
                                        kotlinx.coroutines.runBlocking {
                                            // Save renderer if changed
                                            if (rendererChanged) {
                                                updateRendererSetting(selectedRenderer)
                                            }

                                            // Save aspect ratio and extend_y if changed
                                            if (aspectRatioChanged || extendYChanged) {
                                                writeExtendedAspectRatio(selectedAspectRatio, selectedExtendY)
                                            }

                                            // Save boolean settings if changed
                                            if (disableFrameDelayChanged) {
                                                ConfigManager.writeBool(
                                                    this@MainActivity,
                                                    "General",
                                                    "DisableFrameDelay",
                                                    selectedDisableFrameDelay,
                                                )
                                            }
                                            if (newRendererChanged) {
                                                ConfigManager.writeBool(this@MainActivity, "Graphics", "NewRenderer", selectedNewRenderer)
                                            }
                                            if (enhancedMode7Changed) {
                                                ConfigManager.writeBool(
                                                    this@MainActivity,
                                                    "Graphics",
                                                    "EnhancedMode7",
                                                    selectedEnhancedMode7,
                                                )
                                            }
                                            if (dimFlashesChanged) {
                                                ConfigManager.writeBool(this@MainActivity, "Graphics", "DimFlashes", selectedDimFlashes)
                                            }
                                            if (shaderChanged) {
                                                ConfigManager.writeString(this@MainActivity, "Graphics", "Shader", selectedShader)
                                            }
                                        }

                                        // Show restart dialog
                                        runOnUiThread {
                                            showRendererRestartDialog()
                                        }
                                    } else {
                                        Log.d(TAG, "No graphics settings changed, no restart needed")
                                    }

                                    // Return to drawer
                                    runOnUiThread {
                                        drawerLayout.openDrawer(Gravity.START)
                                    }
                                }.start()
                            }.create()

                    Log.d(TAG, "Showing graphics options dialog")
                    dialog.show()
                    Log.d(TAG, "Graphics options dialog shown successfully")
                }
            }.start()
        } catch (e: Exception) {
            Log.e(TAG, "Error showing graphics options dialog", e)
            showToast("Error: ${e.message}", Toast.LENGTH_LONG)
        }
    }

    private fun showRendererRestartDialog() {
        val dialog =
            MaterialAlertDialogBuilder(this)
                .setTitle("Restart Required")
                .setMessage("Graphics renderer has been changed. Please restart the game for changes to take effect.")
                .setNegativeButton("Later", null)
                .setPositiveButton("Restart app") { _, _ ->
                    // Restart the app
                    val intent =
                        Intent(this, LauncherActivity::class.java).apply {
                            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                        }
                    startActivity(intent)
                    finishAndRemoveTask()
                    System.exit(0)
                }.show()

        // Style positive button as filled with icon
        dialog.getButton(AlertDialog.BUTTON_POSITIVE)?.apply {
            // Make button filled (solid primary color background)
            val primaryColor = resources.getColorStateList(R.color.md_theme_primary, theme)
            val onPrimaryColor = resources.getColorStateList(R.color.md_theme_onPrimary, theme)
            backgroundTintList = primaryColor
            setTextColor(onPrimaryColor)

            // Add restart icon with matching color
            setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_restart, 0, 0, 0)
            compoundDrawablePadding = resources.getDimensionPixelSize(android.R.dimen.app_icon_size) / 4
            setCompoundDrawableTintList(onPrimaryColor)
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

    /**
     * Shows gameplay settings dialog with feature toggles.
     */
    private fun showGameplaySettingsDialog() {
        Log.d(TAG, "showGameplaySettingsDialog() called")

        try {
            val dialogView = layoutInflater.inflate(R.layout.dialog_gameplay_settings, null)

            // Get all switch references
            val switchAutosave = dialogView.findViewById<SwitchMaterial>(R.id.switch_autosave)
            val switchItemSwitchLR = dialogView.findViewById<SwitchMaterial>(R.id.switch_item_switch_lr)
            val switchItemSwitchLRLimit = dialogView.findViewById<SwitchMaterial>(R.id.switch_item_switch_lr_limit)
            val switchTurnWhileDashing = dialogView.findViewById<SwitchMaterial>(R.id.switch_turn_while_dashing)
            val switchSkipIntro = dialogView.findViewById<SwitchMaterial>(R.id.switch_skip_intro)
            val switchMirrorToDarkworld = dialogView.findViewById<SwitchMaterial>(R.id.switch_mirror_to_darkworld)
            val switchCollectItemsSword = dialogView.findViewById<SwitchMaterial>(R.id.switch_collect_items_sword)
            val spinnerBreakPotsSword = dialogView.findViewById<Spinner>(R.id.spinner_break_pots_sword)
            val swordLevels = arrayOf("Disabled", "Level 1 (Wooden)", "Level 2 (Master)", "Level 3 (Tempered)", "Level 4 (Golden)")
            spinnerBreakPotsSword.adapter = ArrayAdapter(this, android.R.layout.simple_spinner_dropdown_item, swordLevels)
            val switchMoreBombs = dialogView.findViewById<SwitchMaterial>(R.id.switch_more_bombs)
            val switchMoreRupees = dialogView.findViewById<SwitchMaterial>(R.id.switch_more_rupees)
            val switchCancelBird = dialogView.findViewById<SwitchMaterial>(R.id.switch_cancel_bird)
            val switchYellowItems = dialogView.findViewById<SwitchMaterial>(R.id.switch_yellow_items)
            val switchMiscBugs = dialogView.findViewById<SwitchMaterial>(R.id.switch_misc_bugs)
            val switchGameplayBugs = dialogView.findViewById<SwitchMaterial>(R.id.switch_gameplay_bugs)
            val switchPokemode = dialogView.findViewById<SwitchMaterial>(R.id.switch_pokemode)
            val switchZeldaHelps = dialogView.findViewById<SwitchMaterial>(R.id.switch_zelda_helps)

            // Read all settings in background thread
            Thread {
                // Read all settings
                val origAutosave =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "General", "Autosave", false)
                    }
                val origItemSwitchLR =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "ItemSwitchLR", false)
                    }
                val origItemSwitchLRLimit =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "ItemSwitchLRLimit", false)
                    }
                val origTurnWhileDashing =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "TurnWhileDashing", false)
                    }
                val origSkipIntro =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "SkipIntroOnKeypress", false)
                    }
                val origMirrorToDarkworld =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "MirrorToDarkworld", false)
                    }
                val origCollectItemsSword =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "CollectItemsWithSword", false)
                    }
                val origBreakPotsSword =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readInt(this@MainActivity, "Features", "BreakPotsWithSword", 0).coerceIn(0, 4)
                    }
                val origMoreBombs =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "MoreActiveBombs", false)
                    }
                val origMoreRupees =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "CarryMoreRupees", false)
                    }
                val origCancelBird =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "CancelBirdTravel", false)
                    }
                val origYellowItems =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "ShowMaxItemsInYellow", false)
                    }
                val origMiscBugs =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "MiscBugFixes", false)
                    }
                val origGameplayBugs =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "GameChangingBugFixes", false)
                    }
                val origPokemode =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "Pokemode", false)
                    }
                val origZeldaHelps =
                    kotlinx.coroutines.runBlocking {
                        ConfigManager.readBool(this@MainActivity, "Features", "PrincessZeldaHelps", false)
                    }

                Log.d(TAG, "showGameplaySettingsDialog: Read all settings")

                // Update UI on main thread
                runOnUiThread {
                    // Set initial toggle states
                    switchAutosave.isChecked = origAutosave
                    switchItemSwitchLR.isChecked = origItemSwitchLR
                    switchItemSwitchLRLimit.isChecked = origItemSwitchLRLimit
                    switchTurnWhileDashing.isChecked = origTurnWhileDashing
                    switchSkipIntro.isChecked = origSkipIntro
                    switchMirrorToDarkworld.isChecked = origMirrorToDarkworld
                    switchCollectItemsSword.isChecked = origCollectItemsSword
                    spinnerBreakPotsSword.setSelection(origBreakPotsSword)
                    switchMoreBombs.isChecked = origMoreBombs
                    switchMoreRupees.isChecked = origMoreRupees
                    switchCancelBird.isChecked = origCancelBird
                    switchYellowItems.isChecked = origYellowItems
                    switchMiscBugs.isChecked = origMiscBugs
                    switchGameplayBugs.isChecked = origGameplayBugs
                    switchPokemode.isChecked = origPokemode
                    switchZeldaHelps.isChecked = origZeldaHelps

                    // Create dialog
                    val dialog =
                        MaterialAlertDialogBuilder(this@MainActivity)
                            .setTitle("Gameplay settings")
                            .setView(dialogView)
                            .setNegativeButton("Done") { _, _ ->
                                Thread {
                                    // Get all new values
                                    val newAutosave = switchAutosave.isChecked
                                    val newItemSwitchLR = switchItemSwitchLR.isChecked
                                    val newItemSwitchLRLimit = switchItemSwitchLRLimit.isChecked
                                    val newTurnWhileDashing = switchTurnWhileDashing.isChecked
                                    val newSkipIntro = switchSkipIntro.isChecked
                                    val newMirrorToDarkworld = switchMirrorToDarkworld.isChecked
                                    val newCollectItemsSword = switchCollectItemsSword.isChecked
                                    val newBreakPotsSword = spinnerBreakPotsSword.selectedItemPosition
                                    val newMoreBombs = switchMoreBombs.isChecked
                                    val newMoreRupees = switchMoreRupees.isChecked
                                    val newCancelBird = switchCancelBird.isChecked
                                    val newYellowItems = switchYellowItems.isChecked
                                    val newMiscBugs = switchMiscBugs.isChecked
                                    val newGameplayBugs = switchGameplayBugs.isChecked
                                    val newPokemode = switchPokemode.isChecked
                                    val newZeldaHelps = switchZeldaHelps.isChecked

                                    // Check if any setting changed
                                    val anyChanged =
                                        (newAutosave != origAutosave) ||
                                            (newItemSwitchLR != origItemSwitchLR) ||
                                            (newItemSwitchLRLimit != origItemSwitchLRLimit) ||
                                            (newTurnWhileDashing != origTurnWhileDashing) ||
                                            (newSkipIntro != origSkipIntro) ||
                                            (newMirrorToDarkworld != origMirrorToDarkworld) ||
                                            (newCollectItemsSword != origCollectItemsSword) ||
                                            (newBreakPotsSword != origBreakPotsSword) ||
                                            (newMoreBombs != origMoreBombs) ||
                                            (newMoreRupees != origMoreRupees) ||
                                            (newCancelBird != origCancelBird) ||
                                            (newYellowItems != origYellowItems) ||
                                            (newMiscBugs != origMiscBugs) ||
                                            (newGameplayBugs != origGameplayBugs) ||
                                            (newPokemode != origPokemode) ||
                                            (newZeldaHelps != origZeldaHelps)

                                    Log.d(TAG, "showGameplaySettingsDialog: anyChanged=$anyChanged")

                                    if (anyChanged) {
                                        kotlinx.coroutines.runBlocking {
                                            // Save changed settings
                                            if (newAutosave != origAutosave) {
                                                ConfigManager.writeBool(this@MainActivity, "General", "Autosave", newAutosave)
                                            }
                                            if (newItemSwitchLR != origItemSwitchLR) {
                                                ConfigManager.writeBool(this@MainActivity, "Features", "ItemSwitchLR", newItemSwitchLR)
                                            }
                                            if (newItemSwitchLRLimit != origItemSwitchLRLimit) {
                                                ConfigManager.writeBool(
                                                    this@MainActivity,
                                                    "Features",
                                                    "ItemSwitchLRLimit",
                                                    newItemSwitchLRLimit,
                                                )
                                            }
                                            if (newTurnWhileDashing != origTurnWhileDashing) {
                                                ConfigManager.writeBool(
                                                    this@MainActivity,
                                                    "Features",
                                                    "TurnWhileDashing",
                                                    newTurnWhileDashing,
                                                )
                                            }
                                            if (newSkipIntro != origSkipIntro) {
                                                ConfigManager.writeBool(this@MainActivity, "Features", "SkipIntroOnKeypress", newSkipIntro)
                                            }
                                            if (newMirrorToDarkworld != origMirrorToDarkworld) {
                                                ConfigManager.writeBool(
                                                    this@MainActivity,
                                                    "Features",
                                                    "MirrorToDarkworld",
                                                    newMirrorToDarkworld,
                                                )
                                            }
                                            if (newCollectItemsSword != origCollectItemsSword) {
                                                ConfigManager.writeBool(
                                                    this@MainActivity,
                                                    "Features",
                                                    "CollectItemsWithSword",
                                                    newCollectItemsSword,
                                                )
                                            }
                                            if (newBreakPotsSword != origBreakPotsSword) {
                                                ConfigManager.writeInt(
                                                    this@MainActivity,
                                                    "Features",
                                                    "BreakPotsWithSword",
                                                    newBreakPotsSword,
                                                )
                                            }
                                            if (newMoreBombs != origMoreBombs) {
                                                ConfigManager.writeBool(this@MainActivity, "Features", "MoreActiveBombs", newMoreBombs)
                                            }
                                            if (newMoreRupees != origMoreRupees) {
                                                ConfigManager.writeBool(this@MainActivity, "Features", "CarryMoreRupees", newMoreRupees)
                                            }
                                            if (newCancelBird != origCancelBird) {
                                                ConfigManager.writeBool(this@MainActivity, "Features", "CancelBirdTravel", newCancelBird)
                                            }
                                            if (newYellowItems != origYellowItems) {
                                                ConfigManager.writeBool(
                                                    this@MainActivity,
                                                    "Features",
                                                    "ShowMaxItemsInYellow",
                                                    newYellowItems,
                                                )
                                            }
                                            if (newMiscBugs != origMiscBugs) {
                                                ConfigManager.writeBool(this@MainActivity, "Features", "MiscBugFixes", newMiscBugs)
                                            }
                                            if (newGameplayBugs != origGameplayBugs) {
                                                ConfigManager.writeBool(
                                                    this@MainActivity,
                                                    "Features",
                                                    "GameChangingBugFixes",
                                                    newGameplayBugs,
                                                )
                                            }
                                            if (newPokemode != origPokemode) {
                                                ConfigManager.writeBool(this@MainActivity, "Features", "Pokemode", newPokemode)
                                            }
                                            if (newZeldaHelps != origZeldaHelps) {
                                                ConfigManager.writeBool(this@MainActivity, "Features", "PrincessZeldaHelps", newZeldaHelps)
                                            }
                                        }

                                        // Show restart dialog
                                        runOnUiThread {
                                            showRendererRestartDialog()
                                        }
                                    } else {
                                        Log.d(TAG, "No gameplay settings changed, no restart needed")
                                    }

                                    // Return to drawer
                                    runOnUiThread {
                                        drawerLayout.openDrawer(Gravity.START)
                                    }
                                }.start()
                            }.create()

                    Log.d(TAG, "Showing gameplay settings dialog")
                    dialog.show()
                    Log.d(TAG, "Gameplay settings dialog shown successfully")
                }
            }.start()
        } catch (e: Exception) {
            Log.e(TAG, "Error showing gameplay settings dialog", e)
            showToast("Error: ${e.message}", Toast.LENGTH_LONG)
        }
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
        // CRITICAL: Intercept back button BEFORE SDL processes it
        if (event.keyCode == KeyEvent.KEYCODE_BACK && event.action == KeyEvent.ACTION_DOWN) {
            Log.d(TAG, "BACK key intercepted in dispatchKeyEvent")
            val isOpen = drawerLayout.isDrawerOpen(Gravity.START)
            Log.d(TAG, "Drawer is currently: ${if (isOpen) "OPEN" else "CLOSED"}")

            if (isOpen) {
                Log.d(TAG, "Closing drawer")
                drawerLayout.closeDrawers()
            } else {
                Log.d(TAG, "Opening drawer")
                drawerLayout.openDrawer(Gravity.START)
            }
            return true // Consume event - don't let SDL process it
        }

        // Track Start+Select combo for opening drawer (gamepad support)
        // Start = KEYCODE_N, Select = KEYCODE_B (from OverlayControl.kt)
        val keyCode = event.keyCode
        val isDown = event.action == KeyEvent.ACTION_DOWN
        val isUp = event.action == KeyEvent.ACTION_UP

        when (keyCode) {
            KeyEvent.KEYCODE_N -> { // Start button
                if (isDown && !isStartPressed) {
                    isStartPressed = true
                    if (isSelectPressed) {
                        // Both pressed - toggle drawer
                        val isOpen = drawerLayout.isDrawerOpen(Gravity.START)
                        if (isOpen) {
                            drawerLayout.closeDrawers()
                        } else {
                            drawerLayout.openDrawer(Gravity.START)
                        }
                        Log.d(TAG, "Start+Select combo detected - drawer toggled")
                    }
                } else if (isUp) {
                    isStartPressed = false
                }
                // Consume event if both buttons are pressed to prevent game input
                if (isStartPressed && isSelectPressed) {
                    return true
                }
            }

            KeyEvent.KEYCODE_B -> { // Select button
                if (isDown && !isSelectPressed) {
                    isSelectPressed = true
                    if (isStartPressed) {
                        // Both pressed - toggle drawer
                        val isOpen = drawerLayout.isDrawerOpen(Gravity.START)
                        if (isOpen) {
                            drawerLayout.closeDrawers()
                        } else {
                            drawerLayout.openDrawer(Gravity.START)
                        }
                        Log.d(TAG, "Start+Select combo detected - drawer toggled")
                    }
                } else if (isUp) {
                    isSelectPressed = false
                }
                // Consume event if both buttons are pressed to prevent game input
                if (isStartPressed && isSelectPressed) {
                    return true
                }
            }
        }

        // Bind dialog input interception - capture gamepad buttons for binding
        if (isBindDialogOpen && isGamepadButton(keyCode)) {
            handleBindDialogInput(event)
            return true // Consume event to block SDL
        }

        return super.dispatchKeyEvent(event)
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        Log.d(TAG, "onBackPressed() called")
        val isOpen = drawerLayout.isDrawerOpen(Gravity.START)
        Log.d(TAG, "Drawer is currently: ${if (isOpen) "OPEN" else "CLOSED"}")

        if (isOpen) {
            Log.d(TAG, "Closing drawer")
            drawerLayout.closeDrawers()
        } else {
            Log.d(TAG, "Opening drawer")
            drawerLayout.openDrawer(Gravity.START)
        }
    }

    // CRITICAL: Public wrappers for SDL key events (called from InputOverlay)
    // These methods MUST remain for JNI integration
    fun sendKeyDown(keycode: Int) {
        onNativeKeyDown(keycode)
    }

    fun sendKeyUp(keycode: Int) {
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
     * Detects MSU format from first valid file (legacy wrapper).
     * @return "MSU1" for PCM, "OPUZ" for Opus, null if no files found
     */
    private suspend fun detectMsuFormat(): String? = detectMsuInfo()?.format

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
     */
    private suspend fun updateMsuSettings(
        enableMsu: Boolean,
        volume: Int,
        resumeMsu: Boolean,
        audioFreq: Int,
        format: String?,
    ): Boolean =
        withContext(Dispatchers.IO) {
            val externalDir = getExternalFilesDir(null) ?: return@withContext false
            val configFile = File(externalDir, "zelda3.ini")

            if (!configFile.exists()) return@withContext false

            try {
                // Detect MSU info (format and file prefix path) if enabling
                val msuInfo = if (enableMsu) detectMsuInfo() else null
                val msuScan = if (enableMsu) scanMsuFiles() else MsuScanResult(0, 0)
                val detectedFormat = msuInfo?.format ?: format
                val detectedPath = msuInfo?.filePrefix
                val isDeluxe = msuScan.maxTrackNumber > 47

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

    /**
     * Checks if a save slot exists in SAF external storage.
     */
    private fun saveSlotExists(slot: Int): Boolean {
        val savesDir = getSafSavesDir() ?: return false
        val file = savesDir.findFile("save$slot.sav")
        return file != null && file.isFile
    }

    /**
     * Gets the thumbnail for a save slot from SAF external storage.
     * Returns null if thumbnail doesn't exist.
     */
    private fun getSaveSlotThumbnail(slot: Int): Bitmap? {
        val savesDir = getSafSavesDir() ?: return null
        val thumbnailFile = savesDir.findFile("save$slot.png") ?: return null

        return try {
            contentResolver.openInputStream(thumbnailFile.uri)?.use { input ->
                android.graphics.BitmapFactory.decodeStream(input)
            }
        } catch (e: Exception) {
            Log.e(TAG, "getSaveSlotThumbnail: Error loading thumbnail for slot $slot", e)
            null
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

    private fun showSaveStateDialog() {
        val dialogView = layoutInflater.inflate(R.layout.dialog_save_load_state, null)
        val recyclerView = dialogView.findViewById<androidx.recyclerview.widget.RecyclerView>(R.id.recycler_save_slots)

        recyclerView.layoutManager = androidx.recyclerview.widget.GridLayoutManager(this, 5) // 5 columns
        recyclerView.adapter =
            SaveStateAdapter(
                slots = (0..9).toList(),
                isSaveMode = true,
            ) { slot ->
                nativeSaveState(slot)
                captureSaveThumbnail(slot)
                currentDialog?.dismiss()
                drawerLayout.openDrawer(android.view.Gravity.START)
            }

        currentDialog =
            com.google.android.material.dialog
                .MaterialAlertDialogBuilder(this)
                .setTitle("Save State")
                .setView(dialogView)
                .setNegativeButton("Cancel") { _, _ ->
                    drawerLayout.openDrawer(android.view.Gravity.START)
                }.create()

        currentDialog?.show()
    }

    private fun showLoadStateDialog() {
        val dialogView = layoutInflater.inflate(R.layout.dialog_save_load_state, null)
        val recyclerView = dialogView.findViewById<androidx.recyclerview.widget.RecyclerView>(R.id.recycler_save_slots)

        recyclerView.layoutManager = androidx.recyclerview.widget.GridLayoutManager(this, 5) // 5 columns
        recyclerView.adapter =
            SaveStateAdapter(
                slots = (0..9).toList(),
                isSaveMode = false,
            ) { slot ->
                if (saveSlotExists(slot)) {
                    nativeLoadState(slot)
                    currentDialog?.dismiss()
                    drawerLayout.openDrawer(android.view.Gravity.START)
                } else {
                    Toast.makeText(this, "Save slot is empty", Toast.LENGTH_SHORT).show()
                }
            }

        currentDialog =
            com.google.android.material.dialog
                .MaterialAlertDialogBuilder(this)
                .setTitle("Load State")
                .setView(dialogView)
                .setNegativeButton("Cancel") { _, _ ->
                    drawerLayout.openDrawer(android.view.Gravity.START)
                }.create()

        currentDialog?.show()
    }

    // ========== SaveStateAdapter Inner Class ==========

    inner class SaveStateAdapter(
        private val slots: List<Int>,
        private val isSaveMode: Boolean,
        private val onSlotClick: (Int) -> Unit,
    ) : androidx.recyclerview.widget.RecyclerView.Adapter<SaveStateAdapter.ViewHolder>() {
        inner class ViewHolder(
            view: View,
        ) : androidx.recyclerview.widget.RecyclerView.ViewHolder(view) {
            val thumbnail: ImageView = view.findViewById(R.id.thumbnail)
            val slotName: TextView = view.findViewById(R.id.slot_name)
            val emptyIndicator: TextView = view.findViewById(R.id.empty_indicator)
        }

        override fun onCreateViewHolder(
            parent: android.view.ViewGroup,
            viewType: Int,
        ): ViewHolder {
            val view = layoutInflater.inflate(R.layout.item_save_slot, parent, false)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(
            holder: ViewHolder,
            position: Int,
        ) {
            val slot = slots[position]
            holder.slotName.text = if (slot == 0) "Quick Save" else "Slot $slot"

            val saveExists = saveSlotExists(slot)
            holder.emptyIndicator.visibility = if (saveExists) View.GONE else View.VISIBLE

            // Load thumbnail from SAF if exists
            val thumbnail = getSaveSlotThumbnail(slot)
            if (thumbnail != null) {
                holder.thumbnail.setImageBitmap(thumbnail)
            } else {
                holder.thumbnail.setImageResource(R.drawable.ic_placeholder_snes)
            }

            holder.itemView.setOnClickListener {
                onSlotClick(slot)
            }
        }

        override fun getItemCount() = slots.size
    }

    // ========== Controller Settings Dialog ==========

    private fun showControllerSettingsDialog() {
        val dialogView = layoutInflater.inflate(R.layout.dialog_controller_settings, null)
        val recyclerView = dialogView.findViewById<androidx.recyclerview.widget.RecyclerView>(R.id.recycler_bindings)
        val buttonResetDefaults = dialogView.findViewById<Button>(R.id.button_reset_defaults)
        val buttonDone = dialogView.findViewById<Button>(R.id.button_done)

        // Load all bindable commands (38 commands from our final list)
        val bindings = getAllBindableCommands()

        // Set up RecyclerView
        recyclerView.layoutManager = androidx.recyclerview.widget.LinearLayoutManager(this)
        val adapter =
            ControllerBindingAdapter(bindings) { binding ->
                showBindButtonDialog(binding.commandId, binding.commandDisplayName)
            }
        recyclerView.adapter = adapter

        // Store references for later refresh
        controllerSettingsRecyclerView = recyclerView
        controllerBindingAdapter = adapter

        // Create dialog
        val dialog =
            MaterialAlertDialogBuilder(this)
                .setTitle("Controller Settings")
                .setView(dialogView)
                .setCancelable(true)
                .setOnDismissListener {
                    // Clean up references when dialog is dismissed
                    controllerSettingsDialog = null
                    controllerSettingsRecyclerView = null
                    controllerBindingAdapter = null
                    savedScrollPosition = 0
                }.create()

        // Reset defaults button
        buttonResetDefaults.setOnClickListener {
            val resetDialog =
                MaterialAlertDialogBuilder(this)
                    .setTitle("Reset to defaults?")
                    .setMessage("This will reset controller inputs to default.")
                    .setPositiveButton("Reset to default") { _, _ ->
                        nativeClearGamepadBindings()
                        nativeApplyDefaultGamepadBindings()
                        showToast("Default bindings restored")

                        // Persist defaults to config file
                        saveGamepadBindingsToConfig()

                        // Refresh adapter in place (don't dismiss and reopen dialog)
                        val updatedBindings = getAllBindableCommands()
                        val newAdapter =
                            ControllerBindingAdapter(updatedBindings) { binding ->
                                showBindButtonDialog(binding.commandId, binding.commandDisplayName)
                            }
                        recyclerView.adapter = newAdapter
                        controllerBindingAdapter = newAdapter
                        recyclerView.scrollToPosition(0) // Reset to top after reset
                    }.setNegativeButton("Cancel", null)
                    .show()

            // Style positive button as filled red (destructive action)
            resetDialog.getButton(AlertDialog.BUTTON_POSITIVE)?.apply {
                val errorColor = resources.getColorStateList(R.color.md_theme_error, theme)
                val onErrorColor = resources.getColorStateList(R.color.md_theme_onError, theme)
                backgroundTintList = errorColor
                setTextColor(onErrorColor)
            }
        }

        // Done button
        buttonDone.setOnClickListener {
            dialog.dismiss()
            drawerLayout.openDrawer(GravityCompat.START)
        }

        // Store dialog references (both currentDialog for orientation handling and dedicated reference)
        currentDialog = dialog
        controllerSettingsDialog = dialog
        dialog.show()
    }

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

        // Convert captured buttons to sorted list of button names
        val buttonNames = capturedButtons.map { getButtonName(it) }.sorted()
        val bindingString = buttonNames.joinToString("+")

        val commandId =
            currentBindingCommandId ?: run {
                showToast("Error: No command selected")
                return
            }

        Log.d(TAG, "finalizeBind: Binding commandId=$commandId to '$bindingString'")

        // Save to config via JNI
        // For single button: buttonName="A", modifiers=null
        // For combo: buttonName="A", modifiers=["B", "Start"]
        val primaryButton = buttonNames.first()
        val modifiers =
            if (buttonNames.size > 1) {
                buttonNames.drop(1).toTypedArray()
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

        // Close bind dialog (but NOT the controller settings dialog)
        currentDialog?.dismiss()
        cleanupBindDialogState()

        // Restore reference to controller settings dialog after bind dialog closes
        currentDialog = controllerSettingsDialog

        // Refresh the controller settings adapter in place (don't reopen dialog)
        controllerSettingsRecyclerView?.let { recyclerView ->
            // Reload bindings data
            val updatedBindings = getAllBindableCommands()

            // Create new adapter with updated data
            val newAdapter =
                ControllerBindingAdapter(updatedBindings) { binding ->
                    showBindButtonDialog(binding.commandId, binding.commandDisplayName)
                }
            recyclerView.adapter = newAdapter
            controllerBindingAdapter = newAdapter

            // Restore scroll position immediately (RecyclerView handles this efficiently)
            recyclerView.scrollToPosition(savedScrollPosition)
        }

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

            // Remove all existing bindings in [GamepadMap] section (except comments and Controls line)
            val newLines = mutableListOf<String>()
            newLines.addAll(lines.subList(0, gamePadMapIndex + 1)) // Everything up to [GamepadMap]

            // Add comments and Controls line
            for (i in gamePadMapIndex + 1 until nextSectionIndex) {
                val line = lines[i].trim()
                if (line.startsWith("#") || line.startsWith("Controls =")) {
                    newLines.add(lines[i])
                }
            }

            // Get all current bindings from native code
            val bindingsJson = nativeGetGamepadBindings()
            Log.d(TAG, "saveGamepadBindingsToConfig: Got bindings JSON: $bindingsJson")

            // Parse JSON and write bindings
            if (bindingsJson.isNotEmpty()) {
                val bindings = parseGamepadBindingsJson(bindingsJson)

                // Special handling: "Controls" must be a single line with all 12 buttons in order
                // Format: Controls = DpadUp, DpadDown, DpadLeft, DpadRight, Back, Start, B, A, Y, X, L1, R1
                val controlsBindings = mutableListOf<String>()
                val otherBindings = mutableMapOf<String, String>()

                for ((cmdName, buttonCombo) in bindings) {
                    if (cmdName == "Controls") {
                        // Collect controls bindings (there will be 12 of them)
                        controlsBindings.add(buttonCombo)
                    } else {
                        // Everything else goes as individual lines
                        otherBindings[cmdName] = buttonCombo
                    }
                }

                // Write Controls line if we have controls bindings
                if (controlsBindings.isNotEmpty()) {
                    // Should have exactly 12 controls in order
                    val controlsLine = "Controls = " + controlsBindings.joinToString(", ")
                    // Replace the existing Controls line
                    val existingControlsIdx = newLines.indexOfFirst { it.trim().startsWith("Controls =") }
                    if (existingControlsIdx != -1) {
                        newLines[existingControlsIdx] = controlsLine
                    } else {
                        newLines.add(controlsLine)
                    }
                }

                // Write other bindings as individual lines
                for ((cmdName, buttonCombo) in otherBindings) {
                    newLines.add("$cmdName = $buttonCombo")
                }
            }

            // Add remaining sections
            if (nextSectionIndex < lines.size) {
                newLines.add("") // Blank line before next section
                newLines.addAll(lines.subList(nextSectionIndex, lines.size))
            }

            // Write back to file
            configFile.writeText(newLines.joinToString("\n"))
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
        // Save current scroll position before opening bind dialog
        controllerSettingsRecyclerView?.let { recyclerView ->
            val layoutManager = recyclerView.layoutManager as? androidx.recyclerview.widget.LinearLayoutManager
            savedScrollPosition = layoutManager?.findFirstVisibleItemPosition() ?: 0
        }

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

    // RecyclerView Adapter for controller bindings
    private inner class ControllerBindingAdapter(
        private val bindings: List<ControllerBinding>,
        private val onEditClick: (ControllerBinding) -> Unit,
    ) : androidx.recyclerview.widget.RecyclerView.Adapter<ControllerBindingAdapter.ViewHolder>() {
        private var lastCategory: String? = null

        inner class ViewHolder(
            view: View,
        ) : androidx.recyclerview.widget.RecyclerView.ViewHolder(view) {
            val textCommandName: TextView = view.findViewById(R.id.text_command_name)
            val textCurrentBinding: TextView = view.findViewById(R.id.text_current_binding)
            val buttonEdit: com.google.android.material.button.MaterialButton = view.findViewById(R.id.button_edit)
            val buttonDelete: com.google.android.material.button.MaterialButton = view.findViewById(R.id.button_delete)
        }

        override fun onCreateViewHolder(
            parent: ViewGroup,
            viewType: Int,
        ): ViewHolder {
            val view =
                LayoutInflater
                    .from(parent.context)
                    .inflate(R.layout.item_controller_binding, parent, false)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(
            holder: ViewHolder,
            position: Int,
        ) {
            val binding = bindings[position]

            holder.textCommandName.text = binding.commandDisplayName
            holder.textCurrentBinding.text = binding.currentBinding ?: "Not bound"

            // Show/hide delete button based on whether it's bound
            holder.buttonDelete.visibility = if (binding.currentBinding != null) View.VISIBLE else View.GONE

            holder.buttonEdit.setOnClickListener {
                onEditClick(binding)
            }

            holder.buttonDelete.setOnClickListener {
                // TODO: Implement unbind logic
                Log.d(TAG, "Delete binding for command ID: ${binding.commandId}")
            }
        }

        override fun getItemCount() = bindings.size
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
