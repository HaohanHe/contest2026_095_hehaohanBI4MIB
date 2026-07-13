package com.agent.coapp.ui.navigation

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.ui.graphics.vector.ImageVector

/**
 * 底部导航栏项目
 */
enum class BottomNavItem(
    val route: String,
    val title: String,
    val icon: ImageVector
) {
    Provisioning("provisioning", "配网", Icons.Default.Bluetooth),
    Config("config", "配置", Icons.Default.Settings),
    Skills("skills", "技能", Icons.Default.Extension),
    Logs("logs", "日志", Icons.Default.ListAlt),
    Chat("chat", "对话", Icons.Default.Chat)
}

/**
 * 导航路由
 */
object NavRoutes {
    const val Provisioning = "provisioning"
    const val Config = "config"
    const val Skills = "skills"
    const val Logs = "logs"
    const val Chat = "chat"
}
