package com.agent.coapp.ui.config

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.agent.coapp.data.AgentConfig
import com.agent.coapp.viewmodel.ConfigViewModel
import com.agent.coapp.viewmodel.SyncResult

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConfigScreen(viewModel: ConfigViewModel = viewModel()) {
    val config by viewModel.configFlow.collectAsState()
    val isLoading by viewModel.isLoading.collectAsState()
    val syncResult by viewModel.syncResult.collectAsState()

    var edited by remember(config) { mutableStateOf(config) }
    var hasChanges by remember { mutableStateOf(false) }

    // 进入页面自动从设备同步
    LaunchedEffect(Unit) { viewModel.syncFromDevice() }

    LaunchedEffect(syncResult) {
        syncResult?.let {
            kotlinx.coroutines.delay(3000)
            viewModel.clearSyncResult()
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Agent配置") },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.background
                )
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp)
                .verticalScroll(rememberScrollState())
        ) {
            // 设备连接
            ConfigSection("设备连接") {
                ConfigTextField(edited.deviceIp, { edited = edited.copy(deviceIp = it); hasChanges = true }, "设备IP地址", placeholder = "192.168.x.x")
                Spacer(Modifier.height(8.dp))
                ConfigTextField(edited.devicePort.toString(), { edited = edited.copy(devicePort = it.toIntOrNull() ?: 28789); hasChanges = true }, "端口")
            }

            Spacer(Modifier.height(16.dp))

            // LLM
            ConfigSection("LLM 配置") {
                ConfigTextField(edited.llmApiKey, { edited = edited.copy(llmApiKey = it); hasChanges = true }, "API Key", isPassword = true)
                Spacer(Modifier.height(8.dp))
                ConfigTextField(edited.llmModel, { edited = edited.copy(llmModel = it); hasChanges = true }, "模型")
                Spacer(Modifier.height(8.dp))
                ConfigTextField(edited.llmHost, { edited = edited.copy(llmHost = it); hasChanges = true }, "LLM Host")
                Spacer(Modifier.height(8.dp))
                ConfigTextField(edited.llmPath, { edited = edited.copy(llmPath = it); hasChanges = true }, "LLM Path")
            }

            Spacer(Modifier.height(16.dp))

            // 搜索
            ConfigSection("Tavily 搜索") {
                ConfigTextField(edited.tavilyKey, { edited = edited.copy(tavilyKey = it); hasChanges = true }, "Tavily API Key", isPassword = true)
            }

            Spacer(Modifier.height(16.dp))

            // 火山引擎 ASR/TTS
            ConfigSection("火山引擎 ASR/TTS") {
                ConfigTextField(edited.volcAppKey, { edited = edited.copy(volcAppKey = it); hasChanges = true }, "AppKey")
                Spacer(Modifier.height(8.dp))
                ConfigTextField(edited.volcToken, { edited = edited.copy(volcToken = it); hasChanges = true }, "Token", isPassword = true)
                Spacer(Modifier.height(8.dp))
                ConfigTextField(edited.volcApiKey, { edited = edited.copy(volcApiKey = it); hasChanges = true }, "API Key", isPassword = true)
                Spacer(Modifier.height(8.dp))
                ConfigTextField(edited.volcCluster, { edited = edited.copy(volcCluster = it); hasChanges = true }, "TTS Cluster")
                Spacer(Modifier.height(8.dp))
                ConfigTextField(edited.volcAsrCluster, { edited = edited.copy(volcAsrCluster = it); hasChanges = true }, "ASR Cluster")
            }

            Spacer(Modifier.height(16.dp))

            // 代理
            ConfigSection("HTTP 代理") {
                ConfigTextField(edited.proxyHost, { edited = edited.copy(proxyHost = it); hasChanges = true }, "代理 Host")
                Spacer(Modifier.height(8.dp))
                ConfigTextField(edited.proxyPort, { edited = edited.copy(proxyPort = it); hasChanges = true }, "代理 Port")
            }

            Spacer(Modifier.height(24.dp))

            // 同步结果
            syncResult?.let { result ->
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = when (result) {
                            is SyncResult.Success -> MaterialTheme.colorScheme.primaryContainer
                            is SyncResult.Error -> MaterialTheme.colorScheme.errorContainer
                        }
                    )
                ) {
                    Text(
                        text = when (result) {
                            is SyncResult.Success -> result.message
                            is SyncResult.Error -> result.message
                        },
                        modifier = Modifier.padding(16.dp),
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
                Spacer(Modifier.height(16.dp))
            }

            // 操作按钮
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { viewModel.syncFromDevice() }, Modifier.weight(1f), enabled = !isLoading) {
                    Text("从设备拉取")
                }
                Button(onClick = {
                    viewModel.saveConfig(edited)
                    viewModel.syncToDevice(edited)
                }, Modifier.weight(1f), enabled = !isLoading) {
                    Text("保存并推送")
                }
            }

            if (isLoading) {
                Spacer(Modifier.height(16.dp))
                Box(Modifier.fillMaxWidth(), contentAlignment = Alignment.Center) {
                    CircularProgressIndicator()
                }
            }

            Spacer(Modifier.height(32.dp))
        }
    }
}

@Composable
private fun ConfigSection(title: String, content: @Composable ColumnScope.() -> Unit) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(12.dp))
            content()
        }
    }
}

@Composable
private fun ConfigTextField(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    placeholder: String = "",
    isPassword: Boolean = false
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        label = { Text(label) },
        placeholder = if (placeholder.isNotEmpty()) {{ Text(placeholder) }} else null,
        modifier = Modifier.fillMaxWidth(),
        singleLine = true,
        visualTransformation = if (isPassword) PasswordVisualTransformation() else VisualTransformation.None,
        keyboardOptions = if (isPassword) KeyboardOptions(keyboardType = KeyboardType.Password) else KeyboardOptions.Default
    )
}
