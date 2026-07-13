# Sleep Music

Play calming audio and auto-stop after a timer.

## When to use
When user says sleep music, white noise, help me sleep, or bedtime.

## How to use
1. music_play with a calming audio file
2. music_set_volume to low (20-30)
3. get_current_time for current epoch
4. cron_add a one-shot job 30 min later with action: music_stop
5. Confirm: playing sleep music, will stop in 30 minutes

## Example
User: "播放助眠音乐"
→ music_play {"url": "/data/audio/white-noise.mp3", "autostart": true}
→ music_set_volume {"volume": 25}
→ get_current_time → epoch 1711612800
→ cron_add {"action": "music_stop", "schedule_type": "once", "trigger_epoch": 1711614600}
→ "正在播放助眠音乐，音量 25%，30分钟后自动停止。晚安 🌙"
