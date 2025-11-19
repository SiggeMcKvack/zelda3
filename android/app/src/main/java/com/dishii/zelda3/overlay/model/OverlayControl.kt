// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package com.dishii.zelda3.overlay.model

import android.view.KeyEvent

enum class OverlayControl(
    val id: String,
    val keycode: Int,
    val defaultVisibility: Boolean,
    val defaultLandscapePosition: Pair<Double, Double>,
    val defaultPortraitPosition: Pair<Double, Double>,
    val defaultFoldablePosition: Pair<Double, Double>,
    val defaultScale: Float
) {
    // Face buttons (Nintendo diamond layout)
    // Keycodes from zelda3.ini KeyMap: A=o, B=l, X=i, Y=k
    // Positions: Standard SNES layout (right side, diamond formation)
    BUTTON_A(
        id = "button_a",
        keycode = KeyEvent.KEYCODE_O,
        defaultVisibility = true,
        defaultLandscapePosition = Pair(0.88, 0.70),  // Right position in diamond
        defaultPortraitPosition = Pair(0.85, 0.75),
        defaultFoldablePosition = Pair(0.88, 0.70),
        defaultScale = 1.0f
    ),
    BUTTON_B(
        id = "button_b",
        keycode = KeyEvent.KEYCODE_L,
        defaultVisibility = true,
        defaultLandscapePosition = Pair(0.80, 0.78),  // Bottom position in diamond
        defaultPortraitPosition = Pair(0.75, 0.85),
        defaultFoldablePosition = Pair(0.80, 0.78),
        defaultScale = 1.0f
    ),
    BUTTON_X(
        id = "button_x",
        keycode = KeyEvent.KEYCODE_I,
        defaultVisibility = true,
        defaultLandscapePosition = Pair(0.80, 0.62),  // Top position in diamond
        defaultPortraitPosition = Pair(0.75, 0.65),
        defaultFoldablePosition = Pair(0.80, 0.62),
        defaultScale = 1.0f
    ),
    BUTTON_Y(
        id = "button_y",
        keycode = KeyEvent.KEYCODE_K,
        defaultVisibility = true,
        defaultLandscapePosition = Pair(0.72, 0.70),  // Left position in diamond
        defaultPortraitPosition = Pair(0.65, 0.75),
        defaultFoldablePosition = Pair(0.72, 0.70),
        defaultScale = 1.0f
    ),

    // Shoulder buttons
    // Keycodes from zelda3.ini KeyMap: L=c, R=v
    BUTTON_L(
        id = "button_l",
        keycode = KeyEvent.KEYCODE_C,
        defaultVisibility = true,
        defaultLandscapePosition = Pair(0.15, 0.15),  // Top-left
        defaultPortraitPosition = Pair(0.15, 0.1),
        defaultFoldablePosition = Pair(0.15, 0.15),
        defaultScale = 1.0f
    ),
    BUTTON_R(
        id = "button_r",
        keycode = KeyEvent.KEYCODE_V,
        defaultVisibility = true,
        defaultLandscapePosition = Pair(0.85, 0.15),  // Top-right
        defaultPortraitPosition = Pair(0.85, 0.1),
        defaultFoldablePosition = Pair(0.85, 0.15),
        defaultScale = 1.0f
    ),

    // Start/Select (renamed from Plus/Minus)
    // Keycodes from zelda3.ini KeyMap: Select=b, Start=n
    BUTTON_START(
        id = "button_start",
        keycode = KeyEvent.KEYCODE_N,
        defaultVisibility = true,
        defaultLandscapePosition = Pair(0.57, 0.90),  // Center-bottom-right
        defaultPortraitPosition = Pair(0.60, 0.90),
        defaultFoldablePosition = Pair(0.57, 0.90),
        defaultScale = 1.0f
    ),
    BUTTON_SELECT(
        id = "button_select",
        keycode = KeyEvent.KEYCODE_B,
        defaultVisibility = true,
        defaultLandscapePosition = Pair(0.43, 0.90),  // Center-bottom-left
        defaultPortraitPosition = Pair(0.40, 0.90),
        defaultFoldablePosition = Pair(0.43, 0.90),
        defaultScale = 1.0f
    ),

    // D-pad (uses 4 directional keys from zelda3.ini: Up=w, Down=s, Left=a, Right=d)
    DPAD(
        id = "dpad",
        keycode = 0,  // Special - handled separately with 4 keys
        defaultVisibility = true,
        defaultLandscapePosition = Pair(0.20, 0.70),  // Bottom-left
        defaultPortraitPosition = Pair(0.15, 0.75),
        defaultFoldablePosition = Pair(0.20, 0.70),
        defaultScale = 1.0f
    );
}
