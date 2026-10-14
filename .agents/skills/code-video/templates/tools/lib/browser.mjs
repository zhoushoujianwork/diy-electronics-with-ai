import { chromium } from 'playwright-core';
import { startServer } from './server.mjs';

// 系统 Chrome 路径可用环境变量 CHROME_PATH 覆盖（Linux/Windows 必设）
const CHROME = process.env.CHROME_PATH || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';

export async function openPages(n) {
  const { server, url } = await startServer();
  const browser = await chromium.launch({ executablePath: CHROME, headless: true });
  const pages = [];
  for (let i = 0; i < n; i++) {
    const page = await browser.newPage({ viewport: { width: 1920, height: 1080 }, deviceScaleFactor: 1 });
    // 页面报错 = 渲染失败，不输出空白帧
    page.on('pageerror', (e) => { throw new Error(`页面错误：${e.message}`); });
    page.on('console', (m) => { if (m.type() === 'error') throw new Error(`控制台错误：${m.text()}`); });
    await page.goto(url);
    await page.waitForFunction(() => window.__ready === true || window.__errors?.length, null, { timeout: 30000 });
    const errs = await page.evaluate(() => window.__errors);
    if (errs.length) throw new Error(`页面初始化失败：${errs.join('; ')}`);
    pages.push(page);
  }
  const close = async () => { await browser.close(); server.close(); };
  return { pages, close };
}

export async function shot(page, t, sceneId) {
  await page.evaluate(([tt, id]) => window.drawAt(tt, id), [t, sceneId ?? null]);
  return page.locator('#c').screenshot({ type: 'png' });
}
