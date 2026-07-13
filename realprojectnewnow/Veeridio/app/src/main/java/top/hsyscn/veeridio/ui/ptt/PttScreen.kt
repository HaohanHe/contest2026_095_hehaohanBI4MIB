package top.hsyscn.veeridio.ui.ptt

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import top.hsyscn.veeridio.ui.logs.LogsViewModel
import top.hsyscn.veeridio.ui.logs.LogsViewModelFactory

@Composable
fun PttScreen(
    viewModel: LogsViewModel = viewModel(factory = LogsViewModelFactory(LocalContext.current))
) {
    val status by viewModel.status.collectAsState()
    val deviceIp by viewModel.deviceIp.collectAsState()
    var isPressed by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(24.dp)
    ) {
        Text(
            text = "远程 PTT",
            style = MaterialTheme.typography.headlineSmall
        )

        Text(
            text = "状态: $status",
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.primary
        )

        deviceIp?.let {
            Text(
                text = "设备: $it",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.outline
            )
        }

        Spacer(modifier = Modifier.height(32.dp))

        Box(
            modifier = Modifier
                .size(200.dp)
                .clip(CircleShape)
                .background(
                    if (isPressed) MaterialTheme.colorScheme.error
                    else MaterialTheme.colorScheme.primaryContainer
                )
                .pointerInput(Unit) {
                    detectTapGestures(
                        onPress = {
                            isPressed = true
                            viewModel.sendPttCommand(true)
                            tryAwaitRelease()
                            isPressed = false
                            viewModel.sendPttCommand(false)
                        }
                    )
                },
            contentAlignment = Alignment.Center
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Icon(
                    imageVector = Icons.Default.Mic,
                    contentDescription = "PTT",
                    modifier = Modifier.size(80.dp),
                    tint = if (isPressed) MaterialTheme.colorScheme.onError
                    else MaterialTheme.colorScheme.onPrimaryContainer
                )
                Text(
                    text = if (isPressed) "发射中" else "按住发射",
                    color = if (isPressed) MaterialTheme.colorScheme.onError
                    else MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }

        Spacer(modifier = Modifier.weight(1f))
    }
}
