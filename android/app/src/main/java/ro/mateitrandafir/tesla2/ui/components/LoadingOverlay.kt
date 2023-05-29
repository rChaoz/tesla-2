package ro.mateitrandafir.tesla2.ui.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.layout.Box
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ProgressIndicatorDefaults
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha

@Composable
fun LoadingOverlay(
    isLoading: Boolean,
    modifier: Modifier = Modifier,
    progress: Float? = null,
    content: @Composable () -> Unit
) {
    Box(modifier, contentAlignment = Alignment.Center) {
        content()
        if (isLoading) {
            Surface(
                modifier = Modifier
                    .matchParentSize()
                    .alpha(.5f)
            ) {}
            if (progress == null) CircularProgressIndicator()
            else {
                val animatedProgress by animateFloatAsState(
                    targetValue = progress,
                    label = "Progress Animation",
                    animationSpec = ProgressIndicatorDefaults.ProgressAnimationSpec
                )
                CircularProgressIndicator(progress = animatedProgress)
            }
        }
    }
}