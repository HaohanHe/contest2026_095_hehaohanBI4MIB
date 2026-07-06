#!/usr/bin/env python3
"""
飞书Wiki全量爬虫 - 完整爬取所有文档
从首页开始，递归发现并爬取所有wiki页面
"""

import requests
import re
import json
import os
import time
import hashlib
from urllib.parse import urljoin

BASE = 'https://rivotek.feishu.cn'
WIKI_HOME = 'Onndw4lmniFBnEk0Rb7cDbwOnTc'
OUTPUT_DIR = '/workspace/feishu_wiki'
HEADERS = {
    'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
    'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
    'Accept-Language': 'zh-CN,zh;q=0.9,en;q=0.8',
}

# 已知的额外wiki URL（从对话历史中获取）
KNOWN_URLS = [
    'MFjGw72yliVOBHkhR83csmsWn9b',  # 开发编译环境准备
    'DatswzFGAiPdbSkc5wGc5TVcnhf',  # DeepSeek LLM Demo
    'Zo0Zw1JYGiYi3ckpq1wcauyVnuh',  # 未知
    'Rjp8we1oviCrsgk9OkccFvOhnJh',  # 未知
]

# 飞书文档API（用于获取纯文本内容）
DOC_API = 'https://rivotek.feishu.cn/docx/api/docs/{token}/raw_text'


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def fetch_page(token):
    """获取wiki页面HTML"""
    url = f'{BASE}/wiki/{token}'
    try:
        r = requests.get(url, headers=HEADERS, timeout=20)
        r.raise_for_status()
        return r.text
    except Exception as e:
        print(f'  [ERROR] fetch_page {token}: {e}')
        return None


def fetch_doc_raw_text(token):
    """尝试通过API获取文档纯文本"""
    url = DOC_API.format(token=token)
    try:
        r = requests.get(url, headers=HEADERS, timeout=20)
        if r.status_code == 200:
            try:
                data = r.json()
                if 'data' in data and 'content' in data.get('data', {}):
                    return data['data']['content']
            except:
                pass
    except:
        pass
    return None


def extract_title(html):
    """从HTML中提取文档标题"""
    # 尝试多种模式提取标题
    patterns = [
        r'<title>([^<]+)</title>',
        r'"title"\s*:\s*"([^"]{2,200})"\s*[,}].*?wiki',
        r'class="wiki-title[^"]*"[^>]*>([^<]+)<',
        r'<h1[^>]*>([^<]+)</h1>',
    ]
    for pat in patterns:
        m = re.search(pat, html, re.IGNORECASE)
        if m:
            title = m.group(1).strip()
            if title and len(title) > 1:
                return title
    return None


def extract_content_text(html):
    """从HTML中提取文档正文文本"""
    # 尝试提取嵌入在script中的文档内容
    # 飞书通常将内容嵌入在window.__INITIAL_STATE__或类似变量中

    # 方法1: 查找 docx content
    content_match = re.search(r'"content"\s*:\s*"((?:[^"\\]|\\.)*)"', html)
    if content_match:
        text = content_match.group(1)
        # 反转义
        text = text.replace('\\n', '\n').replace('\\t', '\t').replace('\\"', '"').replace('\\\\', '\\')
        if len(text) > 100:
            return text

    # 方法2: 查找所有文本块
    text_blocks = re.findall(r'"text"\s*:\s*"((?:[^"\\]|\\.)*)"', html)
    if text_blocks:
        combined = '\n'.join(t.replace('\\n', '\n').replace('\\"', '"') for t in text_blocks if len(t) > 1)
        if len(combined) > 50:
            return combined

    return None


def extract_wiki_links(html):
    """从HTML中提取所有wiki链接token"""
    tokens = set()
    # 匹配 /wiki/xxx 格式
    for m in re.finditer(r'/wiki/([A-Za-z0-9]{20,50})', html):
        tokens.add(m.group(1))
    # 匹配 wikiToken": "xxx" 格式
    for m in re.finditer(r'"wikiToken"\s*:\s*"([A-Za-z0-9]+)"', html):
        tokens.add(m.group(1))
    # 匹配 objToken": "xxx" 格式
    for m in re.finditer(r'"objToken"\s*:\s*"([A-Za-z0-9]+)"', html):
        tokens.add(m.group(1))
    return tokens


