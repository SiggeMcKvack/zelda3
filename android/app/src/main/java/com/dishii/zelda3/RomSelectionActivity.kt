package com.dishii.zelda3

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.widget.*
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File

/**
 * Activity for selecting ROM files to create zelda3_assets.dat.
 * Requires a US ROM for base assets, optionally accepts language ROMs for dialogue.
 */
class RomSelectionActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "RomSelection"
        private const val MAX_LANGUAGE_ROMS = 12

        init {
            System.loadLibrary("main")
        }
    }

    // ROM info data class
    data class RomInfo(
        val path: String,
        val langCode: String,
        val langName: String,
        val valid: Boolean
    )

    // State
    private var baseRomInfo: RomInfo? = null
    private val languageRoms = mutableListOf<RomInfo>()

    // UI references
    private lateinit var baseRomStatus: TextView
    private lateinit var baseRomBrowseBtn: Button
    private lateinit var langRomsContainer: LinearLayout
    private lateinit var langRomsBrowseBtn: Button
    private lateinit var createBtn: Button

    // Native functions
    private external fun nativeIdentifyRom(romPath: String): String?
    private external fun nativeExtractDialogue(romPath: String, outputDir: String): Int
    private external fun nativeCompileAssets(usRomPath: String, outputPath: String,
                                             languages: String?, dialogueDir: String?): Int

    // File pickers
    private val baseRomPicker: ActivityResultLauncher<Intent> =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (result.resultCode == RESULT_OK) {
                result.data?.data?.let { processBaseRom(it) }
            }
        }

    // Counter for unique temp file names
    private var tempFileCounter = 0

    private val langRomPicker: ActivityResultLauncher<Intent> =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (result.resultCode == RESULT_OK) {
                // Handle multiple file selection
                val clipData = result.data?.clipData
                if (clipData != null) {
                    for (i in 0 until clipData.itemCount) {
                        processLanguageRom(clipData.getItemAt(i).uri, tempFileCounter++)
                    }
                } else {
                    result.data?.data?.let { processLanguageRom(it, tempFileCounter++) }
                }
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_rom_selection)

        // Initialize UI references
        baseRomStatus = findViewById(R.id.base_rom_status)
        baseRomBrowseBtn = findViewById(R.id.base_rom_browse_btn)
        langRomsContainer = findViewById(R.id.lang_roms_container)
        langRomsBrowseBtn = findViewById(R.id.lang_roms_browse_btn)
        createBtn = findViewById(R.id.create_btn)

        // Setup click handlers
        baseRomBrowseBtn.setOnClickListener { openBaseRomPicker() }
        langRomsBrowseBtn.setOnClickListener { openLangRomPicker() }
        createBtn.setOnClickListener { createAssetFile() }
        findViewById<Button>(R.id.cancel_btn).setOnClickListener {
            setResult(RESULT_CANCELED)
            finish()
        }

        updateUI()
    }

    private fun openBaseRomPicker() {
        val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }
        baseRomPicker.launch(Intent.createChooser(intent, "Select US SNES ROM"))
    }

    private fun openLangRomPicker() {
        val intent = Intent(Intent.ACTION_GET_CONTENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
        }
        langRomPicker.launch(Intent.createChooser(intent, "Select Language ROM(s)"))
    }

    private fun processBaseRom(uri: Uri) {
        lifecycleScope.launch {
            val tempFile = File(cacheDir, "temp_base_rom.sfc")
            try {
                // Copy to cache
                withContext(Dispatchers.IO) {
                    contentResolver.openInputStream(uri)?.use { input ->
                        tempFile.outputStream().use { output ->
                            input.copyTo(output)
                        }
                    }
                }

                // Identify ROM
                val jsonStr = withContext(Dispatchers.IO) {
                    nativeIdentifyRom(tempFile.absolutePath)
                }

                if (jsonStr == null) {
                    Toast.makeText(this@RomSelectionActivity,
                        "Failed to read ROM file", Toast.LENGTH_LONG).show()
                    tempFile.delete()
                    return@launch
                }

                val json = JSONObject(jsonStr)
                val langCode = json.getString("lang_code")
                val langName = json.getString("lang_name")
                val valid = json.getBoolean("valid")

                if (langCode != "us") {
                    Toast.makeText(this@RomSelectionActivity,
                        "US ROM required. Detected: $langName\n\nUse 'Add Language ROMs' for other languages.",
                        Toast.LENGTH_LONG).show()
                    tempFile.delete()
                    return@launch
                }

                // Store ROM info (keep temp file for later use)
                baseRomInfo = RomInfo(tempFile.absolutePath, langCode, langName, valid)
                updateUI()

            } catch (e: Exception) {
                Log.e(TAG, "Error processing base ROM", e)
                Toast.makeText(this@RomSelectionActivity,
                    "Error: ${e.message}", Toast.LENGTH_LONG).show()
                tempFile.delete()
            }
        }
    }

    private fun processLanguageRom(uri: Uri, uniqueId: Int) {
        if (languageRoms.size >= MAX_LANGUAGE_ROMS) {
            Toast.makeText(this, "Maximum language ROMs reached", Toast.LENGTH_SHORT).show()
            return
        }

        lifecycleScope.launch {
            val tempFile = File(cacheDir, "temp_lang_${uniqueId}.sfc")
            try {
                // Copy to cache
                withContext(Dispatchers.IO) {
                    contentResolver.openInputStream(uri)?.use { input ->
                        tempFile.outputStream().use { output ->
                            input.copyTo(output)
                        }
                    }
                }

                // Identify ROM
                val jsonStr = withContext(Dispatchers.IO) {
                    nativeIdentifyRom(tempFile.absolutePath)
                }

                if (jsonStr == null) {
                    Toast.makeText(this@RomSelectionActivity,
                        "Failed to read ROM file", Toast.LENGTH_LONG).show()
                    tempFile.delete()
                    return@launch
                }

                val json = JSONObject(jsonStr)
                val langCode = json.getString("lang_code")
                val langName = json.getString("lang_name")
                val valid = json.getBoolean("valid")

                // Check for US ROM (should use base ROM field)
                if (langCode == "us") {
                    Toast.makeText(this@RomSelectionActivity,
                        "US ROM detected.\n\nPlease use 'Select Base ROM' for the USA ROM.",
                        Toast.LENGTH_LONG).show()
                    tempFile.delete()
                    return@launch
                }

                // Check for duplicate language (replace if exists)
                val existingIndex = languageRoms.indexOfFirst { it.langCode == langCode }
                if (existingIndex >= 0) {
                    // Delete old temp file
                    File(languageRoms[existingIndex].path).delete()
                    languageRoms[existingIndex] = RomInfo(tempFile.absolutePath, langCode, langName, valid)
                } else {
                    languageRoms.add(RomInfo(tempFile.absolutePath, langCode, langName, valid))
                }

                updateUI()

            } catch (e: Exception) {
                Log.e(TAG, "Error processing language ROM", e)
                Toast.makeText(this@RomSelectionActivity,
                    "Error: ${e.message}", Toast.LENGTH_LONG).show()
                tempFile.delete()
            }
        }
    }

    private fun removeLanguageRom(index: Int) {
        if (index in languageRoms.indices) {
            File(languageRoms[index].path).delete()
            languageRoms.removeAt(index)
            updateUI()
        }
    }

    private fun updateUI() {
        // Base ROM status
        baseRomInfo?.let { info ->
            baseRomStatus.text = if (info.valid) {
                "\u2713 ${info.langName}"
            } else {
                "\u26A0 Unknown ROM (${info.langName})"
            }
            baseRomStatus.setTextColor(ContextCompat.getColor(this,
                if (info.valid) android.R.color.holo_green_dark else android.R.color.holo_orange_dark
            ))
        } ?: run {
            baseRomStatus.text = "No ROM selected"
            baseRomStatus.setTextColor(ContextCompat.getColor(this, android.R.color.darker_gray))
        }

        // Language ROMs list
        langRomsContainer.removeAllViews()
        if (languageRoms.isEmpty()) {
            val emptyText = TextView(this).apply {
                text = "No language ROMs added (optional)"
                setTextColor(ContextCompat.getColor(context, android.R.color.darker_gray))
                setPadding(0, 8, 0, 8)
            }
            langRomsContainer.addView(emptyText)
        } else {
            languageRoms.forEachIndexed { index, info ->
                val row = LayoutInflater.from(this)
                    .inflate(R.layout.item_language_rom, langRomsContainer, false)

                // Set status icon based on ROM validity
                row.findViewById<ImageView>(R.id.status_icon).apply {
                    if (info.valid) {
                        setImageResource(R.drawable.ic_check_circle)
                        imageTintList = ContextCompat.getColorStateList(context, R.color.md_theme_primary)
                    } else {
                        setImageResource(R.drawable.ic_warning)
                        imageTintList = ContextCompat.getColorStateList(context, R.color.md_theme_error)
                    }
                }

                row.findViewById<TextView>(R.id.lang_name).text = info.langName
                row.findViewById<MaterialButton>(R.id.remove_btn).setOnClickListener {
                    removeLanguageRom(index)
                }
                langRomsContainer.addView(row)
            }
        }

        // Create button enabled only if base ROM is valid US ROM
        createBtn.isEnabled = baseRomInfo?.valid == true && baseRomInfo?.langCode == "us"
    }

    private fun createAssetFile() {
        val baseRom = baseRomInfo ?: return

        // Show progress dialog
        val progressView = LayoutInflater.from(this).inflate(R.layout.dialog_progress, null)
        val progressDialog = MaterialAlertDialogBuilder(this)
            .setTitle("Creating Assets")
            .setView(progressView)
            .setCancelable(false)
            .create()
        progressDialog.show()

        lifecycleScope.launch {
            try {
                val externalDir = getExternalFilesDir(null)
                    ?: throw Exception("External storage not available")
                val dialogueDir = File(cacheDir, "dialogue").apply { mkdirs() }

                // Step 1: Extract dialogue from US ROM
                Log.i(TAG, "Extracting US dialogue...")
                var result = withContext(Dispatchers.IO) {
                    nativeExtractDialogue(baseRom.path, dialogueDir.absolutePath)
                }
                if (result != 0) {
                    throw Exception("Failed to extract US dialogue (error $result)")
                }

                // Step 2: Extract dialogue from each language ROM
                val langCodes = mutableListOf<String>()
                for (langRom in languageRoms) {
                    if (!langRom.valid) continue

                    Log.i(TAG, "Extracting ${langRom.langName} dialogue...")
                    result = withContext(Dispatchers.IO) {
                        nativeExtractDialogue(langRom.path, dialogueDir.absolutePath)
                    }
                    if (result != 0) {
                        Log.w(TAG, "Failed to extract ${langRom.langName} dialogue")
                        continue
                    }
                    langCodes.add(langRom.langCode)
                }

                // Step 3: Compile full asset file
                Log.i(TAG, "Compiling assets...")
                val outputPath = File(externalDir, "zelda3_assets.dat").absolutePath
                val languages = if (langCodes.isEmpty()) null else langCodes.joinToString(",")

                result = withContext(Dispatchers.IO) {
                    nativeCompileAssets(baseRom.path, outputPath, languages, dialogueDir.absolutePath)
                }

                // Cleanup temp files
                dialogueDir.deleteRecursively()
                File(baseRom.path).delete()
                languageRoms.forEach { File(it.path).delete() }

                progressDialog.dismiss()

                if (result == 0) {
                    Log.i(TAG, "Asset file created successfully at: $outputPath")
                    Toast.makeText(this@RomSelectionActivity,
                        "Assets created successfully!", Toast.LENGTH_SHORT).show()
                    setResult(RESULT_OK)
                    finish()
                } else {
                    throw Exception("Asset compilation failed (error $result)")
                }

            } catch (e: Exception) {
                progressDialog.dismiss()
                Log.e(TAG, "Asset creation failed", e)
                MaterialAlertDialogBuilder(this@RomSelectionActivity)
                    .setTitle("Error")
                    .setMessage(e.message)
                    .setPositiveButton("OK", null)
                    .show()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        // Cleanup any remaining temp files
        baseRomInfo?.let { File(it.path).delete() }
        languageRoms.forEach { File(it.path).delete() }
    }
}
