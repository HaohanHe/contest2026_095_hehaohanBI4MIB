# LLM Router Guide

Help user manage and optimize multi-backend LLM routing.

## When to use
When user asks about LLM settings, model switching, cost optimization,
which model to use, or router configuration.

## How to use
AI Agent supports up to 4 LLM backends with intelligent routing.

### Check current status
run_shell "router_status" to see all backends, their metrics, and current profile.

### Add/change backends
run_shell "router_set <preset> <api_key>" to add a backend.
Presets: kimi, qwen, deepseek, glm, mimo, openai, claude, openrouter

### Switch routing profile
run_shell "router_profile <profile>" where profile is:
- eco: prefer cheapest backend (mimo-flash, qwen-turbo)
- auto: balance cost and quality based on task complexity
- premium: prefer highest quality (claude, kimi, openai)

### Quick model switch
run_shell "set_llm <preset> <key>" for single-backend mode
run_shell "set_llm model <name>" to change model only

## Recommendations
- For casual chat: eco profile with mimo or qwen
- For complex reasoning: premium profile with kimi or claude
- For code generation: deepseek or openai
- For cost-sensitive: openrouter with free models (list_models --free)

## Example
User: "看看当前用的什么模型"
→ run_shell "router_status"
→ "当前路由状态：
   Slot 0: kimi (api.moonshot.cn, kimi-k2.5) — 活跃
   Profile: auto
   建议：如需省钱可切换 eco 模式：router_profile eco"

User: "切换到省钱模式"
→ run_shell "router_profile eco"
→ "已切换到 eco 模式，优先使用低成本模型。"
