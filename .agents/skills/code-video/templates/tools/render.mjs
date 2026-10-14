// 并发截图 → 按帧序写入 ffmpeg stdin → out/picture.mp4
// 用法：node tools/render.mjs [并发页数，默认 4]
import fs from 'node:fs';
import path from 'node:path';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { total, FPS } from '../src/timeline.js';
import { openPages, shot } from './lib/browser.mjs';

const OUT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../out');
fs.mkdirSync(OUT, { recursive: true });
const tmp = path.join(OUT, 'picture.partial.mp4');
const final = path.join(OUT, 'picture.mp4');
const N = Number(process.argv[2] || 4);
const frames = Math.round(total * FPS);

const ff = spawn('ffmpeg', [
  '-y', '-hide_banner', '-loglevel', 'error',
  '-f', 'image2pipe', '-framerate', String(FPS), '-c:v', 'png', '-i', '-',
  '-c:v', 'libx264', '-preset', 'medium', '-crf', '18', '-pix_fmt', 'yuv420p',
  '-movflags', '+faststart', '-r', String(FPS), tmp,
], { stdio: ['pipe', 'inherit', 'inherit'] });
const ffDone = new Promise((res, rej) => {
  ff.on('error', rej);
  ff.on('close', (code) => (code === 0 ? res() : rej(new Error(`ffmpeg 退出码 ${code}`))));
});

const { pages, close } = await openPages(N);
const t0 = Date.now();
try {
  for (let base = 0; base < frames; base += N) {
    const batch = await Promise.all(
      pages.map((p, k) => (base + k < frames ? shot(p, (base + k) / FPS) : null)),
    );
    for (const buf of batch) {
      if (!buf) continue;
      if (!ff.stdin.write(buf)) await new Promise((r) => ff.stdin.once('drain', r));
    }
    if ((base / N) % 40 === 0) {
      process.stderr.write(`\r帧 ${Math.min(base + N, frames)}/${frames}  ${((Date.now() - t0) / 1000).toFixed(0)}s`);
    }
  }
  ff.stdin.end();
  await ffDone;
} catch (e) {
  ff.kill('SIGKILL');
  fs.rmSync(tmp, { force: true });
  throw e;
} finally {
  await close();
}
fs.renameSync(tmp, final);
console.log(`\n${JSON.stringify({ file: final, frames, fps: FPS, seconds: +(frames / FPS).toFixed(2) })}`);
