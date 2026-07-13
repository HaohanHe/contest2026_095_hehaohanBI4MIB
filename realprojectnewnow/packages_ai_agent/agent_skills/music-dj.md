# Music DJ

Play and control music based on user mood or request.

## When to use
When user asks to play music, wants a song, mentions mood-based music,
or says 下一首/上一首/换一首/切歌/skip/next.

## How to use
1. Parse user intent: play, pause, stop, volume, next/skip, search
2. For play: music_play with file path or URL
3. For volume: music_set_volume (0-100)
4. For status: music_status to check what's playing
5. For pause/resume: music_pause or music_resume
6. For seek: music_seek with milliseconds
7. For search: music_search with keyword, then music_play with result URL

## Next/Skip song (下一首/换一首/切歌)
When user says 下一首, 换一首, 切歌, skip, or next:
1. Call music_search with keyword "热门歌曲" (or a random genre keyword)
2. Pick the FIRST song from the results
3. Call music_play with that song's URL
4. Reply: "正在播放《{song name}》- {artist} 🎵"

IMPORTANT: Do NOT call music_search repeatedly with different keywords.
One search → pick first result → play. Total: 2 tool calls max.

## Supported formats
PCM, WAV, MP3. Local files in /data/ or HTTP URLs.

## Example
User: "播放一首欢快的音乐"
→ music_search {"keyword": "欢快的音乐"}
→ music_play {"url": "https://music.163.com/song/media/outer/url?id=xxx.mp3"}
→ "正在播放《xxx》🎵"

User: "下一首"
→ music_search {"keyword": "热门歌曲"}
→ music_play {"url": "https://music.163.com/song/media/outer/url?id=xxx.mp3"}
→ "正在播放《xxx》- xxx 🎵"

User: "声音小一点"
→ music_set_volume {"volume": 30}
→ "已调低音量到 30%"
