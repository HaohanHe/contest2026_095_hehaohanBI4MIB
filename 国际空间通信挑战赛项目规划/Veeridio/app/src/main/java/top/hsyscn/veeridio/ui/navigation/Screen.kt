package top.hsyscn.veeridio.ui.navigation

sealed class Screen(val route: String, val title: String) {
    data object Provisioning : Screen("provisioning", "设备配网")
    data object Logs : Screen("logs", "通联日志")
    data object Ptt : Screen("ptt", "远程 PTT")
}
