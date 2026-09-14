# Doc Search

Search and read Feishu documents.

## When to use
When user asks to find a document, read a doc, or search files.

## How to use
1. feishu_doc_list to get recent documents
2. Match user query against document names
3. feishu_doc_read to get content of matching doc
4. Summarize or present the relevant content

## Important
- You MUST execute tools and respond with results. Do NOT output this template.
- If feishu_doc_list fails (Feishu not configured), tell the user:
  "Feishu is not configured. Run set_feishu_app <app_id> <app_secret> to set up."
- If no documents match the query, reply "No matching documents found."

## Example
User: "find the meeting notes on Feishu"
→ feishu_doc_list → [{title: "Meeting 3-28", id: "doc123"}, ...]
→ feishu_doc_read {"doc_id": "doc123"}
→ "Found Meeting 3-28, summary: ..."
