#!/usr/bin/env python3
"""
飞书Wiki Playwright爬虫 - 使用无头浏览器渲染JavaScript并提取完整内容
"""

import json
import os
import re
import time
from playwright.sync_api import sync_playwright

BASE = 'https://rivotek.feishu.cn'
OUTPUT_DIR = '/workspace/feishu_docs_full'
TOKENS_FILE = '/workspace/wiki_tokens.json'

# 分类目录映射（根据已知URL推断）
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
    name = re.sub(r'[\\/:*?"<>|]', '_', name)
    name = name.strip()[:80]
    return name if name else 'untitled'

def extract_page_content(page):
    """从Playwright页面提取标题和正文内容"""
    # 提取标题
    title = None
    try:
        # 飞书wiki标题通常在特定元素中
        title_selectors = [
            '.wiki-title',
            '.docx-title',
            'h1.title',
            '.render-unit-wrapper h1',
            '[data-testid="doc-title"]',
            '.doc-title',
        ]
        for sel in title_selectors:
            elem = page.query_selector(sel)
            if elem:
                title = elem.inner_text().strip()
                if title:
                    break
    except:
        pass

    if not title:
        try:
            title = page.title()
        except:
            pass

    # 提取正文内容
    content = None
    try:
        # 方法1: 获取文档主体区域文本
        content_selectors = [
            '.docx-content',
            '.wiki-content',
            '.render-unit-wrapper',
            '.doc-body',
            'main',
            '.lark-editor',
            '#docx-container',
            '.docx-page-block',
        ]
        for sel in content_selectors:
            elem = page.query_selector(sel)
            if elem:
                text = elem.inner_text()
                if text and len(text) > 50:
                    content = text
                    break

        # 方法2: 获取整个页面文本
        if not content or len(content) < 50:
            content = page.inner_text('body')
    except Exception as e:
        print(f'    Content extraction error: {e}')

    return title, content

def extract_sidebar_links(page):
    """从侧边栏提取所有文档链接"""
    links = {}
    try:
        # 查找侧边栏中的所有链接
        sidebar_selectors = [
            '.wiki-sidebar a',
            '.catalog-sidebar a',
            '.tree-node a',
            '[class*="sidebar"] a',
            '[class*="catalog"] a',
            '[class*="tree"] a[href*="/wiki/"]',
        ]
        for sel in sidebar_selectors:
            elems = page.query_selector_all(sel)
            for elem in elems:
                href = elem.get_attribute('href')
                text = elem.inner_text().strip()
                if href and '/wiki/' in href:
                    token = href.split('/wiki/')[-1].split('?')[0]
                    if token and len(token) > 10:
                        links[token] = text
            if links:
                break
    except:
        pass
    return links

def expand_sidebar(page):
    """尝试展开侧边栏的所有节点"""
    try:
        # 点击展开按钮
        expand_buttons = page.query_selector_all('[class*="expand"]', )
        for btn in expand_buttons[:50]:
            try:
                btn.click()
                time.sleep(0.1)
            except:
                pass
    except:
        pass

    try:
        # 查找有子节点的树节点并展开
        tree_nodes = page.query_selector_all('[class*="tree-node"][class*="has-child"]')
        for node in tree_nodes[:50]:
            try:
                node.click()
                time.sleep(0.1)
            except:
                pass
    except:
        pass

def crawl_page(page, token):
    """爬取单个wiki页面"""
    url = f'{BASE}/wiki/{token}'
    print(f'  Loading: {url}')

    try:
        page.goto(url, wait_until='networkidle', timeout=30000)
    except Exception as e:
        print(f'    Navigation timeout/error: {e}')
        try:
            page.goto(url, wait_until='domcontentloaded', timeout=20000)
        except:
            print(f'    Failed to load page')
            return None, None, {}

    # 等待内容加载
    time.sleep(2)

    # 尝试展开侧边栏
    expand_sidebar(page)
    time.sleep(1)

    # 提取侧边栏链接
    sidebar_links = extract_sidebar_links(page)

    # 提取标题和内容
    title, content = extract_page_content(page)

    return title, content, sidebar_links

