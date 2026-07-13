package com.agent.coapp.ui.skills

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.gestures.Orientation
import androidx.compose.foundation.gestures.draggable
import androidx.compose.foundation.gestures.rememberDraggableState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.clickable
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.agent.coapp.data.Skill
import com.agent.coapp.viewmodel.SkillsViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SkillsScreen(viewModel: SkillsViewModel = viewModel()) {
    val skills by viewModel.skills.collectAsState()
    val isLoading by viewModel.isLoading.collectAsState()
    val message by viewModel.message.collectAsState()
    val showPushDialog by viewModel.showPushDialog.collectAsState()

    LaunchedEffect(Unit) { viewModel.loadSkills() }

    LaunchedEffect(message) {
        if (message.isNotEmpty()) {
            kotlinx.coroutines.delay(3000)
            viewModel.clearMessage()
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("技能管理") },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.background
                ),
                actions = {
                    IconButton(onClick = { viewModel.loadSkills() }) {
                        Icon(Icons.Default.Refresh, contentDescription = "刷新")
                    }
                }
            )
        },
        floatingActionButton = {
            SmallFloatingActionButton(onClick = { viewModel.showPushDialog() }) {
                Icon(Icons.Default.Add, contentDescription = "添加技能")
            }
        }
    ) { padding ->
        Column(Modifier.fillMaxSize().padding(padding)) {
            if (message.isNotEmpty()) {
                Card(
                    Modifier.fillMaxWidth().padding(16.dp),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.secondaryContainer
                    )
                ) {
                    Text(message, Modifier.padding(16.dp), style = MaterialTheme.typography.bodyMedium)
                }
            }

            if (isLoading) {
                Box(Modifier.fillMaxWidth().padding(16.dp), contentAlignment = Alignment.Center) {
                    CircularProgressIndicator()
                }
            }

            LazyColumn(
                Modifier.fillMaxSize(),
                contentPadding = PaddingValues(16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(skills) { skill ->
                    SkillCard(skill = skill, onDelete = { viewModel.deleteSkill(skill.name) })
                }
                if (skills.isEmpty() && !isLoading) {
                    item {
                        Box(Modifier.fillMaxWidth().padding(32.dp), contentAlignment = Alignment.Center) {
                            Text("暂无技能，点击 + 添加", style = MaterialTheme.typography.bodyLarge,
                                color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                }
            }
        }
    }

    if (showPushDialog) {
        PushSkillDialog(
            onDismiss = { viewModel.hidePushDialog() },
            onPush = { name, content -> viewModel.pushSkill(name, content) }
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun SkillCard(skill: Skill, onDelete: () -> Unit) {
    var showDeleteConfirm by remember { mutableStateOf(false) }
    var expanded by remember { mutableStateOf(false) }

    Card(Modifier.fillMaxWidth().clickable { expanded = !expanded }) {
        Column(Modifier.fillMaxWidth().padding(16.dp)) {
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.Top
            ) {
                Column(Modifier.weight(1f)) {
                    Text(skill.name, style = MaterialTheme.typography.titleMedium)
                    if (skill.description.isNotEmpty()) {
                        Spacer(Modifier.height(4.dp))
                        Text(skill.description, style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            maxLines = if (expanded) Int.MAX_VALUE else 2)
                    }
                    Spacer(Modifier.height(4.dp))
                    Text(
                        text = "${skill.file}  •  ${formatSize(skill.size)}  •  ${skill.mtime.take(10)}",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                IconButton(onClick = { showDeleteConfirm = true }) {
                    Icon(Icons.Default.Delete, contentDescription = "删除",
                        tint = MaterialTheme.colorScheme.error)
                }
            }
            if (expanded && !skill.content.isNullOrEmpty()) {
                Spacer(Modifier.height(8.dp))
                Divider()
                Spacer(Modifier.height(8.dp))
                val clipboardManager = LocalClipboardManager.current
                var copied by remember { mutableStateOf(false) }
                Text(skill.content,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier.combinedClickable(
                        onClick = {},
                        onLongClick = {
                            clipboardManager.setText(AnnotatedString(skill.content))
                            copied = true
                        }
                    ))
                if (copied) {
                    Text("已复制", style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary)
                    LaunchedEffect(Unit) {
                        kotlinx.coroutines.delay(1500)
                        copied = false
                    }
                }
            } else if (expanded) {
                Spacer(Modifier.height(8.dp))
                Text("（设备未返回内容，请更新到固件最新版）",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }

    if (showDeleteConfirm) {
        AlertDialog(
            onDismissRequest = { showDeleteConfirm = false },
            title = { Text("确认删除") },
            text = { Text("确定要删除技能 \"${skill.name}\" 吗？") },
            confirmButton = {
                TextButton(onClick = { showDeleteConfirm = false; onDelete() }) {
                    Text("删除", color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteConfirm = false }) { Text("取消") }
            }
        )
    }
}

/**
 * 简化推送对话框：只需技能名 + markdown 内容
 */
@Composable
private fun PushSkillDialog(
    onDismiss: () -> Unit,
    onPush: (name: String, content: String) -> Unit
) {
    var name by remember { mutableStateOf("") }
    var content by remember { mutableStateOf("") }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("推送技能") },
        text = {
            Column {
                OutlinedTextField(
                    value = name,
                    onValueChange = { name = it },
                    label = { Text("技能名称") },
                    placeholder = { Text("如: weather") },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true
                )
                Spacer(Modifier.height(8.dp))
                OutlinedTextField(
                    value = content,
                    onValueChange = { content = it },
                    label = { Text("技能内容 (Markdown)") },
                    placeholder = { Text("---\ntitle: ...\ndescription: ...\n---\n技能正文...") },
                    modifier = Modifier.fillMaxWidth().heightIn(min = 200.dp),
                    minLines = 8,
                    maxLines = 15
                )
            }
        },
        confirmButton = {
            Button(
                onClick = { onPush(name.trim(), content) },
                enabled = name.isNotBlank() && content.isNotBlank()
            ) { Text("推送") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("取消") }
        }
    )
}

private fun formatSize(bytes: Long): String = when {
    bytes < 1024 -> "${bytes}B"
    bytes < 1024 * 1024 -> "${bytes / 1024}KB"
    else -> "${"%.1f".format(bytes / 1024.0 / 1024.0)}MB"
}
