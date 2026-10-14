// 最小场景示例：标题逐词弹出 + 宠物走入并随拍子跳。复制后改成自己的内容。
import { text, C, W } from '../lib/draw.js';
import { bgBeat, popWords } from '../lib/fx.js';
import { drawPet, petShadow, hopAt, autoEyes } from '../lib/pet.js';
import { prog, lerp, easeOutCubic } from '../lib/ease.js';

export const id = 'example';
export const dur = 4.25; // 整拍数 + FADE(0.25)，下一场景起点才落在拍子上
export const energy = 0.6;

const WORDS = [{ s: '词一', c: C.cyan }, { s: '词二' }];
const WT = [1.5, 2.0];

// cue 时间与 draw 里的事件时间用同一批常量
export const cues = [
  { t: 0.0, type: 'whoosh', gain: 0.8 },
  ...WT.map((t, i) => ({ t, type: 'pop', seed: i + 2, gain: 0.8 })),
  { t: 3.0, type: 'chime', gain: 0.7 },
];

export function draw(ctx, t) {
  bgBeat(ctx, t);
  popWords(ctx, WORDS, WT, t, 540, 92, { glow: 18 });
  const walking = t < 1.5;
  const x = lerp(-200, W / 2, easeOutCubic(prog(t, 0, 1.5)));
  const hop = walking ? { dy: 0, squash: 0 } : hopAt(t, 0.5, 70, 1.5);
  petShadow(ctx, x, 1000, 14, -hop.dy);
  drawPet(ctx, x, 1000 + hop.dy, 14, { walk: walking ? t * 16 : null, squash: hop.squash, eyes: autoEyes(t) });
  text(ctx, 'caption', W / 2, 60, { size: 30, fam: 'Menlo', color: C.dim, align: 'center', alpha: prog(t, 3.0, 0.3) });
}
