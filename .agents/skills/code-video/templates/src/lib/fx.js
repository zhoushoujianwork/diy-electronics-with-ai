import { hash } from './rand.js';
import { clamp, easeOutCubic } from './ease.js';
import { C, W, H } from './draw.js';

export const PAL = ['#19f0d6', '#ffb454', '#d97757', '#e6f0fa'];

// 鼓点包络：每个 period 起点为 1，指数衰减。120BPM 时 period=0.5
export function kickPulse(t, period = 0.5, k = 7) {
  const p = (((t % period) + period) % period);
  return Math.exp(-k * p);
}

// 沿折线画到 p（0..1），只构造路径不描边；返回终点
export function polyPath(ctx, pts, p = 1) {
  const segs = [];
  let total = 0;
  for (let i = 1; i < pts.length; i++) {
    const l = Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
    segs.push(l);
    total += l;
  }
  let rem = total * clamp(p);
  ctx.beginPath();
  ctx.moveTo(pts[0][0], pts[0][1]);
  let end = pts[0];
  for (let i = 1; i < pts.length && rem > 0; i++) {
    const l = segs[i - 1];
    const k = l === 0 ? 1 : Math.min(1, rem / l);
    end = [pts[i - 1][0] + (pts[i][0] - pts[i - 1][0]) * k, pts[i - 1][1] + (pts[i][1] - pts[i - 1][1]) * k];
    ctx.lineTo(end[0], end[1]);
    rem -= l;
  }
  return { end, total };
}

// 折线上 u（0..1）处的点
export function polyAt(pts, u) {
  const segs = [];
  let total = 0;
  for (let i = 1; i < pts.length; i++) {
    const l = Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
    segs.push(l);
    total += l;
  }
  let d = total * clamp(u);
  for (let i = 1; i < pts.length; i++) {
    const l = segs[i - 1];
    if (d <= l || i === pts.length - 1) {
      const k = l === 0 ? 0 : Math.min(1, d / l);
      return [pts[i - 1][0] + (pts[i][0] - pts[i - 1][0]) * k, pts[i - 1][1] + (pts[i][1] - pts[i - 1][1]) * k];
    }
    d -= l;
  }
  return pts[pts.length - 1];
}

// 方块碎屑爆开
export function burst(ctx, x, y, t, t0, o = {}) {
  const { n = 22, r = 260, life = 0.85, seed = 1, size = 14, colors = PAL } = o;
  const p = (t - t0) / life;
  if (p < 0 || p > 1) return;
  ctx.save();
  const ga = ctx.globalAlpha;
  for (let i = 0; i < n; i++) {
    const a = hash(seed * 131 + i * 3) * Math.PI * 2;
    const sp = r * (0.35 + 0.65 * hash(seed * 131 + i * 3 + 1));
    const d = sp * easeOutCubic(p);
    const s = size * (0.5 + hash(seed * 131 + i * 3 + 2)) * (1 - p);
    ctx.globalAlpha = ga * (1 - p * p);
    ctx.fillStyle = colors[i % colors.length];
    ctx.save();
    ctx.translate(x + Math.cos(a) * d, y + Math.sin(a) * d + 70 * p * p);
    ctx.rotate(a + p * 3);
    ctx.fillRect(-s / 2, -s / 2, s, s);
    ctx.restore();
  }
  ctx.restore();
}

// 扩散的光环
export function ring(ctx, x, y, t, t0, o = {}) {
  const { r = 220, life = 0.7, color = C.cyan, lw = 6 } = o;
  const p = (t - t0) / life;
  if (p < 0 || p > 1) return;
  ctx.save();
  ctx.globalAlpha *= 1 - p;
  ctx.strokeStyle = color;
  ctx.lineWidth = lw * (1 - p) + 1;
  ctx.shadowColor = color;
  ctx.shadowBlur = 18;
  ctx.beginPath();
  ctx.arc(x, y, r * easeOutCubic(p), 0, Math.PI * 2);
  ctx.stroke();
  ctx.restore();
}

// 从中心向 8 个方向射出成对的 PCB 走线（直行 + 45° 折弯 + 端点焊盘）
export function traceRays(ctx, cx, cy, t, t0, o = {}) {
  const { r0 = 160, len = 700, life = 0.9, seed = 1, lw = 5, color = C.cyan, alpha = 0.55, fade = 0 } = o;
  const p0 = (t - t0) / life;
  if (p0 < 0) return;
  const fa = fade ? 1 - clamp((t - t0 - life) / fade) : 1;
  if (fa <= 0) return;
  ctx.save();
  ctx.globalAlpha *= alpha * fa;
  ctx.strokeStyle = color;
  ctx.lineWidth = lw;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  ctx.shadowColor = color;
  ctx.shadowBlur = 14;
  for (let i = 0; i < 8; i++) {
    const a = (i * Math.PI) / 4;
    const dx = Math.cos(a);
    const dy = Math.sin(a);
    for (const s of [-1, 1]) {
      const k = i * 2 + (s > 0 ? 1 : 0);
      const L1 = len * (0.35 + 0.35 * hash(seed * 17 + k));
      const L2 = len * (0.2 + 0.3 * hash(seed * 17 + k + 40));
      const bend = (hash(seed * 17 + k + 80) < 0.5 ? -1 : 1) * (Math.PI / 4);
      const P0 = [cx + dx * r0 - dy * s * 20, cy + dy * r0 + dx * s * 20];
      const P1 = [P0[0] + dx * L1, P0[1] + dy * L1];
      const P2 = [P1[0] + Math.cos(a + bend) * L2, P1[1] + Math.sin(a + bend) * L2];
      polyPath(ctx, [P0, P1, P2], easeOutCubic(clamp(p0)));
      ctx.stroke();
      if (p0 >= 1) {
        ctx.beginPath();
        ctx.arc(P2[0], P2[1], lw * 1.5, 0, Math.PI * 2);
        ctx.stroke();
      }
    }
  }
  ctx.restore();
}

