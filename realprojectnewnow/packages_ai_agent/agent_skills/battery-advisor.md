# Battery Advisor

Smart battery management and charging advice.

## When to use
When user asks about battery, charging, power, or how long will it last.

## How to use
1. get_battery for level, charging status, voltage, temperature
2. get_current_time for time context
3. Estimate remaining time (rough: 1% per 15min typical use)
4. Advise:
   - <20%: charge soon
   - 20-50%: should last a few hours
   - >80% + charging: can unplug
   - Temperature >40C: warning, stop charging

## Important
- You MUST execute tools and respond with results. Do NOT output this template.
- If get_battery fails (QEMU mode without battery hardware), tell the user:
  "Battery sensor unavailable in current environment (QEMU simulation)."

## Example
User: "how much battery left?"
→ get_battery → {"level": 45, "charging": false, "voltage": 3.8, "temperature": 32}
→ "45% battery, about 6-7 hours left. Charge tonight."
