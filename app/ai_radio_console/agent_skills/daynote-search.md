# DayNote Search（语音笔记检索）

在 DayNote 本地记忆库中按关键词、时间或类型检索历史笔记，并在 320x240 屏幕上回看。

## When to use
- 用户问"我之前说过……"、"找一下关于 XX 的记录"、"今天/这周记了什么"、"我有没有记过 XX"。
- 用户在回顾页（Review）输入或语音说出检索词时。
- 仅检索本机 /data/daynote/memos.json 中的笔记，不做全网搜索。

## How to use
1. 从用户语句中提取检索意图：关键词、时间范围（今天 / 最近 7 天 / 指定日期）、类型（memo/todo/schedule）。
2. read_file 读取 /data/daynote/memos.json（由 memory_index 维护的关键词与时间线索引）。
3. 优先按 keywords 字段匹配，其次匹配 content/summary/transcript；按相关度与时间近因排序。
4. 调 memo_store_get_recent / memo_store_get_count 取候选，过滤 is_read 与类型。
5. 在 Review 页以卡片列表展示：标题 content、时间、关键词；选中后展开 summary 与全文 transcript。
6. 命中待办/日程且到期时，提示并可标记 is_read；无结果时明确告知"没有找到相关记录"。

## Example
用户："找一下我之前记的关于 DMR 的笔记。"
→ 关键词="DMR" → 命中 MMDVM/BrandMeister 相关条目 → 卡片列出"配置 DMR 热点""DMR ID 注册"等 → 用户点开看全文。

## 备注
本地检索为纯端侧能力，断网可用；检索结果不离开设备，保障私人记忆隐私。
