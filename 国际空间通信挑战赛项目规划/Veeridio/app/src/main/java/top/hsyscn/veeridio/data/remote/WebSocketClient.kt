package top.hsyscn.veeridio.data.remote

import android.os.Handler
import android.os.Looper
import com.google.gson.Gson
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import top.hsyscn.veeridio.data.model.WsMessage
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

class WebSocketClient {

    private val client = OkHttpClient.Builder()
        .pingInterval(15, TimeUnit.SECONDS)
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()

    private val gson = Gson()
    private var webSocket: WebSocket? = null
    private var currentUrl: String = ""
    private val isConnected = AtomicBoolean(false)
    private val handler = Handler(Looper.getMainLooper())
    private var reconnectAttempts = 0

    private val _messages = MutableSharedFlow<WsMessage>(extraBufferCapacity = 64)
    val messages = _messages.asSharedFlow()

    private val _connectionState = MutableSharedFlow<Boolean>(extraBufferCapacity = 1)
    val connectionState = _connectionState.asSharedFlow()

    private val _error = MutableSharedFlow<String>(extraBufferCapacity = 1)
    val error = _error.asSharedFlow()

    fun connect(url: String) {
        disconnect()
        currentUrl = url
        reconnectAttempts = 0
        doConnect()
    }

    private fun doConnect() {
        if (currentUrl.isBlank()) return
        val request = Request.Builder().url(currentUrl).build()
        webSocket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                isConnected.set(true)
                reconnectAttempts = 0
                _connectionState.tryEmit(true)
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                _messages.tryEmit(parseMessage(text))
            }

            override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
                isConnected.set(false)
                _connectionState.tryEmit(false)
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                isConnected.set(false)
                _connectionState.tryEmit(false)
                scheduleReconnect()
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                isConnected.set(false)
                _connectionState.tryEmit(false)
                _error.tryEmit("WebSocket 错误: ${t.message}")
                scheduleReconnect()
            }
        })
    }

    fun disconnect() {
        cancelReconnect()
        webSocket?.cancel()
        webSocket = null
        isConnected.set(false)
        _connectionState.tryEmit(false)
    }

    fun send(text: String): Boolean {
        return if (isConnected.get()) {
            webSocket?.send(text) ?: false
        } else {
            false
        }
    }

    private fun scheduleReconnect() {
        if (currentUrl.isBlank()) return
        if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) return
        reconnectAttempts++
        val delayMs = (RECONNECT_BASE_DELAY_MS * reconnectAttempts).coerceAtMost(MAX_RECONNECT_DELAY_MS)
        handler.postDelayed(delayMs) {
            doConnect()
        }
    }

    private fun cancelReconnect() {
        handler.removeCallbacksAndMessages(null)
    }

    private fun parseMessage(text: String): WsMessage {
        return try {
            val obj = gson.fromJson(text, Map::class.java)
            when (obj["type"]) {
                "status" -> WsMessage.Status(obj["content"] as? String ?: "")
                "log" -> WsMessage.Log(
                    obj["timestamp"] as? String ?: "",
                    obj["content"] as? String ?: ""
                )
                else -> WsMessage.Unknown(text)
            }
        } catch (_: Exception) {
            WsMessage.Unknown(text)
        }
    }

    companion object {
        private const val MAX_RECONNECT_ATTEMPTS = 5
        private const val RECONNECT_BASE_DELAY_MS = 1000L
        private const val MAX_RECONNECT_DELAY_MS = 10000L
    }
}
