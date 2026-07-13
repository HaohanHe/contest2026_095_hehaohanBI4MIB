# Weather

Get current weather and forecasts using web_search.

## When to use
When the user asks about weather, temperature, or forecasts.
触发词：天气、温度、下雨、weather、forecast。

## How to use
1. Use get_current_time to know the current date
2. Use web_search with a query like "天气 [城市] 今天" or "weather [city] today"
3. Extract temperature, conditions, and forecast from results
4. Present in a concise, friendly format using the user's language

## Output format
Use the user's language (Chinese by default). Format:
- 城市 + 当前温度 + 天气状况
- 最高/最低温度
- 简短建议（带伞/加衣等）

## Example
User: "北京今天天气怎么样"
→ get_current_time → 2026-03-28
→ web_search "北京天气 2026年3月28日"
→ "北京：15°C，晴。最高 20°C，最低 8°C。适合户外活动。"

User: "东京明天会下雨吗"
→ get_current_time → 2026-03-28
→ web_search "东京天气预报 2026年3月29日"
→ "东京明天：12°C，多云转小雨。建议带伞。"
