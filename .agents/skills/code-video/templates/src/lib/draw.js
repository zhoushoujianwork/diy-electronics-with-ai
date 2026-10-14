import { assets } from './assets.js';
import { visibleCount } from './type.js';
import { clamp, prog, easeOutCubic } from './ease.js';

export const W = 1920;
export const H = 1080;

// 配色：深蓝黑底、青绿高亮、冷白字。换项目时按其品牌色改这一处
export const C = {
  bg: '#04080f',
  panel: '#0a1420',
  panel2: '#0f1d2d',
  line: '#1c3248',
  text: '#e6f0fa',
  dim: '#8aa3b8',
  cyan: '#19f0d6',
  amber: '#ffb454',
};

export const SANS = '"PingFang SC","Hiragino Sans GB","Heiti SC",sans-serif';
export const MONO = 'Menlo,"SF Mono",monospace';
export const font = (px, weight = '400', fam = SANS) => `${weight} ${px}px ${fam}`;

// 铺满不透明背景：渐变光晕 + 点阵 + 缓慢下移的扫描光带
export function bg(ctx, t) {
  ctx.fillStyle = C.bg;
  ctx.fillRect(0, 0, W, H);
  const g = ctx.createRadialGradient(W * 0.5, H * 0.4, 80, W * 0.5, H * 0.5, 1250);
  g.addColorStop(0, 'rgba(25,240,214,0.075)');
  g.addColorStop(1, 'rgba(25,240,214,0)');
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, W, H);
  ctx.fillStyle = 'rgba(138,163,184,0.10)';
  for (let y = 40; y < H; y += 60) {
    for (let x = 40; x < W; x += 60) ctx.fillRect(x - 1, y - 1, 2, 2);
  }
  const sy = ((t * 70) % (H + 300)) - 150;
  const sg = ctx.createLinearGradient(0, sy - 120, 0, sy + 120);
  sg.addColorStop(0, 'rgba(25,240,214,0)');
  sg.addColorStop(0.5, 'rgba(25,240,214,0.035)');
  sg.addColorStop(1, 'rgba(25,240,214,0)');
  ctx.fillStyle = sg;
  ctx.fillRect(0, sy - 120, W, 240);
}

export function text(ctx, str, x, y, o = {}) {
  const {
    size = 32, weight = '400', fam = SANS, color = C.text,
    align = 'left', alpha = 1, glow = 0, glowColor = C.cyan, maxW = 0,
  } = o;
  ctx.save();
  ctx.globalAlpha *= clamp(alpha);
  let px = size;
  if (maxW) {
    const w = measure(ctx, str, size, weight, fam);
    if (w > maxW) px = size * (maxW / w);
  }
  ctx.font = font(px, weight, fam);
  ctx.textAlign = align;
  ctx.textBaseline = 'middle';
  ctx.fillStyle = color;
  if (glow) {
    ctx.shadowColor = glowColor;
    ctx.shadowBlur = glow;
  }
  ctx.fillText(str, x, y);
  ctx.restore();
}

export function measure(ctx, str, size, weight = '400', fam = SANS) {
  ctx.save();
  ctx.font = font(size, weight, fam);
  const w = ctx.measureText(str).width;
  ctx.restore();
  return w;
}

// 按打字时间表显示前 n 个字符，可选光标。center 对齐时先量全宽，避免抖动。
export function typedText(ctx, str, times, t, x, y, o = {}) {
  const n = visibleCount(times, t);
  const chars = Array.from(str);
  const sub = chars.slice(0, n).join('');
  const { size = 32, weight = '400', fam = SANS } = o;
  const x0 = o.align === 'center' ? x - measure(ctx, str, size, weight, fam) / 2 : x;
  if (n > 0) text(ctx, sub, x0, y, { ...o, align: 'left' });
  if (o.caret) {
    const until = o.caretUntil ?? times[times.length - 1] + 0.8;
    const on = t >= (o.caretFrom ?? times[0] - 0.06) && t < until;
    if (on && (n < chars.length || Math.floor(t * 2.2) % 2 === 0)) {
      const w = n > 0 ? measure(ctx, sub, size, weight, fam) : 0;
      ctx.fillStyle = C.cyan;
      ctx.fillRect(x0 + w + 4, y - size * 0.5, Math.max(3, size * 0.09), size);
    }
  }
}