// 全屏飘落的方块彩屑（确定性）
export function confetti(ctx, t, t0, o = {}) {
  const { n = 56, seed = 5, colors = PAL } = o;
  if (t < t0) return;
  const tt = t - t0;
  ctx.save();
  const ga = ctx.globalAlpha;
  for (let i = 0; i < n; i++) {
    const speed = 140 + 260 * hash(seed * 91 + i * 5 + 1);
    const y0 = -hash(seed * 91 + i * 5 + 2) * H * 0.9 - 20;
    const y = y0 + tt * speed;
    if (y > H + 30) continue;
    const x = hash(seed * 91 + i * 5) * W + Math.sin(tt * 2 + i) * 28;
    const s = 8 + 12 * hash(seed * 91 + i * 5 + 3);
    ctx.globalAlpha = ga * 0.85 * clamp(tt * 2);
    ctx.fillStyle = colors[i % colors.length];
    ctx.save();
    ctx.translate(x, y);
    ctx.rotate(tt * (1 + 5 * hash(seed * 91 + i * 5 + 4)));
    ctx.fillRect(-s / 2, -s / 2, s, s);
    ctx.restore();
  }
  ctx.restore();
}

export const BEAT = 0.5; // 120 BPM，场景起点都落在拍子上

// 芯片：方形本体 + 四边引脚
export function chip(ctx, cx, cy, s, color = C.cyan, glow = 0, fill = C.panel2) {
  ctx.save();
  if (glow) {
    ctx.shadowColor = color;
    ctx.shadowBlur = glow;
  }
  ctx.strokeStyle = color;
  ctx.lineWidth = Math.max(2, s * 0.035);
  ctx.lineCap = 'round';
  const h = s / 2;
  const b = s * 0.34;
  for (let i = 0; i < 4; i++) {
    const o = -h * 0.6 + (i * h * 1.2) / 3;
    ctx.beginPath();
    ctx.moveTo(cx + o, cy - b); ctx.lineTo(cx + o, cy - h);
    ctx.moveTo(cx + o, cy + b); ctx.lineTo(cx + o, cy + h);
    ctx.moveTo(cx - b, cy + o); ctx.lineTo(cx - h, cy + o);
    ctx.moveTo(cx + b, cy + o); ctx.lineTo(cx + h, cy + o);
    ctx.stroke();
  }
  ctx.fillStyle = fill;
  ctx.beginPath();
  ctx.roundRect(cx - b, cy - b, b * 2, b * 2, s * 0.06);
  ctx.fill();
  ctx.stroke();
  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.arc(cx - b + s * 0.08, cy - b + s * 0.08, s * 0.025, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

// 弹出缩放：从 t0 起 d 秒，0→1 带回弹
export function pop(t, t0, d = 0.3) {
  const u = clamp((t - t0) / d);
  if (u <= 0) return 0;
  const c1 = 2.2;
  const c3 = c1 + 1;
  return 1 + c3 * Math.pow(u - 1, 3) + c1 * Math.pow(u - 1, 2);
}

import { bg } from './draw.js';

// 带鼓点的背景：基础背景 + 随底鼓脉动的光晕 + 底部均衡器条
export function bgBeat(ctx, t, o = {}) {
  const { bars = true, color = C.cyan } = o;
  bg(ctx, t);
  const k = kickPulse(t, BEAT);
  const g = ctx.createRadialGradient(W / 2, H / 2, 200, W / 2, H / 2, 1100);
  g.addColorStop(0, 'rgba(25,240,214,0)');
  g.addColorStop(1, `rgba(25,240,214,${0.07 * k})`);
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, W, H);
  if (!bars) return;
  ctx.save();
  ctx.fillStyle = color;
  const step = Math.floor(t * 8);
  for (let i = 0; i < 64; i++) {
    const h = 4 + 30 * hash(i * 7 + step * 131) * (0.35 + 0.65 * k);
    ctx.globalAlpha = 0.22;
    ctx.fillRect(100 + i * 27, H - 6 - h, 16, h);
  }
  ctx.restore();
}

import { text, measure } from './draw.js';

// 按时间表逐词弹出的居中标题。words: [{s, c?}]；times 为各词出现时刻
export function popWords(ctx, words, times, t, y, size, o = {}) {
  const { weight = '700', gap = 0.32, glow = 0 } = o;
  const ws = words.map((w) => measure(ctx, w.s, size, weight));
  const total = ws.reduce((a, b) => a + b, 0) + size * gap * (words.length - 1);
  let x = W / 2 - total / 2;
  words.forEach((w, i) => {
    const k = pop(t, times[i], 0.28);
    if (k > 0) {
      ctx.save();
      ctx.translate(x + ws[i] / 2, y);
      ctx.scale(k, k);
      text(ctx, w.s, 0, 0, { size, weight, color: w.c || C.text, align: 'center', glow: w.c ? glow : 0, glowColor: w.c });
      ctx.restore();
    }
    x += ws[i] + size * gap;
  });
}

// 左上角小标签：一个词，弹出
export function kicker(ctx, str, t, t0) {
  const k = pop(t, t0, 0.28);
  if (k <= 0) return;
  ctx.save();
  ctx.translate(110, 84);
  ctx.scale(k, k);
  text(ctx, str, 0, 0, { size: 46, weight: '700', color: C.cyan, glow: 14 });
  ctx.restore();
}
