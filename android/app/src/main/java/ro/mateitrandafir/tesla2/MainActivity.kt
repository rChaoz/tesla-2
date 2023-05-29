package ro.mateitrandafir.tesla2

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import ro.mateitrandafir.tesla2.ui.RequestPermissions
import ro.mateitrandafir.tesla2.ui.theme.Tesla2Theme

const val TAG = "Tesla2"

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            Tesla2Theme(darkTheme = true) {
                RequestPermissions()
            }
        }
    }
}