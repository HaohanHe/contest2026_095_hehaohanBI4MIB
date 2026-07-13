package com.agent.coapp.ui.logs

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.agent.coapp.viewmodel.LogsViewModel

/**
 * 日志查看页面
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun LogsScreen(
    viewModel: LogsViewModel = viewModel()
) {
    val logs by viewModel.filteredLogs.collectAsState()
    val isLoading by viewModel.isLoading.collectAsState()
    val message by viewModel.message.collectAsState()
    val filterKeyword by viewModel.filterKeyword.collectAsState()
    
    val listState = rememberLazyListState()
    
    // 加载日志
    LaunchedEffect(Unit) {
        viewModel.loadLogs()
    }
    
    // 自动滚动到底部
    LaunchedEffect(logs.size) {
        if (logs.isNotEmpty()) {
            listState.animateScrollToItem(logs.size - 1)
        }
    }
    
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("日志查看") },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.background
                ),
                actions = {
                    IconButton(onClick = { viewModel.refresh() }) {
                        Icon(Icons.Default.Refresh, contentDescription = "刷新")
                    }
                }
            )
        }
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
        ) {
            // 过滤输入框
            OutlinedTextField(
                value = filterKeyword,
                onValueChange = { viewModel.setFilterKeyword(it) },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 16.dp, vertical = 8.dp),
                placeholder = { Text("输入关键词过滤...") },
                leadingIcon = {
                    Icon(Icons.Default.Search, contentDescription = null)
                },
                trailingIcon = {
                    if (filterKeyword.isNotEmpty()) {
                        IconButton(onClick = { viewModel.setFilterKeyword("") }) {
                            Icon(Icons.Default.Clear, contentDescription = "清除")
                        }
                    }
                },
                singleLine = true
            )
            
            // 消息提示
            if (message.isNotEmpty()) {
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp),
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
            
            // 日志内容
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 8.dp)
            ) {
                if (isLoading) {
                    CircularProgressIndicator(
                        modifier = Modifier.align(Alignment.Center)
                    )
                } else if (logs.isEmpty()) {
                    Text(
                        text = "暂无日志",
                        modifier = Modifier.align(Alignment.Center),
                        style = MaterialTheme.typography.bodyLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                } else {
                    val horizontalScrollState = rememberScrollState()
                    LazyColumn(
                        state = listState,
                        modifier = Modifier
                            .fillMaxSize()
                            .background(Color(0xFF1E1E1E))
                            .horizontalScroll(horizontalScrollState)
                            .padding(8.dp)
                    ) {
                        items(logs) { log ->
                            LogLine(log = log)
                        }
                    }
                }
            }
        }
    }
}

/**
 * 日志行
 */
@Composable
private fun LogLine(log: String) {
    Text(
        text = log,
        modifier = Modifier.padding(vertical = 2.dp),
        fontFamily = FontFamily.Monospace,
        fontSize = 12.sp,
        color = getLogColor(log),
        maxLines = 1,
        softWrap = false
    )
}

/**
 * 根据日志内容返回颜色
 */
@Composable
private fun getLogColor(log: String): Color {
    return when {
        log.contains("ERROR", ignoreCase = true) -> Color(0xFFFF6B6B) // 红色
        log.contains("WARN", ignoreCase = true) -> Color(0xFFFFB347) // 橙色
        log.contains("INFO", ignoreCase = true) -> Color(0xFF69DB7C) // 绿色
        log.contains("DEBUG", ignoreCase = true) -> Color(0xFF74C0FC) // 蓝色
        log.contains("TRACE", ignoreCase = true) -> Color(0xFF868E96) // 灰色
        else -> Color(0xFFE9ECEF) // 浅灰
    }
}