// CJK 逐字、拉丁词整体的贪心换行
export function wrapText(ctx, str, maxW, size, weight = '400', fam = SANS) {
  const tokens = str.match(/[　-鿿＀-￯]|[^\s　-鿿＀-￯]+|\s+/g) || [];
  const noStart = '，。、；：！？）」》';
  const lines = [];
  let cur = '';
  for (const tok of tokens) {
    const trial = cur + tok;
    if (cur && measure(ctx, trial, size, weight, fam) > maxW && !noStart.includes(tok)) {
      lines.push(cur.trimEnd());
      cur = tok.trimStart();
    } else {
      cur = trial;
    }
  }
  if (cur) lines.push(cur.trimEnd());
  return lines;
}

export function card(ctx, x, y, w, h, o = {}) {
  const { r = 20, fill = C.panel, stroke = C.line, lw = 2, alpha = 1, glow = 0 } = o;
  ctx.save();
  ctx.globalAlpha *= clamp(alpha);
  ctx.beginPath();
  ctx.roundRect(x, y, w, h, r);
  if (glow) {
    ctx.shadowColor = C.cyan;
    ctx.shadowBlur = glow;
  }
  ctx.fillStyle = fill;
  ctx.fill();
  ctx.shadowBlur = 0;
  ctx.lineWidth = lw;
  ctx.strokeStyle = stroke;
  ctx.stroke();
  ctx.restore();
}

// 勾：p 为 0..1 的描边进度
export function check(ctx, cx, cy, s, color, p = 1) {
  if (p <= 0) return;
  const pts = [[-0.5, 0.05], [-0.14, 0.42], [0.52, -0.38]];
  const l1 = Math.hypot(pts[1][0] - pts[0][0], pts[1][1] - pts[0][1]);
  const l2 = Math.hypot(pts[2][0] - pts[1][0], pts[2][1] - pts[1][1]);
  let d = p * (l1 + l2);
  ctx.save();
  ctx.strokeStyle = color;
  ctx.lineWidth = Math.max(3, s * 0.14);
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  ctx.beginPath();
  ctx.moveTo(cx + pts[0][0] * s, cy + pts[0][1] * s);
  const d1 = Math.min(d, l1);
  ctx.lineTo(cx + (pts[0][0] + ((pts[1][0] - pts[0][0]) * d1) / l1) * s,
    cy + (pts[0][1] + ((pts[1][1] - pts[0][1]) * d1) / l1) * s);
  d -= d1;
  if (d > 0) {
    ctx.lineTo(cx + (pts[1][0] + ((pts[2][0] - pts[1][0]) * d) / l2) * s,
      cy + (pts[1][1] + ((pts[2][1] - pts[1][1]) * d) / l2) * s);
  }
  ctx.stroke();
  ctx.restore();
}

// logo：裁出图形主体并用径向蒙版淡出方形底，贴进任意背景。crop=[sx,sy,sw,sh] 按目标 logo 图量好再传
export function drawLogo(ctx, cx, cy, size, alpha = 1, crop = [0, 0, 0, 0]) {
  const img = assets.logo;
  const s = Math.round(size);
  if (!img || s < 4) return;
  const oc = new OffscreenCanvas(s, s);
  const o = oc.getContext('2d');
  const [sx, sy, sw, sh] = crop[2] ? crop : [0, 0, img.width, img.height];
  o.drawImage(img, sx, sy, sw, sh, 0, 0, s, s);
  o.globalCompositeOperation = 'destination-in';
  const g = o.createRadialGradient(s / 2, s / 2, s * 0.44, s / 2, s / 2, s * 0.5);
  g.addColorStop(0, 'rgba(0,0,0,1)');
  g.addColorStop(1, 'rgba(0,0,0,0)');
  o.fillStyle = g;
  o.fillRect(0, 0, s, s);
  ctx.save();
  ctx.globalAlpha *= clamp(alpha);
  ctx.drawImage(oc, cx - s / 2, cy - s / 2);
  ctx.restore();
}

export function dot(ctx, x, y, r, color, glow = 0) {
  ctx.save();
  if (glow) {
    ctx.shadowColor = color;
    ctx.shadowBlur = glow;
  }
  ctx.fillStyle = color;
  ctx.beginPath();
  ctx.arc(x, y, r, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

// 淡入上移的场景标题；s 为开始时间
export function heading(ctx, str, t, s, y, o = {}) {
  const e = easeOutCubic(prog(t, s, 0.6));
  text(ctx, str, W / 2, y + (1 - e) * 22, { size: 64, weight: '700', align: 'center', alpha: e, ...o });
}

// 通用淡入上移：返回 {a, dy}，a 为 0..1
export function rise(t, s, d = 0.5, dist = 20) {
  const a = easeOutCubic(prog(t, s, d));
  return { a, dy: (1 - a) * dist };
}
