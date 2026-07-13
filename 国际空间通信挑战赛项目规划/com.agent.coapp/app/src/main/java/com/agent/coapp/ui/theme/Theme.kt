package com.agent.coapp.ui.theme

import android.app.Activity
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

private val DarkColorScheme = darkColorScheme(
    // 品牌 - accent只给线/点
    primary = AccentMist,
    onPrimary = TextPrimary,
    primaryContainer = AccentDark,
    onPrimaryContainer = AccentMist,
    // 二级
    secondary = TextSecondary,
    onSecondary = L1,
    secondaryContainer = L3,
    onSecondaryContainer = TextPrimary,
    // 三级
    tertiary = Green400,
    onTertiary = L1,
    tertiaryContainer = Color(0xFF1A3A2A),
    onTertiaryContainer = Green400,
    // 错误
    error = Red400,
    onError = L1,
    errorContainer = Color(0xFF3B1A1A),
    onErrorContainer = Red400,
    // 表面
    background = L1,
    onBackground = TextPrimary,
    surface = L1,
    onSurface = TextPrimary,
    surfaceVariant = L2,
    onSurfaceVariant = TextSecondary,
    // 轮廓
    outline = L3,
    outlineVariant = Color(0xFF2D333B),
    // 反色
    inverseSurface = TextPrimary,
    inverseOnSurface = L1,
    inversePrimary = AccentDark,
)

@Composable
fun AndroidAgentAppTheme(
    content: @Composable () -> Unit
) {
    val colorScheme = DarkColorScheme

    val view = LocalView.current
    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as Activity).window
            window.statusBarColor = L1.toArgb()
            window.navigationBarColor = L2.toArgb()
            WindowCompat.getInsetsController(window, view).isAppearanceLightStatusBars = false
            WindowCompat.getInsetsController(window, view).isAppearanceLightNavigationBars = false
        }
    }

    MaterialTheme(
        colorScheme = colorScheme,
        typography = AgentTypography,
        shapes = AgentShapes,
        content = content
    )
}
