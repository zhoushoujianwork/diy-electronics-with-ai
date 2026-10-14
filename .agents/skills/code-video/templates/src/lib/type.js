import { hash } from './rand.js';

// 打字时间表：画面和 cues 共用同一份，保证每个可见字符对应一个 click。
// 返回每个字符出现的时间（秒，场景局部时钟），带确定性抖动。
export function typeSchedule(str, start, cps, seed = 1) {
  const times = [];
  let t = start;
  const chars = Array.from(str);
  for (let i = 0; i < chars.length; i++) {
    times.push(t);
    t += (1 / cps) * (0.65 + 0.7 * hash(i * 31 + seed));
  }
  return times;
}

// 多行依次打字：每行接在上一行结束之后，返回 [{str, times, start, end}]
export function typeLines(lines, start, cps, gap, seed = 1) {
  let t = start;
  return lines.map((str, li) => {
    const times = typeSchedule(str, t, cps, seed + li * 101);
    const end = times[times.length - 1] + 1 / cps;
    t = end + gap;
    return { str, times, start: times[0], end };
  });
}

// 在 t 时刻已显示的字符数
export function visibleCount(times, t) {
  let n = 0;
  while (n < times.length && times[n] <= t) n++;
  return n;
}

// 由时间表生成 click cues：空白字符不发声（不可见）
export function clickCues(str, times, gain = 1) {
  const chars = Array.from(str);
  const cues = [];
  for (let i = 0; i < chars.length; i++) {
    if (/\s/.test(chars[i])) continue;
    cues.push({ t: times[i], type: 'click', seed: i + 1, gain });
  }
  return cues;
}
