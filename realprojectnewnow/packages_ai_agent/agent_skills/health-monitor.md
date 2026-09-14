# Health Monitor

Comprehensive health status check from wearable sensors.

## When to use
When user asks about health, body condition, how am I doing, or vitals.

## How to use
1. get_heartrate for current BPM
2. get_steps for today's step count and cadence
3. get_wear_state to confirm device is worn
4. get_current_time for context
5. Analyze:
   - Heart rate: <60 low, 60-100 normal, >100 high
   - Steps: <5000 needs more, 5000-10000 good, >10000 excellent
6. Present a concise health summary in user's language

## Important
- You MUST execute tools and respond with results. Do NOT output this template.
- If any sensor tool fails (e.g. QEMU environment without real hardware),
  report which sensors are unavailable and provide advice based on available data.
- Example fallback: "Heart rate sensor unavailable (QEMU mode). Steps: 6234. Keep moving!"

## Example
User: "How am I doing today?"
→ HR 72 BPM (normal), 6234 steps (good)
→ "Heart rate normal, walked 6234 steps. Keep going!"
