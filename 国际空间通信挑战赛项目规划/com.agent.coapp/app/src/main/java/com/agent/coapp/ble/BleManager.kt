package com.agent.coapp.ble

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.agent.coapp.data.BleDevice
import com.agent.coapp.data.ProvisioningStatus
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.util.UUID

/**
 * BLE管理器
 * 处理蓝牙扫描和设备连接
 */
@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {
    
    companion object {
        private const val TAG = "BleManager"
        // Nordic UART Service (NUS) UUIDs - matches watch ble_gatt.c
        val SERVICE_UUID: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
        val CHAR_RX: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")  // Write to watch
        val CHAR_TX: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")  // Notify from watch
        
        // Legacy aliases for compatibility
        val CHAR_SSID: UUID = CHAR_RX
        val CHAR_PASSWORD: UUID = CHAR_RX
        val CHAR_STATUS: UUID = CHAR_TX
        
        // 扫描过滤器前缀
        const val DEVICE_PREFIX = "Agent-"
        // 目标设备MAC地址
        const val TARGET_DEVICE_ADDRESS = "D0:C1:BF:B0:DF:F4"
    }
    
    private val bluetoothManager: BluetoothManager? = 
        context.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager?.adapter
    private var bluetoothLeScanner: BluetoothLeScanner? = null
    private var bluetoothGatt: BluetoothGatt? = null
    
    private val mainHandler = Handler(Looper.getMainLooper())
    
    private val _scanResults = MutableStateFlow<List<BleDevice>>(emptyList())
    val scanResults: StateFlow<List<BleDevice>> = _scanResults
    
    private val _provisioningStatus = MutableStateFlow(ProvisioningStatus.IDLE)
    val provisioningStatus: StateFlow<ProvisioningStatus> = _provisioningStatus
    
    private val _statusMessage = MutableStateFlow("")
    val statusMessage: StateFlow<String> = _statusMessage
    
    private val _deviceIp = MutableStateFlow("")
    val deviceIp: StateFlow<String> = _deviceIp
    
    private var isScanning = false
    private val devices = mutableMapOf<String, BleDevice>()
    
    /**
     * 检查蓝牙是否可用
     */
    fun isBluetoothAvailable(): Boolean {
        return bluetoothAdapter != null
    }
    
    /**
     * 检查蓝牙是否开启
     */
    fun isBluetoothEnabled(): Boolean {
        return bluetoothAdapter?.isEnabled == true
    }
    
    /**
     * 开始扫描BLE设备
     */
    fun startScan() {
        if (isScanning) return
        
        if (!isBluetoothEnabled()) {
            Log.w(TAG, "蓝牙未开启，无法扫描")
            _statusMessage.value = "请先开启蓝牙"
            return
        }
        
        devices.clear()
        _scanResults.value = emptyList()
        _provisioningStatus.value = ProvisioningStatus.SCANNING
        _statusMessage.value = "正在扫描..."
        
        try {
            bluetoothLeScanner = bluetoothAdapter?.bluetoothLeScanner
            if (bluetoothLeScanner == null) {
                Log.e(TAG, "无法获取 BLE Scanner")
                _statusMessage.value = "BLE Scanner 不可用"
                _provisioningStatus.value = ProvisioningStatus.IDLE
                return
            }
            
            val scanSettings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setLegacy(false)  // false = 接收 legacy + extended 广播
                .setPhy(ScanSettings.PHY_LE_ALL_SUPPORTED)
                .build()
            
            // 按MAC地址过滤目标设备
            val macFilter = ScanFilter.Builder()
                .setDeviceAddress(TARGET_DEVICE_ADDRESS)
                .build()
            
            Log.d(TAG, "开始BLE扫描（MAC过滤 + Extended Advertising）")
            bluetoothLeScanner?.startScan(listOf(macFilter), scanSettings, scanCallback)
            isScanning = true
            Log.d(TAG, "BLE 扫描已启动")
            
            // 10秒后自动停止扫描
            mainHandler.postDelayed({
                stopScan()
                if (devices.isEmpty()) {
                    _statusMessage.value = "未发现设备，请确认设备已开启并在附近"
                } else {
                    _statusMessage.value = "扫描完成，发现 ${devices.size} 个设备"
                }
            }, 10000)
        } catch (e: Exception) {
            Log.e(TAG, "扫描失败: ${e.message}", e)
            _statusMessage.value = "扫描失败: ${e.message}"
            _provisioningStatus.value = ProvisioningStatus.IDLE
        }
    }
    
    /**
     * 停止扫描
     */
    fun stopScan() {
        if (!isScanning) return
        
        try {
            bluetoothLeScanner?.stopScan(scanCallback)
        } catch (e: Exception) {
            Log.e(TAG, "停止扫描失败: ${e.message}")
        }
        isScanning = false
        
        if (_provisioningStatus.value == ProvisioningStatus.SCANNING) {
            _provisioningStatus.value = ProvisioningStatus.IDLE
        }
    }
    
    /**
     * 从已绑定设备列表中查找目标设备（跳过扫描）
     */
    fun loadBondedDevices() {
        val bondedDevices = bluetoothAdapter?.bondedDevices ?: emptySet()
        Log.d(TAG, "已绑定设备数: ${bondedDevices.size}")
        
        devices.clear()
        for (bd in bondedDevices) {
            val name = bd.name ?: "Unknown"
            val address = bd.address
            Log.d(TAG, "已绑定: name=$name, address=$address")
            
            val bleDevice = BleDevice(name = name, address = address, rssi = 0)
            devices[address] = bleDevice
        }
        _scanResults.value = devices.values.toList()
        
        if (devices.containsKey(TARGET_DEVICE_ADDRESS)) {
            _statusMessage.value = "已找到目标设备（已绑定）"
        } else {
            _statusMessage.value = "已绑定设备中未找到目标，共 ${devices.size} 个设备"
        }
        _provisioningStatus.value = ProvisioningStatus.IDLE
    }

    /**
     * 直接通过MAC地址连接GATT（用已绑定关系连接，连接后刷新缓存发现NUS服务）
     */
    fun connectDirectByAddress(ssid: String, password: String) {
        _provisioningStatus.value = ProvisioningStatus.CONNECTING
        _statusMessage.value = "正在连接 $TARGET_DEVICE_ADDRESS ..."
        _pendingSsid = ssid
        _pendingPassword = password
        
        try {
            val bluetoothDevice = bluetoothAdapter?.getRemoteDevice(TARGET_DEVICE_ADDRESS)
            // 不指定 TRANSPORT_LE，让系统用已绑定的连接方式
            bluetoothGatt = bluetoothDevice?.connectGatt(context, false, gattCallback)
            Log.d(TAG, "连接 GATT (bonded): $TARGET_DEVICE_ADDRESS")
        } catch (e: Exception) {
            Log.e(TAG, "连接失败: ${e.message}")
            _statusMessage.value = "连接失败: ${e.message}"
            _provisioningStatus.value = ProvisioningStatus.FAILED
        }
    }
    
    private var _pendingSsid = ""
    private var _pendingPassword = ""

    /**
     * 连接到设备并进行配网
     */
    fun connectAndProvision(ssid: String, password: String) {
        // 优先用扫描到的原始设备对象连接（确保连到正确的GATT server）
        val rawDevice = rawDevices[TARGET_DEVICE_ADDRESS] ?: rawDevices.values.firstOrNull()
        val device = devices[TARGET_DEVICE_ADDRESS] ?: devices.values.firstOrNull()
        if (device == null) {
            _statusMessage.value = "没有找到设备"
            _provisioningStatus.value = ProvisioningStatus.FAILED
            return
        }
        
        _provisioningStatus.value = ProvisioningStatus.CONNECTING
        _statusMessage.value = "正在连接 ${device.name}..."
        _pendingSsid = ssid
        _pendingPassword = password
        
        try {
            val btDevice = rawDevice ?: bluetoothAdapter?.getRemoteDevice(device.address)
            // 使用 TRANSPORT_LE 确保走 BLE 广播连接通道
            bluetoothGatt = btDevice?.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
            Log.d(TAG, "连接 GATT (scan device, TRANSPORT_LE): ${device.address}")
        } catch (e: Exception) {
            Log.e(TAG, "连接失败: ${e.message}")
            _statusMessage.value = "连接失败: ${e.message}\n提示: 若反复失败，请在手机蓝牙设置中忘记该设备后重试"
            _provisioningStatus.value = ProvisioningStatus.FAILED
        }
    }
    
    /**
     * 发送WiFi凭据 - 通过NUS RX Characteristic写入JSON
     */
    private fun sendWifiCredentials(ssid: String, password: String) {
        _provisioningStatus.value = ProvisioningStatus.SENDING_WIFI
        _statusMessage.value = "正在发送WiFi凭据..."
        
        try {
            val service = bluetoothGatt?.getService(SERVICE_UUID)
            if (service == null) {
                Log.e(TAG, "未找到NUS服务: $SERVICE_UUID")
                _statusMessage.value = "未找到BLE数据服务"
                _provisioningStatus.value = ProvisioningStatus.FAILED
                return
            }
            
            val rxChar = service.getCharacteristic(CHAR_RX)
            if (rxChar == null) {
                Log.e(TAG, "未找到RX特征: $CHAR_RX")
                _statusMessage.value = "未找到写入特征"
                _provisioningStatus.value = ProvisioningStatus.FAILED
                return
            }
            
            // 发送WiFi配置JSON
            val json = """{"cmd":"wifi_config","ssid":"$ssid","password":"$password"}"""
            rxChar.value = json.toByteArray(Charsets.UTF_8)
            val success = bluetoothGatt?.writeCharacteristic(rxChar) ?: false
            Log.d(TAG, "写入WiFi凭据: success=$success, len=${json.length}")
            
            if (success) {
                _statusMessage.value = "WiFi凭据已发送，等待设备响应..."
                // 15秒超时
                mainHandler.postDelayed({
                    if (_provisioningStatus.value == ProvisioningStatus.SENDING_WIFI) {
                        _statusMessage.value = "响应超时，请重试"
                        _provisioningStatus.value = ProvisioningStatus.FAILED
                        disconnect()
                    }
                }, 15000)
            } else {
                _statusMessage.value = "写入失败"
                _provisioningStatus.value = ProvisioningStatus.FAILED
            }
        } catch (e: Exception) {
            Log.e(TAG, "发送WiFi凭据失败: ${e.message}")
            _statusMessage.value = "发送失败: ${e.message}"
            _provisioningStatus.value = ProvisioningStatus.FAILED
        }
    }
    
    /**
     * 发送任意JSON命令到设备
     */
    fun sendCommand(json: String) {
        try {
            val service = bluetoothGatt?.getService(SERVICE_UUID) ?: return
            val rxChar = service.getCharacteristic(CHAR_RX) ?: return
            rxChar.value = json.toByteArray(Charsets.UTF_8)
            bluetoothGatt?.writeCharacteristic(rxChar)
            Log.d(TAG, "发送命令: $json")
        } catch (e: Exception) {
            Log.e(TAG, "发送命令失败: ${e.message}")
        }
    }
    
    /**
     * 断开连接
     */
    fun disconnect() {
        try {
            bluetoothGatt?.disconnect()
            bluetoothGatt?.close()
            bluetoothGatt = null
        } catch (e: Exception) {
            Log.e(TAG, "断开连接失败: ${e.message}")
        }
    }
    
    /**
     * 扫描回调
     */
    private val rawDevices = mutableMapOf<String, BluetoothDevice>()
    
    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val deviceName = result.device.name
            val address = result.device.address
            
            Log.d(TAG, "发现设备: name=$deviceName, address=$address, rssi=${result.rssi}")
            
            // 显示目标MAC设备 + 有名称的NUS设备
            val displayName = when {
                address.equals(TARGET_DEVICE_ADDRESS, ignoreCase = true) -> deviceName ?: "VelaClaw"
                !deviceName.isNullOrBlank() -> deviceName
                else -> return
            }
            
            val bleDevice = BleDevice(
                name = displayName,
                address = address,
                rssi = result.rssi
            )
            devices[address] = bleDevice
            rawDevices[address] = result.device
            _scanResults.value = devices.values.toList().sortedByDescending { it.rssi }
        }
        
        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "扫描失败, errorCode=$errorCode")
            _statusMessage.value = "扫描失败 (错误码: $errorCode)"
            _provisioningStatus.value = ProvisioningStatus.IDLE
        }
    }
    
    /**
     * GATT回调
     */
    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            Log.d(TAG, "onConnectionStateChange: status=$status, newState=$newState")
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    mainHandler.post {
                        _provisioningStatus.value = ProvisioningStatus.CONNECTED
                        _statusMessage.value = "已连接，协商MTU..."
                    }
                    // 请求更大的MTU（默认23太小，JSON会被截断）
                    gatt.requestMtu(512)
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    mainHandler.post {
                        _statusMessage.value = "连接已断开 (status=$status)"
                        _provisioningStatus.value = ProvisioningStatus.IDLE
                    }
                    gatt.close()
                }
            }
        }
        
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            Log.d(TAG, "onServicesDiscovered: status=$status")
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val services = gatt.services
                Log.d(TAG, "发现 ${services.size} 个服务:")
                services.forEach { service ->
                    Log.d(TAG, "  服务: ${service.uuid}")
                    service.characteristics.forEach { char ->
                        Log.d(TAG, "    特征: ${char.uuid}")
                    }
                }
                
                val service = gatt.getService(SERVICE_UUID)
                if (service == null) {
                    Log.w(TAG, "未找到目标服务 $SERVICE_UUID")
                    mainHandler.post {
                        _statusMessage.value = "设备不支持配网服务\n提示: 请在手机蓝牙设置中忘记该设备后重试"
                        _provisioningStatus.value = ProvisioningStatus.FAILED
                    }
                    return
                }
                
                val statusChar = service.getCharacteristic(CHAR_TX)
                if (statusChar != null) {
                    gatt.setCharacteristicNotification(statusChar, true)
                    // 写入CCCD descriptor启用通知
                    val descriptor = statusChar.getDescriptor(
                        UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
                    )
                    if (descriptor != null) {
                        descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(descriptor)
                    }
                    Log.d(TAG, "已启用 TX 通知")
                    mainHandler.post {
                        _statusMessage.value = "服务已就绪，正在发送WiFi凭据..."
                        _provisioningStatus.value = ProvisioningStatus.CONNECTED
                        // 自动发送WiFi凭据
                        if (_pendingSsid.isNotEmpty()) {
                            mainHandler.postDelayed({
                                sendWifiCredentials(_pendingSsid, _pendingPassword)
                            }, 500)
                        }
                    }
                } else {
                    Log.w(TAG, "未找到 TX 特征")
                }
            } else {
                mainHandler.post {
                    _statusMessage.value = "服务发现失败 (status=$status)"
                    _provisioningStatus.value = ProvisioningStatus.FAILED
                }
            }
        }
        
        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            Log.d(TAG, "MTU changed: mtu=$mtu, status=$status")
            mainHandler.post {
                _statusMessage.value = "MTU=$mtu，正在发现服务..."
            }
            refreshGattCache(gatt)
            mainHandler.postDelayed({
                gatt.discoverServices()
            }, 1500)
        }
        
        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "写入成功: ${characteristic.uuid}")
            }
        }
        
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            val value = characteristic.value
            if (value == null || value.isEmpty()) return
            
            val json = String(value, Charsets.UTF_8)
            Log.d(TAG, "收到通知: $json")
            
            mainHandler.post {
                try {
                    // Parse JSON response: {"status":"ok|error","msg":"...","network":bool,"ip":"..."}
                    val statusOk = json.contains("\"status\":\"ok\"")
                    val msg = extractJsonString(json, "msg") ?: ""
                    val network = json.contains("\"network\":true")
                    val ip = extractJsonString(json, "ip")
                    
                    if (statusOk && msg.contains("wifi connected")) {
                        _statusMessage.value = "配网成功！正在获取设备IP..."
                        _provisioningStatus.value = ProvisioningStatus.SUCCESS
                        // 自动查询设备IP
                        mainHandler.postDelayed({
                            sendCommand("{\"cmd\":\"status\"}")
                        }, 1000)
                    } else if (statusOk && msg == "pong") {
                        _statusMessage.value = "设备连接正常"
                    } else if (statusOk && ip != null) {
                        _deviceIp.value = ip
                        _statusMessage.value = "设备IP: $ip"
                        _provisioningStatus.value = ProvisioningStatus.SUCCESS
                    } else if (!statusOk) {
                        _statusMessage.value = "失败: $msg"
                        _provisioningStatus.value = ProvisioningStatus.FAILED
                    } else {
                        _statusMessage.value = msg.ifEmpty { json }
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "解析响应失败: ${e.message}")
                    _statusMessage.value = json
                }
            }
        }
    }
    
    /** Extract a string value from simple JSON */
    private fun extractJsonString(json: String, key: String): String? {
        val pattern = "\"$key\":\"" 
        var idx = json.indexOf(pattern)
        if (idx < 0) {
            // Try with space
            idx = json.indexOf("\"$key\": \"")
            if (idx < 0) return null
            idx += key.length + 5
        } else {
            idx += pattern.length
        }
        val end = json.indexOf('"', idx)
        return if (end > idx) json.substring(idx, end) else null
    }
    
    /**
     * 清除GATT服务缓存（反射调用隐藏API）
     * 解决已绑定设备缓存旧服务列表的问题
     */
    private fun refreshGattCache(gatt: BluetoothGatt): Boolean {
        try {
            val method = gatt.javaClass.getMethod("refresh")
            val result = method.invoke(gatt) as? Boolean ?: false
            Log.d(TAG, "GATT cache refresh: $result")
            return result
        } catch (e: Exception) {
            Log.w(TAG, "GATT cache refresh failed: ${e.message}")
            return false
        }
    }
    
    /**
     * 设置配网回调（外部调用传入SSID和密码）
     */
    fun setWifiCredentials(ssid: String, password: String) {
        sendWifiCredentials(ssid, password)
    }
}
