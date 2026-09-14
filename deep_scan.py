"""
深度扫描飞书Wiki首页，发现所有可能的文档链接。
特别寻找：SPI屏驱动、触摸屏驱动、烧录指南等可能遗漏的文档。
"""
import asyncio
import json
import re
from playwright.async_api import async_playwright

WIKI_URL = "https://rivotek.feishu.cn/wiki/Onndw4lmniFBnEk0Rb7cDbwOnTc"

async def deep_scan():
    async with async_playwright() as p:
        browser = await p.chromium.launch(headless=True)
        context = await browser.new_context(
            viewport={'width': 1920, 'height': 1080},
            user_agent='Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
        )
        page = await context.new_page()

        print(f"[INFO] 访问首页: {WIKI_URL}")
        await page.goto(WIKI_URL, wait_until='networkidle', timeout=60000)
        await asyncio.sleep(3)

        # 滚动加载所有内容
        for i in range(10):
            await page.evaluate("""() => {
                const containers = ['.page-main', '.suite-body', '.bear-web-x-container', '.render-unit-wrapper'];
                containers.forEach(c => {
                    const el = document.querySelector(c);
                    if (el) el.scrollTop = el.scrollHeight;
                });
                window.scrollTo(0, document.body.scrollHeight);
            }""")
            await asyncio.sleep(1)

        # 提取所有可能的链接
        all_links = await page.evaluate("""() => {
            const results = new Set();
            // 1. 所有 <a> 标签的 href
            document.querySelectorAll('a[href]').forEach(a => {
                const href = a.href || '';
                if (href.includes('/wiki/') || href.includes('feishu.cn')) {
                    results.add(JSON.stringify({type: 'a', href: href, text: a.innerText?.trim()?.substring(0, 80)}));
                }
            });
            // 2. data-href 属性
            document.querySelectorAll('[data-href]').forEach(el => {
                const href = el.getAttribute('data-href') || '';
                if (href) results.add(JSON.stringify({type: 'data-href', href: href, text: el.innerText?.trim()?.substring(0, 80)}));
            });
            // 3. data-token 属性
            document.querySelectorAll('[data-token]').forEach(el => {
                const token = el.getAttribute('data-token') || '';
                if (token) results.add(JSON.stringify({type: 'data-token', token: token, text: el.innerText?.trim()?.substring(0, 80)}));
            });
            // 4. 侧边栏所有元素
            document.querySelectorAll('.wiki-sidebar, .workspace-tree-view, [class*="wiki"], [class*="tree"], [class*="sidebar"]').forEach(el => {
                el.querySelectorAll('a, [data-href], [data-token], [data-wiki-token]').forEach(sub => {
                    const href = sub.getAttribute('data-href') || sub.getAttribute('href') || '';
                    const token = sub.getAttribute('data-token') || sub.getAttribute('data-wiki-token') || '';
                    if (href || token) {
                        results.add(JSON.stringify({type: 'sidebar', href: href, token: token, text: sub.innerText?.trim()?.substring(0, 80), className: sub.className}));
                    }
                });
            });
            return Array.from(results).map(s => JSON.parse(s));
        }""")

        # 同时获取页面完整文本，搜索可能的关键词
        page_text = await page.evaluate("() => document.body.innerText")
        
        # 关键词搜索
        keywords = ['烧录', 'flash', '烧写', 'SPI屏', 'LCD', '显示屏', '触摸', 'touch', 'tp驱动', 
                    '屏幕', 'display', '屏驱动', '烧写工具', 'PhoenixSuit', 'LiveSuit',
                    'kernel', '内核', 'wifi', 'bluetooth', 'audio', 'lvgl', 'deepseek',
                    'agent', 'MCP', '工具', '传感器', 'sensor', 'uart', '串口', 'adb']
        found_keywords = {}
        for kw in keywords:
            if kw.lower() in page_text.lower():
                # 找到上下文
                idx = page_text.lower().find(kw.lower())
                context = page_text[max(0, idx-50):idx+100]
                found_keywords[kw] = context

        # 提取所有wiki token
        token_pattern = re.compile(r'/wiki/([A-Za-z0-9]{20,})')
        all_tokens = set()
        for link in all_links:
            href = link.get('href', '')
            token = link.get('token', '')
            if href:
                m = token_pattern.search(href)
                if m:
                    all_tokens.add(m.group(1))
            if token:
                all_tokens.add(token)

        # 从页面文本中也提取token
        for m in token_pattern.finditer(page_text):
            all_tokens.add(m.group(1))

        # 保存结果
        result = {
            'total_links': len(all_links),
            'total_unique_tokens': len(all_tokens),
            'all_links': all_links,
            'all_tokens': sorted(list(all_tokens)),
            'found_keywords': found_keywords,
            'page_text_length': len(page_text),
            'page_text_sample': page_text[:2000]
        }

        with open('/workspace/deep_scan_result.json', 'w', encoding='utf-8') as f:
            json.dump(result, f, ensure_ascii=False, indent=2)

        print(f"[OK] 发现 {len(all_links)} 个链接")
        print(f"[OK] 发现 {len(all_tokens)} 个唯一 wiki token")
        print(f"[OK] 页面文本长度: {len(page_text)}")
        print(f"[OK] 关键词命中: {list(found_keywords.keys())}")
        print(f"[OK] 结果保存到 /workspace/deep_scan_result.json")

        # 打印所有token
        print("\n[Token列表]")
        for t in sorted(all_tokens):
            print(f"  - {t}")

        await browser.close()

if __name__ == '__main__':
    asyncio.run(deep_scan())
