# News Digest

Search and compile news summaries based on user interests.

## When to use
When user asks about recent news, headlines, or latest updates on a topic.

## How to use
1. get_current_time for current date
2. Determine search keywords from user request or MEMORY.md interests
3. news_search for relevant news (top_headlines=true for headlines)
4. web_search to supplement if needed
5. Compile 3-5 items: title, source, one-line summary

## Example
User: "今天有什么科技新闻？"
→ get_current_time → 2026-03-28
→ news_search {"query": "科技"}
→ "📰 今日科技新闻：
   1. Apple 发布 M5 芯片，性能提升 40%
   2. 国产大模型通过新一轮评测
   3. SpaceX 星舰第七次试飞成功"