def extract_doc_links(html):
    """从HTML中提取所有docx链接（飞书文档的另一种格式）"""
    tokens = set()
    for m in re.finditer(r'/docx/([A-Za-z0-9]+)', html):
        tokens.add(m.group(1))
    return tokens


def sanitize_filename(name):
    """清理文件名"""
    if not name:
        return 'untitled'
    # 替换特殊字符
    name = re.sub(r'[\\/:*?"<>|]', '_', name)
    name = name.strip()[:100]
    return name if name else 'untitled'


def save_doc(token, title, content, html, idx):
    """保存文档到文件"""
    ensure_dir(OUTPUT_DIR)

    safe_name = sanitize_filename(title) if title else f'doc_{token}'
    filename = f'{idx:03d}_{safe_name}.md'
    filepath = os.path.join(OUTPUT_DIR, filename)

    # 写入内容
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(f'# {title or "无标题"}\n\n')
        f.write(f'> Wiki Token: {token}\n')
        f.write(f'> URL: {BASE}/wiki/{token}\n\n')
        f.write('---\n\n')
        if content:
            f.write(content)
        else:
            f.write('（未能提取文本内容，请访问原始URL查看）\n')

    print(f'  Saved: {filename}')
    return filepath


def crawl():
    """主爬取函数"""
    ensure_dir(OUTPUT_DIR)

    # 待爬取队列
    to_visit = set()
    to_visit.add(WIKI_HOME)
    for url in KNOWN_URLS:
        to_visit.add(url)

    # 已爬取
    visited = set()
    # 文档索引
    index = []

    idx = 0
    total_found = len(to_visit)

    while to_visit:
        token = to_visit.pop()

        if token in visited:
            continue

        idx += 1
        print(f'\n[{idx}] Crawling: {token}')
        print(f'    URL: {BASE}/wiki/{token}')

        # 获取页面
        html = fetch_page(token)
        if not html:
            print(f'    Failed to fetch, skipping')
            continue

        visited.add(token)

        # 提取标题
        title = extract_title(html)
        print(f'    Title: {title}')

        # 提取内容
        content = extract_content_text(html)

        # 如果内容为空，尝试通过API获取
        if not content or len(content) < 50:
            # 尝试获取objToken对应的docx内容
            obj_tokens = re.findall(r'"objToken"\s*:\s*"([A-Za-z0-9]+)"', html)
            for ot in obj_tokens[:3]:
                raw = fetch_doc_raw_text(ot)
                if raw and len(raw) > 50:
                    content = raw
                    print(f'    Got raw text via API: {len(content)} chars')
                    break

        # 保存文档
        filepath = save_doc(token, title, content, html, idx)
        index.append({
            'idx': idx,
            'token': token,
            'title': title or f'doc_{token}',
            'url': f'{BASE}/wiki/{token}',
            'file': filepath,
            'content_length': len(content) if content else 0,
        })

        # 提取新链接
        new_links = extract_wiki_links(html)
        new_doc_links = extract_doc_links(html)

        new_tokens = new_links - visited - to_visit
        if new_tokens:
            print(f'    Found {len(new_tokens)} new wiki links')
            to_visit.update(new_tokens)
            total_found = len(visited) + len(to_visit)

        # 礼貌延时
        time.sleep(0.5)

    # 保存索引
    index_path = os.path.join(OUTPUT_DIR, '_index.json')
    with open(index_path, 'w', encoding='utf-8') as f:
        json.dump(index, f, ensure_ascii=False, indent=2)

    # 打印摘要
    print(f'\n{"="*60}')
    print(f'爬取完成！')
    print(f'总文档数: {len(index)}')
    print(f'已访问: {len(visited)}')
    print(f'索引文件: {index_path}')
    print(f'{"="*60}')

    # 打印文档列表
    for item in index:
        status = f'[{item["content_length"]} chars]' if item['content_length'] > 0 else '[空]'
        print(f'  {item["idx"]:03d}. {item["title"]} {status}')


if __name__ == '__main__':
    crawl()
