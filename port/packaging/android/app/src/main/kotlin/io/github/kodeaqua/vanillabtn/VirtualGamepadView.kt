package io.github.kodeaqua.vanillabtn

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.os.FileObserver
import android.os.Handler
import android.os.Looper
import android.util.AtomicFile
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import org.json.JSONArray
import org.json.JSONObject
import org.libsdl.app.SDLActivity
import java.io.File
import java.io.FileOutputStream
import kotlin.math.atan2
import kotlin.math.hypot

class VirtualGamepadView(context: Context, private val userDataDir: File) : View(context) {
    private val density = resources.displayMetrics.density
    private val settingsFile = File(userDataDir, "settings.json")
    private val layoutFile = File(userDataDir, "virtual_gamepad_layout.json")
    private val mainHandler = Handler(Looper.getMainLooper())
    private var gamepadScale = 1f
    private var editMode = false
    private var draggedButton = -1
    private var dragPointerId = -1
    private val toolbarHeight get() = 48f * density

    /* F1-F12, Print Screen and Scroll Lock are reserved as internal touch-pad
     * input keys. The native Android host translates them using saved mappings. */
    private val virtualKeyCodes = intArrayOf(
        KeyEvent.KEYCODE_F1, KeyEvent.KEYCODE_F2, KeyEvent.KEYCODE_F3, KeyEvent.KEYCODE_F4,
        KeyEvent.KEYCODE_F5, KeyEvent.KEYCODE_F6, KeyEvent.KEYCODE_F7, KeyEvent.KEYCODE_F8,
        KeyEvent.KEYCODE_F9, KeyEvent.KEYCODE_F10, KeyEvent.KEYCODE_F11, KeyEvent.KEYCODE_F12,
        KeyEvent.KEYCODE_SYSRQ, KeyEvent.KEYCODE_SCROLL_LOCK
    )

    private val settingsObserver = object : FileObserver(
        userDataDir.absolutePath,
        CLOSE_WRITE or MOVED_TO or CREATE or DELETE
    ) {
        override fun onEvent(event: Int, path: String?) {
            if (path == null || path == settingsFile.name)
                mainHandler.post { refreshSettings() }
        }
    }