def save_doc(token, title, content, idx, all_sidebar_links):
    """保存文档"""
    # 确定分类目录
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

    # 加载已知的token列表
    with open(TOKENS_FILE, 'r') as f:
        tokens = json.load(f)

    print(f'共 {len(tokens)} 个文档待爬取')

    # 所有发现的侧边栏链接
    all_sidebar_links = {}
    # 已爬取的token
    crawled = set()
    # 文档索引
    index = []

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        context = browser.new_context(
            viewport={'width': 1920, 'height': 1080},
            user_agent='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
            locale='zh-CN',
        )
        page = context.new_page()

        # 先访问首页，展开侧边栏，获取完整目录树
        print('\n=== 第一步：获取首页侧边栏目录树 ===')
        title, content, sidebar_links = crawl_page(page, tokens[0])
        all_sidebar_links.update(sidebar_links)
        print(f'  首页标题: {title}')
        print(f'  侧边栏链接: {len(sidebar_links)}')

        # 再次尝试展开侧边栏
        for _ in range(3):
            expand_sidebar(page)
            time.sleep(1)
            new_links = extract_sidebar_links(page)
            if len(new_links) > len(sidebar_links):
                sidebar_links = new_links
                all_sidebar_links.update(sidebar_links)
                print(f'  展开后侧边栏链接: {len(sidebar_links)}')
            else:
                break

        # 保存首页
        crawled.add(tokens[0])
        filepath = save_doc(tokens[0], title, content, 1, all_sidebar_links)
        index.append({
            'idx': 1,
            'token': tokens[0],
            'title': title or tokens[0],
            'url': f'{BASE}/wiki/{tokens[0]}',
            'file': filepath,
        })

        # 爬取剩余页面
        print('\n=== 第二步：逐个爬取文档 ===')
        for i, token in enumerate(tokens[1:], 2):
            if token in crawled:
                continue

            print(f'\n[{i}/{len(tokens)}] Token: {token}')

            title, content, sidebar_links = crawl_page(page, token)
            all_sidebar_links.update(sidebar_links)

            crawled.add(token)
            filepath = save_doc(token, title, content, i, all_sidebar_links)
            index.append({
                'idx': i,
                'token': token,
                'title': title or f'doc_{token}',
                'url': f'{BASE}/wiki/{token}',
                'file': filepath,
                'content_length': len(content) if content else 0,
            })

            print(f'  标题: {title}')
            print(f'  内容长度: {len(content) if content else 0}')

            # 定期保存索引
            if i % 10 == 0:
                save_index(index, all_sidebar_links)
                print(f'  [已保存索引，进度: {i}/{len(tokens)}]')

            # 礼貌延时
            time.sleep(0.5)

        browser.close()

    # 保存最终索引
    save_index(index, all_sidebar_links)

    # 打印摘要
    print(f'\n{"="*60}')
    print(f'爬取完成！')
    print(f'总文档数: {len(index)}')
    print(f'侧边栏发现链接: {len(all_sidebar_links)}')
    print(f'{"="*60}')

    # 打印有内容的文档
    for item in index:
        cl = item.get('content_length', 0)
        status = f'[{cl} chars]' if cl > 0 else '[空]'
        print(f'  {item["idx"]:03d}. {item["title"]} {status}')

def save_index(index, sidebar_links):
    """保存索引文件"""
    ensure_dir(OUTPUT_DIR)
    with open(os.path.join(OUTPUT_DIR, '_index.json'), 'w', encoding='utf-8') as f:
        json.dump({
            'documents': index,
            'sidebar_links': sidebar_links,
        }, f, ensure_ascii=False, indent=2)

if __name__ == '__main__':
    main()
