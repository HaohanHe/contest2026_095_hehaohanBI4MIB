package top.hsyscn.veeridio.data.model

/**
 * Generic wrapper for ai_agent / radio_console REST responses.
 */
sealed class Result<out T> {
    data class Success<T>(val data: T) : Result<T>()
    data class Error(val message: String) : Result<Nothing>()
}

/**
 * Device connection info saved after BLE provisioning.
 */
data class DeviceInfo(
    val name: String,
    val address: String,
    val ip: String,
    val port: Int = 28790
)

/**
 * WiFi configuration sent to the device over BLE NUS.
 */
data class WifiConfig(
    val ssid: String,
    val password: String
)

/**
 * Device response received from BLE NUS after WiFi provisioning.
 */
data class BleStatusResponse(
    val status: String = "",
    val msg: String = "",
    val network: Boolean = false,
    val ip: String = ""
)

/**
 * Flat config entry returned by GET /api/config.
 */
typealias DeviceConfig = Map<String, String>

/**
 * Skill metadata from GET /api/skills.
 */
data class Skill(
    val name: String,
    val description: String,
    val file: String,
    val size: Long,
    val mtime: String,
    val content: String? = null
)

data class SkillsResponse(
    val skills: List<Skill>
)

/**
 * Log entry from GET /api/logs.
 */
data class RadioLog(
    val timestamp: String = "",
    val content: String = ""
)

data class LogsResponse(
    val logs: List<String>
)

/**
 * WebSocket message pushed by the device.
 */
sealed class WsMessage {
    data class Status(val content: String) : WsMessage()
    data class Log(val timestamp: String, val content: String) : WsMessage()
    data class Unknown(val raw: String) : WsMessage()
}
