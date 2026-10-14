import { mulberry32 } from '../../src/lib/rand.js';

export const SR = 48000;

// ---- 基础件 ----
const TAU = Math.PI * 2;
const midi = (n) => 440 * 2 ** ((n - 69) / 12);

// 双二阶滤波（RBJ），返回逐样本处理函数
function biquad(type, f0, q = 0.707) {
  const w = (TAU * f0) / SR;
  const al = Math.sin(w) / (2 * q);
  const c = Math.cos(w);
  let b0, b1, b2;
  if (type === 'lp') { b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; }
  else if (type === 'hp') { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; }
  else { b0 = al; b1 = 0; b2 = -al; } // bp
  const a0 = 1 + al;
  const a1 = (-2 * c) / a0;
  const a2 = (1 - al) / a0;
  b0 /= a0; b1 /= a0; b2 /= a0;
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  return (x) => {
    const y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    return y;
  };
}

export function makeBus(seconds) {
  const n = Math.ceil(seconds * SR);
  return { n, L: new Float32Array(n), R: new Float32Array(n) };
}

// 把单声道片段混入总线：pan ∈ [-1,1]，等功率声像
function mix(bus, t0, mono, gain, pan = 0) {
  const i0 = Math.round(t0 * SR);
  const gl = gain * Math.cos(((pan + 1) * Math.PI) / 4);
  const gr = gain * Math.sin(((pan + 1) * Math.PI) / 4);
  for (let i = 0; i < mono.length; i++) {
    const k = i0 + i;
    if (k < 0 || k >= bus.n) continue;
    bus.L[k] += mono[i] * gl;
    bus.R[k] += mono[i] * gr;
  }
}

const env = (i, atk, dec) => {
  const t = i / SR;
  return (t < atk ? t / atk : 1) * Math.exp(-t / dec);
};

