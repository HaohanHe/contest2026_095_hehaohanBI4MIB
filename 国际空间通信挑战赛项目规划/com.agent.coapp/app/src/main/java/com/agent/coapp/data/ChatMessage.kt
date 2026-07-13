package com.agent.coapp.data

/**
 * 聊天消息数据类
 */
data class ChatMessage(
    val content: String,
    val isFromUser: Boolean,
    val timestamp: Long = System.currentTimeMillis()
)

/**
 * WebSocket消息请求
 */
data class WebSocketRequest(
    val type: String,
    val content: String
)

/**
 * WebSocket消息响应
 */
data class WebSocketResponse(
    val type: String,
    val content: String
)

/**
 * 日志响应
 */
data class LogsResponse(
    val logs: List<String>? = null
)

/**
 * BLE设备
 */
data class BleDevice(
    val name: String,
    val address: String,
    val rssi: Int = 0
)

/**
 * 配网状态
 */
enum class ProvisioningStatus {
    IDLE,
    SCANNING,
    CONNECTING,
    CONNECTED,
    SENDING_WIFI,
    SUCCESS,
    FAILED
}
