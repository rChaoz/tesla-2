package ro.mateitrandafir.tesla2.ui

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.os.Build.VERSION
import android.os.Build.VERSION_CODES
import android.util.Log
import androidx.compose.animation.core.animateIntAsState
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilledIconToggleButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Popup
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.rememberMultiplePermissionsState
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import ro.mateitrandafir.tesla2.R
import ro.mateitrandafir.tesla2.TAG
import ro.mateitrandafir.tesla2.ui.components.Joystick
import ro.mateitrandafir.tesla2.ui.components.LoadingOverlay
import java.io.IOException
import java.util.UUID
import kotlin.time.Duration.Companion.milliseconds
import kotlin.time.Duration.Companion.seconds

@OptIn(ExperimentalPermissionsApi::class)
@Composable
fun RequestPermissions() {
    val snackbar = remember { SnackbarHostState() }

    val permissions = rememberMultiplePermissionsState(
        permissions =
        if (VERSION.SDK_INT >= VERSION_CODES.S)
            listOf(Manifest.permission.BLUETOOTH, Manifest.permission.BLUETOOTH_ADMIN, Manifest.permission.BLUETOOTH_CONNECT)
        else
            listOf(Manifest.permission.BLUETOOTH, Manifest.permission.BLUETOOTH_ADMIN)
    )
    if (!permissions.allPermissionsGranted) {
        if (permissions.shouldShowRationale) LaunchedEffect(true) {
            while (true) {
                snackbar.showSnackbar("App needs bluetooth permissions to work", actionLabel = "Grant")
                permissions.launchMultiplePermissionRequest()
            }
        } else LaunchedEffect(true) {
            permissions.launchMultiplePermissionRequest()
        }
    }

    Scaffold(snackbarHost = { SnackbarHost(snackbar) }) {
        Box(modifier = Modifier.padding(it)) {
            Image(
                painter = painterResource(R.drawable.background),
                contentDescription = "Background",
                contentScale = ContentScale.Crop,
                alignment = Alignment.Center,
                modifier = Modifier.matchParentSize()
            )
            if (permissions.allPermissionsGranted) Main(snackbar)
        }
    }
}

private val BT_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