// ---- cue 音效：全部由振荡器和滤波噪声合成 ----
const VOICES = {
  // 噪声扫频：转场
  whoosh(seed) {
    const rnd = mulberry32(seed + 7);
    const len = Math.round(0.9 * SR);
    const out = new Float32Array(len);
    for (let i = 0; i < len; i++) out[i] = rnd() * 2 - 1;
    // 分块改变中心频率，块内连续
    const B = 256;
    for (let s = 0; s < len; s += B) {
      const u = s / len;
      const f = biquad('bp', 300 + 3200 * Math.sin(Math.PI * u * 0.9) ** 2, 1.2);
      for (let i = s; i < Math.min(len, s + B); i++) out[i] = f(out[i]);
    }
    for (let i = 0; i < len; i++) {
      const u = i / len;
      out[i] *= Math.sin(Math.PI * u) ** 2 * 2.2;
    }
    return out;
  },
  // 低频重击：开场与结尾
  hit(seed) {
    const rnd = mulberry32(seed + 11);
    const len = Math.round(1.4 * SR);
    const out = new Float32Array(len);
    let ph = 0;
    const nf = biquad('lp', 900);
    for (let i = 0; i < len; i++) {
      const t = i / SR;
      const f = 46 + 90 * Math.exp(-t * 14);
      ph += (TAU * f) / SR;
      out[i] = (Math.sin(ph) * Math.exp(-t * 3.2) + nf(rnd() * 2 - 1) * Math.exp(-t * 38) * 0.5) * 0.95;
    }
    return out;
  },
  // 键盘：一小段高通噪声加短音
  click(seed) {
    const rnd = mulberry32(seed * 977 + 3);
    const len = Math.round(0.035 * SR);
    const out = new Float32Array(len);
    const hp = biquad('hp', 1800 + (seed % 5) * 220);
    const f = 1700 + (seed % 7) * 130;
    let ph = 0;
    for (let i = 0; i < len; i++) {
      ph += (TAU * f) / SR;
      out[i] = (hp(rnd() * 2 - 1) * 0.7 + Math.sin(ph) * 0.3) * Math.exp(-i / SR / 0.007);
    }
    return out;
  },
  tick(seed) {
    const len = Math.round(0.16 * SR);
    const out = new Float32Array(len);
    const f = 1480 + (seed % 4) * 110;
    for (let i = 0; i < len; i++) out[i] = Math.sin((TAU * f * i) / SR) * env(i, 0.002, 0.035);
    return out;
  },
  pop(seed) {
    const len = Math.round(0.22 * SR);
    const out = new Float32Array(len);
    const base = 420 + (seed % 5) * 60;
    let ph = 0;
    for (let i = 0; i < len; i++) {
      const t = i / SR;
      ph += (TAU * (base + 520 * (1 - Math.exp(-t * 40)))) / SR;
      out[i] = Math.sin(ph) * env(i, 0.003, 0.055);
    }
    return out;
  },
  // 两个上行音：校验通过
  ok() {
    const len = Math.round(0.5 * SR);
    const out = new Float32Array(len);
    const notes = [[0, midi(76)], [0.09, midi(83)]];
    for (const [d, f] of notes) {
      const o = Math.round(d * SR);
      for (let i = 0; i + o < len; i++) out[i + o] += Math.sin((TAU * f * i) / SR) * env(i, 0.004, 0.12) * 0.6;
    }
    return out;
  },
  // 底鼓：正弦下扫
  kick() {
    const len = Math.round(0.42 * SR);
    const out = new Float32Array(len);
    let ph = 0;
    for (let i = 0; i < len; i++) {
      const t = i / SR;
      ph += (TAU * (44 + 110 * Math.exp(-t * 28))) / SR;
      out[i] = Math.tanh(Math.sin(ph) * 1.6) * Math.exp(-t * 9) * (t < 0.002 ? t / 0.002 : 1);
    }
    return out;
  },
  // 踩镲：高通噪声
  hat(seed) {
    const rnd = mulberry32(seed * 53 + 9);
    const len = Math.round(0.07 * SR);
    const out = new Float32Array(len);
    const hp = biquad('hp', 7000);
    for (let i = 0; i < len; i++) out[i] = hp(rnd() * 2 - 1) * Math.exp(-i / SR / 0.016);
    return out;
  },
  // 拍手：带通噪声三连击
  clap(seed) {
    const rnd = mulberry32(seed * 71 + 5);
    const len = Math.round(0.24 * SR);
    const out = new Float32Array(len);
    const bp = biquad('bp', 1700, 0.9);
    for (let i = 0; i < len; i++) {
      const t = i / SR;
      const burst = [0, 0.012, 0.026].reduce((a, d) => a + (t >= d ? Math.exp(-(t - d) / 0.006) : 0), 0);
      out[i] = bp(rnd() * 2 - 1) * (burst * 0.5 + Math.exp(-t / 0.07) * 0.6);
    }
    return out;
  },
  // 上升扫频：转场前的蓄力
  riser(seed) {
    const rnd = mulberry32(seed + 23);
    const len = Math.round(1.0 * SR);
    const out = new Float32Array(len);
    const B = 256;
    for (let s0 = 0; s0 < len; s0 += B) {
      const u = s0 / len;
      const f = biquad('bp', 250 + 6500 * u * u, 2.2);
      for (let i = s0; i < Math.min(len, s0 + B); i++) out[i] = f(rnd() * 2 - 1);
    }
    for (let i = 0; i < len; i++) out[i] *= (i / len) ** 2 * 2.6;
    return out;
  },
  // 弹跳：音高快速上滑
  boing() {
    const len = Math.round(0.3 * SR);
    const out = new Float32Array(len);
    let ph = 0;
    for (let i = 0; i < len; i++) {
      const t = i / SR;
      ph += (TAU * (260 + 640 * Math.min(1, t / 0.12))) / SR;
      out[i] = Math.sin(ph) * env(i, 0.004, 0.09);
    }
    return out;
  },
  // 低沉落地
  thud() {
    const len = Math.round(0.18 * SR);
    const out = new Float32Array(len);
    let ph = 0;
    for (let i = 0; i < len; i++) {
      const t = i / SR;
      ph += (TAU * (60 + 70 * Math.exp(-t * 40))) / SR;
      out[i] = Math.sin(ph) * Math.exp(-t * 22);
    }
    return out;
  },
  // 钟声：非谐泛音
  chime() {
    const len = Math.round(2.4 * SR);
    const out = new Float32Array(len);
    const f0 = midi(81);
    const parts = [[1, 1], [2.76, 0.45], [5.4, 0.22], [8.93, 0.1]];
    for (const [r, a] of parts) {
      for (let i = 0; i < len; i++) out[i] += Math.sin((TAU * f0 * r * i) / SR) * a * env(i, 0.003, 0.9 / r ** 0.5);
    }
    return out.map((v) => v * 0.45);
  },
};

export function renderCue(bus, cue) {
  const voice = VOICES[cue.type];
  if (!voice) throw new Error(`未知 cue 类型：${cue.type}`);
  const seed = cue.seed ?? 1;
  const mono = voice(seed);
  // 声像由 seed 确定性决定，避免所有点击都在正中
  const pan = ((seed * 0.6180339887) % 1) * 1.0 - 0.5;
  const BASE = { click: 0.25, tick: 0.22, whoosh: 0.3, kick: 0.6, hat: 0.2, clap: 0.35, riser: 0.28, boing: 0.22, thud: 0.5 };
  const base = BASE[cue.type] ?? 0.3;
  mix(bus, cue.t, mono, base * (cue.gain ?? 1), ['hit', 'chime', 'kick', 'thud', 'clap', 'riser'].includes(cue.type) ? 0 : pan * 0.8);
}

