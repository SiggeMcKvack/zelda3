package com.dishii.zelda3

import android.content.Context
import android.content.res.AssetManager
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import java.io.File
import java.io.IOException

/**
 * Utility for copying assets from APK to external storage.
 * Provides both suspend (coroutine) and blocking versions for compatibility.
 */
object AssetCopyUtil {
    private const val TAG = "AssetCopyUtil"
    private const val BUFFER_SIZE = 8192

    /**
     * Blocking version for Java compatibility.
     * Copies all files from an asset folder to external storage.
     *
     * @param context Android context for accessing AssetManager
     * @param assetsFolderPath Path within the assets directory (e.g., "saves/ref")
     * @param externalFolderPath Destination path in external storage
     * @throws IOException if any file operation fails
     */
    @JvmStatic
    @Throws(IOException::class)
    fun copyAssetsToExternal(
        context: Context,
        assetsFolderPath: String,
        externalFolderPath: String
    ) = runBlocking {
        copyAssetsToExternalAsync(context, assetsFolderPath, externalFolderPath)
    }

    /**
     * Suspend version for Kotlin coroutines.
     * Copies all files from an asset folder to external storage.
     * Runs on IO dispatcher for non-blocking file operations.
     *
     * @param context Android context for accessing AssetManager
     * @param assetsFolderPath Path within the assets directory (e.g., "saves/ref")
     * @param externalFolderPath Destination path in external storage
     * @throws IOException if any file operation fails
     */
    @Throws(IOException::class)
    suspend fun copyAssetsToExternalAsync(
        context: Context,
        assetsFolderPath: String,
        externalFolderPath: String
    ) = withContext(Dispatchers.IO) {
        val assetManager = context.assets
        val assetFiles = assetManager.list(assetsFolderPath)

        if (assetFiles.isNullOrEmpty()) {
            Log.w(TAG, "No assets found in: $assetsFolderPath")
            return@withContext
        }

        assetFiles.forEach { assetFile ->
            try {
                copyAssetFile(
                    assetManager = assetManager,
                    assetPath = assetsFolderPath.appendPath(assetFile),
                    outputPath = externalFolderPath.appendPath(assetFile)
                )
            } catch (e: IOException) {
                Log.e(TAG, "Failed to copy asset: $assetFile", e)
                throw e
            }
        }
    }

    /**
     * Copies a single asset file to external storage.
     * Uses Kotlin's `.use {}` for automatic resource management.
     */
    private fun copyAssetFile(
        assetManager: AssetManager,
        assetPath: String,
        outputPath: String
    ) {
        assetManager.open(assetPath).use { input ->
            File(outputPath).outputStream().use { output ->
                input.copyTo(output, bufferSize = BUFFER_SIZE)
            }
        }
    }

    /**
     * Extension function for cleaner path concatenation.
     */
    private fun String.appendPath(child: String): String =
        this + File.separator + child
}
