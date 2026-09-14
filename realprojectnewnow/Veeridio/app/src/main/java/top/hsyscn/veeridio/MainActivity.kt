package top.hsyscn.veeridio

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.SettingsEthernet
import androidx.compose.material.icons.filled.ViewList
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.navigation.NavDestination.Companion.hierarchy
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import top.hsyscn.veeridio.ui.logs.LogsScreen
import top.hsyscn.veeridio.ui.navigation.Screen
import top.hsyscn.veeridio.ui.provisioning.ProvisioningScreen
import top.hsyscn.veeridio.ui.ptt.PttScreen
import top.hsyscn.veeridio.ui.theme.VeeridioTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            VeeridioTheme {
                RadioConsoleApp()
            }
        }
    }
}

private data class NavItem(
    val screen: Screen,
    val icon: ImageVector,
    val label: String
)

@Composable
fun RadioConsoleApp() {
    val navController = rememberNavController()

    val items = listOf(
        NavItem(Screen.Provisioning, Icons.Default.SettingsEthernet, "配网"),
        NavItem(Screen.Logs, Icons.Default.ViewList, "日志"),
        NavItem(Screen.Ptt, Icons.Default.Mic, "PTT")
    )

    Scaffold(
        modifier = Modifier.fillMaxSize(),
        bottomBar = {
            NavigationBar {
                val navBackStackEntry by navController.currentBackStackEntryAsState()
                val currentDestination = navBackStackEntry?.destination
                items.forEach { item ->
                    NavigationBarItem(
                        icon = { Icon(item.icon, contentDescription = item.label) },
                        label = { Text(item.label) },
                        selected = currentDestination?.hierarchy?.any {
                            it.route == item.screen.route
                        } == true,
                        onClick = {
                            navController.navigate(item.screen.route) {
                                popUpTo(navController.graph.findStartDestination().id) {
                                    saveState = true
                                }
                                launchSingleTop = true
                                restoreState = true
                            }
                        }
                    )
                }
            }
        }
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = Screen.Provisioning.route,
            modifier = Modifier.padding(innerPadding)
        ) {
            composable(Screen.Provisioning.route) { ProvisioningScreen() }
            composable(Screen.Logs.route) { LogsScreen() }
            composable(Screen.Ptt.route) { PttScreen() }
        }
    }
}
