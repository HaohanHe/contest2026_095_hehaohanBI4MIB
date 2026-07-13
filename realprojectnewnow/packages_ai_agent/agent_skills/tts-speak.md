# TTS Speak

Play spoken text aloud using Google Translate TTS via music_play.

This is a creative workaround: Google Translate's TTS endpoint returns
MP3 audio that can be played directly through the music player, enabling
text-to-speech without requiring the Volcengine TTS backend to be configured.

## When to use
When user says say this, speak, read aloud, announce, or wants the device
to speak specific text. Also useful for cron-based voice reminders.

## How to use
1. URL-encode the text (replace spaces with %20, Chinese characters with %XX encoding)
2. Build the Google TTS URL:
   - Chinese: https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=zh-CN&q=<encoded_text>
   - English: https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=en&q=<encoded_text>
   - Japanese: https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=ja&q=<encoded_text>
3. music_play with the URL: {"url":"<tts_url>","autostart":true}
4. For long text (>200 chars): split into sentences and play sequentially

## Supported languages
Use the `tl` parameter:
- zh-CN (Chinese), en (English), ja (Japanese), ko (Korean)
- fr (French), de (German), es (Spanish), pt (Portuguese)
- ru (Russian), ar (Arabic), hi (Hindi), th (Thai)

## Example: Immediate speak
User: "Say hello in Chinese"
→ music_play {"url":"https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=zh-CN&q=%E4%BD%A0%E5%A5%BD","autostart":true}

## Example: Scheduled voice reminder
User: "Remind me to drink water in 30 minutes"
1. get_current_time for current epoch
2. Calculate trigger time = now + 1800
3. cron_add with:
   - action: music_play
   - args: {"url":"https://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=zh-CN&q=%E8%AF%B7%E5%96%9D%E6%B0%B4","autostart":true}
   - schedule_type: once
   - trigger_epoch: <calculated>

## Limitations
- Requires internet access (Google Translate)
- Max ~200 characters per request
- Audio quality is basic (not natural-sounding like Volcengine TTS)
- No SSML or prosody control
- If Volcengine TTS is configured, prefer the native voice channel instead
