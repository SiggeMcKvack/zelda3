package com.dishii.zelda3.util

import android.content.Context
import android.widget.Toast
import androidx.appcompat.app.AlertDialog

/**
 * UI extension functions for common patterns.
 */

/**
 * Shows a toast message from any Context.
 */
fun Context.showToast(message: String, duration: Int = Toast.LENGTH_SHORT) {
    Toast.makeText(this, message, duration).show()
}

/**
 * Styles the positive button as filled (solid primary color) for emphasis.
 * Call after show() to ensure button exists.
 */
fun AlertDialog.stylePositiveAsFilled() {
    getButton(AlertDialog.BUTTON_POSITIVE)?.apply {
        val primaryColor = context.resources.getColorStateList(
            context.resources.getIdentifier("md_theme_primary", "color", context.packageName),
            context.theme
        )
        val onPrimaryColor = context.resources.getColorStateList(
            context.resources.getIdentifier("md_theme_onPrimary", "color", context.packageName),
            context.theme
        )
        backgroundTintList = primaryColor
        setTextColor(onPrimaryColor)
    }
}

/**
 * Styles the positive button as destructive (error color) for dangerous actions.
 * Call after show() to ensure button exists.
 */
fun AlertDialog.stylePositiveAsDestructive() {
    getButton(AlertDialog.BUTTON_POSITIVE)?.apply {
        val errorColor = context.resources.getColorStateList(
            context.resources.getIdentifier("md_theme_error", "color", context.packageName),
            context.theme
        )
        val onErrorColor = context.resources.getColorStateList(
            context.resources.getIdentifier("md_theme_onError", "color", context.packageName),
            context.theme
        )
        backgroundTintList = errorColor
        setTextColor(onErrorColor)
    }
}
