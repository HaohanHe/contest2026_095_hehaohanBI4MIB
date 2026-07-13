package com.agent.coapp.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.agent.coapp.ble.BleManager
import com.agent.coapp.data.BleDevice
import com.agent.coapp.data.ProvisioningStatus
import com.agent.coapp.repository.ConfigRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

/**
 * 配网ViewModel
 */
class ProvisioningViewModel(application: Application) : AndroidViewModel(application) {
    
    private val bleManager = BleManager(application)
    private val configRepository = ConfigRepository(application)
    
    val scanResults: StateFlow<List<BleDevice>> = bleManager.scanResults
    val provisioningStatus: StateFlow<ProvisioningStatus> = bleManager.provisioningStatus
    val statusMessage: StateFlow<String> = bleManager.statusMessage
    
    private val _isBluetoothEnabled = MutableStateFlow(false)
    val isBluetoothEnabled: StateFlow<Boolean> = _isBluetoothEnabled
    
    init {
        checkBluetoothState()
        // 配网获取到IP后自动保存到配置
        viewModelScope.launch {
            bleManager.deviceIp.collect { ip ->
                if (ip.isNotEmpty()) {
                    configRepository.updateDeviceIp(ip)
                }
            }
        }
    }
    
    /**
     * 检查蓝牙状态
     */
    fun checkBluetoothState() {
        _isBluetoothEnabled.value = bleManager.isBluetoothEnabled()
    }
    
    /**
     * 开始扫描
     */
    fun startScan() {
        checkBluetoothState()
        if (_isBluetoothEnabled.value) {
            bleManager.startScan()
        }
    }
    
    /**
     * 加载已绑定设备（跳过扫描）
     */
    fun loadBondedDevices() {
        bleManager.loadBondedDevices()
    }
    
    /**
     * 停止扫描
     */
    fun stopScan() {
        bleManager.stopScan()
    }
    
    /**
     * 开始配网
     */
    fun startProvisioning(ssid: String, password: String) {
        viewModelScope.launch {
            bleManager.connectAndProvision(ssid, password)
        }
    }
    
    /**
     * 直连配网（跳过扫描，直接用MAC地址连接NUS服务）
     */
    fun directProvision(ssid: String, password: String) {
        viewModelScope.launch {
            bleManager.connectDirectByAddress(ssid, password)
        }
    }
    
    /**
     * 断开连接
     */
    fun disconnect() {
        bleManager.disconnect()
    }
    
    /**
     * 检查蓝牙是否可用
     */
    fun isBluetoothAvailable(): Boolean = bleManager.isBluetoothAvailable()
}
