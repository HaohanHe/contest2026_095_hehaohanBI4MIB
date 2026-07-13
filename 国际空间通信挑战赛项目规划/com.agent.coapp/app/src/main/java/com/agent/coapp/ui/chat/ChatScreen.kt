package com.agent.coapp.ui.chat

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.agent.coapp.data.ChatMessage
import com.agent.coapp.network.WebSocketState
import com.agent.coapp.ui.theme.Teal400
import com.agent.coapp.ui.theme.Green400
import com.agent.coapp.ui.theme.AccentMist
import com.agent.coapp.ui.theme.TextPrimary
import com.agent.coapp.ui.theme.TextSecondary
import com.agent.coapp.ui.theme.UserBubble
import com.agent.coapp.ui.theme.AiBubble
import com.agent.coapp.ui.theme.Red400
import com.agent.coapp.viewmodel.ChatViewModel

/**
 * 聊天页面
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ChatScreen(
    viewModel: ChatViewModel = viewModel()
) {
    val messages by viewModel.messages.collectAsState()
    val connectionState by viewModel.connectionState.collectAsState()
    val statusMessage by viewModel.statusMessage.collectAsState()
    val inputText by viewModel.inputText.collectAsState()
    val message by viewModel.message.collectAsState()
    
    val listState = rememberLazyListState()
    
    // 自动滚动到底部（消息变化或键盘弹出时）
    LaunchedEffect(messages.size) {
        if (messages.isNotEmpty()) {
            listState.scrollToItem(messages.size - 1)
        }
    }
    
    // 显示消息提示
    LaunchedEffect(message) {
        if (message.isNotEmpty()) {
            kotlinx.coroutines.delay(3000)
            viewModel.clearMessage()
        }
    }
    
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("对话", maxLines = 1) },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.background
                ),
                actions = {
                    // 连接状态指示器
                    ConnectionIndicator(
                        state = connectionState,
                        statusMessage = statusMessage
                    )
                }
            )
        }
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
        ) {
            // 消息提示
            if (message.isNotEmpty()) {
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp, vertical = 8.dp),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.errorContainer
                    )
                ) {
                    Text(
                        text = message,
                        modifier = Modifier.padding(16.dp),
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
            }
            
            // 消息列表
            LazyColumn(
                state = listState,
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth(),
                contentPadding = PaddingValues(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                if (messages.isEmpty()) {
                    item {
                        Box(
                            modifier = Modifier.fillParentMaxSize(),
                            contentAlignment = Alignment.Center
                        ) {
                            Column(
                                horizontalAlignment = Alignment.CenterHorizontally
                            ) {
                                Icon(
                                    Icons.Default.Chat,
                                    contentDescription = null,
                                    modifier = Modifier.size(64.dp),
                                    tint = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                                Spacer(modifier = Modifier.height(16.dp))
                                Text(
                                    text = "开始与AI Agent对话",
                                    style = MaterialTheme.typography.bodyLarge,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                                Spacer(modifier = Modifier.height(8.dp))
                                Text(
                                    text = getConnectionHint(connectionState),
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                        }
                    }
                }
                
                items(messages) { chatMessage ->
                    ChatBubble(chatMessage = chatMessage)
                }
            }
            
            // 底部输入区域
            Surface(
                modifier = Modifier
                    .fillMaxWidth()
                    .imePadding(),
                shadowElevation = 8.dp
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    OutlinedTextField(
                        value = inputText,
                        onValueChange = { viewModel.updateInputText(it) },
                        modifier = Modifier.weight(1f),
                        placeholder = { Text("输入消息...") },
                        maxLines = 4
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    if (connectionState == WebSocketState.CONNECTED) {
                        IconButton(
                            onClick = { viewModel.sendMessage() },
                            enabled = inputText.isNotBlank()
                        ) {
                            Icon(
                                Icons.Default.Send,
                                contentDescription = "发送",
                                tint = if (inputText.isNotBlank())
                                    MaterialTheme.colorScheme.primary
                                else
                                    MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    } else {
                        Button(onClick = { viewModel.connect() }) {
                            Text("连接")
                        }
                    }
                }
            }
        }
    }
}

/**
 * 消息气泡
 */
@Composable
@OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class)
private fun ChatBubble(chatMessage: ChatMessage) {
    val clipboardManager = LocalClipboardManager.current
    var showCopied by remember { mutableStateOf(false) }

    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = if (chatMessage.isFromUser) Arrangement.End else Arrangement.Start
    ) {
        Box(
            modifier = Modifier
                .widthIn(max = 280.dp)
                .clip(
                    RoundedCornerShape(
                        topStart = 16.dp,
                        topEnd = 16.dp,
                        bottomStart = if (chatMessage.isFromUser) 16.dp else 4.dp,
                        bottomEnd = if (chatMessage.isFromUser) 4.dp else 16.dp
                    )
                )
                .background(
                    if (chatMessage.isFromUser) UserBubble else AiBubble
                )
                .then(
                    if (chatMessage.isFromUser) Modifier.border(
                        width = 1.dp,
                        color = AccentMist,
                        shape = RoundedCornerShape(
                            topStart = 16.dp, topEnd = 16.dp,
                            bottomStart = 16.dp, bottomEnd = 4.dp
                        )
                    ) else Modifier
                )
                .combinedClickable(
                    onClick = {},
                    onLongClick = {
                        clipboardManager.setText(AnnotatedString(chatMessage.content))
                        showCopied = true
                    }
                )
                .padding(12.dp)
        ) {
            Column {
                SelectionContainer {
                    Text(
                        text = chatMessage.content,
                        color = TextPrimary
                    )
                }
                if (showCopied) {
                    LaunchedEffect(Unit) {
                        kotlinx.coroutines.delay(1500)
                        showCopied = false
                    }
                    Text("已复制", style = MaterialTheme.typography.labelSmall,
                        color = TextSecondary)
                }
            }
        }
    }
}

/**
 * 连接状态指示器
 */
@Composable
private fun ConnectionIndicator(
    state: WebSocketState,
    statusMessage: String
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.padding(end = 12.dp)
    ) {
        val (color, text) = when (state) {
            WebSocketState.CONNECTED -> Green400 to "已连接"
            WebSocketState.CONNECTING -> AccentMist to "连接中..."
            WebSocketState.ERROR -> Red400 to "错误"
            WebSocketState.DISCONNECTED -> TextSecondary to "未连接"
        }
        
        // 如果状态消息包含"失败"/"断开"则用红色
        val dotColor = if (statusMessage.contains("失败") || statusMessage.contains("错误")) Red400 else color
        
        Box(
            modifier = Modifier
                .size(8.dp)
                .clip(RoundedCornerShape(4.dp))
                .background(dotColor)
        )
        Spacer(modifier = Modifier.width(4.dp))
        Text(
            text = if (statusMessage.isNotEmpty()) statusMessage else text,
            style = MaterialTheme.typography.labelSmall,
            maxLines = 1,
            modifier = Modifier.widthIn(max = 160.dp),
            color = TextSecondary
        )
    }
}

/**
 * 获取连接提示
 */
private fun getConnectionHint(state: WebSocketState): String {
    return when (state) {
        WebSocketState.CONNECTED -> "已连接到设备，可以开始对话"
        WebSocketState.CONNECTING -> "正在连接..."
        WebSocketState.ERROR -> "连接失败，请检查设备IP和端口"
        WebSocketState.DISCONNECTED -> "点击「连接」按钮连接设备"
    }
}
