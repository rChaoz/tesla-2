package ro.mateitrandafir.tesla2.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

@Composable
fun Main() {
    Scaffold { paddingValue ->
        Box(modifier = Modifier.padding(paddingValue)) {
        }
    }
}