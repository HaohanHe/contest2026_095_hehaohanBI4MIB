package com.agent.coapp.ui.theme

import androidx.compose.ui.graphics.Color

// === 底色三档 ===
val L1 = Color(0xFF0D1117)           // 最深底 - 背景基底
val L2 = Color(0xFF161B22)           // 中间层 - 卡片/输入框/导航栏
val L3 = Color(0xFF1C2128)           // 浅层 - AI气泡/可点击区域

// === Accent 雾青 ===
val Teal400 = Color(0xFF5EEAD4)      // Accent主 - 图标/文字高亮/状态点
val AccentMist = Color(0xFF3ECFB8)   // Accent雾 - 按钮描边/边框线/小面积点缀
val AccentDark = Color(0xFF1A6B5A)   // Accent暗 - 用户气泡底色

// === 文字色 ===
val TextPrimary = Color(0xFFE6EDF3)  // 主文字
val TextSecondary = Color(0xFF8B949E)// 次文字
val TextWeak = Color(0xFF484F58)     // 弱文字/占位符

// === 功能色 ===
val Green400 = Color(0xFF3FB950)     // 成功/完成
val Red400 = Color(0xFFF85149)       // 错误/失败
val Amber400 = Color(0xFFFBBF24)     // 警告

// === 对话气泡 ===
val UserBubble = AccentDark          // 用户气泡底色
val AiBubble = L3                    // AI气泡底色
