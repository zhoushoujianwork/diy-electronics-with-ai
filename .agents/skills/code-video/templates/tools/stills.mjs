// 单帧验收：node tools/stills.mjs <outDir> <sceneId|all|global> [t1,t2,...]
import fs from 'node:fs';
import path from 'node:path';
import { openPages, shot } from './lib/browser.mjs';

const [outDir, which = 'all', tsArg] = process.argv.slice(2);
fs.mkdirSync(outDir, { recursive: true });
const { pages, close } = await openPages(1);
const page = pages[0];
const info = await page.evaluate(async () => {
  const m = await import('/timeline.js');
  return { scenes: m.scenes.map((s) => ({ id: s.id, dur: s.dur })), starts: m.starts, total: m.total };
});
try {
  if (which === 'global') {
    const ts = tsArg.split(',').map(Number);
    for (const t of ts) {
      fs.writeFileSync(path.join(outDir, `g_${t.toFixed(2)}.png`), await shot(page, t, null));
    }
  } else {
    const list = which === 'all' ? info.scenes : info.scenes.filter((s) => s.id === which);
    for (const s of list) {
      const ts = tsArg
        ? tsArg.split(',').map(Number)
        : [0.4, 0.8, s.dur * 0.3, s.dur * 0.5, s.dur * 0.75, s.dur - 0.05];
      for (const t of ts) {
        fs.writeFileSync(path.join(outDir, `${s.id}_${t.toFixed(2)}.png`), await shot(page, t, s.id));
      }
    }
  }
  console.log(JSON.stringify(info));
} finally {
  await close();
}
