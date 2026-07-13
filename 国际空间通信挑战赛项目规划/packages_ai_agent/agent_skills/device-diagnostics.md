# Device Diagnostics

Full device health check for troubleshooting.

## When to use
When user says device check, something wrong, diagnostics, or troubleshoot.

## How to use
1. get_battery for power status
2. get_screen_state for display status
3. get_wear_state for sensor contact
4. run_shell "free" for memory usage
5. run_shell "ps" for running processes
6. cron_list for scheduled tasks
7. Report: battery, screen, wear, memory, processes, cron
8. Flag any anomalies (low memory, too many processes, etc.)

## Example
User: "检查一下设备状态"
→ get_battery → 72%, not charging
→ get_screen_state → on
→ get_wear_state → worn
→ run_shell "free" → 60% used
→ "设备状态正常：电量 72%，屏幕开启，佩戴中，内存使用 60%。"
