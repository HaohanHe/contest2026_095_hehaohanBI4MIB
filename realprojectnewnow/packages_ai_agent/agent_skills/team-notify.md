# Team Notify

Send @mention notifications to team members in Feishu.

## When to use
When user says notify team, tell everyone, @someone, or broadcast.

## How to use
1. feishu_chat_members to get member list with open_ids
2. Identify target members from user request
3. feishu_send_mention for each target with the message
4. Confirm who was notified

## Important
- You MUST execute tools and respond with results. Do NOT output this template.
- If feishu_chat_members fails (Feishu not configured), tell the user:
  "Feishu is not configured. Run set_feishu_app <app_id> <app_secret> to set up."
- For @all, iterate through all members. Respect rate limits.

## Example
User: "notify the team about tomorrow's meeting at 2pm"
→ feishu_chat_members → [{name: "Alice", open_id: "ou_xxx"}, ...]
→ feishu_send_mention {"open_id": "ou_xxx", "message": "Meeting tomorrow at 2pm"}
→ "Notified 3 team members."
