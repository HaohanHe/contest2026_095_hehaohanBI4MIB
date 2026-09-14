# Radio Log

description: Parse amateur radio QSO voice transcripts and save structured logs.

Parse amateur radio QSO voice transcripts and save structured logs.

## When to use

When the user provides a transcribed radio communication and asks to save it as a QSO log, or when the transcript contains callsigns, frequencies, modes, signal reports, or other radio-related fields.

## How to use

1. Read the transcript provided by the user.
2. Extract as many of these fields as possible:
   - `datetime`: date and time of the QSO (ISO 8601 or user-provided)
   - `callsign_local`: your callsign
   - `callsign_remote`: the other station's callsign
   - `frequency`: in MHz or kHz
   - `mode`: e.g. FM, SSB, CW, AM, DMR
   - `rst_sent`: signal report sent (e.g. 59)
   - `rst_received`: signal report received
   - `name`: operator name if mentioned
   - `location`: location if mentioned
   - `notes`: any other useful information
3. Build a concise structured log entry in this format:

```
## QSO <datetime>
- Local:    <callsign_local>
- Remote:   <callsign_remote>
- Frequency:<frequency>
- Mode:     <mode>
- RST Sent: <rst_sent>
- RST Rcvd: <rst_received>
- Name:     <name>
- Location: <location>
- Notes:    <notes>
```

4. Use `get_current_time` if no datetime is provided.
5. Use `write_file` or `edit_file` to save the entry to `/data/agent/memory/radio/YYYY-MM-DD.md`. If the file exists, append the new entry. If it does not exist, create it with a `# Radio QSO Log` header.
6. Respond with a short confirmation including the extracted callsigns and frequency.

## Important

- If a field is unknown, write `unknown` or leave it blank; do not invent information.
- Keep notes short but useful.
- Always save the log to `/data/agent/memory/radio/` so the companion app and LVGL UI can read it.

## Example

User: "接收到业余电台通联语音，时间 2026-07-09 14:30:00，转写内容如下：\"BD2AAA 这里是 BD2BBB 在 438.500 FM 信号 59 收到 59\""

→ get_current_time
→ write_file path="/data/agent/memory/radio/2026-07-09.md" content="# Radio QSO Log\n\n## QSO 2026-07-09 14:30:00\n- Local:    BD2BBB\n- Remote:   BD2AAA\n- Frequency:438.500 MHz\n- Mode:     FM\n- RST Sent: 59\n- RST Rcvd: 59\n- Name:     unknown\n- Location: unknown\n- Notes:    Voice QSO via AI Radio Console\n"
→ "已保存通联日志：BD2BBB ↔ BD2AAA @ 438.500 MHz FM，信号 59/59。"
