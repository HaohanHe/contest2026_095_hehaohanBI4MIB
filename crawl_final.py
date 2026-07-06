#!/usr/bin/env python3
"""
飞书Wiki全量爬虫 - Playwright版
正确处理懒加载，滚动加载全部内容
"""

import json
import os
import re
import time
from playwright.sync_api import sync_playwright

BASE = 'https://rivotek.feishu.cn'
OUTPUT_DIR = '/workspace/feishu_docs_full'
TOKENS_FILE = '/workspace/wiki_tokens.json'

CATEGORY_MAP = {
    'Onndw4lmniFBnEk0Rb7cDbwOnTc': '00_首页',
    'MFjGw72yliVOBHkhR83csmsWn9b': '01_入门指南',
    'DatswzFGAiPdbSkc5wGc5TVcnhf': '04_场景应用',
    'Zo0Zw1JYGiYi3ckpq1wcauyVnuh': '01_入门指南',
    'Rjp8we1oviCrsgk9OkccFvOhnJh': '01_入门指南',
}

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
        # 尝试多种滚动方式
        page.evaluate('''() => {
            // 滚动文档容器
            const containers = [
                document.querySelector('.page-main'),
                document.querySelector('.suite-body'),
                document.querySelector('.app-main'),
                document.querySelector('.bear-web-x-container'),
                document.querySelector('.docx-in-wiki'),
            ];
            for (const c of containers) {
                if (c) {
                    c.scrollTop = c.scrollHeight;
                }
            }
            // 也滚动窗口
            window.scrollTo(0, document.body.scrollHeight);
        }''')
        time.sleep(0.3)

        # 检查内容是否增长
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
    # 提取标题
    title = None
    try:
        title = page.title()
        # 清理标题中的 " - 飞书云文档" 后缀
        if title:
            title = re.sub(r'\s*-\s*飞书云文档\s*$', '', title)
    except:
        pass

    # 滚动加载所有内容
    content_len = scroll_and_load(page)

    # 提取正文内容
    content = None
    try:
        # 优先从 page-main 提取
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

    # 清理内容 - 移除导航和UI元素
    if content:
        lines = content.split('\n')
        cleaned = []
        in_content = False
        for line in lines:
            line = line.strip()
            # 跳过明显的UI元素
            if line in ['飞书云文档', '目录（暂无内容）', '与我分享', '登录/注册', '评论（0）', '复制']:
                continue
            if line.startswith('最新修改时间为'):
                continue
            if line == '代码块':
                continue
            if line in ['Bash', 'Plain Text', 'Python', 'C', 'C++', 'JSON', 'JavaScript']:
                continue
            cleaned.append(line)
        content = '\n'.join(cleaned)

    return title, content

def crawl_page(page, token):
    """爬取单个wiki页面"""
    url = f'{BASE}/wiki/{token}'
    try:
        page.goto(url, wait_until='domcontentloaded', timeout=30000)
    except Exception as e:
        print(f'    Navigation error: {e}')
        try:
            page.goto(url, wait_until='load', timeout=20000)
        except:
            return None, None

    time.sleep(2)

    title, content = extract_content(page)
    return title, content

def save_doc(token, title, content, idx):
    """保存文档"""
    category = CATEGORY_MAP.get(token, '02_其他文档')
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

    with open(TOKENS_FILE, 'r') as f:
        tokens = json.load(f)

    # 去重
    tokens = list(dict.fromkeys(tokens))
    print(f'共 {len(tokens)} 个文档待爬取')

    index = []
    errors = []

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True, args=['--no-sandbox', '--disable-dev-shm-usage'])
        context = browser.new_context(
            viewport={'width': 1920, 'height': 1080},
            user_agent='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
            locale='zh-CN',
        )
        page = context.new_page()

        for i, token in enumerate(tokens, 1):
            print(f'\n[{i}/{len(tokens)}] {token}')

            try:
                title, content = crawl_page(page, token)
                cl = len(content) if content else 0
                print(f'  标题: {title}')
                print(f'  内容: {cl} chars')

                filepath = save_doc(token, title, content, i)
                index.append({
                    'idx': i,
                    'token': token,
                    'title': title or f'doc_{token}',
                    'url': f'{BASE}/wiki/{token}',
                    'file': filepath,
                    'content_length': cl,
                })
            except Exception as e:
                print(f'  ERROR: {e}')
                errors.append({'token': token, 'error': str(e)})
                # 保存空文件
                filepath = save_doc(token, f'ERROR_{token}', None, i)
                index.append({
                    'idx': i,
                    'token': token,
                    'title': f'ERROR_{token}',
                    'url': f'{BASE}/wiki/{token}',
                    'file': filepath,
                    'content_length': 0,
                    'error': str(e),
                })

            # 定期保存索引
            if i % 10 == 0:
                save_index(index)
                print(f'  [进度已保存: {i}/{len(tokens)}]')

            time.sleep(0.3)

        browser.close()

    # 保存最终索引
    save_index(index)

    # 摘要
    total = len(index)
    success = sum(1 for x in index if x.get('content_length', 0) > 0)
    failed = total - success

    print(f'\n{"="*60}')
    print(f'爬取完成！')
    print(f'总文档数: {total}')
    print(f'成功: {success}')
    print(f'失败: {failed}')
    if errors:
        print(f'错误列表:')
        for e in errors:
            print(f'  {e["token"]}: {e["error"][:80]}')
    print(f'{"="*60}')

    # 打印所有文档
    for item in index:
        cl = item.get('content_length', 0)
        status = f'[{cl} chars]' if cl > 0 else '[空]'
        print(f'  {item["idx"]:03d}. {item["title"][:40]} {status}')

def save_index(index):
    ensure_dir(OUTPUT_DIR)
    with open(os.path.join(OUTPUT_DIR, '_index.json'), 'w', encoding='utf-8') as f:
        json.dump(index, f, ensure_ascii=False, indent=2)

if __name__ == '__main__':
    main()
