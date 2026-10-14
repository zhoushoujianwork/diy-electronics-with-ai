// 汇总全部场景的 cues，合成 48kHz 立体声配乐 → out/score.wav
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { total, globalCues, energyAt } from '../src/timeline.js';
import { BEAT } from '../src/lib/fx.js';
import { makeBus, renderScore, renderCue, finalize, wavBuffer, SR } from './lib/synth.mjs';

const OUT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../out');
fs.mkdirSync(OUT, { recursive: true });

const bus = makeBus(total);
renderScore(bus, total, energyAt);
// 每拍一声底鼓，反拍加踩镲；画面的脉冲用同一个 BEAT
const cues = globalCues();
for (let b = 0; b * BEAT < total - 1.0; b++) {
  const t = b * BEAT;
  const e = energyAt(t);
  cues.push({ t, type: 'kick', gain: 0.55 + 0.45 * e });
  if (e >= 0.6) cues.push({ t: t + BEAT / 2, type: 'hat', seed: b + 1, gain: 0.5 + 0.4 * e });
}
for (const c of cues) renderCue(bus, c);
const { scale } = finalize(bus);

const file = path.join(OUT, 'score.wav');
fs.writeFileSync(file, wavBuffer(bus));
const byType = {};
for (const c of cues) byType[c.type] = (byType[c.type] || 0) + 1;
console.log(JSON.stringify({ file, seconds: total, sampleRate: SR, cues: cues.length, byType, scale: +scale.toFixed(3) }));
