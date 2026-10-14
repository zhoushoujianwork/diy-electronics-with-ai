// 缓动与进度：全部是纯函数。
export const clamp = (x, a = 0, b = 1) => Math.min(b, Math.max(a, x));
export const lerp = (a, b, u) => a + (b - a) * u;
// 从 s 开始、持续 d 秒的 0..1 进度
export const prog = (t, s, d) => clamp((t - s) / d);

export const easeOutCubic = (u) => 1 - Math.pow(1 - u, 3);
export const easeInOutCubic = (u) =>
  u < 0.5 ? 4 * u * u * u : 1 - Math.pow(-2 * u + 2, 3) / 2;
export const easeOutExpo = (u) => (u >= 1 ? 1 : 1 - Math.pow(2, -10 * u));
export const easeOutBack = (u) => {
  const c1 = 1.70158;
  const c3 = c1 + 1;
  return 1 + c3 * Math.pow(u - 1, 3) + c1 * Math.pow(u - 1, 2);
};
