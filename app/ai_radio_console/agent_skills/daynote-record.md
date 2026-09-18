# DayNote Record（全天语音记录）

DayNote 全天候语音记忆的记录技能：把一段语音自动转写、分类、摘要并归档为一条笔记。

## When to use
- 用户说"记录一下"、"帮我记一条"、"开始录音"、"我说句话"，或按下 PTT / 录音键时。
- VAD 后台检测到一段完整语音（静音约 2 秒分段）自动触发时。
- 仅用于把用户的口述沉淀为笔记；不要把闲聊、天气、播放音乐等无关语音存档。

## How to use
1. get_current_time 获取当前时间戳。
2. 启动一次录音（PTT 走 voice_start / voice_stop；自动模式由 VAD 分段），采样固定为 16kHz / 16bit / 单声道。
3. 录音结束后调用 ASR（SiliconFlow，OpenAI 兼容：POST /v1/audio/transcriptions，模型 FunAudioLLM/SenseVoiceSmall；未配置密钥时回退内置火山 ASR）得到全文 transcript。
4. 调用 LLM 对 transcript 做结构化分析，输出：type（memo 备忘 / todo 待办 / schedule 日程）、20 字内 content 标题、summary 摘要、keywords 关键词；若为日程给出 remind_at。
5. 网络或 LLM 不可用时，改用本地关键词规则 memo_classify_local 兜底分类，content 取转录首句，保证不丢记录。
6. 通过 memo_store_add 写入 /data/daynote/memos.json，字段含 id/type/content/transcript/summary/keywords/timestamp/duration_ms/is_read；容量上限 100 条，自动淘汰最旧已读条目。
7. 通过消息总线发布 daynote.new_recording，刷新首页时间线。

## Example
用户："记录一下，明天下午三点和张老师对一下 PCB 的接线。"
→ ASR 得到全文 → LLM 判定 type=schedule，keywords="PCB,接线,张老师"，remind_at=次日15:00
→ 写入 memos.json 并在时间线生成一张日程卡片 → "已记录，明天 15:00 提醒你。"

## 断网降级
无网络时仍完成录音与本地分类，标记为待同步；恢复网络后补跑 ASR/LLM 并更新该条笔记。
