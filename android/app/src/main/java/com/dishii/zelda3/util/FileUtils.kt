package com.dishii.zelda3.util

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.IOException

/**
 * File copy utilities for common I/O patterns.
 */
object FileUtils {
    private const val BUFFER_SIZE = 8192

    /**
     * Copies content from a URI to a file.
     * @param uri Source URI (from document picker)
     * @param destFile Destination file
     * @return Total bytes copied
     * @throws IOException if copy fails
     */
    suspend fun copyUriToFile(
        contentResolver: ContentResolver,
        uri: Uri,
        destFile: File,
    ): Long =
        withContext(Dispatchers.IO) {
            val inputStream =
                contentResolver.openInputStream(uri)
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

    /**
     * Copies an APK asset to a file.
     * @param assetPath Path within assets folder
     * @param destFile Destination file
     * @throws IOException if copy fails
     */
    suspend fun copyAssetToFile(
        context: Context,
        assetPath: String,
        destFile: File,
    ): Unit =
        withContext(Dispatchers.IO) {
            context.assets.open(assetPath).use { input ->
                destFile.outputStream().use { output ->
                    val buffer = ByteArray(BUFFER_SIZE)
                    var length: Int
                    while (input.read(buffer).also { length = it } > 0) {
                        output.write(buffer, 0, length)
                    }
                }
            }
        }

    /**
     * Copies a file to a SAF DocumentFile.
     * @param srcFile Source file
     * @param destDocument Destination DocumentFile
     * @return true if copy succeeded
     */
    suspend fun copyFileToDocument(
        contentResolver: ContentResolver,
        srcFile: File,
        destDocument: DocumentFile,
    ): Boolean =
        withContext(Dispatchers.IO) {
            if (!srcFile.exists()) return@withContext false

            val destUri = destDocument.uri
            try {
                srcFile.inputStream().use { input ->
                    contentResolver.openOutputStream(destUri)?.use { output ->
                        val buffer = ByteArray(BUFFER_SIZE)
                        var length: Int
                        while (input.read(buffer).also { length = it } > 0) {
                            output.write(buffer, 0, length)
                        }
                    } ?: return@withContext false
                }
                true
            } catch (e: IOException) {
                false
            }
        }
}
