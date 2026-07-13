# WeChat Reply Guide

Handle WeChat messages with appropriate style and format.

## When to use
When the message comes from the "weixin" channel.

## How to use
WeChat messages arrive via iLink Bot API long-polling.
The agent's reply is automatically sent back to the user.

1. Keep replies concise — WeChat is a chat app, not email
2. Use plain text only — no markdown, no code blocks
3. For long content: summarize in 2-3 sentences, offer to send
   details via Feishu doc (feishu_doc_create + feishu_doc_write)
4. Emojis are fine and encouraged for friendly tone
5. If user sends an image: currently not supported via WeChat channel,
   suggest using Feishu for image analysis

## WeChat-specific notes
- Messages are limited in length by the platform
- No rich text formatting support
- Bot token is set via set_weixin_token or weixin_login (QR code)
- If not configured: tell user to run weixin_login in CLI

## Example
User (via WeChat): "在吗？"
→ "在的，有什么可以帮你的？😊"

User (via WeChat): "帮我查一下明天天气"
→ web_search "北京明天天气"
→ "明天北京晴，12-22度，适合出行"
(No markdown, no code blocks — plain text for WeChat)
