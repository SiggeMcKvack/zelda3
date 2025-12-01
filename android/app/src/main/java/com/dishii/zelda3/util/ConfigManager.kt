package com.dishii.zelda3.util

import android.content.Context
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.IOException

/**
 * Generic INI configuration manager for zelda3.ini.
 * Provides suspend functions for reading/writing INI key-value pairs in sections.
 */
object ConfigManager {
    private const val TAG = "ConfigManager"
    private const val CONFIG_FILENAME = "zelda3.ini"

    /**
     * Reads a string value from the config file.
     * @param section The INI section name (e.g., "Sound", "Graphics")
     * @param key The key name (e.g., "EnableMSU", "OutputMethod")
     * @param default Default value if key not found
     * @return The value, or default if not found
     */
    suspend fun readString(
        context: Context,
        section: String,
        key: String,
        default: String = ""
    ): String = withContext(Dispatchers.IO) {
        val configFile = getConfigFile(context) ?: return@withContext default
        if (!configFile.exists()) return@withContext default

        try {
            var inTargetSection = false
            configFile.readLines().forEach { line ->
                val trimmed = line.trim()

                when {
                    trimmed == "[$section]" -> inTargetSection = true
                    trimmed.startsWith("[") && trimmed.endsWith("]") -> inTargetSection = false
                    inTargetSection && trimmed.startsWith(key, ignoreCase = true) && trimmed.contains("=") -> {
                        val value = trimmed.substringAfter("=").trim()
                        if (value.isNotEmpty()) {
                            Log.d(TAG, "Read [$section] $key = $value")
                            return@withContext value
                        }
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error reading [$section] $key", e)
        }

        default
    }

    /**
     * Reads an integer value from the config file.
     */
    suspend fun readInt(
        context: Context,
        section: String,
        key: String,
        default: Int = 0
    ): Int = readString(context, section, key, default.toString()).toIntOrNull() ?: default

    /**
     * Reads a boolean value from the config file.
     * Treats "1", "true", "yes" as true (case insensitive).
     */
    suspend fun readBool(
        context: Context,
        section: String,
        key: String,
        default: Boolean = false
    ): Boolean {
        val value = readString(context, section, key, if (default) "1" else "0")
        return value == "1" || value.equals("true", ignoreCase = true) || value.equals("yes", ignoreCase = true)
    }

    /**
     * Writes a string value to the config file.
     * If the key exists in the section, updates it. Otherwise, adds it under the section.
     * @return true if write succeeded
     */
    suspend fun writeString(
        context: Context,
        section: String,
        key: String,
        value: String
    ): Boolean = withContext(Dispatchers.IO) {
        val configFile = getConfigFile(context) ?: return@withContext false
        if (!configFile.exists()) return@withContext false

        try {
            val lines = configFile.readLines().toMutableList()
            var inTargetSection = false
            var keyUpdated = false
            var sectionEndIndex = -1

            for (i in lines.indices) {
                val trimmed = lines[i].trim()

                when {
                    trimmed == "[$section]" -> {
                        inTargetSection = true
                        sectionEndIndex = i
                    }
                    trimmed.startsWith("[") && trimmed.endsWith("]") -> {
                        if (inTargetSection && !keyUpdated) {
                            // Section ended without finding key - insert before next section
                            lines.add(sectionEndIndex + 1, "$key = $value")
                            keyUpdated = true
                            Log.d(TAG, "Added [$section] $key = $value")
                        }
                        inTargetSection = false
                    }
                    inTargetSection && trimmed.startsWith(key, ignoreCase = true) && trimmed.contains("=") -> {
                        lines[i] = "$key = $value"
                        keyUpdated = true
                        Log.d(TAG, "Updated [$section] $key = $value")
                    }
                    inTargetSection -> {
                        sectionEndIndex = i
                    }
                }
            }

            // If still in target section at EOF and key wasn't found, add it
            if (inTargetSection && !keyUpdated) {
                lines.add("$key = $value")
                keyUpdated = true
                Log.d(TAG, "Added [$section] $key = $value at end")
            }

            if (!keyUpdated) {
                Log.w(TAG, "Section [$section] not found in config file")
                return@withContext false
            }

            configFile.writeText(lines.joinToString("\n"))
            true
        } catch (e: IOException) {
            Log.e(TAG, "Error writing [$section] $key", e)
            false
        }
    }

    /**
     * Writes an integer value to the config file.
     */
    suspend fun writeInt(
        context: Context,
        section: String,
        key: String,
        value: Int
    ): Boolean = writeString(context, section, key, value.toString())

    /**
     * Writes a boolean value to the config file (as 0 or 1).
     */
    suspend fun writeBool(
        context: Context,
        section: String,
        key: String,
        value: Boolean
    ): Boolean = writeString(context, section, key, if (value) "1" else "0")

    private fun getConfigFile(context: Context): File? {
        val externalDir = context.getExternalFilesDir(null) ?: return null
        return File(externalDir, CONFIG_FILENAME)
    }
}
