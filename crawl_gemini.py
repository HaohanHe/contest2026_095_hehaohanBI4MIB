#!/usr/bin/env python3
"""
Gemini-S1 Wiki精准爬虫 - 从已知文档开始递归发现并爬取所有相关文档
"""

import json
import os
import re
import time
from playwright.sync_api import sync_playwright
from urllib.parse import unquote

BASE = 'https://rivotek.feishu.cn'
OUTPUT_DIR = '/workspace/feishu_docs_full'

# 已知的Gemini-S1文档token和标题
INITIAL_TOKENS = {
    'Onndw4lmniFBnEk0Rb7cDbwOnTc': 'Gemini-S1开发板首页',
    'MFjGw72yliVOBHkhR83csmsWn9b': '开发编译环境准备',
    'Zo0Zw1JYGiYi3ckpq1wcauyVnuh': 'openvela整体分层结构',
    'NFZtwKOXMi3z3dkQZlecQaONnBc': 'Sensor驱动开发示例2_SHTC3',
    'Rjp8we1oviCrsgk9OkccFvOhnJh': '按键驱动开发',
    'BEuKwE7soiYqpgkfanLc80upnqb': '网络配置',
    'JupcwY42yiGBJHkr4v0cs453nje': '蓝牙开发概述',
    'ATxowcM8siHkAKkWvLRclI7RnYe': '音频驱动',
    'O7P3wW0IhivuEKkSvNXclR8enZc': 'led灯控制程序',
    'JtP2wBTl2iVketkcQ8ucFt4Znpd': '在openvela上构建lvgl页面程序',
    'DatswzFGAiPdbSkc5wGc5TVcnhf': 'DeepSeek LLM Demo实现设计与新手指南',
    'Ctk2wRJT3iMBPokAV4gcgkltn5c': '常见问题FAQ',
    'TOccwKc3Ni8BZqkPRZWcVPVjnMf': '系统日志分析',
}

# 分类映射
def get_category(title, token):
    if not title:
        return '99_其他'
    title = title.lower()
    if '首页' in title or 'gemini' in title.lower() or token == 'Onndw4lmniFBnEk0Rb7cDbwOnTc':
        return '00_首页'
    if '环境' in title or '编译' in title or '烧录' in title or '下载' in title or 'repo' in title.lower():
        return '01_入门指南'
    if '硬件' in title or '原理' in title or '接口' in title or 'pcb' in title.lower() or 'bom' in title.lower():
        return '02_硬件手册'
    if '驱动' in title or 'sensor' in title.lower() or 'lradc' in title.lower() or 'spi' in title.lower() or '触摸' in title or '按键' in title:
        return '03_驱动开发'
    if 'deepseek' in title.lower() or 'llm' in title.lower() or 'ai' in title.lower() or 'led' in title.lower() or 'lvgl' in title.lower() or '图形' in title or '页面' in title:
        return '04_场景应用'
    if '故障' in title or 'faq' in title.lower() or '日志' in title or '调试' in title or '排除' in title:
        return '05_故障排除'
    if 'nuttx' in title.lower() or '内核' in title or '架构' in title or '分层' in title:
        return '06_内核与架构'
    if '网络' in title or 'wifi' in title.lower() or '蓝牙' in title or 'bluetooth' in title.lower():
        return '03_驱动开发'
    if '音频' in title or 'audio' in title.lower():
        return '03_驱动开发'
    return '99_其他'

def ensure_dir(path):
    os.makedirs(path, exist_ok=True)

def sanitize_filename(name):
    if not name:
        return 'untitled'
    name = re.sub(r'[\\/:*?"<>|\n\r\t]', '_', name)
    name = name.strip()[:80]
    return name if name else 'untitled'

def scroll_and_load(page):
    """滚动页面加载所有懒加载内容"""
    prev_text_len = 0
    for i in range(30):
        page.evaluate('''() => {
            const containers = [
                document.querySelector('.page-main'),
                document.querySelector('.suite-body'),
                document.querySelector('.bear-web-x-container'),
                document.querySelector('.docx-in-wiki'),
            ];
            for (const c of containers) {
                if (c) c.scrollTop = c.scrollHeight;
            }
            window.scrollTo(0, document.body.scrollHeight);
        }''')
        time.sleep(0.3)
        curr_text = page.evaluate('''() => {
            const el = document.querySelector('.page-main') || document.body;
            return el ? el.innerText : '';
        }''')
        curr_len = len(curr_text)
        if curr_len == prev_text_len:
            break
        prev_text_len = curr_len
    return curr_len

def extract_content(page):
    """提取页面标题和正文内容"""
    title = None
    try:
        title = page.title()
        if title:
            title = re.sub(r'\s*-\s*飞书云文档\s*$', '', title)
    except:
        pass

    scroll_and_load(page)

    content = None
    try:
        elem = page.query_selector('.page-main')
        if elem:
            content = elem.inner_text()
    except:
        pass

    if not content or len(content) < 50:
        try:
            content = page.query_selector('.suite-body').inner_text()
        except:
            pass

    if not content or len(content) < 50:
        try:
            content = page.inner_text('body')
        except:
            pass

    # 清理内容
    if content:
        lines = content.split('\n')
        cleaned = []
        for line in lines:
            line = line.strip()
            if line in ['飞书云文档', '目录（暂无内容）', '与我分享', '登录/注册', '评论（0）', '复制']:
                continue
            if line.startswith('最新修改时间为'):
                continue
            if line == '代码块':
                continue
            if line in ['Bash', 'Plain Text', 'Python', 'C', 'C++', 'JSON', 'JavaScript', 'Shell']:
                continue
            cleaned.append(line)
        content = '\n'.join(cleaned)

    return title, content

