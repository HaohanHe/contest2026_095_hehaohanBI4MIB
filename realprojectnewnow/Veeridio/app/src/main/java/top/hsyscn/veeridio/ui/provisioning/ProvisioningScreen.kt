package top.hsyscn.veeridio.ui.provisioning

import android.Manifest
import android.bluetooth.BluetoothDevice
import android.content.pm.PackageManager
import android.os.Build
import android.widget.Toast
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.lifecycle.viewmodel.compose.viewModel
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.rememberMultiplePermissionsState
import top.hsyscn.veeridio.data.ble.BleManager

@OptIn(ExperimentalPermissionsApi::class)
@Composable
fun ProvisioningScreen(
    viewModel: ProvisioningViewModel = viewModel(factory = ProvisioningViewModelFactory(LocalContext.current))
) {
    val context = LocalContext.current
    val scanResults by viewModel.scanResults.collectAsState()
    val connectionState by viewModel.connectionState.collectAsState()
    val lastStatus by viewModel.lastStatus.collectAsState()
    val uiState by viewModel.uiState.collectAsState()
    val savedDevice by viewModel.savedDevice.collectAsState()

    var ssid by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }

    val permissions = remember {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            listOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            listOf(
                Manifest.permission.BLUETOOTH,
                Manifest.permission.BLUETOOTH_ADMIN,
                Manifest.permission.ACCESS_FINE_LOCATION
            )
        }
    }
    val permissionState = rememberMultiplePermissionsState(permissions = permissions)

    LaunchedEffect(lastStatus) {
        lastStatus?.let { status ->
            if (status.network && status.ip.isNotBlank()) {
                Toast.makeText(context, "设备已联网: ${status.ip}", Toast.LENGTH_LONG).show()
                // Save IP automatically.
                val device = viewModel.currentDevice()
                viewModel.saveDevice(
                    name = device?.name ?: "AI Radio",
                    address = device?.address ?: "",
                    ip = status.ip,
                    port = device?.port ?: 28790
                )
            }
        }
    }

    DisposableEffect(Unit) {
        onDispose {
            viewModel.stopScan()
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text(
            text = "BLE 网络配网",
            style = MaterialTheme.typography.headlineSmall
        )

        savedDevice?.let { device ->
            Card(modifier = Modifier.fillMaxWidth()) {
                Column(modifier = Modifier.padding(12.dp)) {
                    Text("已保存设备", style = MaterialTheme.typography.titleMedium)
                    Text("名称: ${device.name}")
                    Text("地址: ${device.address}")
                    Text("IP: ${device.ip}:${device.port}")
                }
            }
        }

        if (!permissionState.allPermissionsGranted) {
            Button(
                onClick = { permissionState.launchMultiplePermissionRequest() },
                modifier = Modifier.fillMaxWidth()
            ) {
                Icon(Icons.Default.Bluetooth, contentDescription = null)
                Text("授权蓝牙与位置权限")
            }
        } else {
            Button(
                onClick = {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
                        ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN)
                        != PackageManager.PERMISSION_GRANTED
                    ) {
                        permissionState.launchMultiplePermissionRequest()
                    } else {
                        viewModel.startScan()
                    }
                },
                modifier = Modifier.fillMaxWidth()
            ) {
                Icon(Icons.Default.Refresh, contentDescription = null)
                Text("扫描 AI Radio 设备")
            }
        }

        if (uiState.message.isNotBlank()) {
            Text(
                text = uiState.message,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.primary
            )
        }

        when (connectionState) {
            is BleManager.ConnectionState.Connecting -> {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator()
                    Text(" 连接中...", modifier = Modifier.padding(start = 8.dp))
                }
            }

            is BleManager.ConnectionState.Connected -> {
                val name = (connectionState as BleManager.ConnectionState.Connected).deviceName
                Text("已连接: $name", color = MaterialTheme.colorScheme.primary)

                OutlinedTextField(
                    value = ssid,
                    onValueChange = { ssid = it },
                    label = { Text("WiFi SSID") },
                    modifier = Modifier.fillMaxWidth(),
                    leadingIcon = { Icon(Icons.Default.Wifi, contentDescription = null) }
                )
                OutlinedTextField(
                    value = password,
                    onValueChange = { password = it },
                    label = { Text("WiFi 密码") },
                    modifier = Modifier.fillMaxWidth()
                )
                Button(
                    onClick = { viewModel.sendWifiConfig(ssid, password) },
                    modifier = Modifier.fillMaxWidth(),
                    enabled = ssid.isNotBlank()
                ) {
                    Text("发送 WiFi 配置")
                }
                TextButton(
                    onClick = { viewModel.disconnect() },
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text("断开连接")
                }
            }

            else -> {
                LazyColumn(
                    modifier = Modifier.weight(1f),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    items(scanResults, key = { it.address }) { device ->
                        DeviceItem(
                            device = device,
                            onClick = { viewModel.connect(context, device) }
                        )
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(8.dp))
    }
}

@Composable
private fun DeviceItem(device: BluetoothDevice, onClick: () -> Unit) {
    Card(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(modifier = Modifier.padding(12.dp)) {
            Text(
                text = device.name ?: "未知设备",
                style = MaterialTheme.typography.titleMedium
            )
            Text(
                text = device.address,
                style = MaterialTheme.typography.bodySmall
            )
        }
    }
}
