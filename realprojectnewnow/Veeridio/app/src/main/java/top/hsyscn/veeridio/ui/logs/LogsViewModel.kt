package top.hsyscn.veeridio.ui.logs

import android.content.Context
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import top.hsyscn.veeridio.data.model.DeviceInfo
import top.hsyscn.veeridio.data.model.RadioLog
import top.hsyscn.veeridio.data.model.Result
import top.hsyscn.veeridio.data.model.WsMessage
import top.hsyscn.veeridio.data.preferences.DevicePreferences
import top.hsyscn.veeridio.data.remote.RadioApi
import top.hsyscn.veeridio.data.remote.WebSocketClient

class LogsViewModel(context: Context) : ViewModel() {

    private val prefs = DevicePreferences(context)
    private val radioApi = RadioApi()
    private val wsClient = WebSocketClient()

    private val _logs = MutableStateFlow<List<RadioLog>>(emptyList())
    val logs: StateFlow<List<RadioLog>> = _logs.asStateFlow()

    private val _status = MutableStateFlow("未连接")
    val status: StateFlow<String> = _status.asStateFlow()

    private val _deviceIp = MutableStateFlow<String?>(null)
    val deviceIp: StateFlow<String?> = _deviceIp.asStateFlow()

    @Volatile
    private var currentDeviceInfo: DeviceInfo? = null

    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading.asStateFlow()

    private val _error = MutableStateFlow<String?>(null)
    val error: StateFlow<String?> = _error.asStateFlow()

    init {
        viewModelScope.launch {
            prefs.deviceInfo.collect { info ->
                currentDeviceInfo = info
                val ip = info?.ip
                _deviceIp.value = ip
                ip?.let {
                    val port = info.port.takeIf { it > 0 } ?: 28790
                    radioApi.updateBaseUrl("http://$it:$port")
                    connectWebSocket(it, port)
                }
            }
        }
        viewModelScope.launch {
            wsClient.messages.collect { msg ->
                when (msg) {
                    is WsMessage.Status -> _status.value = msg.content
                    is WsMessage.Log -> {
                        _logs.value = listOf(RadioLog(msg.timestamp, msg.content)) + _logs.value
                    }
                    is WsMessage.Unknown -> {
                        // Ignore unknown messages.
                    }
                }
            }
        }
        viewModelScope.launch {
            wsClient.connectionState.collect { connected ->
                _status.value = if (connected) "WebSocket 已连接" else "WebSocket 已断开"
            }
        }
        viewModelScope.launch {
            wsClient.error.collect { err ->
                _error.value = err
            }
        }
    }

    fun setIp(ip: String, port: Int = 28790) {
        viewModelScope.launch {
            prefs.saveDevice(DeviceInfo(name = "AI Radio", address = "", ip = ip, port = port))
        }
    }

    fun loadLogs() {
        val info = currentDeviceInfo ?: run {
            _error.value = "请先配网或手动输入设备 IP"
            return
        }
        val ip = info.ip
        val port = info.port.takeIf { it > 0 } ?: 28790
        radioApi.updateBaseUrl("http://$ip:$port")
        _isLoading.value = true
        _error.value = null
        viewModelScope.launch {
            when (val result = radioApi.getLogs()) {
                is Result.Success -> {
                    _logs.value = result.data.map { text ->
                        val parts = text.split(" ", limit = 2)
                        if (parts.size == 2) RadioLog(parts[0], parts[1]) else RadioLog("", text)
                    }
                }
                is Result.Error -> {
                    _error.value = result.message
                }
            }
            _isLoading.value = false
        }
    }

    fun clearError() {
        _error.value = null
    }

    fun sendPttCommand(pressed: Boolean) {
        val action = if (pressed) "start" else "stop"
        val sent = wsClient.send("""{"type":"ptt","action":"$action"}""")
        if (!sent) {
            // Fallback to REST if WebSocket is not connected.
            viewModelScope.launch {
                when (radioApi.sendPtt(action)) {
                    is Result.Success -> _status.value = "PTT $action (REST)"
                    is Result.Error -> _error.value = "PTT 发送失败"
                }
            }
        }
    }

    private fun connectWebSocket(ip: String, port: Int) {
        wsClient.connect("ws://$ip:$port/ws/chat")
    }

    override fun onCleared() {
        super.onCleared()
        wsClient.disconnect()
    }
}
