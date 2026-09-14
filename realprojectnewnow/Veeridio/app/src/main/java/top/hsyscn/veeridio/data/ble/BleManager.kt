package top.hsyscn.veeridio.data.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import com.google.gson.Gson
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import top.hsyscn.veeridio.data.model.BleStatusResponse
import top.hsyscn.veeridio.data.model.WifiConfig
import java.nio.charset.StandardCharsets
import java.util.UUID
import java.util.concurrent.atomic.AtomicBoolean

@SuppressLint("MissingPermission")
class BleManager(context: Context) {

    private val appContext = context.applicationContext
    private val bluetoothManager = appContext.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var txCharacteristic: BluetoothGattCharacteristic? = null
    private var rxCharacteristic: BluetoothGattCharacteristic? = null

    private val writeQueue = java.util.concurrent.ConcurrentLinkedQueue<ByteArray>()
    private val isWriting = AtomicBoolean(false)

    private val _scanResults = MutableStateFlow<List<BluetoothDevice>>(emptyList())
    val scanResults: StateFlow<List<BluetoothDevice>> = _scanResults.asStateFlow()

    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Disconnected)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _lastStatus = MutableStateFlow<BleStatusResponse?>(null)
    val lastStatus: StateFlow<BleStatusResponse?> = _lastStatus.asStateFlow()

    private val _error = MutableStateFlow<String?>(null)
    val error: StateFlow<String?> = _error.asStateFlow()

    private val gson = Gson()
    private val handler = Handler(Looper.getMainLooper())
    private val isConnecting = AtomicBoolean(false)
    private var pendingReconnectDevice: BluetoothDevice? = null
    private var reconnectAttempts = 0

    private var activeScanCallback: ScanCallback? = null
    private var connectTimeoutRunnable: Runnable? = null

    private val nusScanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            addScanResult(result.device)
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            results.forEach { addScanResult(it.device) }
        }

        override fun onScanFailed(errorCode: Int) {
            _error.value = "扫描失败: $errorCode"
        }
    }

    private fun createNameScanCallback(filter: String) = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            val name = result.scanRecord?.deviceName ?: device.name
            if (name?.contains(filter, ignoreCase = true) == true) {
                addScanResult(device)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            _error.value = "扫描失败: $errorCode"
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    isConnecting.set(false)
                    reconnectAttempts = 0
                    connectTimeoutRunnable?.let { handler.removeCallbacks(it) }
                    _connectionState.value = ConnectionState.Connected(gatt.device.name ?: gatt.device.address)
                    gatt.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    val wasConnecting = isConnecting.getAndSet(false)
                    connectTimeoutRunnable?.let { handler.removeCallbacks(it) }
                    _connectionState.value = ConnectionState.Disconnected
                    resetGatt()
                    writeQueue.clear()
                    isWriting.set(false)
                    if (!wasConnecting && pendingReconnectDevice != null && reconnectAttempts < MAX_RECONNECT) {
                        scheduleReconnect()
                    }
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                _error.value = "服务发现失败: $status"
                disconnect()
                return
            }
            val service = gatt.getService(NUS_SERVICE_UUID)
            if (service == null) {
                _error.value = "未找到 NUS 服务"
                disconnect()
                return
            }
            txCharacteristic = service.getCharacteristic(NUS_TX_UUID)
            rxCharacteristic = service.getCharacteristic(NUS_RX_UUID)

            if (txCharacteristic == null || rxCharacteristic == null) {
                _error.value = "未找到 NUS 特征"
                disconnect()
                return
            }

            enableNotifications(gatt, txCharacteristic!!)
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            val value = characteristic.getStringValue(0)
            handleIncoming(value)
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            val text = String(value, StandardCharsets.UTF_8)
            handleIncoming(text)
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                _error.value = "BLE 写入失败: $status"
            }
            processWriteQueue()
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int
        ) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                _error.value = "通知启用失败: $status"
            }
        }
    }

    private fun addScanResult(device: BluetoothDevice) {
        val current = _scanResults.value.toMutableList()
        if (current.none { it.address == device.address }) {
            current.add(device)
            _scanResults.value = current
        }
    }

    private fun enableNotifications(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
        val success = gatt.setCharacteristicNotification(characteristic, true)
        if (!success) {
            _error.value = "无法启用通知"
            return
        }
        val descriptor = characteristic.getDescriptor(CLIENT_CHARACTERISTIC_CONFIG_UUID)
        if (descriptor != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                gatt.writeDescriptor(descriptor)
            }
        }
    }

    fun isBluetoothEnabled(): Boolean = bluetoothAdapter?.isEnabled == true

    fun startScan() {
        if (!isBluetoothEnabled()) {
            _error.value = "蓝牙未开启"
            return
        }
        stopScan()
        _scanResults.value = emptyList()
        _error.value = null
        scanner = bluetoothAdapter?.bluetoothLeScanner
        activeScanCallback = nusScanCallback
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(NUS_SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        try {
            scanner?.startScan(listOf(filter), settings, nusScanCallback)
        } catch (e: Exception) {
            _error.value = "启动扫描失败: ${e.message}"
        }
    }

    fun startScanByName(nameFilter: String) {
        if (!isBluetoothEnabled()) {
            _error.value = "蓝牙未开启"
            return
        }
        stopScan()
        _scanResults.value = emptyList()
        _error.value = null
        scanner = bluetoothAdapter?.bluetoothLeScanner
        val callback = createNameScanCallback(nameFilter)
        activeScanCallback = callback
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        try {
            scanner?.startScan(emptyList(), settings, callback)
        } catch (e: Exception) {
            _error.value = "启动扫描失败: ${e.message}"
        }
    }

    fun stopScan() {
        activeScanCallback?.let { callback ->
            try {
                scanner?.stopScan(callback)
            } catch (_: Exception) {
            }
        }
        activeScanCallback = null
        scanner = null
    }

    fun connect(context: Context, device: BluetoothDevice) {
        if (isConnecting.get()) return
        isConnecting.set(true)
        pendingReconnectDevice = device
        _error.value = null
        _connectionState.value = ConnectionState.Connecting

        disconnectInternal()
        gatt = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } else {
            device.connectGatt(context, false, gattCallback)
        }

        // Connection timeout fallback.
        connectTimeoutRunnable?.let { handler.removeCallbacks(it) }
        connectTimeoutRunnable = Runnable {
            if (isConnecting.get()) {
                isConnecting.set(false)
                _error.value = "连接超时"
                disconnectInternal()
                scheduleReconnect()
            }
        }
        handler.postDelayed(connectTimeoutRunnable!!, CONNECT_TIMEOUT_MS)
    }

    fun disconnect() {
        pendingReconnectDevice = null
        reconnectAttempts = MAX_RECONNECT
        disconnectInternal()
    }

    fun resetReconnectAttempts() {
        reconnectAttempts = 0
    }

    private fun disconnectInternal() {
        try {
            gatt?.disconnect()
            gatt?.close()
        } catch (_: Exception) {
        }
        gatt = null
        txCharacteristic = null
        rxCharacteristic = null
    }

    private fun scheduleReconnect() {
        if (isConnecting.get()) return
        if (reconnectAttempts >= MAX_RECONNECT) return
        reconnectAttempts++
        val delayMs = (RECONNECT_BASE_DELAY_MS * reconnectAttempts)
            .coerceAtMost(MAX_RECONNECT_DELAY_MS)
        handler.postDelayed(delayMs) {
            val device = pendingReconnectDevice ?: return@postDelayed
            connect(appContext, device)
        }
    }

    fun sendWifiConfig(ssid: String, password: String) {
        val config = WifiConfig(ssid, password)
        val json = gson.toJson(config)
        send(json)
    }

    fun sendCommand(command: String) {
        send(command)
    }

    private fun send(text: String) {
        val characteristic = rxCharacteristic
        if (characteristic == null) {
            _error.value = "未连接或 RX 特征未就绪"
            return
        }

        val bytes = text.toByteArray(StandardCharsets.UTF_8)
        val mtu = DEFAULT_MTU
        var offset = 0
        while (offset < bytes.size) {
            val chunkSize = minOf(mtu, bytes.size - offset)
            writeQueue.offer(bytes.copyOfRange(offset, offset + chunkSize))
            offset += chunkSize
        }
        processWriteQueue()
    }

    private fun processWriteQueue() {
        if (isWriting.getAndSet(true)) return
        val characteristic = rxCharacteristic ?: run {
            isWriting.set(false)
            return
        }
        val chunk = writeQueue.poll() ?: run {
            isWriting.set(false)
            return
        }
        val success = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt?.writeCharacteristic(characteristic, chunk, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothStatusCodes.SUCCESS
        } else {
            characteristic.value = chunk
            characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            gatt?.writeCharacteristic(characteristic) == true
        }
        if (!success) {
            isWriting.set(false)
            _error.value = "BLE 写入入队失败"
        }
    }

    private fun handleIncoming(text: String) {
        try {
            val status = gson.fromJson(text, BleStatusResponse::class.java)
            _lastStatus.value = status
        } catch (_: Exception) {
            // Not a JSON status message, ignore for status parsing.
        }
    }

    private fun resetGatt() {
        txCharacteristic = null
        rxCharacteristic = null
    }

    sealed class ConnectionState {
        data object Disconnected : ConnectionState()
        data object Connecting : ConnectionState()
        data class Connected(val deviceName: String) : ConnectionState()
    }

    companion object {
        val NUS_SERVICE_UUID: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val NUS_RX_UUID: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
        val NUS_TX_UUID: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")
        val CLIENT_CHARACTERISTIC_CONFIG_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

        private const val NAME_FILTER = "AI Radio"
        private const val DEFAULT_MTU = 20
        private const val CHUNK_DELAY_MS = 20L
        private const val CONNECT_TIMEOUT_MS = 15000L
        private const val RECONNECT_BASE_DELAY_MS = 1000L
        private const val MAX_RECONNECT_DELAY_MS = 5000L
        private const val MAX_RECONNECT = 3
    }
}