def extract_wiki_links(page):
    """从文档内容中提取所有wiki链接"""
    links = {}
    try:
        link_data = page.eval_on_selector_all('.page-main a[href*="/wiki/"], .suite-body a[href*="/wiki/"]', '''els => els.map(el => ({
            href: el.href || '',
            text: (el.innerText || '').trim().substring(0, 100),
        }))''')
        for link in link_data:
            href = link['href']
            text = link['text']
            # 提取wiki token
            match = re.search(r'/wiki/([A-Za-z0-9]+)', href)
            if match:
                token = match.group(1)
                # 跳过锚点链接（指向同一页面的链接）
                if '#' not in href.split(token)[-1]:
                    if token not in links and text:
                        links[token] = text
    except:
        pass
    return links

def crawl_page(page, token):
    """爬取单个wiki页面"""
    url = f'{BASE}/wiki/{token}'
    try:
        page.goto(url, wait_until='domcontentloaded', timeout=30000)
    except:
        try:
            page.goto(url, wait_until='load', timeout=20000)
        except:
            return None, None, {}

    time.sleep(2)
    title, content = extract_content(page)
    links = extract_wiki_links(page)
    return title, content, links

def save_doc(token, title, content, idx):
    """保存文档"""
    category = get_category(title, token)
    if not title:
        title = f'doc_{token}'

    safe_title = sanitize_filename(title)
    dir_path = os.path.join(OUTPUT_DIR, category)
    ensure_dir(dir_path)

    filename = f'{idx:03d}_{safe_title}.md'
    filepath = os.path.join(dir_path, filename)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(f'# {title}\n\n')
        f.write(f'> Wiki Token: {token}\n')
        f.write(f'> URL: {BASE}/wiki/{token}\n')
        f.write(f'> 爬取时间: {time.strftime("%Y-%m-%d %H:%M:%S")}\n\n')
        f.write('---\n\n')
        if content:
            f.write(content)
        else:
            f.write('（内容提取失败，请访问原始URL查看）\n')

    return filepath

def main():
    ensure_dir(OUTPUT_DIR)

    # 初始待爬取队列
    to_visit = dict(INITIAL_TOKENS)
    visited = set()
    index = []
    all_discovered_links = {}

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True, args=['--no-sandbox', '--disable-dev-shm-usage'])
        context = browser.new_context(
            viewport={'width': 1920, 'height': 1080},
            user_agent='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
            locale='zh-CN',
        )
        page = context.new_page()

        idx = 0
        while to_visit:
            # 取下一个token
            token = next(iter(to_visit))
            expected_title = to_visit.pop(token)

            if token in visited:
                continue

            idx += 1
            print(f'\n[{idx}] {token} (预期: {expected_title})')

            try:
                title, content, links = crawl_page(page, token)
                cl = len(content) if content else 0
                print(f'  标题: {title}')
                print(f'  内容: {cl} chars')
                print(f'  发现链接: {len(links)}')

                visited.add(token)
                filepath = save_doc(token, title or expected_title, content, idx)
                index.append({
                    'idx': idx,
                    'token': token,
                    'title': title or expected_title,
                    'url': f'{BASE}/wiki/{token}',
                    'file': filepath,
                    'content_length': cl,
                    'links_found': list(links.keys()),
                })

                # 记录发现的链接
                for link_token, link_title in links.items():
                    all_discovered_links[link_token] = link_title
                    if link_token not in visited and link_token not in to_visit:
                        to_visit[link_token] = link_title
                        print(f'    新发现: {link_title} -> {link_token}')

                # 打印发现的链接
                for lt, ltitle in links.items():
                    print(f'    -> {ltitle} ({lt})')

            except Exception as e:
                print(f'  ERROR: {e}')
                visited.add(token)
                filepath = save_doc(token, expected_title, None, idx)
                index.append({
                    'idx': idx,
                    'token': token,
                    'title': expected_title,
                    'url': f'{BASE}/wiki/{token}',
                    'file': filepath,
                    'content_length': 0,
                    'error': str(e),
                })

            # 定期保存
            if idx % 5 == 0:
                save_index(index, all_discovered_links)
                print(f'  [进度已保存: {idx} docs]')

            time.sleep(0.3)

        browser.close()

    # 保存最终索引
    save_index(index, all_discovered_links)

    # 摘要
    total = len(index)
    success = sum(1 for x in index if x.get('content_length', 0) > 0)

    print(f'\n{"="*60}')
    print(f'爬取完成！')
    print(f'总文档数: {total}')
    print(f'成功: {success}')
    print(f'失败: {total - success}')
    print(f'发现的总链接数: {len(all_discovered_links)}')
    print(f'{"="*60}')

    for item in index:
        cl = item.get('content_length', 0)
        status = f'[{cl} chars]' if cl > 0 else '[空]'
        links = item.get('links_found', [])
        print(f'  {item["idx"]:03d}. {item["title"][:40]} {status} (links: {len(links)})')

def save_index(index, discovered_links):
    ensure_dir(OUTPUT_DIR)
    with open(os.path.join(OUTPUT_DIR, '_index.json'), 'w', encoding='utf-8') as f:
        json.dump({
            'documents': index,
            'all_discovered_links': discovered_links,
        }, f, ensure_ascii=False, indent=2)

if __name__ == '__main__':
    main()
