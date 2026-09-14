# Weather Alert

Proactive weather monitoring with scheduled alerts.

## When to use
When user asks for weather alerts, rain warning, temperature drop alert,
or wants to be notified about weather changes.

## How to use
1. get_current_time for current date
2. web_search for weather forecast (next 24-48 hours)
3. Analyze for alert conditions:
   - Rain/snow expected → remind to bring umbrella
   - Temperature drop >10°C → remind to dress warm
   - Extreme heat >35°C → remind to stay hydrated
   - Strong wind >50km/h → outdoor activity warning
4. If user wants recurring alerts:
   cron_add a daily job (e.g. 7:00 AM) that triggers this skill
5. Deliver alert via the user's active channel (feishu/voice/cli)

## Example
User: "Warn me if it's going to rain tomorrow"
1. web_search "weather forecast Beijing tomorrow rain"
2. If rain expected: cron_add one-shot job for tomorrow 7:00 AM
   with message "Rain expected today, bring an umbrella!"
3. Confirm: "I'll alert you tomorrow morning if rain is forecast."
