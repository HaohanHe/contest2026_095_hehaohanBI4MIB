# Translate

Translate text between languages.

## When to use
When user asks to translate text, or says 翻译、translate、how to say.

## How to use
1. Identify source and target languages from context
2. If not specified, assume: Chinese input → English output, English input → Chinese output
3. Translate using language knowledge
4. For specialized/technical terms, use web_search to verify accuracy
5. Provide translation with romanization for non-Latin scripts

## Output format
Always use this consistent format:
- First line: the translation
- Second line: romanization in parentheses (for Japanese/Korean/Arabic etc.)
- Third line: brief note on usage context if relevant

Keep it concise — no lengthy explanations unless user asks.

## Example
User: "帮我把'你好世界'翻译成英文"
→ "Hello World"

User: "Translate 'good morning' to Japanese"
→ "おはようございます (Ohayou gozaimasu)
   用于早上见面时的正式问候。"

User: "'嵌入式系统'用英文怎么说"
→ "Embedded System
   技术术语，指嵌入到设备中的专用计算机系统。"
