package com.agent.coapp

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.agent.coapp.ui.chat.ChatScreen
import com.agent.coapp.ui.config.ConfigScreen
import com.agent.coapp.ui.logs.LogsScreen
import com.agent.coapp.ui.navigation.BottomNavItem
import com.agent.coapp.ui.navigation.NavRoutes
import com.agent.coapp.ui.provisioning.BleProvisioningScreen
import com.agent.coapp.ui.skills.SkillsScreen
import com.agent.coapp.ui.theme.AndroidAgentAppTheme
import com.agent.coapp.viewmodel.ChatViewModel
import androidx.lifecycle.viewmodel.compose.viewModel

/**
 * 主Activity
 */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            AndroidAgentAppTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MainScreen()
                }
            }
        }
    }
}

/**
 * 主界面
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen() {
    val navController = rememberNavController()
    val navBackStackEntry by navController.currentBackStackEntryAsState()
    val currentRoute = navBackStackEntry?.destination?.route
    val chatViewModel: ChatViewModel = viewModel()
    
    Scaffold(
        bottomBar = {
            NavigationBar {
                BottomNavItem.entries.forEach { item ->
                    NavigationBarItem(
                        icon = { Icon(item.icon, contentDescription = item.title) },
                        label = { Text(item.title) },
                        selected = currentRoute == item.route,
                        onClick = {
                            if (currentRoute != item.route) {
                                navController.navigate(item.route) {
                                    // 避免重复创建相同的导航栈
                                    popUpTo(navController.graph.startDestinationId) {
                                        saveState = true
                                    }
                                    launchSingleTop = true
                                    restoreState = true
                                }
                            }
                        }
                    )
                }
            }
        }
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = NavRoutes.Provisioning,
            modifier = Modifier.padding(innerPadding)
        ) {
            composable(NavRoutes.Provisioning) {
                BleProvisioningScreen()
            }
            composable(NavRoutes.Config) {
                ConfigScreen()
            }
            composable(NavRoutes.Skills) {
                SkillsScreen()
            }
            composable(NavRoutes.Logs) {
                LogsScreen()
            }
            composable(NavRoutes.Chat) {
                ChatScreen(viewModel = chatViewModel)
            }
        }
    }
}
