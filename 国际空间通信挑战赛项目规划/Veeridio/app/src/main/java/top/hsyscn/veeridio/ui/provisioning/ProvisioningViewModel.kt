package top.hsyscn.veeridio.ui.provisioning

import android.bluetooth.BluetoothDevice
import android.content.Context
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import top.hsyscn.veeridio.data.ble.BleManager
import top.hsyscn.veeridio.data.model.DeviceInfo
import top.hsyscn.veeridio.data.preferences.DevicePreferences

class ProvisioningViewModel(context: Context) : ViewModel() {

    private val bleManager = BleManager(context)
    private val prefs = DevicePreferences(context)

    val scanResults: StateFlow<List<BluetoothDevice>> = bleManager.scanResults
    val connectionState: StateFlow<BleManager.ConnectionState> = bleManager.connectionState
    val lastStatus = bleManager.lastStatus
    val error: StateFlow<String?> = bleManager.error

    private val _uiState = MutableStateFlow(UiState())
    val uiState: StateFlow<UiState> = _uiState.asStateFlow()

    private val _savedDevice = MutableStateFlow<DeviceInfo?>(null)
    val savedDevice: StateFlow<DeviceInfo?> = _savedDevice.asStateFlow()

    private var currentDeviceName: String = ""
    private var currentDeviceAddress: String = ""

    init {
        viewModelScope.launch {
            prefs.deviceInfo.collect { _savedDevice.value = it }
        }
        viewModelScope.launch {
            bleManager.error.collect { msg ->
                msg?.let { _uiState.value = _uiState.value.copy(message = it) }
            }
        }
    }

    fun currentDevice(): DeviceInfo? {
        return if (currentDeviceName.isNotBlank() && currentDeviceAddress.isNotBlank()) {
            DeviceInfo(currentDeviceName, currentDeviceAddress, savedDevice.value?.ip ?: "", savedDevice.value?.port ?: 28790)
        } else {
            savedDevice.value
        }
    }

    fun clearError() {
        _uiState.value = _uiState.value.copy(message = "")
    }

    fun startScan(filter: String = "AI Radio") {
        _uiState.value = _uiState.value.copy(message = "正在扫描...")
        if (filter.isBlank()) {
            bleManager.startScan()
        } else {
            bleManager.startScanByName(filter)
        }
    }

    fun stopScan() {
        bleManager.stopScan()
    }

    fun connect(context: Context, device: BluetoothDevice) {
        currentDeviceName = device.name ?: "AI Radio"
        currentDeviceAddress = device.address
        bleManager.resetReconnectAttempts()
        bleManager.connect(context, device)
    }

    fun disconnect() {
        bleManager.disconnect()
    }

    fun sendWifiConfig(ssid: String, password: String) {
        if (ssid.isBlank()) {
            _uiState.value = _uiState.value.copy(message = "SSID 不能为空")
            return
        }
        bleManager.sendWifiConfig(ssid, password)
        _uiState.value = _uiState.value.copy(message = "已发送 WiFi 配置，等待设备连接...")
    }

    fun saveDevice(name: String, address: String, ip: String, port: Int = 28790) {
        viewModelScope.launch {
            prefs.saveDevice(DeviceInfo(name, address, ip, port))
        }
    }

    fun clearMessage() {
        _uiState.value = _uiState.value.copy(message = "")
    }

    override fun onCleared() {
        super.onCleared()
        bleManager.disconnect()
        bleManager.stopScan()
    }

    data class UiState(
        val message: String = ""
    )
}
