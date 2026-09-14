# App Launcher

Launch and manage quick apps by name or intent.

## When to use
When user says open app, launch, start, or mentions an app name.
Also when user says close app, exit, go home.

## How to use
- To launch: launch_quickapp with the package name
- To exit: exit_quickapp to return to home screen
- If user gives app name instead of package: infer the package name
  or ask user to clarify

## Example
User: "打开计算器"
→ launch_quickapp {"package": "com.example.calculator"}
→ "已为你打开计算器"

User: "回到主页"
→ exit_quickapp {}
→ "已返回主屏幕"
