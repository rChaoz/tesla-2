package ro.mateitrandafir.tesla2.ui.components

import android.util.Log
import androidx.compose.animation.core.animateIntOffsetAsState
import androidx.compose.animation.core.animateOffsetAsState
import androidx.compose.foundation.Image
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.offset
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.round
import ro.mateitrandafir.tesla2.R
import ro.mateitrandafir.tesla2.TAG
import kotlin.math.sqrt

@Composable
fun Joystick(offset: Offset, onOffsetChange: (Offset) -> Unit, modifier: Modifier = Modifier) = Box(modifier, contentAlignment = Alignment.Center) {
    var size by remember { mutableStateOf(IntSize(0, 0)) }
    val offsetAnimated by animateOffsetAsState(Offset(offset.x * size.width, -offset.y * size.height), label = "JoystickOffset")
    Image(painter = painterResource(R.drawable.joystick_outer), contentDescription = null)
    Image(
        painter = painterResource(R.drawable.joystick),
        contentDescription = null,
        modifier = Modifier
            .onSizeChanged { size = it }
            .pointerInput(true) {
                detectDragGestures(onDragEnd = { onOffsetChange(Offset(0f, 0f)) }) { change, _ ->
                    val r = size.width.toFloat() * .9f
                    var x = change.position.x - size.width / 2f
                    var y = size.height / 2f - change.position.y
                    val sq = x * x + y * y
                    if (sq > r * r) {
                        val div = sqrt(sq / (r * r))
                        x /= div
                        y /= div
                    }
                    onOffsetChange(Offset(x / size.width, y / size.height))
                }
            }
            .offset { offsetAnimated.round() }
    )
}