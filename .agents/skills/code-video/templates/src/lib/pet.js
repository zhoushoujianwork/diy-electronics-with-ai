import { clamp } from './ease.js';

// Claude 风格的像素小宠物：橙色方块身体、竖条眼睛、两只小手、四条短腿。
// 原点在脚底中心，1 个单位 = u 像素（身体 12×6）。
export const PET_COLOR = '#d97757';
const SHADE = '#b85d41';
const EYE = '#1a1210';

// o.squash: 正=压扁 负=拉长；o.walk: 走路相位（null=站立）；o.armL/armR: -0.3..1 抬手；
// o.eyes: open | blink | happy；o.look: -1..1 眼睛朝向；o.flip: -1 镜像
export function drawPet(ctx, x, y, u, o = {}) {
  const {
    squash = 0, lean = 0, walk = null, armL = 0, armR = 0,
    eyes = 'open', look = 0, alpha = 1, flip = 1, glow = 0,
  } = o;
  ctx.save();
  ctx.globalAlpha *= clamp(alpha);
  ctx.translate(x, y);
  ctx.rotate(lean);
  ctx.scale(u * flip * (1 + 0.22 * squash), u * (1 - 0.22 * squash));
  if (glow) {
    ctx.shadowColor = PET_COLOR;
    ctx.shadowBlur = glow;
  }

  // 腿
  ctx.fillStyle = SHADE;
  [-5, -2.6, 1.1, 3.5].forEach((lx, i) => {
    const lift = walk === null ? 0 : Math.max(0, Math.sin(walk + (i % 2) * Math.PI)) * 1.1;
    ctx.fillRect(lx, -2, 1.5, 2 - lift);
  });

  // 手（抬起时往上移）
  ctx.fillStyle = PET_COLOR;
  ctx.fillRect(-8, -6 - armL * 3.4, 2, 2.2);
  ctx.fillRect(6, -6 - armR * 3.4, 2, 2.2);

  // 身体
  ctx.fillRect(-6, -8, 12, 6);
  ctx.shadowBlur = 0;
  ctx.fillStyle = SHADE;
  ctx.fillRect(-6, -3, 12, 1);

  // 眼睛
  ctx.fillStyle = EYE;
  ctx.strokeStyle = EYE;
  ctx.lineWidth = 0.5;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  for (const ex of [-3.5, 2.3]) {
    const px = ex + look * 0.6;
    if (eyes === 'blink') ctx.fillRect(px, -5.6, 1.2, 0.4);
    else if (eyes === 'happy') {
      ctx.beginPath();
      ctx.moveTo(px - 0.3, -4.7);
      ctx.lineTo(px + 0.6, -6.1);
      ctx.lineTo(px + 1.5, -4.7);
      ctx.stroke();
    } else ctx.fillRect(px, -6.7, 1.2, 2.3);
  }
  ctx.restore();
}

// 地面阴影；lift 为离地高度（px），越高越小越淡
export function petShadow(ctx, x, y, u, lift = 0) {
  const k = clamp(lift / (u * 10));
  ctx.save();
  ctx.globalAlpha *= 0.4 * (1 - 0.6 * k);
  ctx.fillStyle = '#000';
  ctx.beginPath();
  ctx.ellipse(x, y + 3, u * 6.5 * (1 - 0.3 * k), u * 0.9 * (1 - 0.3 * k), 0, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

// 按拍子跳：返回 { dy, squash }。period 为一跳的时长，phase 为落地时刻
export function hopAt(t, period, h, phase = 0) {
  const p = (((t - phase) / period) % 1 + 1) % 1;
  const dy = -h * 4 * p * (1 - p);
  const squash = p < 0.15 ? 0.7 * (1 - p / 0.15) : -0.3 * Math.sin((Math.PI * (p - 0.15)) / 0.85);
  return { dy, squash };
}

// 每 3.1 秒眨一次眼
export const autoEyes = (t, mood = 'open') => (mood === 'open' && t % 3.1 < 0.12 ? 'blink' : mood);