// ---- 配乐：固定和弦循环，层次随 energy 增减 ----
// Am - F - C - G，每个和弦 4 秒
const CHORDS = [
  { root: 45, tones: [57, 60, 64] },
  { root: 41, tones: [57, 60, 65] },
  { root: 48, tones: [55, 60, 64] },
  { root: 43, tones: [55, 59, 62] },
];
const CHORD_DUR = 4;

export function renderScore(bus, total, energyAt) {
  const rnd = mulberry32(2026);
  const step = CHORD_DUR / 8; // 八分音符
  const nChord = Math.ceil(total / CHORD_DUR);

  for (let ci = 0; ci < nChord; ci++) {
    const ch = CHORDS[ci % CHORDS.length];
    const t0 = ci * CHORD_DUR;
    const e = energyAt(Math.min(total, t0 + CHORD_DUR / 2));

    // 垫音：始终存在，音量随 energy 小幅变化
    const padLen = Math.round((CHORD_DUR + 1.2) * SR);
    const pad = new Float32Array(padLen);
    const lp = biquad('lp', 900 + 1400 * e, 0.6);
    for (const n of ch.tones) {
      const f = midi(n);
      for (let i = 0; i < padLen; i++) {
        const t = i / SR;
        const a = Math.min(1, t / 1.0) * Math.min(1, (CHORD_DUR + 1.2 - t) / 1.2);
        const det = Math.sin((TAU * f * 1.004 * i) / SR);
        pad[i] += (Math.sin((TAU * f * i) / SR) + det) * 0.5 * a;
      }
    }
    for (let i = 0; i < padLen; i++) pad[i] = lp(pad[i]) * 0.1;
    mix(bus, t0 - 0.2, pad, 0.55 + 0.5 * e, 0);

    // 低音：energy ≥ 0.4 才出现，每拍一声，留空
    if (e >= 0.4) {
      for (let b = 0; b < 4; b += 2) {
        const len = Math.round(1.6 * SR);
        const out = new Float32Array(len);
        const f = midi(ch.root);
        for (let i = 0; i < len; i++) {
          const ph = (TAU * f * i) / SR;
          out[i] = (Math.sin(ph) + 0.25 * Math.sin(2 * ph)) * env(i, 0.01, 0.55);
        }
        mix(bus, t0 + b * (CHORD_DUR / 4), out, 0.16 * (e - 0.2) * 2, 0);
      }
    }

    // 琶音：energy ≥ 0.55 才出现，隔拍弹奏，左右错开
    if (e >= 0.55) {
      for (let k = 0; k < 8; k += 2) {
        const n = ch.tones[(k / 2) % 3] + 12;
        const f = midi(n);
        const len = Math.round(0.7 * SR);
        const out = new Float32Array(len);
        for (let i = 0; i < len; i++) out[i] = Math.sin((TAU * f * i) / SR) * env(i, 0.004, 0.16);
        mix(bus, t0 + k * step, out, 0.1 * (e - 0.3), ((k / 2) % 2 ? 0.5 : -0.5));
      }
    }

    // 轻微的空气噪声，让底层不那么干
    const airLen = Math.round(CHORD_DUR * SR);
    const air = new Float32Array(airLen);
    const alp = biquad('lp', 2500);
    for (let i = 0; i < airLen; i++) air[i] = alp(rnd() * 2 - 1) * 0.012;
    mix(bus, t0, air, 0.5 + 0.5 * e, 0);
  }
}

// 首尾淡入淡出，归一化到目标样本峰值
export function finalize(bus, { peak = 0.6, fadeIn = 0.4, fadeOut = 1.5 } = {}) {
  let mx = 0;
  for (let i = 0; i < bus.n; i++) mx = Math.max(mx, Math.abs(bus.L[i]), Math.abs(bus.R[i]));
  const g = mx > 0 ? peak / mx : 1;
  const fi = fadeIn * SR;
  const fo = fadeOut * SR;
  for (let i = 0; i < bus.n; i++) {
    const a = Math.min(1, i / fi, (bus.n - i) / fo);
    bus.L[i] *= g * a;
    bus.R[i] *= g * a;
  }
  return { scale: g };
}

// 16-bit PCM 立体声 WAV
export function wavBuffer(bus) {
  const data = Buffer.alloc(bus.n * 4);
  for (let i = 0; i < bus.n; i++) {
    data.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(bus.L[i] * 32767))), i * 4);
    data.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(bus.R[i] * 32767))), i * 4 + 2);
  }
  const h = Buffer.alloc(44);
  h.write('RIFF', 0); h.writeUInt32LE(36 + data.length, 4); h.write('WAVE', 8);
  h.write('fmt ', 12); h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20); h.writeUInt16LE(2, 22);
  h.writeUInt32LE(SR, 24); h.writeUInt32LE(SR * 4, 28); h.writeUInt16LE(4, 32); h.writeUInt16LE(16, 34);
  h.write('data', 36); h.writeUInt32LE(data.length, 40);
  return Buffer.concat([h, data]);
}
