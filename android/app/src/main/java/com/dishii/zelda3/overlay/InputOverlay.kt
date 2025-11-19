// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package com.dishii.zelda3.overlay

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Point
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.graphics.drawable.VectorDrawable
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.util.AttributeSet
import android.util.DisplayMetrics
import android.view.HapticFeedbackConstants
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceView
import android.view.View
import android.view.View.OnTouchListener
import androidx.core.content.ContextCompat
import androidx.window.layout.WindowMetricsCalculator
import com.dishii.zelda3.MainActivity
import com.dishii.zelda3.R
import com.dishii.zelda3.overlay.model.OverlayControl
import com.dishii.zelda3.overlay.model.OverlayControlData
import com.dishii.zelda3.overlay.model.OverlayLayout
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.max
import kotlin.math.min

/**
 * Draws the interactive input overlay on top of the
 * [SurfaceView] that is rendering emulation.
 */
class InputOverlay(context: Context, attrs: AttributeSet?) :
    SurfaceView(context, attrs),
    OnTouchListener {

    constructor(context: Context) : this(context, null)

    private val overlayButtons: MutableSet<InputOverlayDrawableButton> = HashSet()
    private val overlayDpads: MutableSet<InputOverlayDrawableDpad> = HashSet()

    private var inEditMode = false
    private var buttonBeingConfigured: InputOverlayDrawableButton? = null
    private var dpadBeingConfigured: InputOverlayDrawableDpad? = null

    private var scaleDialog: OverlayScaleDialog? = null
    private var touchStartX = 0f
    private var touchStartY = 0f
    private var hasMoved = false
    private val moveThreshold = 20f

    // Track drawer state to allow touches to pass through when drawer is open
    private var isDrawerOpen = false

    var layout = OverlayLayout.Landscape

    // Settings
    private var overlayEnabled = true
    private var overlayScale = 0  // -50 to +100
    private var overlayOpacity = 100  // 0 to 100
    private var dpadSlideEnabled = true
    private var hapticFeedbackEnabled = false

    private val prefs: SharedPreferences by lazy {
        context.getSharedPreferences("overlay_config", Context.MODE_PRIVATE)
    }

    private val vibrator: Vibrator? by lazy {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val vibratorManager = context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager
            vibratorManager?.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
        }
    }

    // D-pad directional keycodes (from zelda3.ini)
    // D-pad keycodes from zelda3.ini KeyMap: Up=w, Down=s, Left=a, Right=d
    private val DPAD_UP = KeyEvent.KEYCODE_W
    private val DPAD_DOWN = KeyEvent.KEYCODE_S
    private val DPAD_LEFT = KeyEvent.KEYCODE_A
    private val DPAD_RIGHT = KeyEvent.KEYCODE_D

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)

        val overlayControlData = loadOverlayData()
        if (overlayControlData.isEmpty()) {
            populateDefaultConfig()
        }

        // Detect current layout
        layout = detectLayout()

        // Load the controls
        refreshControls()

        // Set the on touch listener
        setOnTouchListener(this)

        // Force draw
        setWillNotDraw(false)

        // Request focus for the overlay so it has priority on presses
        requestFocus()
    }

    private fun detectLayout(): OverlayLayout {
        val metrics = DisplayMetrics()
        (context as Activity).windowManager.defaultDisplay.getRealMetrics(metrics)

        val widthDp = metrics.widthPixels / metrics.density
        val heightDp = metrics.heightPixels / metrics.density
        val smallestWidth = min(widthDp, heightDp)

        val orientation = resources.configuration.orientation

        return when {
            smallestWidth >= 600 -> OverlayLayout.Foldable  // Tablet/foldable
            orientation == Configuration.ORIENTATION_LANDSCAPE -> OverlayLayout.Landscape
            else -> OverlayLayout.Portrait
        }
    }

    override fun draw(canvas: Canvas) {
        super.draw(canvas)
        if (!overlayEnabled) {
            return
        }
        for (button in overlayButtons) {
            button.draw(canvas)
        }
        for (dpad in overlayDpads) {
            dpad.draw(canvas)
        }
    }

    override fun onTouch(v: View, event: MotionEvent): Boolean {
        // Pass through touches when drawer is open so NavigationView can receive them
        if (isDrawerOpen) {
            android.util.Log.d("InputOverlay", "Drawer is open, passing touch through: ${event.action} at (${event.x}, ${event.y})")
            return false
        }

        // Ignore touches when overlay is disabled
        if (!overlayEnabled) {
            return false
        }

        if (inEditMode) {
            return onTouchWhileEditing(event)
        }

        var shouldUpdateView = false

        for (button in overlayButtons) {
            if (!button.updateStatus(event)) {
                continue
            }
            sendButtonEvent(button.buttonId, button.pressedState)
            // Trigger haptics on any state change (press or release)
            playHaptics()
            shouldUpdateView = true
        }

        for (dpad in overlayDpads) {
            if (!dpad.updateStatus(event, dpadSlideEnabled)) {
                continue
            }
            sendDpadEvent(dpad)
            // Trigger haptics on any dpad state change
            playHaptics()
            shouldUpdateView = true
        }

        if (shouldUpdateView) {
            invalidate()
        }

        return true
    }

    private fun sendButtonEvent(button: OverlayControl, pressed: Boolean) {
        val mainActivity = context as? MainActivity ?: return
        if (pressed) {
            mainActivity.sendKeyDown(button.keycode)
        } else {
            mainActivity.sendKeyUp(button.keycode)
        }
    }

    private fun sendDpadEvent(dpad: InputOverlayDrawableDpad) {
        val mainActivity = context as? MainActivity ?: return

        // Send key events for each direction
        if (dpad.upButtonState) {
            mainActivity.sendKeyDown(DPAD_UP)
        } else {
            mainActivity.sendKeyUp(DPAD_UP)
        }

        if (dpad.downButtonState) {
            mainActivity.sendKeyDown(DPAD_DOWN)
        } else {
            mainActivity.sendKeyUp(DPAD_DOWN)
        }

        if (dpad.leftButtonState) {
            mainActivity.sendKeyDown(DPAD_LEFT)
        } else {
            mainActivity.sendKeyUp(DPAD_LEFT)
        }

        if (dpad.rightButtonState) {
            mainActivity.sendKeyDown(DPAD_RIGHT)
        } else {
            mainActivity.sendKeyUp(DPAD_RIGHT)
        }
    }

    private fun playHaptics() {
        if (!hapticFeedbackEnabled) return

        vibrator?.let { vib ->
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                // Use VibrationEffect for Android 8.0+ (more precise control)
                vib.vibrate(VibrationEffect.createOneShot(20, VibrationEffect.DEFAULT_AMPLITUDE))
            } else {
                // Fallback for older devices
                @Suppress("DEPRECATION")
                vib.vibrate(20)
            }
        }
    }

    private fun onTouchWhileEditing(event: MotionEvent): Boolean {
        val pointerIndex = event.actionIndex
        val fingerPositionX = event.getX(pointerIndex).toInt()
        val fingerPositionY = event.getY(pointerIndex).toInt()

        for (button in overlayButtons) {
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN ->
                    if (buttonBeingConfigured == null &&
                        button.bounds.contains(fingerPositionX, fingerPositionY)
                    ) {
                        buttonBeingConfigured = button
                        buttonBeingConfigured!!.onConfigureTouch(event)
                        touchStartX = event.getX(pointerIndex)
                        touchStartY = event.getY(pointerIndex)
                        hasMoved = false
                    }

                MotionEvent.ACTION_MOVE -> if (buttonBeingConfigured != null) {
                    val moveDistance = kotlin.math.sqrt(
                        (event.getX(pointerIndex) - touchStartX).let { it * it } +
                                (event.getY(pointerIndex) - touchStartY).let { it * it }
                    )

                    if (moveDistance > moveThreshold) {
                        hasMoved = true
                        buttonBeingConfigured!!.onConfigureTouch(event)
                        invalidate()
                        return true
                    }
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> if (buttonBeingConfigured === button) {
                    if (!hasMoved) {
                        showScaleDialog(
                            buttonBeingConfigured,
                            null,
                            fingerPositionX,
                            fingerPositionY
                        )
                    } else {
                        saveControlPosition(
                            buttonBeingConfigured!!.overlayControlData.id,
                            buttonBeingConfigured!!.bounds.centerX(),
                            buttonBeingConfigured!!.bounds.centerY(),
                            buttonBeingConfigured!!.overlayControlData.individualScale,
                            layout
                        )
                    }
                    buttonBeingConfigured = null
                }
            }
        }

        for (dpad in overlayDpads) {
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN ->
                    if (dpadBeingConfigured == null &&
                        dpad.bounds.contains(fingerPositionX, fingerPositionY)
                    ) {
                        dpadBeingConfigured = dpad
                        dpadBeingConfigured!!.onConfigureTouch(event)
                        touchStartX = event.getX(pointerIndex)
                        touchStartY = event.getY(pointerIndex)
                        hasMoved = false
                    }

                MotionEvent.ACTION_MOVE -> if (dpadBeingConfigured != null) {
                    val moveDistance = kotlin.math.sqrt(
                        (event.getX(pointerIndex) - touchStartX).let { it * it } +
                                (event.getY(pointerIndex) - touchStartY).let { it * it }
                    )

                    if (moveDistance > moveThreshold) {
                        hasMoved = true
                        dpadBeingConfigured!!.onConfigureTouch(event)
                        invalidate()
                        return true
                    }
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> if (dpadBeingConfigured === dpad) {
                    if (!hasMoved) {
                        showScaleDialog(
                            null,
                            dpadBeingConfigured,
                            fingerPositionX,
                            fingerPositionY
                        )
                    } else {
                        saveControlPosition(
                            OverlayControl.DPAD.id,
                            dpadBeingConfigured!!.bounds.centerX(),
                            dpadBeingConfigured!!.bounds.centerY(),
                            dpadBeingConfigured!!.individualScale,
                            layout
                        )
                    }
                    dpadBeingConfigured = null
                }
            }
        }

        return true
    }

    private fun addOverlayControls(layout: OverlayLayout) {
        val windowSize = getSafeScreenSize(context, Pair(measuredWidth, measuredHeight))
        val overlayControlData = loadOverlayData()

        for (data in overlayControlData) {
            if (!data.enabled) {
                continue
            }

            val position = data.positionFromLayout(layout)
            when (data.id) {
                OverlayControl.BUTTON_A.id -> {
                    overlayButtons.add(
                        initializeOverlayButton(
                            context,
                            windowSize,
                            R.drawable.facebutton_a,
                            R.drawable.facebutton_a_depressed,
                            OverlayControl.BUTTON_A,
                            data,
                            position
                        )
                    )
                }

                OverlayControl.BUTTON_B.id -> {
                    overlayButtons.add(
                        initializeOverlayButton(
                            context,
                            windowSize,
                            R.drawable.facebutton_b,
                            R.drawable.facebutton_b_depressed,
                            OverlayControl.BUTTON_B,
                            data,
                            position
                        )
                    )
                }

                OverlayControl.BUTTON_X.id -> {
                    overlayButtons.add(
                        initializeOverlayButton(
                            context,
                            windowSize,
                            R.drawable.facebutton_x,
                            R.drawable.facebutton_x_depressed,
                            OverlayControl.BUTTON_X,
                            data,
                            position
                        )
                    )
                }

                OverlayControl.BUTTON_Y.id -> {
                    overlayButtons.add(
                        initializeOverlayButton(
                            context,
                            windowSize,
                            R.drawable.facebutton_y,
                            R.drawable.facebutton_y_depressed,
                            OverlayControl.BUTTON_Y,
                            data,
                            position
                        )
                    )
                }

                OverlayControl.BUTTON_START.id -> {
                    overlayButtons.add(
                        initializeOverlayButton(
                            context,
                            windowSize,
                            R.drawable.facebutton_start,
                            R.drawable.facebutton_start_depressed,
                            OverlayControl.BUTTON_START,
                            data,
                            position
                        )
                    )
                }

                OverlayControl.BUTTON_SELECT.id -> {
                    overlayButtons.add(
                        initializeOverlayButton(
                            context,
                            windowSize,
                            R.drawable.facebutton_select,
                            R.drawable.facebutton_select_depressed,
                            OverlayControl.BUTTON_SELECT,
                            data,
                            position
                        )
                    )
                }

                OverlayControl.BUTTON_L.id -> {
                    overlayButtons.add(
                        initializeOverlayButton(
                            context,
                            windowSize,
                            R.drawable.l_shoulder,
                            R.drawable.l_shoulder_depressed,
                            OverlayControl.BUTTON_L,
                            data,
                            position
                        )
                    )
                }

                OverlayControl.BUTTON_R.id -> {
                    overlayButtons.add(
                        initializeOverlayButton(
                            context,
                            windowSize,
                            R.drawable.r_shoulder,
                            R.drawable.r_shoulder_depressed,
                            OverlayControl.BUTTON_R,
                            data,
                            position
                        )
                    )
                }

                OverlayControl.DPAD.id -> {
                    overlayDpads.add(
                        initializeOverlayDpad(
                            context,
                            windowSize,
                            R.drawable.dpad_standard,
                            R.drawable.dpad_standard_cardinal_depressed,
                            R.drawable.dpad_standard_diagonal_depressed,
                            data,
                            position
                        )
                    )
                }
            }
        }
    }

    fun refreshControls() {
        overlayButtons.clear()
        overlayDpads.clear()

        // Always show controls (visibility managed by MainActivity)
        addOverlayControls(layout)
        invalidate()
    }

    fun setLayoutOrientation(newLayout: OverlayLayout) {
        layout = newLayout
        refreshControls()
    }

    private fun saveControlPosition(
        id: String,
        x: Int,
        y: Int,
        individualScale: Float,
        layout: OverlayLayout
    ) {
        val windowSize = getSafeScreenSize(context, Pair(measuredWidth, measuredHeight))
        val min = windowSize.first
        val max = windowSize.second
        val overlayControlData = loadOverlayData().toMutableList()
        val data = overlayControlData.firstOrNull { it.id == id }
        val newPosition = Pair((x - min.x).toDouble() / max.x, (y - min.y).toDouble() / max.y)

        when (layout) {
            OverlayLayout.Landscape -> data?.landscapePosition = newPosition
            OverlayLayout.Portrait -> data?.portraitPosition = newPosition
            OverlayLayout.Foldable -> data?.foldablePosition = newPosition
        }

        data?.individualScale = individualScale

        saveOverlayData(overlayControlData)
    }

    fun setIsInEditMode(editMode: Boolean) {
        inEditMode = editMode
        if (!editMode) {
            scaleDialog?.dismiss()
            scaleDialog = null
        }
    }

    fun setDrawerOpen(open: Boolean) {
        isDrawerOpen = open
        // Make the overlay completely pass through touches when drawer is open
        isClickable = !open
        isFocusable = !open
        android.util.Log.d("InputOverlay", "setDrawerOpen($open) - clickable=${!open}, focusable=${!open}")
    }

    private fun showScaleDialog(
        button: InputOverlayDrawableButton?,
        dpad: InputOverlayDrawableDpad?,
        x: Int, y: Int
    ) {
        scaleDialog?.dismiss()

        when {
            button != null -> {
                scaleDialog = OverlayScaleDialog(context, button.overlayControlData) { newScale ->
                    saveControlPosition(
                        button.overlayControlData.id,
                        button.bounds.centerX(),
                        button.bounds.centerY(),
                        newScale,
                        layout
                    )
                    refreshControls()
                }
                scaleDialog?.showDialog(x, y, button.bounds.width(), button.bounds.height())
            }

            dpad != null -> {
                val overlayControlData = loadOverlayData()
                val dpadData = overlayControlData.firstOrNull { it.id == OverlayControl.DPAD.id }
                if (dpadData != null) {
                    scaleDialog = OverlayScaleDialog(context, dpadData) { newScale ->
                        saveControlPosition(
                            OverlayControl.DPAD.id,
                            dpad.bounds.centerX(),
                            dpad.bounds.centerY(),
                            newScale,
                            layout
                        )
                        refreshControls()
                    }
                    scaleDialog?.showDialog(x, y, dpad.bounds.width(), dpad.bounds.height())
                }
            }
        }
    }

    private fun populateDefaultConfig() {
        val newConfig = OverlayControl.values().map { control ->
            OverlayControlData(
                id = control.id,
                enabled = control.defaultVisibility,
                landscapePosition = control.defaultLandscapePosition,
                portraitPosition = control.defaultPortraitPosition,
                foldablePosition = control.defaultFoldablePosition,
                individualScale = control.defaultScale
            )
        }
        saveOverlayData(newConfig)
    }

    // SharedPreferences persistence
    private fun saveOverlayData(data: List<OverlayControlData>) {
        val json = JSONObject()
        json.put("overlay_enabled", overlayEnabled)
        json.put("overlay_scale", overlayScale)
        json.put("overlay_opacity", overlayOpacity)
        json.put("dpad_slide", dpadSlideEnabled)
        json.put("haptic_feedback", hapticFeedbackEnabled)

        val controlsJson = JSONArray()
        for (control in data) {
            val controlObj = JSONObject()
            controlObj.put("id", control.id)
            controlObj.put("enabled", control.enabled)
            controlObj.put("landscape_x", control.landscapePosition.first)
            controlObj.put("landscape_y", control.landscapePosition.second)
            controlObj.put("portrait_x", control.portraitPosition.first)
            controlObj.put("portrait_y", control.portraitPosition.second)
            controlObj.put("foldable_x", control.foldablePosition.first)
            controlObj.put("foldable_y", control.foldablePosition.second)
            controlObj.put("scale", control.individualScale)
            controlsJson.put(controlObj)
        }
        json.put("controls", controlsJson)

        prefs.edit().putString("overlay_data", json.toString()).apply()
    }

    private fun hasTouchscreen(): Boolean {
        return context.packageManager.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)
    }

    private fun loadOverlayData(): List<OverlayControlData> {
        val jsonString = prefs.getString("overlay_data", null) ?: return emptyList()

        try {
            val json = JSONObject(jsonString)
            // Default to enabled if touchscreen is available, disabled if not (e.g., Android TV)
            overlayEnabled = json.optBoolean("overlay_enabled", hasTouchscreen())
            overlayScale = json.optInt("overlay_scale", 0)
            overlayOpacity = json.optInt("overlay_opacity", 100)
            dpadSlideEnabled = json.optBoolean("dpad_slide", true)
            hapticFeedbackEnabled = json.optBoolean("haptic_feedback", false)

            val controlsJson = json.getJSONArray("controls")
            val controls = mutableListOf<OverlayControlData>()

            for (i in 0 until controlsJson.length()) {
                val controlObj = controlsJson.getJSONObject(i)
                controls.add(
                    OverlayControlData(
                        id = controlObj.getString("id"),
                        enabled = controlObj.getBoolean("enabled"),
                        landscapePosition = Pair(
                            controlObj.getDouble("landscape_x"),
                            controlObj.getDouble("landscape_y")
                        ),
                        portraitPosition = Pair(
                            controlObj.getDouble("portrait_x"),
                            controlObj.getDouble("portrait_y")
                        ),
                        foldablePosition = Pair(
                            controlObj.getDouble("foldable_x"),
                            controlObj.getDouble("foldable_y")
                        ),
                        individualScale = controlObj.getDouble("scale").toFloat()
                    )
                )
            }
            return controls
        } catch (e: Exception) {
            e.printStackTrace()
            return emptyList()
        }
    }

    fun resetOverlay() {
        prefs.edit().clear().apply()
        populateDefaultConfig()
        refreshControls()
    }

    // Getters and setters for settings
    fun setOverlayEnabled(enabled: Boolean) {
        overlayEnabled = enabled
        saveSettings()
        invalidate()  // Force redraw to show/hide overlay
    }

    fun getOverlayEnabled(): Boolean {
        return overlayEnabled
    }

    fun setDpadSlide(enabled: Boolean) {
        dpadSlideEnabled = enabled
        saveSettings()
    }

    fun setHapticFeedback(enabled: Boolean) {
        hapticFeedbackEnabled = enabled
        saveSettings()
    }

    fun getDpadSlideEnabled(): Boolean {
        return dpadSlideEnabled
    }

    fun getHapticFeedbackEnabled(): Boolean {
        return hapticFeedbackEnabled
    }

    fun setOverlayScale(scale: Int) {
        overlayScale = scale
        saveSettings()
        refreshControls()
    }

    fun setOverlayOpacity(opacity: Int) {
        overlayOpacity = opacity
        saveSettings()
        refreshControls()
    }

    fun getControlVisibility(): BooleanArray {
        val data = loadOverlayData()
        return OverlayControl.values().map { control ->
            data.firstOrNull { it.id == control.id }?.enabled ?: control.defaultVisibility
        }.toBooleanArray()
    }

    fun setControlVisibility(index: Int, visible: Boolean) {
        val controls = OverlayControl.values()
        if (index < 0 || index >= controls.size) return

        val data = loadOverlayData().toMutableList()
        val controlData = data.firstOrNull { it.id == controls[index].id }
        controlData?.enabled = visible
        saveOverlayData(data)
        refreshControls()
    }

    private fun saveSettings() {
        val data = loadControlDataOnly()
        saveOverlayData(data)  // This saves all settings
    }

    // Load only control positions/scales without overwriting setting variables
    private fun loadControlDataOnly(): List<OverlayControlData> {
        val jsonString = prefs.getString("overlay_data", null) ?: return emptyList()

        try {
            val json = JSONObject(jsonString)
            // Don't load settings here - they're already set in memory
            val controlsJson = json.getJSONArray("controls")
            val controls = mutableListOf<OverlayControlData>()

            for (i in 0 until controlsJson.length()) {
                val controlObj = controlsJson.getJSONObject(i)
                controls.add(
                    OverlayControlData(
                        id = controlObj.getString("id"),
                        enabled = controlObj.getBoolean("enabled"),
                        landscapePosition = Pair(
                            controlObj.getDouble("landscape_x"),
                            controlObj.getDouble("landscape_y")
                        ),
                        portraitPosition = Pair(
                            controlObj.getDouble("portrait_x"),
                            controlObj.getDouble("portrait_y")
                        ),
                        foldablePosition = Pair(
                            controlObj.getDouble("foldable_x"),
                            controlObj.getDouble("foldable_y")
                        ),
                        individualScale = controlObj.getDouble("scale").toFloat()
                    )
                )
            }
            return controls
        } catch (e: Exception) {
            e.printStackTrace()
            return emptyList()
        }
    }

    private fun getSafeScreenSize(
        context: Context,
        screenSize: Pair<Int, Int>
    ): Pair<Point, Point> {
        val windowMetrics = WindowMetricsCalculator.getOrCreate()
            .computeCurrentWindowMetrics(context as Activity)
        val screenBounds = windowMetrics.bounds

        val leftCutoutInset: Int
        val rightCutoutInset: Int
        val topCutoutInset: Int
        val bottomCutoutInset: Int

        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
            val insets = (context as Activity).windowManager.currentWindowMetrics.windowInsets
            val cutoutInsets = insets.displayCutout
            if (cutoutInsets != null) {
                leftCutoutInset = cutoutInsets.safeInsetLeft
                rightCutoutInset = cutoutInsets.safeInsetRight
                topCutoutInset = cutoutInsets.safeInsetTop
                bottomCutoutInset = cutoutInsets.safeInsetBottom
            } else {
                leftCutoutInset = 0
                rightCutoutInset = 0
                topCutoutInset = 0
                bottomCutoutInset = 0
            }
        } else {
            leftCutoutInset = 0
            rightCutoutInset = 0
            topCutoutInset = 0
            bottomCutoutInset = 0
        }

        val insetLeft = max(leftCutoutInset, rightCutoutInset)
        val insetTop = max(topCutoutInset, bottomCutoutInset)
        val insetRight = insetLeft
        val insetBottom = insetTop

        return Pair(
            Point(insetLeft, insetTop),
            Point(screenBounds.width() - insetRight, screenBounds.height() - insetBottom)
        )
    }

    private fun initializeOverlayButton(
        context: Context,
        windowSize: Pair<Point, Point>,
        defaultResId: Int,
        pressedResId: Int,
        buttonId: OverlayControl,
        controlData: OverlayControlData,
        position: Pair<Double, Double>
    ): InputOverlayDrawableButton {
        val res = context.resources

        val defaultDrawable = ContextCompat.getDrawable(context, defaultResId) as VectorDrawable
        val pressedDrawable = ContextCompat.getDrawable(context, pressedResId) as VectorDrawable

        val defaultBitmap = Bitmap.createBitmap(
            defaultDrawable.intrinsicWidth,
            defaultDrawable.intrinsicHeight,
            Bitmap.Config.ARGB_8888
        )
        val pressedBitmap = Bitmap.createBitmap(
            pressedDrawable.intrinsicWidth,
            pressedDrawable.intrinsicHeight,
            Bitmap.Config.ARGB_8888
        )

        val defaultCanvas = Canvas(defaultBitmap)
        val pressedCanvas = Canvas(pressedBitmap)
        defaultDrawable.setBounds(0, 0, defaultDrawable.intrinsicWidth, defaultDrawable.intrinsicHeight)
        pressedDrawable.setBounds(0, 0, pressedDrawable.intrinsicWidth, pressedDrawable.intrinsicHeight)
        defaultDrawable.draw(defaultCanvas)
        pressedDrawable.draw(pressedCanvas)

        val min = windowSize.first
        val max = windowSize.second

        var baseScale = 1.0f
        val scaleModifier = (overlayScale + 50) / 100f
        baseScale *= scaleModifier

        val width = (defaultBitmap.width * baseScale * controlData.individualScale).toInt()
        val height = (defaultBitmap.height * baseScale * controlData.individualScale).toInt()

        val scaledDefaultBitmap = Bitmap.createScaledBitmap(defaultBitmap, width, height, true)
        val scaledPressedBitmap = Bitmap.createScaledBitmap(pressedBitmap, width, height, true)

        val drawableX = (min.x + (max.x - min.x) * position.first).toInt()
        val drawableY = (min.y + (max.y - min.y) * position.second).toInt()

        val button = InputOverlayDrawableButton(
            res,
            scaledDefaultBitmap,
            scaledPressedBitmap,
            buttonId,
            controlData
        )

        button.setBounds(
            drawableX - width / 2,
            drawableY - height / 2,
            drawableX + width / 2,
            drawableY + height / 2
        )
        button.setOpacity(overlayOpacity * 255 / 100)

        return button
    }

    private fun initializeOverlayDpad(
        context: Context,
        windowSize: Pair<Point, Point>,
        defaultResId: Int,
        pressedOneDirectionResId: Int,
        pressedTwoDirectionsResId: Int,
        controlData: OverlayControlData,
        position: Pair<Double, Double>
    ): InputOverlayDrawableDpad {
        val res = context.resources

        val defaultDrawable = ContextCompat.getDrawable(context, defaultResId) as VectorDrawable
        val pressedOneDirectionDrawable = ContextCompat.getDrawable(context, pressedOneDirectionResId) as VectorDrawable
        val pressedTwoDirectionsDrawable = ContextCompat.getDrawable(context, pressedTwoDirectionsResId) as VectorDrawable

        val defaultBitmap = Bitmap.createBitmap(
            defaultDrawable.intrinsicWidth,
            defaultDrawable.intrinsicHeight,
            Bitmap.Config.ARGB_8888
        )
        val pressedOneDirectionBitmap = Bitmap.createBitmap(
            pressedOneDirectionDrawable.intrinsicWidth,
            pressedOneDirectionDrawable.intrinsicHeight,
            Bitmap.Config.ARGB_8888
        )
        val pressedTwoDirectionsBitmap = Bitmap.createBitmap(
            pressedTwoDirectionsDrawable.intrinsicWidth,
            pressedTwoDirectionsDrawable.intrinsicHeight,
            Bitmap.Config.ARGB_8888
        )

        val defaultCanvas = Canvas(defaultBitmap)
        val pressedOneDirectionCanvas = Canvas(pressedOneDirectionBitmap)
        val pressedTwoDirectionsCanvas = Canvas(pressedTwoDirectionsBitmap)

        defaultDrawable.setBounds(0, 0, defaultDrawable.intrinsicWidth, defaultDrawable.intrinsicHeight)
        pressedOneDirectionDrawable.setBounds(0, 0, pressedOneDirectionDrawable.intrinsicWidth, pressedOneDirectionDrawable.intrinsicHeight)
        pressedTwoDirectionsDrawable.setBounds(0, 0, pressedTwoDirectionsDrawable.intrinsicWidth, pressedTwoDirectionsDrawable.intrinsicHeight)

        defaultDrawable.draw(defaultCanvas)
        pressedOneDirectionDrawable.draw(pressedOneDirectionCanvas)
        pressedTwoDirectionsDrawable.draw(pressedTwoDirectionsCanvas)

        val min = windowSize.first
        val max = windowSize.second

        var baseScale = 1.0f
        val scaleModifier = (overlayScale + 50) / 100f
        baseScale *= scaleModifier

        val width = (defaultBitmap.width * baseScale * controlData.individualScale).toInt()
        val height = (defaultBitmap.height * baseScale * controlData.individualScale).toInt()

        val scaledDefaultBitmap = Bitmap.createScaledBitmap(defaultBitmap, width, height, true)
        val scaledPressedOneDirectionBitmap = Bitmap.createScaledBitmap(pressedOneDirectionBitmap, width, height, true)
        val scaledPressedTwoDirectionsBitmap = Bitmap.createScaledBitmap(pressedTwoDirectionsBitmap, width, height, true)

        val drawableX = (min.x + (max.x - min.x) * position.first).toInt()
        val drawableY = (min.y + (max.y - min.y) * position.second).toInt()

        val dpad = InputOverlayDrawableDpad(
            res,
            scaledDefaultBitmap,
            scaledPressedOneDirectionBitmap,
            scaledPressedTwoDirectionsBitmap
        )
        dpad.individualScale = controlData.individualScale

        dpad.setBounds(
            drawableX - width / 2,
            drawableY - height / 2,
            drawableX + width / 2,
            drawableY + height / 2
        )
        dpad.setPosition(drawableX - width / 2, drawableY - height / 2)
        dpad.setOpacity(overlayOpacity * 255 / 100)

        return dpad
    }
}
