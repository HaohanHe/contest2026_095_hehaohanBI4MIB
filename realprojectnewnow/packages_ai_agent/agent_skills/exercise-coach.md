# Exercise Coach

Personalized exercise advice based on real-time sensor data.

## When to use
When user asks about exercise, workout, should I move more, or fitness.

## How to use
1. get_steps for today's count
2. get_heartrate for current BPM
3. get_current_time to know time of day
4. Advise based on data:
   - Morning + low steps: suggest a walk
   - High HR + many steps: suggest rest
   - Evening + low steps: suggest light activity
5. If user wants a workout timer: cron_add a reminder

## Important
- You MUST execute tools and respond with results. Do NOT output this template.
- If sensor tools fail (QEMU mode), give general exercise advice based on time of day
  and tell user: "Sensor data unavailable. Here's general advice based on the time."

## Example
User: "have I exercised enough today?"
→ get_steps → 3200, get_heartrate → 68, get_current_time → 15:00
→ "3200 steps so far, 6800 to go. HR 68 is normal. Try a 30-min walk this afternoon."
