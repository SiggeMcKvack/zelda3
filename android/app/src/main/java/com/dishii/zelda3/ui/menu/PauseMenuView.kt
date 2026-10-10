package com.dishii.zelda3.ui.menu

import android.content.Context
import android.graphics.Typeface
import android.util.AttributeSet
import android.util.Log
import android.view.FocusFinder
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.dishii.zelda3.R
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * Full-screen, controller-first pause menu.
 *
 * Tabs across the top (L1/R1), one list of [MenuItem] rows below, button hints in the footer.
 * Sub-pages are pushed with [push] and popped with B. Settings apply as they change; rows marked
 * `restart` set a flag that is reported through [onClosed] so the host can offer one restart.
 */
class PauseMenuView
    @JvmOverloads
    constructor(
        context: Context,
        attrs: AttributeSet? = null,
    ) : FrameLayout(context, attrs) {
        private companion object {
            const val TAG = "PauseMenuView"
            const val SLOT_COLUMNS = 5
            const val SPINNER_DELAY_MS = 150L
        }

        /** Called after the menu is hidden. */
        var onClosed: ((restartNeeded: Boolean) -> Unit)? = null

        /** Called right before the menu is shown. */
        var onOpened: (() -> Unit)? = null

        val isOpen: Boolean get() = visibility == VISIBLE

        private class Page(
            val title: String,
            val build: suspend () -> List<MenuItem>,
        ) {
            var focusIndex = -1
        }

        private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
        private var loadJob: Job? = null

        private var tabs: List<MenuTab> = emptyList()
        private var currentTab = 0
        private val tabFocus = HashMap<Int, Int>()
        private val stack = ArrayList<Page>()
        private var restartNeeded = false

        private val tabStrip: LinearLayout
        private val list: RecyclerView
        private val title: TextView
        private val loading: ProgressBar
        private val hints: TextView
        private val restartNotice: TextView
        private val adapter = RowAdapter()

        init {
            LayoutInflater.from(context).inflate(R.layout.pause_menu, this, true)
            setBackgroundColor(ContextCompat.getColor(context, R.color.menu_scrim))
            // Swallow all touches so nothing reaches the overlay or the game underneath
            isClickable = true
            visibility = GONE

            tabStrip = findViewById(R.id.menu_tab_strip)
            list = findViewById(R.id.menu_list)
            title = findViewById(R.id.menu_title)
            loading = findViewById(R.id.menu_loading)
            hints = findViewById(R.id.menu_hints)
            restartNotice = findViewById(R.id.menu_restart_notice)

            list.layoutManager = LinearLayoutManager(context)
            list.adapter = adapter
            // Rebinding a row in place keeps its focus; change animations would swap the view
            list.itemAnimator = null

            findViewById<View>(R.id.menu_close).setOnClickListener { close() }
            findViewById<View>(R.id.menu_hint_l).setOnClickListener { switchTab(-1) }
            findViewById<View>(R.id.menu_hint_r).setOnClickListener { switchTab(1) }
        }

        override fun onDetachedFromWindow() {
            super.onDetachedFromWindow()
            scope.coroutineContext.cancelChildren()
        }

        fun setTabs(newTabs: List<MenuTab>) {
            tabs = newTabs
            tabFocus.clear()
            currentTab = currentTab.coerceIn(0, (newTabs.size - 1).coerceAtLeast(0))
            tabStrip.removeAllViews()
            newTabs.forEachIndexed { index, tab ->
                val tabView =
                    TextView(context).apply {
                        text = tab.title
                        textSize = 16f
                        setTypeface(typeface, Typeface.BOLD)
                        setTextColor(ContextCompat.getColorStateList(context, R.color.menu_row_text))
                        background = ContextCompat.getDrawable(context, R.drawable.menu_tab_bg)
                        setPadding(dp(16), dp(6), dp(16), dp(6))
                        isFocusable = true
                        isClickable = true
                        setOnClickListener { selectTab(index, focusList = true) }
                        // TV pattern: moving focus across tabs switches the page
                        setOnFocusChangeListener { _, hasFocus ->
                            if (hasFocus && index != currentTab) selectTab(index, focusList = false)
                        }
                    }
                tabStrip.addView(
                    tabView,
                    LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply {
                        marginStart = dp(4)
                        marginEnd = dp(4)
                    },
                )
            }
            if (isOpen && newTabs.isNotEmpty()) selectTab(currentTab, focusList = list.hasFocus())
        }

        fun open() {
            if (isOpen || tabs.isEmpty()) return
            restartNeeded = false
            updateRestartNotice()
            onOpened?.invoke()
            visibility = VISIBLE
            alpha = 0f
            animate().alpha(1f).setDuration(150).start()
            tabFocus.clear()
            selectTab(0, focusList = true)
        }

        fun close() {
            if (!isOpen) return
            loadJob?.cancel()
            stack.clear()
            visibility = GONE
            val restart = restartNeeded
            restartNeeded = false
            onClosed?.invoke(restart)
        }

        /** Opens a sub-page on top of the current one. B returns. */
        fun push(
            pageTitle: String,
            build: suspend () -> List<MenuItem>,
        ) {
            saveFocus()
            stack += Page(pageTitle, build)
            load(focusList = true)
        }

        /** Rebuilds the current page (e.g. after an external change such as a new key binding). */
        fun reload() {
            if (!isOpen || stack.isEmpty()) return
            saveFocus()
            load(focusList = true)
        }

        /** Re-renders the visible rows after state changed outside the menu (e.g. a file picker result). */
        fun refresh() {
            if (isOpen) adapter.refresh()
        }

        /** For settings whose restart requirement is only known after applying them. */
        fun markRestartNeeded() {
            restartNeeded = true
            updateRestartNotice()
        }

        /**
         * Maps controller/remote keys onto the menu. Returns true if consumed; unconsumed D-pad
         * events fall through to normal Android focus navigation.
         */
        fun handleKey(event: KeyEvent): Boolean {
            if (!isOpen) return false
            val down = event.action == KeyEvent.ACTION_DOWN
            val firstDown = down && event.repeatCount == 0

            when (event.keyCode) {
                KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK, KeyEvent.KEYCODE_ESCAPE -> {
                    if (firstDown) back()
                    return true
                }

                KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_BUTTON_MODE, KeyEvent.KEYCODE_MENU -> {
                    if (firstDown) close()
                    return true
                }

                KeyEvent.KEYCODE_BUTTON_L1 -> {
                    if (firstDown) switchTab(-1)
                    return true
                }

                KeyEvent.KEYCODE_BUTTON_R1 -> {
                    if (firstDown) switchTab(1)
                    return true
                }
            }

            // Focus can be lost while a page swaps its rows; pull it back instead of letting
            // input reach the game surface.
            if (findFocus() == null) {
                if (firstDown) focusRow(stack.lastOrNull()?.focusIndex ?: -1)
                return true
            }

            when (event.keyCode) {
                KeyEvent.KEYCODE_BUTTON_A -> {
                    if (firstDown) findFocus()?.performClick()
                    return true
                }

                KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT -> {
                    val item = focusedItem() ?: return false
                    if (!down) return item.isAdjustable()
                    return adjust(item, if (event.keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) 1 else -1)
                }

                KeyEvent.KEYCODE_DPAD_UP -> {
                    // Top row goes to the *current* tab, not whichever tab is geometrically closest
                    if (firstDown && focusedRowIndex() == adapter.firstFocusableRow() && !canMoveUpWithinRow()) {
                        saveFocus()
                        tabStrip.getChildAt(currentTab)?.requestFocusFromTouch()
                        return true
                    }
                }

                KeyEvent.KEYCODE_DPAD_DOWN -> {
                    if (firstDown && tabStrip.hasFocus()) {
                        focusRow(stack.lastOrNull()?.focusIndex ?: -1)
                        return true
                    }
                }
            }
            return false
        }

        // ---- Navigation ----

        private fun selectTab(
            index: Int,
            focusList: Boolean,
        ) {
            if (index !in tabs.indices) return
            if (stack.size == 1) {
                saveFocus()
                tabFocus[currentTab] = stack[0].focusIndex
            }
            currentTab = index
            for (i in 0 until tabStrip.childCount) tabStrip.getChildAt(i).isSelected = i == index
            tabStrip.getChildAt(index)?.let { tab ->
                (tabStrip.parent as? HorizontalScrollView)?.smoothScrollTo(tab.left - dp(48), 0)
            }
            stack.clear()
            stack += Page(tabs[index].title, tabs[index].build).also { it.focusIndex = tabFocus[index] ?: -1 }
            load(focusList)
        }

        private fun switchTab(direction: Int) {
            if (tabs.isEmpty()) return
            selectTab((currentTab + direction + tabs.size) % tabs.size, focusList = true)
        }

        private fun back() {
            if (stack.size > 1) {
                stack.removeAt(stack.lastIndex)
                // Rebuild the parent so it reflects anything changed in the sub-page
                load(focusList = true)
            } else {
                close()
            }
        }

        private fun load(focusList: Boolean) {
            val page = stack.lastOrNull() ?: return
            loadJob?.cancel()
            updateChrome()
            loadJob =
                scope.launch {
                    val spinner =
                        launch {
                            delay(SPINNER_DELAY_MS)
                            loading.visibility = VISIBLE
                        }
                    val items =
                        try {
                            page.build()
                        } catch (e: CancellationException) {
                            throw e
                        } catch (e: Exception) {
                            Log.e(TAG, "Failed to build page '${page.title}'", e)
                            listOf(MenuItem.Info({ "Error: ${e.message}" }))
                        }
                    spinner.cancel()
                    loading.visibility = GONE
                    adapter.submit(items)
                    if (focusList || findFocus() == null) focusRow(page.focusIndex)
                }
        }

        private fun updateChrome() {
            if (stack.size > 1) {
                title.text = stack.joinToString("  ›  ") { it.title }
                title.visibility = VISIBLE
            } else {
                title.visibility = GONE
            }
            val backAction = if (stack.size > 1) "Back" else "Resume"
            hints.text = "Ⓐ Select    Ⓑ $backAction    L/R Tabs"
        }

        private fun updateRestartNotice() {
            restartNotice.visibility = if (restartNeeded) VISIBLE else INVISIBLE
        }

        private fun saveFocus() {
            val page = stack.lastOrNull() ?: return
            focusedRowIndex()?.let { page.focusIndex = it }
        }

        private fun focusedRowIndex(): Int? {
            val focused = list.findFocus() ?: return null
            val holder = list.findContainingViewHolder(focused) ?: return null
            return holder.bindingAdapterPosition.takeIf { it != RecyclerView.NO_POSITION }
        }

        private fun focusedItem(): MenuItem? = focusedRowIndex()?.let { adapter.rows.getOrNull(it) }

        /** True when focus sits inside a row with more focusables above it (the save slot grid). */
        private fun canMoveUpWithinRow(): Boolean {
            val focused = list.findFocus() ?: return false
            val row = list.findContainingItemView(focused) as? ViewGroup ?: return false
            return row !== focused && FocusFinder.getInstance().findNextFocus(row, focused, View.FOCUS_UP) != null
        }

        /**
         * Focuses a row, leaving touch mode if needed. In touch mode requestFocus() fails, focus
         * stays on the game surface and it swallows gamepad hat/stick motion, so the D-pad of
         * controllers that report it as axes (e.g. AYN Thor) could never move into the menu.
         */
        private fun focusRow(preferred: Int) {
            val target = adapter.nearestFocusableRow(preferred)
            if (target == null) {
                tabStrip.getChildAt(currentTab)?.requestFocusFromTouch()
                return
            }
            list.scrollToPosition(target)
            list.post {
                val view = list.findViewHolderForAdapterPosition(target)?.itemView
                if (view?.requestFocusFromTouch() != true) tabStrip.getChildAt(currentTab)?.requestFocusFromTouch()
            }
        }

        /** Keeps joystick motion away from the game while open; recovers focus if it was lost. */
        fun handleMotion(event: MotionEvent): Boolean {
            if (!isOpen || findFocus() != null) return false
            focusRow(stack.lastOrNull()?.focusIndex ?: -1)
            return true
        }

        // ---- Item behaviour ----

        private fun MenuItem.isAdjustable() = this is MenuItem.Toggle || this is MenuItem.Choice || this is MenuItem.Slider

        private fun activate(item: MenuItem) {
            if (!item.enabled()) return
            when (item) {
                is MenuItem.Toggle -> changed(item) { item.set(!item.get()) }
                is MenuItem.Choice -> adjust(item, 1)
                is MenuItem.Action -> {
                    item.onClick()
                    // Actions may change state other rows depend on (e.g. "Clear shader")
                    refresh()
                }

                else -> {}
            }
        }

        /** Moves a value one step. Returns true if the item takes left/right input. */
        private fun adjust(
            item: MenuItem,
            direction: Int,
        ): Boolean {
            if (!item.enabled()) return item.isAdjustable()
            when (item) {
                is MenuItem.Toggle -> {
                    val want = direction > 0
                    if (item.get() != want) changed(item) { item.set(want) }
                }

                is MenuItem.Choice -> {
                    val count = item.options.size
                    if (count > 1) changed(item) { item.set((item.get() + direction + count) % count) }
                }

                is MenuItem.Slider -> {
                    val value = (item.get() + direction * item.step).coerceIn(item.min, item.max)
                    if (value != item.get()) changed(item) { item.set(value) }
                }

                else -> {
                    return false
                }
            }
            return true
        }

        private inline fun changed(
            item: MenuItem,
            apply: () -> Unit,
        ) {
            apply()
            val restart =
                when (item) {
                    is MenuItem.Toggle -> item.restart
                    is MenuItem.Choice -> item.restart
                    else -> false
                }
            if (restart) markRestartNeeded()
            adapter.refresh()
        }

        private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

        // ---- Rows ----

        private inner class RowHolder(
            view: View,
        ) : RecyclerView.ViewHolder(view) {
            val label: TextView? = view.findViewById(R.id.row_label)
            val restart: ImageView? = view.findViewById(R.id.row_restart)
            val bar: ProgressBar? = view.findViewById(R.id.row_bar)
            val prev: ImageView? = view.findViewById(R.id.row_prev)
            val value: TextView? = view.findViewById(R.id.row_value)
            val next: ImageView? = view.findViewById(R.id.row_next)
            val slots: RecyclerView? = view.findViewById(R.id.row_slots)
        }

        private inner class RowAdapter : RecyclerView.Adapter<RowHolder>() {
            private val typeHeader = 0
            private val typeInfo = 1
            private val typeSlots = 2
            private val typeRow = 3

            private var all: List<MenuItem> = emptyList()
            var rows: List<MenuItem> = emptyList()
                private set

            init {
                setHasStableIds(true)
            }

            @Suppress("NotifyDataSetChanged") // Whole page swaps; rows have no finer-grained identity
            fun submit(items: List<MenuItem>) {
                all = items
                rows = items.filter { it.visible() }
                notifyDataSetChanged()
            }

            /** Re-evaluates values and visibility after a change. */
            @Suppress("NotifyDataSetChanged")
            fun refresh() {
                val newRows = all.filter { it.visible() }
                if (newRows == rows) {
                    notifyItemRangeChanged(0, rows.size)
                } else {
                    val focusedIndex = focusedRowIndex()
                    val focused = focusedIndex?.let { rows.getOrNull(it) }
                    rows = newRows
                    notifyDataSetChanged()
                    if (focusedIndex != null) {
                        // Stay on the same item, or its neighbour if it was just hidden
                        val index = rows.indexOf(focused).takeIf { it >= 0 } ?: focusedIndex.coerceAtMost(rows.lastIndex)
                        focusRow(index)
                    }
                }
            }

            fun isFocusable(position: Int): Boolean {
                val item = rows[position]
                return item !is MenuItem.Header && item !is MenuItem.Info && item.enabled()
            }

            fun firstFocusableRow(): Int? = rows.indices.firstOrNull { isFocusable(it) }

            /** [from] if focusable, else the next focusable row below it, else above it. */
            fun nearestFocusableRow(from: Int): Int? {
                if (from < 0 || rows.isEmpty()) return firstFocusableRow()
                val start = from.coerceAtMost(rows.lastIndex)
                return (start..rows.lastIndex).firstOrNull { isFocusable(it) } ?: (start downTo 0).firstOrNull { isFocusable(it) }
            }

            override fun getItemCount() = rows.size

            override fun getItemId(position: Int) = all.indexOf(rows[position]).toLong()

            override fun getItemViewType(position: Int) =
                when (rows[position]) {
                    is MenuItem.Header -> typeHeader
                    is MenuItem.Info -> typeInfo
                    is MenuItem.Slots -> typeSlots
                    else -> typeRow
                }

            override fun onCreateViewHolder(
                parent: ViewGroup,
                viewType: Int,
            ): RowHolder {
                val layout =
                    when (viewType) {
                        typeHeader -> R.layout.menu_header
                        typeInfo -> R.layout.menu_info
                        typeSlots -> R.layout.menu_slots
                        else -> R.layout.menu_row
                    }
                val holder = RowHolder(LayoutInflater.from(parent.context).inflate(layout, parent, false))
                holder.slots?.layoutManager = GridLayoutManager(parent.context, SLOT_COLUMNS)
                return holder
            }

            override fun onBindViewHolder(
                holder: RowHolder,
                position: Int,
            ) {
                when (val item = rows[position]) {
                    is MenuItem.Header -> holder.label?.text = item.title
                    is MenuItem.Info -> holder.label?.text = item.text()
                    is MenuItem.Slots -> holder.slots?.adapter = SlotAdapter(item)
                    else -> bindRow(holder, item)
                }
            }

            private fun bindRow(
                holder: RowHolder,
                item: MenuItem,
            ) {
                val enabled = item.enabled()
                holder.itemView.isEnabled = enabled
                holder.itemView.isFocusable = enabled
                holder.itemView.alpha = if (enabled) 1f else 0.38f
                holder.itemView.setOnClickListener { activate(item) }

                var label = ""
                var value: String? = null
                var restart = false
                var arrows = false
                var chevron = false
                var pill = false
                var destructive = false
                holder.bar?.visibility = GONE

                when (item) {
                    is MenuItem.Toggle -> {
                        label = item.label
                        value = if (item.get()) "ON" else "OFF"
                        restart = item.restart
                        pill = true
                    }

                    is MenuItem.Choice -> {
                        label = item.label
                        value = item.options.getOrElse(item.get()) { "" }
                        restart = item.restart
                        arrows = true
                    }

                    is MenuItem.Slider -> {
                        label = item.label
                        value = item.format(item.get())
                        arrows = true
                        holder.bar?.apply {
                            visibility = VISIBLE
                            max = item.max - item.min
                            progress = item.get() - item.min
                        }
                    }

                    is MenuItem.Action -> {
                        label = item.label
                        value = item.value()
                        chevron = item.opensPage
                        destructive = item.destructive
                    }

                    else -> {}
                }

                holder.label?.text = label
                holder.label?.setTextColor(
                    ContextCompat.getColorStateList(
                        context,
                        if (destructive) R.color.menu_row_text_destructive else R.color.menu_row_text,
                    ),
                )
                holder.value?.apply {
                    text = value
                    visibility = if (value != null) VISIBLE else GONE
                    background = if (pill) ContextCompat.getDrawable(context, R.drawable.menu_value_pill) else null
                }
                holder.restart?.visibility = if (restart) VISIBLE else GONE
                holder.prev?.apply {
                    visibility = if (arrows) VISIBLE else GONE
                    setOnClickListener { adjust(item, -1) }
                }
                holder.next?.apply {
                    visibility = if (arrows || chevron) VISIBLE else GONE
                    setOnClickListener { if (arrows) adjust(item, 1) else activate(item) }
                }
            }
        }

        private inner class SlotAdapter(
            private val item: MenuItem.Slots,
        ) : RecyclerView.Adapter<SlotAdapter.Holder>() {
            inner class Holder(
                view: View,
            ) : RecyclerView.ViewHolder(view) {
                val thumbnail: ImageView = view.findViewById(R.id.thumbnail)
                val name: TextView = view.findViewById(R.id.slot_name)
                val empty: TextView = view.findViewById(R.id.empty_indicator)
            }

            override fun getItemCount() = item.slots.size

            override fun onCreateViewHolder(
                parent: ViewGroup,
                viewType: Int,
            ): Holder {
                val view = LayoutInflater.from(parent.context).inflate(R.layout.item_save_slot, parent, false)
                view.isFocusable = true
                view.isClickable = true
                (view as? FrameLayout)?.foreground = ContextCompat.getDrawable(parent.context, R.drawable.menu_card_focus)
                view.setOnFocusChangeListener { v, hasFocus ->
                    val scale = if (hasFocus) 1.06f else 1f
                    v.animate().scaleX(scale).scaleY(scale).setDuration(120).start()
                }
                return Holder(view)
            }

            override fun onBindViewHolder(
                holder: Holder,
                position: Int,
            ) {
                val slot = item.slots[position]
                holder.name.text = slot.name
                holder.empty.visibility = if (slot.exists) GONE else VISIBLE
                if (slot.thumbnail != null) {
                    holder.thumbnail.setImageBitmap(slot.thumbnail)
                } else {
                    holder.thumbnail.setImageResource(R.drawable.ic_placeholder_snes)
                }
                holder.itemView.setOnClickListener { item.onPick(slot) }
            }
        }
    }
