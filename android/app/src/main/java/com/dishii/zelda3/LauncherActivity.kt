package com.dishii.zelda3

import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.provider.DocumentsContract
import android.util.Log
import android.widget.Toast
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.IOException

/**
 * First activity shown on app launch.
 * Handles asset file validation and Zelda3 folder selection using modern Android patterns.
 */
class LauncherActivity : AppCompatActivity() {

    private companion object {
        const val TAG = "Zelda3Launcher"
        const val PREFS_NAME = "app_prefs"
        const val PREF_ZELDA3_FOLDER_PATH = "zelda3_folder_path"
        const val PREF_ZELDA3_FOLDER_URI = "zelda3_folder_uri"
        const val BUFFER_SIZE = 8192
    }

    private val prefs: SharedPreferences by lazy {
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE)
    }

    private var zelda3FolderPath: String?
        get() = prefs.getString(PREF_ZELDA3_FOLDER_PATH, null)
        set(value) = prefs.edit().putString(PREF_ZELDA3_FOLDER_PATH, value).apply()

    private var zelda3FolderUri: String?
        get() = prefs.getString(PREF_ZELDA3_FOLDER_URI, null)
        set(value) = prefs.edit().putString(PREF_ZELDA3_FOLDER_URI, value).apply()

    // Modern Activity Result API launchers (replaces onActivityResult)
    private val assetsFilePicker: ActivityResultLauncher<Intent> =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (result.resultCode == RESULT_OK) {
                result.data?.data?.let { uri ->
                    copyAssetsFileFromUri(uri)
                } ?: run {
                    showToast("No file selected")
                    promptForAssetsFile()
                }
            } else {
                showToast("File selection cancelled")
                promptForAssetsFile()
            }
        }

    private val folderPicker: ActivityResultLauncher<Uri?> =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
            if (uri != null) {
                handleFolderSelection(uri)
            } else {
                showToast("Folder selection cancelled")
                finish()
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Check external storage availability
        if (!isExternalStorageWritable()) {
            showToast("External storage not available", Toast.LENGTH_LONG)
            finish()
            return
        }

        // Setup directories asynchronously with coroutines
        lifecycleScope.launch {
            if (!setupFilesAndDirectories()) {
                showToast("Failed to setup directories", Toast.LENGTH_LONG)
                finish()
                return@launch
            }

            // Determine initialization state and proceed accordingly
            when {
                zelda3FolderPath == null -> {
                    Log.i(TAG, "Zelda3 folder not selected, prompting user")
                    promptForZelda3Folder()
                }
                assetsFileExists() -> {
                    startMainActivity()
                }
                else -> {
                    Log.i(TAG, "zelda3_assets.dat not found, prompting user")
                    promptForAssetsFile()
                }
            }
        }
    }

    /**
     * Sets up required directories and copies initial files.
     * Runs on IO dispatcher for non-blocking file operations.
     */
    private suspend fun setupFilesAndDirectories(): Boolean = withContext(Dispatchers.IO) {
        val externalDir = getExternalFilesDir(null) ?: return@withContext false

        val configFile = File(externalDir, "zelda3.ini")
        val savesFolder = File(externalDir, "saves")
        val savesRefFolder = File(savesFolder, "ref")

        // Create directories
        savesFolder.mkdirs()
        savesRefFolder.mkdirs()

        try {
            // Copy reference saves using suspend version
            AssetCopyUtil.copyAssetsToExternalAsync(
                context = this@LauncherActivity,
                assetsFolderPath = "saves/ref",
                externalFolderPath = savesRefFolder.absolutePath
            )

            // Copy zelda3.ini if it doesn't exist (only on first run)
            if (!configFile.exists()) {
                copyAssetToFile("zelda3.ini", configFile)
            }

            true
        } catch (e: IOException) {
            Log.e(TAG, "Failed to create config files", e)
            false
        }
    }

    /**
     * Copies a single asset file to external storage using Kotlin's .use {} for resource management.
     */
    private suspend fun copyAssetToFile(assetPath: String, destFile: File) = withContext(Dispatchers.IO) {
        assets.open(assetPath).use { input ->
            destFile.outputStream().use { output ->
                input.copyTo(output, bufferSize = BUFFER_SIZE)
            }
        }
    }

    private fun assetsFileExists(): Boolean {
        val externalDir = getExternalFilesDir(null) ?: return false
        return File(externalDir, "zelda3_assets.dat").exists()
    }

    private fun startMainActivity() {
        startActivity(Intent(this, MainActivity::class.java))
        finish()
    }

    private fun promptForAssetsFile() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Missing Assets File")
            .setMessage("zelda3_assets.dat not found.\n\nThis file is required to run the game. Please select the zelda3_assets.dat file from your device.")
            .setPositiveButton("Select File") { _, _ -> openFilePicker() }
            .setNegativeButton("Exit") { _, _ -> finish() }
            .setCancelable(false)
            .create()
            .apply {
                show()
                stylePositiveButtonAsFilled()
            }
    }

    private fun openFilePicker() {
        val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }
        assetsFilePicker.launch(Intent.createChooser(intent, "Select zelda3_assets.dat"))
    }

    /**
     * Copies the selected assets file from URI to app's external files directory.
     * Uses coroutines for non-blocking I/O.
     */
    private fun copyAssetsFileFromUri(uri: Uri) {
        val externalDir = getExternalFilesDir(null)
        if (externalDir == null) {
            showToast("External storage not available", Toast.LENGTH_LONG)
            finish()
            return
        }

        lifecycleScope.launch {
            try {
                val totalBytes = withContext(Dispatchers.IO) {
                    copyUriToFile(uri, File(externalDir, "zelda3_assets.dat"))
                }

                Log.i(TAG, "Successfully copied zelda3_assets.dat ($totalBytes bytes)")
                showToast("Assets file loaded successfully. Starting game...")

                delay(1000)
                startMainActivity()
            } catch (e: IOException) {
                Log.e(TAG, "Failed to copy assets file", e)
                showToast("Failed to copy file: ${e.message}", Toast.LENGTH_LONG)
                promptForAssetsFile()
            }
        }
    }

    /**
     * Copies content from URI to file, returning total bytes copied.
     */
    private suspend fun copyUriToFile(uri: Uri, destFile: File): Long = withContext(Dispatchers.IO) {
        val inputStream = contentResolver.openInputStream(uri)
            ?: throw IOException("Could not open input stream")

        var totalBytes = 0L
        inputStream.use { input ->
            destFile.outputStream().use { output ->
                val buffer = ByteArray(BUFFER_SIZE)
                var length: Int
                while (input.read(buffer).also { length = it } > 0) {
                    output.write(buffer, 0, length)
                    totalBytes += length
                }
            }
        }
        totalBytes
    }

    private fun isExternalStorageWritable(): Boolean =
        Environment.getExternalStorageState() == Environment.MEDIA_MOUNTED

    private fun promptForZelda3Folder() {
        MaterialAlertDialogBuilder(this)
            .setTitle("Select Zelda3 Folder")
            .setMessage("Choose where to store game files. Select or create a 'Zelda3' folder on internal or external storage.\n\nMSU audio files will be stored here.")
            .setPositiveButton("Select Folder") { _, _ -> openZelda3FolderPicker() }
            .setNegativeButton("Exit") { _, _ -> finish() }
            .setCancelable(false)
            .create()
            .apply {
                show()
                stylePositiveButtonAsFilled()
            }
    }

    private fun openZelda3FolderPicker() {
        folderPicker.launch(null)
    }

    /**
     * Handles folder selection from document tree picker.
     * Takes persistent URI permission for long-term access.
     */
    private fun handleFolderSelection(treeUri: Uri) {
        // Take persistent permission
        try {
            contentResolver.takePersistableUriPermission(
                treeUri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            )
        } catch (e: SecurityException) {
            Log.e(TAG, "Failed to take persistent URI permission", e)
        }

        // Convert to real path and setup folder
        val folderPath = getRealPathFromTreeUri(treeUri)
        setupZelda3Folder(folderPath, treeUri)
    }

    /**
     * Extracts real filesystem path from document tree URI.
     * Handles both primary (internal) and external storage.
     */
    private fun getRealPathFromTreeUri(treeUri: Uri): String? {
        return try {
            val docId = DocumentsContract.getTreeDocumentId(treeUri)
            val (type, path) = docId.split(":").let { parts ->
                parts[0] to (parts.getOrNull(1) ?: "")
            }

            when {
                type.equals("primary", ignoreCase = true) ->
                    "${Environment.getExternalStorageDirectory()}/$path"
                else ->
                    "/storage/$type/$path"
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error extracting path from tree URI", e)
            null
        }
    }

    /**
     * Sets up the Zelda3 folder structure and updates configuration.
     * Creates MSU subdirectory and updates zelda3.ini with correct path.
     */
    private fun setupZelda3Folder(folderPath: String?, treeUri: Uri) {
        if (folderPath == null) {
            showToast("Failed to get folder path", Toast.LENGTH_LONG)
            promptForZelda3Folder()
            return
        }

        lifecycleScope.launch {
            try {
                // Create MSU subdirectory
                val msuDir = File(folderPath, "MSU")
                withContext(Dispatchers.IO) {
                    if (!msuDir.exists() && !msuDir.mkdirs()) {
                        throw IOException("Failed to create MSU directory")
                    }
                }

                // Save folder path and URI to SharedPreferences
                zelda3FolderPath = folderPath
                zelda3FolderUri = treeUri.toString()
                Log.d(TAG, "Saved Zelda3 folder path: $folderPath")
                Log.d(TAG, "Saved Zelda3 folder URI: ${treeUri.toString()}")

                // Update zelda3.ini MSUPath
                val msuPath = "$folderPath/MSU/alttp_msu-"
                if (updateMsuPathInConfig(msuPath)) {
                    Log.i(TAG, "Zelda3 folder set to: $folderPath")
                    Log.i(TAG, "MSU path updated to: $msuPath")
                    showToast("Zelda3 folder set to:\n$folderPath", Toast.LENGTH_LONG)

                    delay(1000)

                    // Continue with normal flow
                    if (assetsFileExists()) {
                        startMainActivity()
                    } else {
                        promptForAssetsFile()
                    }
                } else {
                    throw IOException("Failed to update configuration")
                }
            } catch (e: IOException) {
                Log.e(TAG, "Failed to setup Zelda3 folder", e)
                showToast("Failed to setup folder: ${e.message}", Toast.LENGTH_LONG)
                promptForZelda3Folder()
            }
        }
    }

    /**
     * Updates MSUPath in zelda3.ini [Sound] section.
     * Uses functional approach for cleaner INI parsing.
     */
    private suspend fun updateMsuPathInConfig(msuPath: String): Boolean = withContext(Dispatchers.IO) {
        val externalDir = getExternalFilesDir(null) ?: return@withContext false
        val configFile = File(externalDir, "zelda3.ini")

        if (!configFile.exists()) return@withContext false

        try {
            val lines = configFile.readLines()
            var inSoundSection = false
            var msuPathUpdated = false

            val updatedLines = lines.map { line ->
                val trimmed = line.trim()

                when {
                    // Track [Sound] section
                    trimmed == "[Sound]" -> {
                        inSoundSection = true
                        line
                    }
                    // Detect other sections
                    trimmed.startsWith("[") && trimmed.endsWith("]") -> {
                        inSoundSection = false
                        line
                    }
                    // Update MSUPath in [Sound] section
                    inSoundSection && trimmed.startsWith("MSUPath") -> {
                        msuPathUpdated = true
                        Log.d(TAG, "Updated MSUPath to: $msuPath")
                        "MSUPath = $msuPath"
                    }
                    else -> line
                }
            }

            if (!msuPathUpdated) {
                Log.w(TAG, "MSUPath not found in config file")
                return@withContext false
            }

            // Write updated config
            configFile.writeText(updatedLines.joinToString("\n"))
            true
        } catch (e: IOException) {
            Log.e(TAG, "Error updating MSU path in config", e)
            false
        }
    }

    // Extension functions for cleaner dialog styling and common operations

    /**
     * Styles dialog positive button as filled (solid primary color) for emphasis.
     */
    private fun AlertDialog.stylePositiveButtonAsFilled() {
        getButton(AlertDialog.BUTTON_POSITIVE)?.apply {
            val primaryColor = resources.getColorStateList(R.color.md_theme_primary, theme)
            val onPrimaryColor = resources.getColorStateList(R.color.md_theme_onPrimary, theme)
            backgroundTintList = primaryColor
            setTextColor(onPrimaryColor)
        }
    }

    private fun showToast(message: String, duration: Int = Toast.LENGTH_SHORT) {
        Toast.makeText(this, message, duration).show()
    }
}
