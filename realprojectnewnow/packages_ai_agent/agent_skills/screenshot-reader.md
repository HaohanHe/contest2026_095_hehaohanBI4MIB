# Screenshot Reader

Analyze screen content using Vision AI.

## When to use
When user says what's on screen, read this, OCR, or analyze image.

## How to use
1. analyze_image (auto-captures screen if no image provided)
2. Parse the Vision AI response
3. Present extracted text or image description
4. If user asks follow-up: use the extracted content as context

## Example
User: "屏幕上显示了什么？"
→ analyze_image (auto-captures screen)
→ "屏幕上显示的是时钟界面，当前时间 15:30，日期 3月28日。"
