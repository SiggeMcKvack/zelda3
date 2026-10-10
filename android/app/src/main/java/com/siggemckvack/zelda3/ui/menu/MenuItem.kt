package com.siggemckvack.zelda3.ui.menu

import android.graphics.Bitmap

/**
 * Declarative pause menu entries. Pages are plain lists of these; [PauseMenuView] renders them
 * and maps controller input onto them (A = activate, D-pad left/right = change value).
 *
 * [visible] and [enabled] are re-evaluated after every change, so one setting can show/hide or
 * grey out others (e.g. the shader row only exists for Vulkan).
 */
sealed class MenuItem(
    val visible: () -> Boolean,
    val enabled: () -> Boolean,
) {
    class Header(
        val title: String,
        visible: () -> Boolean = { true },
    ) : MenuItem(visible, { true })

    class Info(
        val text: () -> String,
        visible: () -> Boolean = { true },
    ) : MenuItem(visible, { true })

    /** A flips it; D-pad left/right sets off/on. */
    class Toggle(
        val label: String,
        val restart: Boolean = false,
        visible: () -> Boolean = { true },
        enabled: () -> Boolean = { true },
        val get: () -> Boolean,
        val set: (Boolean) -> Unit,
    ) : MenuItem(visible, enabled)

    /** D-pad left/right cycles through [options]; A steps forward. */
    class Choice(
        val label: String,
        val options: List<String>,
        val restart: Boolean = false,
        visible: () -> Boolean = { true },
        enabled: () -> Boolean = { true },
        val get: () -> Int,
        val set: (Int) -> Unit,
    ) : MenuItem(visible, enabled)

    /** D-pad left/right moves by [step] within [min]..[max]. */
    class Slider(
        val label: String,
        val min: Int,
        val max: Int,
        val step: Int,
        val format: (Int) -> String = { "$it%" },
        visible: () -> Boolean = { true },
        enabled: () -> Boolean = { true },
        val get: () -> Int,
        val set: (Int) -> Unit,
    ) : MenuItem(visible, enabled)

    /** A runs [onClick]. [opensPage] shows a chevron; [value] shows a trailing status text. */
    class Action(
        val label: String,
        val value: () -> String? = { null },
        val opensPage: Boolean = false,
        val destructive: Boolean = false,
        visible: () -> Boolean = { true },
        enabled: () -> Boolean = { true },
        val onClick: () -> Unit,
    ) : MenuItem(visible, enabled)

    /** Grid of save-state slot cards. */
    class Slots(
        val slots: List<SaveSlot>,
        val onPick: (SaveSlot) -> Unit,
    ) : MenuItem({ true }, { true })
}

data class SaveSlot(
    val slot: Int,
    val exists: Boolean,
    val thumbnail: Bitmap?,
) {
    val name: String get() = if (slot == 0) "Quick Save" else "Slot $slot"
}

/** A top-level tab. [build] runs on the main thread but may suspend for I/O. */
class MenuTab(
    val title: String,
    val build: suspend () -> List<MenuItem>,
)
