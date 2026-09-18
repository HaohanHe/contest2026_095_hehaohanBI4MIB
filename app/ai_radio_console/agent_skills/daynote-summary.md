# DayNote Summary（每日回顾与高光）

把一天中散落的多条语音笔记聚合成一份每日回顾：今日高光、待办与关键词云。

## When to use
- 用户说"总结一下今天"、"今天都说了什么"、"今日回顾"、"日报"。
- 到达设定的每日回顾时间（默认 DAYNOTE_REVIEW_INTERVAL=14400 秒周期，或晚间定时）自动触发。
- 需要跨多条笔记做归纳时使用；单条笔记的摘要由 daynote-record 完成。

## How to use
1. get_current_time 确定目标日期。
2. read_file 读取 /data/daynote/memos.json，筛出当天全部笔记（含 memo/todo/schedule）。
3. 先在端侧聚合：统计条数、类型分布、提取高频 keywords、列出未完成 todo 与到期 schedule。
4. 联网时把当天 transcript/summary 批量交给 LLM，生成：2–3 条"今日高光"、一段连贯的每日总结、明日建议。
5. 断网时仅输出端侧聚合结果（条数 + 关键词 + 待办清单），不阻塞。
6. 在 Home 页顶部展示今日高光，并可经消息总线 daynote.daily_summary 推送到手机端。

## Example
晚间用户："总结一下今天。"
→ 聚合当天 12 条记录 → LLM 输出高光："① 确定 DayNote 走 ai_agent 框架；② 修好电阻触摸配置；③ 注册了 BrandMeister DMR ID"，并列出 2 条未完成待办。

## 输出约束
- 只基于当天真实笔记归纳，不臆造未记录的内容；素材不足时如实说明"今天记录较少"。
- 总结语言简洁，适配小屏阅读，单屏可看完核心结论。
