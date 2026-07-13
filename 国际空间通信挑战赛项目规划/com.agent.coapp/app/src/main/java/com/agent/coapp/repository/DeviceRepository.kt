package com.agent.coapp.repository

import com.agent.coapp.data.Skill
import com.agent.coapp.data.SkillPushRequest
import com.agent.coapp.network.DeviceApiService
import com.agent.coapp.network.WebSocketManager

/**
 * 设备交互仓库
 */
class DeviceRepository(
    private val apiService: DeviceApiService = DeviceApiService(),
    private val webSocketManager: WebSocketManager = WebSocketManager()
) {

    private fun getBaseUrl(deviceIp: String, port: Int): String =
        "http://$deviceIp:$port"

    /** GET /api/config → raw key-value map */
    suspend fun getConfig(deviceIp: String, port: Int): Result<Map<String, String>> =
        apiService.getConfig(getBaseUrl(deviceIp, port))

    /** PUT /api/config */
    suspend fun updateConfig(deviceIp: String, port: Int, config: Map<String, String>): Result<Unit> =
        apiService.updateConfig(getBaseUrl(deviceIp, port), config)

    /** GET /api/skills */
    suspend fun getSkills(deviceIp: String, port: Int): Result<List<Skill>> =
        apiService.getSkills(getBaseUrl(deviceIp, port))

    /** POST /api/skills (name + content) */
    suspend fun pushSkill(deviceIp: String, port: Int, name: String, content: String): Result<Unit> =
        apiService.pushSkill(getBaseUrl(deviceIp, port), SkillPushRequest(name, content))

    /** DELETE /api/skills/{name} */
    suspend fun deleteSkill(deviceIp: String, port: Int, skillName: String): Result<Unit> =
        apiService.deleteSkill(getBaseUrl(deviceIp, port), skillName)

    /** GET /api/logs */
    suspend fun getLogs(deviceIp: String, port: Int, lines: Int = 100): Result<List<String>> =
        apiService.getLogs(getBaseUrl(deviceIp, port), lines)

    // WebSocket
    fun connectWebSocket(deviceIp: String, port: Int) = webSocketManager.connect(deviceIp, port)
    fun disconnectWebSocket() = webSocketManager.disconnect()
    fun sendMessage(content: String) = webSocketManager.sendMessage(content)
    fun getWebSocketState() = webSocketManager.connectionState
    fun getWebSocketMessages() = webSocketManager.messages
    fun getWebSocketStatus() = webSocketManager.statusMessage
    fun clearWebSocketMessages() = webSocketManager.clearMessages()
}
