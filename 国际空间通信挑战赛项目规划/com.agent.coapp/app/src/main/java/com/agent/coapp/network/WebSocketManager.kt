package com.agent.coapp.network

import android.util.Log
import com.agent.coapp.data.WebSocketRequest
import com.agent.coapp.data.WebSocketResponse
import com.google.gson.Gson
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import okhttp3.*
import java.util.concurrent.TimeUnit

/**
 * WebSocket连接状态
 */
enum class WebSocketState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
}

/**
 * WebSocket管理器
 * 处理与Agent设备的实时通信
 */
class WebSocketManager {
    
    private val client = OkHttpClient.Builder()
        .readTimeout(0, TimeUnit.MILLISECONDS)
        .pingInterval(0, TimeUnit.SECONDS)  // 禁用ping（手表ws_server不回pong）
        .build()
    
    private val gson = Gson()
    private var webSocket: WebSocket? = null
    
    private val _connectionState = MutableStateFlow(WebSocketState.DISCONNECTED)
    val connectionState: StateFlow<WebSocketState> = _connectionState
    
    private val _messages = MutableStateFlow<List<WebSocketResponse>>(emptyList())
    val messages: StateFlow<List<WebSocketResponse>> = _messages
    
    private val _statusMessage = MutableStateFlow("")
    val statusMessage: StateFlow<String> = _statusMessage
    
    companion object {
        private const val TAG = "WebSocketManager"
    }
    
    /**
     * 连接到WebSocket服务器
     */
    fun connect(deviceIp: String, port: Int) {
        if (_connectionState.value == WebSocketState.CONNECTED) {
            return
        }
        
        // 关闭旧连接
        webSocket?.close(1000, null)
        webSocket = null
        
        _connectionState.value = WebSocketState.CONNECTING
        
        val wsUrl = "ws://$deviceIp:$port/ws/chat"
        val request = Request.Builder()
            .url(wsUrl)
            .build()
        
        webSocket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                Log.d(TAG, "WebSocket连接已建立")
                _connectionState.value = WebSocketState.CONNECTED
            }
            
            override fun onMessage(webSocket: WebSocket, text: String) {
                Log.d(TAG, "收到消息: $text")
                try {
                    val response = gson.fromJson(text, WebSocketResponse::class.java)
                    val currentMessages = _messages.value.toMutableList()
                    currentMessages.add(response)
                    _messages.value = currentMessages
                    
                    // 处理状态消息
                    if (response.type == "status") {
                        _statusMessage.value = when (response.content) {
                            "thinking" -> "正在思考..."
                            "speaking" -> "正在说话..."
                            "idle" -> "空闲"
                            else -> response.content
                        }
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "解析消息失败: ${e.message}")
                }
            }
            
            override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
                Log.d(TAG, "WebSocket正在关闭: $reason")
                webSocket.close(1000, null)
            }
            
            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                Log.d(TAG, "WebSocket已关闭: $reason")
                _connectionState.value = WebSocketState.DISCONNECTED
            }
            
            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                Log.e(TAG, "WebSocket错误: ${t.message}")
                if (_connectionState.value == WebSocketState.CONNECTED) {
                    _connectionState.value = WebSocketState.DISCONNECTED
                    _statusMessage.value = "连接断开，可重新连接"
                } else {
                    _connectionState.value = WebSocketState.ERROR
                    _statusMessage.value = "连接失败: ${t.message}"
                }
            }
        })
    }
    
    /**
     * 发送消息
     */
    fun sendMessage(content: String) {
        if (_connectionState.value != WebSocketState.CONNECTED) {
            Log.w(TAG, "WebSocket未连接，无法发送消息")
            return
        }
        
        val request = WebSocketRequest(
            type = "message",
            content = content
        )
        val json = gson.toJson(request)
        webSocket?.send(json)
    }
    
    /**
     * 断开连接
     */
    fun disconnect() {
        webSocket?.close(1000, "用户断开")
        webSocket = null
        _connectionState.value = WebSocketState.DISCONNECTED
        _messages.value = emptyList()
    }
    
    /**
     * 清空消息
     */
    fun clearMessages() {
        _messages.value = emptyList()
    }
}