@SuppressLint("MissingPermission")
@Composable
fun Main(snackbar: SnackbarHostState) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val adapter = remember { (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter }

    var socket by remember { mutableStateOf<BluetoothSocket?>(null) }
    if (socket == null) Popup(alignment = Alignment.Center) {
        Surface(shape = MaterialTheme.shapes.large, tonalElevation = 2.dp, shadowElevation = 2.dp) {
            var isConnecting by remember { mutableStateOf(false) }
            LoadingOverlay(isLoading = isConnecting) {
                LazyColumn(modifier = Modifier.fillMaxSize(.6f)) {
                    items(adapter.bondedDevices.toList()) {
                        Surface(onClick = {
                            isConnecting = true
                            scope.launch {
                                try {
                                    withContext(Dispatchers.IO) {
                                        socket = it.createInsecureRfcommSocketToServiceRecord(BT_UUID).apply { connect() }
                                    }
                                } catch (e: IOException) {
                                    Log.e(TAG, "Connection failed", e)
                                    scope.launch { snackbar.showSnackbar("Connection failed") }
                                } finally {
                                    isConnecting = false
                                }
                            }
                        }, modifier = Modifier.fillMaxWidth()) {
                            Text(text = it.name, style = MaterialTheme.typography.titleMedium, modifier = Modifier.padding(16.dp))
                        }
                    }
                }
            }
        }
    }

    // Distances data
    var distances by remember { mutableStateOf(emptyList<Int>()) }

    // Read incoming arduino data
    LaunchedEffect(socket) {
        if (socket == null) return@LaunchedEffect
        withContext(Dispatchers.IO) {
            val reader = socket?.inputStream?.reader() ?: run {
                Log.e(TAG, "Socket.inputStream is null")
                return@withContext
            }
            while (true) try {
                reader.forEachLine {
                    Log.d(TAG, it)
                    distances = it.split(',').map(String::toInt)
                }
            } catch (e: IOException) {
                Log.e(TAG, "SocketError", e)
                delay(1.seconds)
                if (socket?.isConnected == false) {
                    socket = null
                    break
                }
            }
        }
    }

    // Controllable values
    var leftOffset by remember { mutableStateOf(Offset(0f, 0f)) }
    var rightOffset by remember { mutableStateOf(Offset(0f, 0f)) }
    var sweeping by remember { mutableStateOf(false) }
    var dodgeMode by remember { mutableStateOf(false) }
    var soundEnabled by remember { mutableStateOf(true) }

    // Write joystick data every .1s
    LaunchedEffect(socket) {
        if (socket == null) return@LaunchedEffect
        withContext(Dispatchers.IO) {
            val writer = socket?.outputStream?.writer() ?: run {
                Log.e(TAG, "Socket.outputStream is null")
                return@withContext
            }
            while (true) try {
                val total = leftOffset + rightOffset
                writer.append("${total.x},${total.y},${if (sweeping) 1 else 0},${if (dodgeMode) 1 else 0},${if (soundEnabled) 1 else 0}")
                writer.append("\r\n")
                writer.flush()
                delay(100.milliseconds)
            } catch (e: IOException) {
                Log.d(TAG, "SocketError", e)
                if (socket?.isConnected == false) {
                    socket = null
                    break
                }
            }
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        var swap by remember { mutableStateOf(false) }
        Joystick(
            leftOffset, { leftOffset = if (swap) it.copy(y = 0f) else it.copy(x = 0f) }, modifier = Modifier
                .padding(40.dp)
                .align(Alignment.BottomStart)
        )
        Joystick(
            rightOffset, { rightOffset = if (!swap) it.copy(y = 0f) else it.copy(x = 0f) }, modifier = Modifier
                .padding(40.dp)
                .align(Alignment.BottomEnd)
        )

        Row(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(20.dp),
            horizontalArrangement = Arrangement.spacedBy(20.dp)
        ) {
            FilledIconToggleButton(checked = swap, onCheckedChange = { swap = it }) {
                Icon(painter = painterResource(R.drawable.baseline_swap_horiz_24), contentDescription = "Swap controls")
            }
            FilledIconToggleButton(checked = soundEnabled, onCheckedChange = { soundEnabled = it }) {
                Icon(
                    painter = painterResource(if (soundEnabled) R.drawable.baseline_volume_24 else R.drawable.baseline_volume_off_24),
                    contentDescription = "Mute sound"
                )
            }
        }

        Button(
            onClick = { sweeping = !sweeping; if (!sweeping) dodgeMode = false }, colors = ButtonDefaults.buttonColors(
                containerColor = if (sweeping) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                contentColor = if (sweeping) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.primary
            ), modifier = Modifier
                .align(Alignment.TopStart)
                .padding(20.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                Icon(painter = painterResource(R.drawable.baseline_radar_24), contentDescription = null)
                Text(text = "Sweeping mode")
            }
        }

        Button(
            onClick = { dodgeMode = !dodgeMode }, enabled = sweeping, colors = ButtonDefaults.buttonColors(
                containerColor = if (dodgeMode) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                contentColor = if (dodgeMode) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.primary
            ), modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(20.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                Icon(painter = painterResource(R.drawable.baseline_car_24), contentDescription = null)
                Text(text = "Mode: ${if (dodgeMode) "obstacle avoidance" else "speed limiter"}")
            }
        }

        Row(
            modifier = Modifier
                .padding(20.dp)
                .width(350.dp)
                .align(Alignment.TopCenter),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            val brush = Brush.verticalGradient(
                listOf(Color.Green, Color.Yellow, Color.Red), endY = with(LocalDensity.current) { 80.dp.toPx() }
            )
            for (distance in distances) {
                val height by animateIntAsState(targetValue = (70 - distance).coerceIn(15..65), label = "DistanceSensor Box")
                Box(modifier = Modifier
                    .weight(1f)
                    .height((height * 1.5f).dp)
                    .background(brush))
            }
        }
    }
}