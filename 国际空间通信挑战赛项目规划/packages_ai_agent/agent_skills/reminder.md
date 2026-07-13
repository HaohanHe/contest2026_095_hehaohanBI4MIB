# Reminder

Set timed reminders that auto-notify the user.

## When to use
When user says remind me, set alarm, notify me later, 提醒我, 定时.

## How to use
1. get_current_time for current epoch
2. Parse user request into schedule_type and timing:
   - "5分钟后" → once, trigger_epoch = now + 300
   - "每天早上8点" → daily, cron = "0 8 * * *"
   - "明天下午3点" → once, trigger_epoch = tomorrow 15:00
3. Set channel/chat_id matching the message source (feishu/system)
4. cron_add to create the job
5. Confirm with human-readable trigger time (not epoch)

## Example
User: "提醒我5分钟后喝水"
→ get_current_time → epoch 1711612800
→ cron_add {"action": "notify", "message": "该喝水了！", "schedule_type": "once", "trigger_epoch": 1711613100}
→ "好的，5分钟后提醒你喝水。"
