// 合成音画并用 ffprobe 校验：必须同时有视频流和音频流，时长差 ≤ 0.25s
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const OUT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../out');
const picture = path.join(OUT, 'picture.mp4');
const score = path.join(OUT, 'score.wav');
const final = path.join(OUT, 'promo.mp4');
for (const f of [picture, score]) if (!fs.existsSync(f)) throw new Error(`缺少 ${f}`);

execFileSync('ffmpeg', [
  '-y', '-hide_banner', '-loglevel', 'error',
  '-i', picture, '-i', score,
  '-map', '0:v:0', '-map', '1:a:0',
  '-c:v', 'copy', '-c:a', 'aac', '-b:a', '192k', '-ar', '48000',
  '-movflags', '+faststart', final,
], { stdio: 'inherit' });

const probe = JSON.parse(execFileSync('ffprobe', [
  '-v', 'error', '-show_entries', 'stream=codec_type,codec_name,duration,width,height,sample_rate,channels',
  '-show_entries', 'format=duration', '-of', 'json', final,
]).toString());
const v = probe.streams.find((s) => s.codec_type === 'video');
const a = probe.streams.find((s) => s.codec_type === 'audio');
if (!v || !a) throw new Error('成片缺少视频流或音频流');
const dv = Number(v.duration);
const da = Number(a.duration);
const diff = Math.abs(dv - da);
console.log(JSON.stringify({ file: final, video: v, audio: a, formatDuration: Number(probe.format.duration), diff: +diff.toFixed(3) }, null, 2));
if (diff > 0.25) throw new Error(`音画时长差 ${diff.toFixed(3)}s 超过 0.25s`);
