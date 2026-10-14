import * as example from './scenes/s1-example.js';

// 场景顺序；相邻场景交叉淡入 FADE 秒；dur = 整拍数 + FADE，使每个场景起点落在 120BPM 的拍子上，全局时钟由此推出。
export const scenes = [example];
export const FADE = 0.25;
export const FPS = 30;

export const starts = [];
let acc = 0;
for (const s of scenes) {
  starts.push(acc);
  acc += s.dur - FADE;
}
export const total = starts[starts.length - 1] + scenes[scenes.length - 1].dur;

// 全部 cue 的全局时间（音频与画面同源）
export function globalCues() {
  const out = [];
  scenes.forEach((s, i) => {
    for (const c of s.cues) out.push({ ...c, t: starts[i] + c.t, scene: s.id });
  });
  return out.sort((a, b) => a.t - b.t);
}

// 全局时刻的配乐强度：按交叉淡入线性插值
export function energyAt(t) {
  let e = scenes[0].energy;
  for (let i = 0; i < scenes.length; i++) {
    const s0 = starts[i];
    if (t < s0) break;
    const u = Math.min(1, Math.max(0, (t - s0) / FADE));
    e = i === 0 ? scenes[0].energy : e + (scenes[i].energy - e) * u;
  }
  return e;
}

// 在全局时刻 t 画一帧：先画仍在显示的前一场景，再叠上带淡入的当前场景
export function drawFrame(ctx, t) {
  let i = 0;
  while (i + 1 < scenes.length && t >= starts[i + 1]) i++;
  const s = scenes[i];
  const local = t - starts[i];
  if (i > 0 && local < FADE) {
    const prev = scenes[i - 1];
    prev.draw(ctx, Math.min(prev.dur, t - starts[i - 1]));
    ctx.save();
    ctx.globalAlpha = local / FADE;
    s.draw(ctx, local);
    ctx.restore();
  } else {
    s.draw(ctx, Math.min(s.dur, local));
  }
}

// 单场景预览：与全局时钟无关
export function drawScene(ctx, id, t) {
  const s = scenes.find((x) => x.id === id);
  if (!s) throw new Error(`未知场景 ${id}`);
  s.draw(ctx, t);
}
