package com.agent.coapp.ui.provisioning

import android.Manifest
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.agent.coapp.data.BleDevice
import com.agent.coapp.data.ProvisioningStatus
import com.agent.coapp.viewmodel.ProvisioningViewModel

/**
 * BLE配网页面
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun BleProvisioningScreen(
    viewModel: ProvisioningViewModel = viewModel()
) {
    val scanResults by viewModel.scanResults.collectAsState()
    val provisioningStatus by viewModel.provisioningStatus.collectAsState()
    val statusMessage by viewModel.statusMessage.collectAsState()
    val isBluetoothEnabled by viewModel.isBluetoothEnabled.collectAsState()
    
    var showWifiDialog by remember { mutableStateOf(false) }
    var wifiSsid by remember { mutableStateOf("") }
    var wifiPassword by remember { mutableStateOf("") }
    
    // 权限请求
    val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT,
            Manifest.permission.ACCESS_FINE_LOCATION
        )
    } else {
        arrayOf(
            Manifest.permission.BLUETOOTH,
            Manifest.permission.BLUETOOTH_ADMIN,
            Manifest.permission.ACCESS_FINE_LOCATION
        )
    }
    
    val permissionLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissionsResult ->
        val allGranted = permissionsResult.values.all { it }
        if (allGranted) {
            viewModel.startScan()
        } else {
            val denied = permissionsResult.filter { !it.value }.keys
            android.util.Log.w("BleProvisioning", "权限被拒绝: $denied")
        }
    }
    
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("蓝牙配网") },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.background
                )
            )
        }
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
                .padding(16.dp)
        ) {
            // 蓝牙状态
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = if (isBluetoothEnabled) 
                        MaterialTheme.colorScheme.primaryContainer 
                    else 
                        MaterialTheme.colorScheme.errorContainer
                )
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = if (isBluetoothEnabled) "蓝牙已开启" else "蓝牙未开启",
                        style = MaterialTheme.typography.bodyLarge
                    )
                    Button(
                        onClick = { permissionLauncher.launch(permissions) },
                        enabled = viewModel.isBluetoothAvailable()
                    ) {
                        Text("扫描设备")
                    }
                }
            }
            
            Spacer(modifier = Modifier.height(16.dp))
            
            // 状态信息
            if (statusMessage.isNotEmpty()) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = when (provisioningStatus) {
                            ProvisioningStatus.SUCCESS -> MaterialTheme.colorScheme.primaryContainer
                            ProvisioningStatus.FAILED -> MaterialTheme.colorScheme.errorContainer
                            else -> MaterialTheme.colorScheme.secondaryContainer
                        }
                    )
                ) {
                    Text(
                        text = statusMessage,
                        modifier = Modifier.padding(16.dp),
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
                Spacer(modifier = Modifier.height(16.dp))
            }
            
            // 设备列表标题
            Text(
                text = "发现的设备 (${scanResults.size})",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            
            Spacer(modifier = Modifier.height(8.dp))
            
            // 设备列表
            LazyColumn(
                modifier = Modifier.weight(1f)
            ) {
                items(scanResults) { device ->
                    DeviceItem(
                        device = device,
                        onClick = {
                            wifiSsid = ""
                            wifiPassword = ""
                            showWifiDialog = true
                        }
                    )
                }
                
                if (scanResults.isEmpty() && provisioningStatus == ProvisioningStatus.SCANNING) {
                    item {
                        Box(
                            modifier = Modifier.fillMaxWidth(),
                            contentAlignment = Alignment.Center
                        ) {
                            CircularProgressIndicator()
                        }
                    }
                }
            }
        }
    }
    
    // WiFi凭据输入对话框
    if (showWifiDialog) {
        AlertDialog(
            onDismissRequest = { showWifiDialog = false },
            title = { Text("输入WiFi凭据") },
            text = {
                Column {
                    OutlinedTextField(
                        value = wifiSsid,
                        onValueChange = { wifiSsid = it },
                        label = { Text("WiFi名称 (SSID)") },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedTextField(
                        value = wifiPassword,
                        onValueChange = { wifiPassword = it },
                        label = { Text("WiFi密码") },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true
                    )
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        showWifiDialog = false
                        viewModel.startProvisioning(wifiSsid, wifiPassword)
                    },
                    enabled = wifiSsid.isNotBlank()
                ) {
                    Text("开始配网")
                }
            },
            dismissButton = {
                TextButton(onClick = { showWifiDialog = false }) {
                    Text("取消")
                }
            }
        )
    }
}

/**
 * 设备列表项
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun DeviceItem(
    device: BleDevice,
    onClick: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        onClick = onClick
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(
                    text = device.name,
                    style = MaterialTheme.typography.titleMedium
                )
                Text(
                    text = device.address,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            Text(
                text = "${device.rssi} dBm",
                style = MaterialTheme.typography.bodySmall
            )
        }
    }
}