    private val basePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(80, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 3f * density
    }
    private val baseFillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(40, 0, 0, 0)
        style = Paint.Style.FILL
    }
    private val knobPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(180, 255, 255, 255)
        style = Paint.Style.FILL
    }
    private val buttonPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(70, 0, 0, 0)
        style = Paint.Style.FILL
    }
    private val buttonBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(120, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 2f * density
    }
    private val buttonPressedPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(140, 255, 255, 255)
        style = Paint.Style.FILL
    }
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 18f * density
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }
    private val smallTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(220, 255, 255, 255)
        textSize = 12f * density
        textAlign = Paint.Align.CENTER
        isFakeBoldText = true
    }
    private val editorPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(235, 27, 35, 42)
        style = Paint.Style.FILL
    }
    private val editorBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        style = Paint.Style.STROKE
        strokeWidth = 2f * density
    }

    private var stickActive = false
    private var stickPointerId = -1
    private var stickBaseX = 0f
    private var stickBaseY = 0f
    private var stickKnobX = 0f
    private var stickKnobY = 0f
    private val maxStickRadius get() = 55f * density * gamepadScale
    private val deadzoneRadius get() = 8f * density * gamepadScale
    private val baseCircleRadius get() = 55f * density * gamepadScale
    private val knobRadius get() = 26f * density * gamepadScale
    private var activeDpadUp = false
    private var activeDpadDown = false
    private var activeDpadLeft = false
    private var activeDpadRight = false

    data class PadButton(
        val label: String,
        val controlIndex: Int,
        var keyCode: Int,
        var cx: Float = 0f,
        var cy: Float = 0f,
        var radius: Float = 28f,
        var isRect: Boolean = false,
        var rect: RectF = RectF(),
        var pressed: Boolean = false,
        var pointerId: Int = -1,
        var customPosition: Boolean = false,
        var normalizedX: Float = 0f,
        var normalizedY: Float = 0f
    )

    private val buttons = mutableListOf(
        PadButton("X", 4, virtualKeyCodes[4]),
        PadButton("O", 5, virtualKeyCodes[5]),
        PadButton("□", 6, virtualKeyCodes[6]),
        PadButton("△", 7, virtualKeyCodes[7]),
        PadButton("L1", 8, virtualKeyCodes[8], isRect = true),
        PadButton("L2", 10, virtualKeyCodes[10], isRect = true),
        PadButton("R1", 9, virtualKeyCodes[9], isRect = true),
        PadButton("R2", 11, virtualKeyCodes[11], isRect = true),
        PadButton("SELECT", 12, virtualKeyCodes[12], isRect = true),
        PadButton("START", 13, virtualKeyCodes[13], isRect = true)
    )
    init {
        setWillNotDraw(false)
        loadButtonLayout()
        refreshSettings()
        settingsObserver.startWatching()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        layoutButtons(w.toFloat(), h.toFloat())
    }

    private fun refreshSettings() {
        val wasEditing = editMode
        var visible = true
        var scale = 100
        var editing = false
        try {
            if (settingsFile.isFile) {
                val controls = JSONObject(settingsFile.readText()).optJSONObject("controls")
                if (controls != null) {
                    visible = controls.optBoolean("virtual_gamepad", true)
                    scale = controls.optInt("virtual_gamepad_size", 100).coerceIn(70, 140)
                    editing = controls.optBoolean("virtual_gamepad_edit", false)
                }
            }
        } catch (_: Exception) {
            // A partially written settings file must not hide the default controls.
        }
        gamepadScale = scale / 100f
        editMode = visible && editing
        visibility = if (visible) VISIBLE else GONE
        isClickable = editMode
        if (!visible) {
            releaseAllButtons()
            resetAnalogStick()
        }
        if (!editMode && wasEditing) {
            draggedButton = -1
            dragPointerId = -1
            resetAnalogStick()
        }
        if (width > 0 && height > 0) layoutButtons(width.toFloat(), height.toFloat())
        invalidate()
    }

    private fun loadButtonLayout() {
        try {
            val saved = JSONObject(layoutFile.readText()).optJSONArray("buttons") ?: return
            for (i in 0 until minOf(saved.length(), buttons.size)) {
                val item = saved.optJSONObject(i) ?: continue
                val x = item.optDouble("x", Double.NaN).toFloat()
                val y = item.optDouble("y", Double.NaN).toFloat()
                if (x.isFinite() && y.isFinite()) {
                    buttons[i].normalizedX = x.coerceIn(0.03f, 0.97f)
                    buttons[i].normalizedY = y.coerceIn(0.03f, 0.97f)
                    buttons[i].customPosition = true
                }
            }
        } catch (_: Exception) {
            // Invalid custom layout falls back to the built-in positions.
        }
    }

    private fun saveButtonLayout() {
        try {
            val array = JSONArray()
            for (button in buttons) {
                val item = JSONObject()
                    .put("x", button.normalizedX.toDouble())
                    .put("y", button.normalizedY.toDouble())
                array.put(item)
            }
            val contents = JSONObject().put("buttons", array).toString()
            val temp = File(userDataDir, "virtual_gamepad_layout.json.tmp")
            temp.writeText(contents)
            if (!temp.renameTo(layoutFile)) {
                layoutFile.delete()
                if (!temp.renameTo(layoutFile)) temp.delete()
            }
        } catch (_: Exception) {
            // Keep the in-memory layout active for this session if storage is unavailable.
        }
    }

    private fun writeEditMode(enabled: Boolean) {
        var output: FileOutputStream? = null
        try {
            val root = if (settingsFile.isFile) JSONObject(settingsFile.readText()) else JSONObject()
            val controls = root.optJSONObject("controls") ?: JSONObject()
            controls.put("virtual_gamepad_edit", enabled)
            root.put("controls", controls)
            val atomic = AtomicFile(settingsFile)
            val stream = atomic.startWrite()
            output = stream
            stream.write(root.toString(2).toByteArray(Charsets.UTF_8))
            atomic.finishWrite(stream)
            output = null
        } catch (_: Exception) {
            if (output != null) AtomicFile(settingsFile).failWrite(output)
            // The editor remains usable even if its close preference cannot be saved.
        }
    }

    private fun finishEditing() {
        saveButtonLayout()
        editMode = false
        isClickable = false
        writeEditMode(false)
        invalidate()
    }

    private fun resetButtonLayout() {
        layoutFile.delete()
        for (button in buttons) {
            button.customPosition = false
            button.normalizedX = 0f
            button.normalizedY = 0f
        }
        if (width > 0 && height > 0) layoutButtons(width.toFloat(), height.toFloat())
        invalidate()
    }

    private fun setButtonCenter(button: PadButton, x: Float, y: Float) {
        if (button.isRect) {
            val halfWidth = button.rect.width() / 2f
            val halfHeight = button.rect.height() / 2f
            button.rect.set(x - halfWidth, y - halfHeight, x + halfWidth, y + halfHeight)
            button.cx = x
            button.cy = y
        } else {
            button.cx = x
            button.cy = y
        }
    }

    private fun buttonCenter(button: PadButton): Pair<Float, Float> =
        if (button.isRect) Pair(button.rect.centerX(), button.rect.centerY())
        else Pair(button.cx, button.cy)

    private fun layoutButtons(w: Float, h: Float) {
        if (w <= 0f || h <= 0f) return
        val scale = gamepadScale
        val btnRadius = 28f * density * scale
        val rightMargin = 85f * density * scale
        val bottomMargin = 85f * density * scale
        val btnCenterX = w - rightMargin
        val btnCenterY = h - bottomMargin
        val spacing = 46f * density * scale

        for (btn in buttons) {
            btn.radius = btnRadius
            when (btn.controlIndex) {
                4 -> { btn.cx = btnCenterX; btn.cy = btnCenterY + spacing }
                5 -> { btn.cx = btnCenterX + spacing; btn.cy = btnCenterY }
                6 -> { btn.cx = btnCenterX - spacing; btn.cy = btnCenterY }
                7 -> { btn.cx = btnCenterX; btn.cy = btnCenterY - spacing }
                8, 10, 9, 11 -> {
                    btn.isRect = true
                    val rw = 58f * density * scale
                    val rh = 34f * density * scale
                    val pad = 20f * density
                    val gap = 10f * density * scale
                    val left = when (btn.controlIndex) {
                        8 -> pad
                        10 -> pad + rw + gap
                        9 -> w - pad - rw
                        else -> w - pad - rw * 2f - gap
                    }
                    btn.rect.set(left, 16f * density, left + rw, 16f * density + rh)
                    btn.cx = btn.rect.centerX()
                    btn.cy = btn.rect.centerY()
                }
                12, 13 -> {
                    btn.isRect = true
                    val rw = 72f * density * scale
                    val rh = 32f * density * scale
                    val cx = w * if (btn.controlIndex == 12) 0.40f else 0.60f
                    val cy = h - 34f * density
                    btn.rect.set(cx - rw / 2, cy - rh / 2, cx + rw / 2, cy + rh / 2)
                    btn.cx = cx
                    btn.cy = cy
                }
            }
            btn.keyCode = virtualKeyCodes[btn.controlIndex]
            if (btn.customPosition) {
                setButtonCenter(btn, w * btn.normalizedX, h * btn.normalizedY)
            } else {
                val center = buttonCenter(btn)
                btn.normalizedX = center.first / w
                btn.normalizedY = center.second / h
            }
        }

        /* Keep the right shoulder buttons separated, including layouts saved
         * by older builds whose default R1/R2 positions overlapped. */
        val r1 = buttons.firstOrNull { it.controlIndex == 9 }
        val r2 = buttons.firstOrNull { it.controlIndex == 11 }
        if (r1 != null && r2 != null &&
            r1.rect.left < r2.rect.right && r2.rect.left < r1.rect.right &&
            r1.rect.top < r2.rect.bottom && r2.rect.top < r1.rect.bottom) {
            val gap = 10f * density * scale
            val r2LeftCenter = r1.rect.left - gap - r2.rect.width() / 2f
            if (r2LeftCenter >= r2.rect.width() / 2f) {
                setButtonCenter(r2, r2LeftCenter, r2.rect.centerY())
                r2.normalizedX = r2.cx / w
            } else {
                val r1RightCenter = r2.rect.right + gap + r1.rect.width() / 2f
                if (r1RightCenter <= w - r1.rect.width() / 2f) {
                    setButtonCenter(r1, r1RightCenter, r1.rect.centerY())
                    r1.normalizedX = r1.cx / w
                }
            }
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        if (stickActive) {
            canvas.drawCircle(stickBaseX, stickBaseY, baseCircleRadius, baseFillPaint)
            canvas.drawCircle(stickBaseX, stickBaseY, baseCircleRadius, basePaint)
            canvas.drawCircle(stickKnobX, stickKnobY, knobRadius, knobPaint)
        }
        for (button in buttons) {
            val fill = if (button.pressed) buttonPressedPaint else buttonPaint
            if (button.isRect) {
                val round = 8f * density
                canvas.drawRoundRect(button.rect, round, round, fill)
                canvas.drawRoundRect(button.rect, round, round, buttonBorderPaint)
                val textY = button.rect.centerY() - (smallTextPaint.descent() + smallTextPaint.ascent()) / 2
                canvas.drawText(button.label, button.rect.centerX(), textY, smallTextPaint)
            } else {
                canvas.drawCircle(button.cx, button.cy, button.radius, fill)
                canvas.drawCircle(button.cx, button.cy, button.radius, buttonBorderPaint)
                val textY = button.cy - (textPaint.descent() + textPaint.ascent()) / 2
                canvas.drawText(button.label, button.cx, textY, textPaint)
            }
        }
        if (editMode) drawEditorToolbar(canvas)
    }

    private fun drawEditorToolbar(canvas: Canvas) {
        canvas.drawRect(0f, 0f, width.toFloat(), toolbarHeight, editorPaint)
        val buttonWidth = minOf(124f * density, width * 0.40f)
        val top = 7f * density
        val bottom = toolbarHeight - 7f * density
        val done = RectF(12f * density, top, 12f * density + buttonWidth, bottom)
        val reset = RectF(width - 12f * density - buttonWidth, top, width - 12f * density, bottom)
        canvas.drawRoundRect(done, 6f * density, 6f * density, buttonPressedPaint)
        canvas.drawRoundRect(done, 6f * density, 6f * density, editorBorderPaint)
        canvas.drawRoundRect(reset, 6f * density, 6f * density, buttonPressedPaint)
        canvas.drawRoundRect(reset, 6f * density, 6f * density, editorBorderPaint)
        val textY = toolbarHeight / 2f - (smallTextPaint.descent() + smallTextPaint.ascent()) / 2
        canvas.drawText("Done", done.centerX(), textY, smallTextPaint)
        canvas.drawText("Reset", reset.centerX(), textY, smallTextPaint)
        val hint = Paint(smallTextPaint).apply {
            textSize = 10f * density
            color = Color.WHITE
        }
        canvas.drawText("Drag buttons to move", width / 2f, toolbarHeight + 16f * density, hint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val action = event.actionMasked
        if (event.pointerCount == 0) {
            if (action == MotionEvent.ACTION_CANCEL) {
                releaseAllButtons()
                resetAnalogStick()
                return true
            }
            return false
        }
        val actionIndex = event.actionIndex.coerceIn(0, event.pointerCount - 1)
        val pointerId = event.getPointerId(actionIndex)
        val x = event.getX(actionIndex)
        val y = event.getY(actionIndex)

        if (editMode) {
            when (action) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    if (y <= toolbarHeight) {
                        val middle = width / 2f
                        if (x < middle) finishEditing() else resetButtonLayout()
                        return true
                    }
                    draggedButton = buttons.indexOf(findHitButton(x, y))
                    dragPointerId = if (draggedButton >= 0) pointerId else -1
                    return true
                }
                MotionEvent.ACTION_MOVE -> {
                    if (dragPointerId >= 0) {
                        val pointerIndex = event.findPointerIndex(dragPointerId)
                        if (pointerIndex >= 0 && draggedButton in buttons.indices) {
                            val button = buttons[draggedButton]
                            val px = event.getX(pointerIndex)
                            val py = event.getY(pointerIndex)
                            val marginX = if (button.isRect) button.rect.width() / 2 else button.radius
                            val marginY = if (button.isRect) button.rect.height() / 2 else button.radius
                            val centerX = px.coerceIn(marginX, width - marginX)
                            val centerY = py.coerceIn(maxOf(toolbarHeight + marginY, marginY), height - marginY)
                            setButtonCenter(button, centerX, centerY)
                            button.customPosition = true
                            button.normalizedX = centerX / width
                            button.normalizedY = centerY / height
                            invalidate()
                        }
                    }
                    return true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                    if (pointerId == dragPointerId) {
                        saveButtonLayout()
                        draggedButton = -1
                        dragPointerId = -1
                    }
                    return true
                }
                MotionEvent.ACTION_CANCEL -> {
                    draggedButton = -1
                    dragPointerId = -1
                    return true
                }
            }
            return true
        }

        when (action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val button = findHitButton(x, y)
                if (button != null) {
                    button.pressed = true
                    button.pointerId = pointerId
                    SDLActivity.onNativeKeyDown(button.keyCode)
                    invalidate()
                    return true
                }
                if (x < width * 0.45f && y > height * 0.20f && !stickActive) {
                    stickActive = true
                    stickPointerId = pointerId
                    stickBaseX = x
                    stickBaseY = y
                    stickKnobX = x
                    stickKnobY = y
                    invalidate()
                    return true
                }
                return false
            }
            MotionEvent.ACTION_MOVE -> {
                var changed = false
                for (i in 0 until event.pointerCount) {
                    val pid = event.getPointerId(i)
                    val px = event.getX(i)
                    val py = event.getY(i)
                    if (stickActive && pid == stickPointerId) {
                        val oldX = stickKnobX
                        val oldY = stickKnobY
                        val oldUp = activeDpadUp
                        val oldDown = activeDpadDown
                        val oldLeft = activeDpadLeft
                        val oldRight = activeDpadRight
                        updateAnalogStick(px, py)
                        changed = changed || hypot(stickKnobX - oldX, stickKnobY - oldY) >= 4f * density ||
                            oldUp != activeDpadUp || oldDown != activeDpadDown ||
                            oldLeft != activeDpadLeft || oldRight != activeDpadRight
                    } else {
                        for (button in buttons) {
                            if (button.pointerId == pid && !isHit(button, px, py) && button.pressed) {
                                button.pressed = false
                                button.pointerId = -1
                                SDLActivity.onNativeKeyUp(button.keyCode)
                                changed = true
                            }
                        }
                    }
                }
                if (changed) invalidate()
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                if (stickActive && pointerId == stickPointerId) resetAnalogStick()
                for (button in buttons) {
                    if (button.pointerId == pointerId) {
                        if (button.pressed) {
                            button.pressed = false
                            SDLActivity.onNativeKeyUp(button.keyCode)
                        }
                        button.pointerId = -1
                    }
                }
                invalidate()
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                releaseAllButtons()
                resetAnalogStick()
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    private fun findHitButton(x: Float, y: Float): PadButton? {
        for (button in buttons) {
            if (button.isRect) {
                if (button.rect.contains(x, y)) return button
            } else if (hypot(x - button.cx, y - button.cy) <= button.radius) {
                return button
            }
        }
        var closest: PadButton? = null
        var closestDistance = Float.MAX_VALUE
        for (button in buttons) {
            if (button.isRect) {
                val expand = 12f * density
                val rect = RectF(button.rect.left - expand, button.rect.top - expand,
                    button.rect.right + expand, button.rect.bottom + expand)
                if (rect.contains(x, y)) {
                    val distance = hypot(x - button.rect.centerX(), y - button.rect.centerY())
                    if (distance < closestDistance) { closest = button; closestDistance = distance }
                }
            } else {
                val distance = hypot(x - button.cx, y - button.cy)
                if (distance <= button.radius + 14f * density && distance < closestDistance) {
                    closest = button
                    closestDistance = distance
                }
            }
        }
        return closest
    }

    private fun isHit(button: PadButton, x: Float, y: Float): Boolean = if (button.isRect) {
        val expand = 14f * density
        x >= button.rect.left - expand && x <= button.rect.right + expand &&
            y >= button.rect.top - expand && y <= button.rect.bottom + expand
    } else {
        hypot(x - button.cx, y - button.cy) <= button.radius + 14f * density
    }

    private fun updateAnalogStick(x: Float, y: Float) {
        val dx = x - stickBaseX
        val dy = y - stickBaseY
        val dist = hypot(dx, dy)
        if (dist > maxStickRadius) {
            stickKnobX = stickBaseX + dx / dist * maxStickRadius
            stickKnobY = stickBaseY + dy / dist * maxStickRadius
        } else {
            stickKnobX = x
            stickKnobY = y
        }
        if (dist > deadzoneRadius) {
            val absX = kotlin.math.abs(dx)
            val absY = kotlin.math.abs(dy)
            val major = maxOf(absX, absY)
            val minor = minOf(absX, absY)
            val alreadyDiagonal = (activeDpadLeft || activeDpadRight) &&
                (activeDpadUp || activeDpadDown)
            val diagonalThreshold = if (alreadyDiagonal) 0.28f else 0.38f
            val diagonal = minor >= major * diagonalThreshold
            val currentHorizontal = activeDpadLeft || activeDpadRight
            val currentVertical = activeDpadUp || activeDpadDown
            val horizontal = when {
                diagonal -> true
                currentHorizontal && absX >= absY * 0.82f -> true
                currentVertical && absY >= absX * 0.82f -> false
                else -> absX >= absY
            }
            val up = dy < 0f && (diagonal || !horizontal)
            val down = dy > 0f && (diagonal || !horizontal)
            val left = dx < 0f && (diagonal || horizontal)
            val right = dx > 0f && (diagonal || horizontal)
            syncDpad(up, down, left, right)
        } else {
            releaseAllDpad()
        }
    }

    private fun syncDpad(up: Boolean, down: Boolean, left: Boolean, right: Boolean) {
        /* Press new directions before releasing old ones so SDL never samples a
         * neutral movement frame while the finger changes direction. */
        if (up && !activeDpadUp) SDLActivity.onNativeKeyDown(virtualKeyCodes[0])
        if (down && !activeDpadDown) SDLActivity.onNativeKeyDown(virtualKeyCodes[2])
        if (left && !activeDpadLeft) SDLActivity.onNativeKeyDown(virtualKeyCodes[3])
        if (right && !activeDpadRight) SDLActivity.onNativeKeyDown(virtualKeyCodes[1])
        if (!up && activeDpadUp) SDLActivity.onNativeKeyUp(virtualKeyCodes[0])
        if (!down && activeDpadDown) SDLActivity.onNativeKeyUp(virtualKeyCodes[2])
        if (!left && activeDpadLeft) SDLActivity.onNativeKeyUp(virtualKeyCodes[3])
        if (!right && activeDpadRight) SDLActivity.onNativeKeyUp(virtualKeyCodes[1])
        activeDpadUp = up
        activeDpadDown = down
        activeDpadLeft = left
        activeDpadRight = right
    }

    private fun releaseAllDpad() {
        if (activeDpadUp) { SDLActivity.onNativeKeyUp(virtualKeyCodes[0]); activeDpadUp = false }
        if (activeDpadRight) { SDLActivity.onNativeKeyUp(virtualKeyCodes[1]); activeDpadRight = false }
        if (activeDpadDown) { SDLActivity.onNativeKeyUp(virtualKeyCodes[2]); activeDpadDown = false }
        if (activeDpadLeft) { SDLActivity.onNativeKeyUp(virtualKeyCodes[3]); activeDpadLeft = false }
    }

    private fun releaseAllButtons() {
        for (button in buttons) {
            if (button.pressed) SDLActivity.onNativeKeyUp(button.keyCode)
            button.pressed = false
            button.pointerId = -1
        }
    }

    private fun resetAnalogStick() {
        stickActive = false
        stickPointerId = -1
        releaseAllDpad()
    }

    override fun onDetachedFromWindow() {
        settingsObserver.stopWatching()
        releaseAllButtons()
        resetAnalogStick()
        super.onDetachedFromWindow()
    }
}
