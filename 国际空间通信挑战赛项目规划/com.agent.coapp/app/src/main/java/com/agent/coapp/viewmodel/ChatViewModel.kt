package com.agent.coapp.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.agent.coapp.data.ChatMessage
import com.agent.coapp.network.WebSocketState
import com.agent.coapp.repository.ConfigRepository
import com.agent.coapp.repository.DeviceRepository
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import java.io.File

/**
 * 聊天ViewModel
 */
class ChatViewModel(application: Application) : AndroidViewModel(application) {
    
    private val configRepository = ConfigRepository(application)
    private val deviceRepository = DeviceRepository()
    private val gson = Gson()
    private val chatFile = File(application.filesDir, "chat_history.json")
    
    private val _messages = MutableStateFlow<List<ChatMessage>>(emptyList())
    val messages: StateFlow<List<ChatMessage>> = _messages
    
    private val _connectionState = MutableStateFlow(WebSocketState.DISCONNECTED)
    val connectionState: StateFlow<WebSocketState> = _connectionState
    
    private val _statusMessage = MutableStateFlow("")
    val statusMessage: StateFlow<String> = _statusMessage
    
    private val _inputText = MutableStateFlow("")
    val inputText: StateFlow<String> = _inputText
    
    private val _message = MutableStateFlow("")
    val message: StateFlow<String> = _message
    
    init {
        // 加载历史消息
        loadMessages()
        
        // 自动连接上次的设备
        viewModelScope.launch {
            val config = configRepository.configFlow.first()
            if (config.deviceIp.isNotEmpty()) {
                deviceRepository.connectWebSocket(config.deviceIp, config.devicePort)
            }
        }
        
        // 监听WebSocket消息
        viewModelScope.launch {
            deviceRepository.getWebSocketMessages().collect { wsMessages ->
                // 处理收到的WebSocket消息
                wsMessages.forEach { wsResponse ->
                    if (wsResponse.type == "message" || wsResponse.type == "response") {
                        val currentMessages = _messages.value.toMutableList()
                        // 避免重复添加
                        if (currentMessages.none { 
                            it.content == wsResponse.content && !it.isFromUser 
                        }) {
                            currentMessages.add(
                                ChatMessage(
                                    content = wsResponse.content,
                                    isFromUser = false
                                )
                            )
                            _messages.value = currentMessages
                            saveMessages()
                            pendingReplyCount++  // 取消超时
                        }
                    }
                }
            }
        }
        
        // 监听连接状态
        viewModelScope.launch {
            deviceRepository.getWebSocketState().collect { state ->
                _connectionState.value = state
                if (state == WebSocketState.CONNECTED) {
                    _statusMessage.value = ""
                }
            }
        }
        
        // 监听状态消息
        viewModelScope.launch {
            deviceRepository.getWebSocketStatus().collect { status ->
                _statusMessage.value = status
            }
        }
    }
    
    /**
     * 连接设备
     */
    fun connect() {
        viewModelScope.launch {
            val config = configRepository.configFlow.first()
            
            if (config.deviceIp.isEmpty()) {
                _message.value = "请先配置设备IP地址"
                return@launch
            }
            
            deviceRepository.connectWebSocket(config.deviceIp, config.devicePort)
        }
    }
    
    /**
     * 断开连接
     */
    fun disconnect() {
        deviceRepository.disconnectWebSocket()
    }
    
    /**
     * 发送消息
     */
    private var pendingReplyCount = 0
    
    fun sendMessage() {
        val content = _inputText.value.trim()
        if (content.isEmpty()) return
        
        // 添加用户消息
        val currentMessages = _messages.value.toMutableList()
        currentMessages.add(ChatMessage(content = content, isFromUser = true))
        _messages.value = currentMessages
        saveMessages()
        
        // 清空输入框
        _inputText.value = ""
        
        // 发送消息
        deviceRepository.sendMessage(content)
        
        // 2分钟超时检测
        pendingReplyCount++
        val expectedCount = pendingReplyCount
        viewModelScope.launch {
            kotlinx.coroutines.delay(120_000)
            if (pendingReplyCount == expectedCount) {
                // 超时未收到回复
                val msgs = _messages.value.toMutableList()
                msgs.add(ChatMessage(content = "⏱ 响应超时，设备可能离线或服务不可用", isFromUser = false))
                _messages.value = msgs
                saveMessages()
            }
        }
    }
    
    /**
     * 更新输入文本
     */
    fun updateInputText(text: String) {
        _inputText.value = text
    }
    
    /**
     * 清空消息
     */
    fun clearMessages() {
        _messages.value = emptyList()
        deviceRepository.clearWebSocketMessages()
        saveMessages()
    }
    
    /**
     * 清除消息提示
     */
    fun clearMessage() {
        _message.value = ""
    }
    
    private fun saveMessages() {
        viewModelScope.launch(Dispatchers.IO) {
            try {
                // 只保留最近100条
                val toSave = _messages.value.takeLast(100)
                chatFile.writeText(gson.toJson(toSave))
            } catch (_: Exception) {}
        }
    }
    
    private fun loadMessages() {
        viewModelScope.launch(Dispatchers.IO) {
            try {
                if (chatFile.exists()) {
                    val json = chatFile.readText()
                    val type = object : TypeToken<List<ChatMessage>>() {}.type
                    val saved: List<ChatMessage> = gson.fromJson(json, type) ?: emptyList()
                    _messages.value = saved
                }
            } catch (_: Exception) {}
        }
    }
    
    override fun onCleared() {
        super.onCleared()
        disconnect()
    }
}
