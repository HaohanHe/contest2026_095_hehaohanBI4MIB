# Deep Research

In-depth research on a topic with multiple sources.

## When to use
When user says research, investigate, deep dive, or learn about.

## How to use
1. web_search with the topic (broad query first)
2. fetch_url on the most relevant 2-3 results
3. web_search with refined/specific queries based on findings
4. Compile findings into a structured summary
5. Optionally: write_file to save the research report
6. Present key findings with source attribution

## Example
User: "帮我研究一下RISC-V在嵌入式领域的最新进展"
→ web_search "RISC-V IoT applications 2026"
→ fetch_url top 2 results
→ web_search "RISC-V vs ARM IoT comparison"
→ "调研结果：
   1. RISC-V 在低功耗 IoT 芯片中份额增长至 15%
   2. 主要优势：开源指令集、低授权成本
   3. 挑战：生态系统成熟度不如 ARM
   来源：[url1], [url2]"
