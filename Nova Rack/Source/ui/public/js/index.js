// Nova Rack UI: a horizontal chain of 500-series-style modules. Each Nova plugin gets a
// faceplate in its own look (colours, knob style, meter), and FULL SCREEN opens the real plugin.

const $ = (id) => document.getElementById(id);
const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

const CATEGORY_COLORS = {
  Dynamics: '#ff9836', EQ: '#3fd0c9', Color: '#ff6b4a', Space: '#ff9a3c',
  Pitch: '#ff6fb5', Master: '#ffd24a', Utility: '#3ddc84', Vocal: '#ff8f6b', Effect: '#7fb2ff',
};
const colorFor = (cat) => CATEGORY_COLORS[cat] || CATEGORY_COLORS.Effect;

//==============================================================================
// Faceplate skins, matched to each plugin's own UI.
const BRUSHED = 'repeating-linear-gradient(90deg, rgba(255,255,255,0.025) 0 1px, transparent 1px 3px)';
const SKINS = {
  neve:    { sub: 'Mic pre · EQ', panel: `${BRUSHED}, linear-gradient(180deg,#4d5d70,#3a4858)`, ink: '#f3e8cf', inkSoft: '#c9bb94', brand: '#f6ecd4', label: '#eaa64c',
             knob: 'skirt', fill: 'g-dark', heroFill: 'g-red', tick: '#f3e8cf', display: null },
  varimu:  { sub: 'Variable-mu tube limiter', panel: 'linear-gradient(180deg,#1b1812,#110f0b)', band: 'linear-gradient(180deg,#0b0a08,#16130e)', ink: '#e9dcc0', inkSoft: '#a8925f',
             brand: '#dcb86a', brandglow: '0 0 10px rgba(220,184,106,0.35)', label: '#cfae6a', knob: 'bakelite', fill: 'g-dark', heroFill: 'g-red', tick: '#cfae6a', display: 'tubes-vu' },
  opto:    { sub: 'Tube optical compressor', panel: `${BRUSHED}, linear-gradient(180deg,#2c5394,#1c3a6e)`, ink: '#ffffff', inkSoft: '#c9d6f0', brand: '#ffffff', label: '#eef3ff',
             knob: 'chicken', fill: 'g-gloss', heroFill: 'g-gloss', tick: '#ffffff', display: 'vu' },
  glow:    { sub: 'Leveling amplifier', panel: 'linear-gradient(180deg,#202020,#141414)', band: 'linear-gradient(180deg,#0d0d0d,#1a1a1a)', ink: '#e8e1d0', inkSoft: '#9a927f',
             brand: '#e7a94a', label: '#dcd4c2', knob: 'skirt', fill: 'g-gloss', heroFill: 'g-gloss', tick: '#dcd4c2', display: 'vu' },
  glue:    { sub: 'Bus compressor', panel: 'linear-gradient(180deg,#151515,#0c0c0c)', ink: '#ebe5d4', inkSoft: '#8f8875', brand: '#f2ead6', label: '#e5ddc8',
             knob: 'gloss', fill: 'g-gloss', heroFill: 'g-gloss', tick: '#8f8875', display: 'vu-amber' },
  tape:    { sub: 'Tape machine', panel: 'linear-gradient(180deg,#141518,#0b0c0e)', ink: '#d8d1c2', inkSoft: '#8a8475', brand: '#f0a64a', brandglow: '0 0 12px rgba(240,166,74,0.35)',
             label: '#c9c2b4', knob: 'gloss', fill: 'g-gloss', heroFill: 'g-gloss', tick: '#6f6a5e', display: 'vu' },
  desk:    { sub: 'Console EQ', panel: `${BRUSHED}, linear-gradient(180deg,#2e3035,#25272b)`, ink: '#eef0f3', inkSoft: '#9da3ab', brand: '#f2f3f5', label: '#d4d8de',
             knob: 'cap', fill: 'g-dark', heroFill: 'g-red', caps: ['g-red', 'g-green', 'g-green', 'g-blue', 'g-blue', 'g-cream', 'g-brown', 'g-cream'], tick: '#c7ccd3', display: null },
  console: { sub: 'Channel strip', panel: `${BRUSHED}, linear-gradient(180deg,#30333a,#26282d)`, ink: '#eef0f3', inkSoft: '#9da3ab', brand: '#f2f3f5', label: '#d4d8de',
             knob: 'cap', fill: 'g-dark', heroFill: 'g-brown', caps: ['g-brown', 'g-red', 'g-green', 'g-blue', 'g-cream', 'g-red', 'g-dark', 'g-dark'], tick: '#c7ccd3', display: 'vu' },
  tone:    { sub: 'Program equalizer', panel: 'linear-gradient(180deg,#121213,#0b0b0c)', ink: '#e6d7b4', inkSoft: '#9b8758', brand: '#d8b46a', label: '#c9a25a',
             knob: 'skirt', fill: 'g-gloss', heroFill: 'g-gloss', tick: '#c9a25a', display: null },
  heat:    { sub: 'Saturator', panel: 'linear-gradient(180deg,#3b1611,#210d0a)', ink: '#ffe3d6', inkSoft: '#c98c74', brand: '#ff7a45', brandglow: '0 0 12px rgba(255,122,69,0.4)',
             label: '#ffc2a6', knob: 'gloss', fill: 'g-gloss', heroFill: 'g-red', tick: '#ff9a72', display: 'vu-amber' },
  level:   { sub: 'Leveler', panel: 'linear-gradient(180deg,#181b20,#101317)', ink: '#e8ecf0', inkSoft: '#8a929c', brand: '#ff9836', label: '#d3d8de',
             knob: 'skirt', fill: 'g-dark', heroFill: 'g-dark', tick: '#9aa2ad', display: 'vu' },
  silk:    { sub: 'Spectral de-esser', panel: 'linear-gradient(180deg,#1d1a3d,#110f25)', ink: '#ecebff', inkSoft: '#8f88c4', brand: '#ffffff', label: '#c3bcf2',
             knob: 'ring', fill: 'g-navy', heroFill: 'g-navy', arc: '#a594ff', display: 'scope' },
  space:   { sub: 'Reverb', panel: 'linear-gradient(180deg,#152641,#0b1526)', ink: '#eaf1fb', inkSoft: '#7f93b0', brand: '#ffffff', label: '#a9bad3',
             knob: 'ring', fill: 'g-navy', heroFill: 'g-navy', arc: '#ff9a3c', display: 'scope' },
  pitch:   { sub: 'Pitch correction', panel: 'linear-gradient(180deg,#191c21,#101216)', ink: '#eceff3', inkSoft: '#8a929c', brand: '#ffffff', label: '#c8ced6',
             knob: 'ring', fill: 'g-navy', heroFill: 'g-navy', arc: '#ff6fb5', display: 'lcd' },
  mid:     { sub: 'Midrange weight', panel: 'radial-gradient(120% 70% at 50% 20%, rgba(18,31,72,0.45), transparent 65%), linear-gradient(180deg,#071021,#03080f)',
             ink: '#e9edf6', inkSoft: '#aeb7cb', brand: '#ffbd4a', brandglow: '0 0 12px rgba(255,170,56,0.35)', label: '#ffbd4a',
             knob: 'ring', fill: 'g-navy', heroFill: 'g-navy', arc: '#ffbd4a', tick: '#6f7a92', display: 'vu-amber' },
  modern:  { sub: '', panel: 'linear-gradient(180deg,#13263d,#0a1625)', ink: '#eaf1fb', inkSoft: '#7f93b0', brand: '#ffffff', label: '#a9bad3',
             knob: 'ring', fill: 'g-navy', heroFill: 'g-navy', arc: null, display: 'scope' },
};
// Real artwork from each plugin's own UI (Source/ui/public/skins). Knob images point up at 0°.
const U = (f) => `url("skins/${f}")`;
const SHADE = 'linear-gradient(180deg, rgba(255,255,255,0.06), rgba(0,0,0,0) 30%, rgba(0,0,0,0.25))';
Object.assign(SKINS.neve,    { panel: `${SHADE}, ${U('7even-panel.jpg')} center / 210px repeat`, hero: '7even-hero.png', knobs: ['7even-knob.png', '7even-knob2.png'] });
Object.assign(SKINS.varimu,  { panel: `linear-gradient(180deg, rgba(20,17,12,0.72), rgba(12,10,7,0.86)), ${U('670-panel.png')} 0 0 / 60px repeat, #15120d`, vuTint: 'sepia(0.45) brightness(0.88) contrast(1.05)', hero: '670-hero.png', knobs: ['670-knob.png'],
                               tubesImg: '670-tubes.png', vuBg: 'vu-face.jpg', vuScale: true });
Object.assign(SKINS.opto,    { panel: `${SHADE}, ${U('velvet-panel.png')} center / cover, #1d3a6e`, hero: 'velvet-hero.png', knobs: ['velvet-knob.png'],
                               vuBg: 'velvet-vu-face.png', vuFrame: 'velvet-vu-housing.png', vuScale: true });
Object.assign(SKINS.tape,    { hero: 'tape-hero.png', knobs: ['tape-knob.png'], vuPair: 'tape-vu.png' });
Object.assign(SKINS.glow,    { panel: `${SHADE}, ${U('glow-panel.png')} center / 300px repeat, #151515`, band: `${U('glow-cream.png')} center / 300px`,
                               ink: '#2a2418', inkSoft: '#6d6250', hero: 'glow-hero.png', knobs: ['glow-knob.png'], vuBg: 'glow-vu.png', vuScale: true, vuInset: '14% 12% 16%' });
Object.assign(SKINS.glue,    { panel: `${SHADE}, ${U('glue-panel.png')} center / cover, #111`, hero: 'glue-hero.png', knobs: ['glue-knob.png'], vuBg: 'glue-vu.png', vuScale: true, vuInset: '16% 10% 18%' });
Object.assign(SKINS.desk,    { hero: '7even-hero.png', knobs: ['desk-green.png', 'desk-green.png', 'desk-blue.png', 'desk-blue.png', 'desk-tan.png', 'desk-tan.png'] });
Object.assign(SKINS.console, { vuTint: 'sepia(0.3) brightness(0.92)', panel: `${SHADE}, ${U('console-panel.jpg')} center / cover, #26282d`, hero: '670-hero.png', knobs: ['desk-green.png', 'desk-blue.png', 'desk-tan.png', '670-knob.png'],
                               vuBg: 'vu-face.jpg', vuScale: true });
SKINS.punch = { sub: 'Transient designer', panel: `${SHADE}, ${U('punch-panel.png')} center / cover, #2a2d31`, ink: '#f1f2f4', inkSoft: '#9aa0a8', brand: '#ffffff',
                label: '#dfe2e6', knob: 'gloss', fill: 'g-gloss', heroFill: 'g-gloss', tick: '#c9ced6', hero: 'punch-hero.png', knobs: ['punch-knob.png'], display: null };

const PLUGIN_SKINS = {
  'Nova Punch': 'punch', 'Nova Mid': 'mid', 'Nova 2-Track': 'mid',
  'Nova 7even': 'neve', 'Nova Royal 670': 'varimu', 'Nova 670': 'varimu', 'Nova Velvet': 'opto', 'Nova Glow': 'glow',
  'Nova Glue': 'glue', 'Nova Tape': 'tape', 'Nova Desk EQ': 'desk', 'Nova Console': 'console', 'Nova Tone': 'tone',
  'Nova Heat': 'heat', 'Nova Level': 'level', 'Nova Silk': 'silk', 'Nova Space': 'space',
  'Nova Autotune': 'pitch', 'Nova Tune': 'pitch', 'Nova Pitch': 'pitch', 'Nova Harmony': 'pitch', 'Nova Bend': 'pitch', 'Nova Align': 'pitch',
};
const SUBTITLES = {
  'Nova Desk EQ': 'E / G console EQ', 'Nova Punch': 'Transient designer', 'Nova Clean': 'Vocal cleanup', 'Nova Delay': 'Analog delay',
  'Nova Doubler': 'Vocal doubler', 'Nova Aura': 'Harmonic exciter', 'Nova Autotune': 'Pitch correction', 'Nova Tune': 'Voice mutation',
  'Nova Master': 'Mastering', 'Nova Meter': 'Loudness meter', 'Nova Clip': 'Clipper', 'Nova Mid': 'Midrange weight', 'Nova 2-Track': 'Stereo mix surgeon', 'Nova Apex': 'Maximizer', 'Nova Vox': 'Vocal strip',
  'Nova Voice': 'Voice designer', 'Nova Ride': 'Vocal rider', 'Nova Motion FX': 'Motion effects', 'Nova Curve': 'Curve EQ',
};
function skinFor(slot) {
  const base = SKINS[PLUGIN_SKINS[slot.plugin]] || SKINS.modern;
  const s = { ...base };
  if (!s.arc) s.arc = colorFor(slot.category);
  s.sub = SUBTITLES[slot.plugin] || s.sub || slot.category || '';
  return s;
}

//==============================================================================
// Knobs (viewBox 100 x 100, -135° … +135°)
const A0 = -135, A1 = 135;
const polar = (r, deg) => { const t = (deg - 90) * Math.PI / 180; return [50 + r * Math.cos(t), 50 + r * Math.sin(t)]; };
const arcPath = (r, from, to) => {
  const [x0, y0] = polar(r, from), [x1, y1] = polar(r, to);
  return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${r} ${r} 0 ${to - from > 180 ? 1 : 0} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
};
function ticks(color, r0 = 44, r1 = 49, n = 11) {
  let s = '';
  for (let i = 0; i < n; ++i) {
    const a = A0 + (A1 - A0) * i / (n - 1);
    const [x0, y0] = polar(r0, a), [x1, y1] = polar(r1, a);
    s += `<line x1="${x0}" y1="${y0}" x2="${x1}" y2="${y1}" stroke="${color}" stroke-width="${i % 5 === 0 ? 2.2 : 1.4}" stroke-linecap="round" opacity="0.85"/>`;
  }
  return s;
}
function knobSVG(style, fill, sk) {
  const t = sk.tick || '#ccc';
  switch (style) {
    case 'skirt': return `${ticks(t)}
      <circle cx="50" cy="52" r="38" fill="rgba(0,0,0,0.45)"/>
      <g class="rot"><circle cx="50" cy="50" r="38" fill="url(#g-dark)" stroke="rgba(255,255,255,0.12)"/>
        <line x1="50" y1="14" x2="50" y2="24" stroke="${t}" stroke-width="3" stroke-linecap="round"/>
        <circle cx="50" cy="50" r="26" fill="url(#${fill})" stroke="rgba(0,0,0,0.6)"/>
        <line x1="50" y1="27" x2="50" y2="40" stroke="#f4f4f4" stroke-width="2.6" stroke-linecap="round" opacity="0.9"/></g>`;
    case 'chicken': return `${ticks(t)}
      <circle cx="50" cy="53" r="30" fill="rgba(0,0,0,0.4)"/>
      <g class="rot"><path d="M50 10 L61 44 A16 16 0 1 1 39 44 Z" fill="url(#${fill})" stroke="rgba(255,255,255,0.18)" stroke-width="1"/>
        <circle cx="50" cy="56" r="20" fill="url(#${fill})" stroke="rgba(255,255,255,0.1)"/>
        <line x1="50" y1="14" x2="50" y2="36" stroke="#f4f4f4" stroke-width="2.4" stroke-linecap="round"/></g>`;
    case 'bakelite': return `${ticks(t)}
      <circle cx="50" cy="53" r="38" fill="rgba(0,0,0,0.5)"/>
      <g class="rot"><circle cx="50" cy="50" r="38" fill="url(#${fill})" stroke="rgba(0,0,0,0.7)" stroke-width="3" stroke-dasharray="3 2.2"/>
        <circle cx="50" cy="50" r="27" fill="url(#${fill})" stroke="rgba(255,255,255,0.12)"/>
        <line x1="50" y1="16" x2="50" y2="34" stroke="#f6e9cf" stroke-width="3" stroke-linecap="round"/></g>`;
    case 'gloss': return `${ticks(t, 45, 49, 21)}
      <circle cx="50" cy="54" r="39" fill="rgba(0,0,0,0.55)"/>
      <g class="rot"><circle cx="50" cy="50" r="39" fill="url(#${fill})" stroke="rgba(255,255,255,0.14)"/>
        <line x1="50" y1="14" x2="50" y2="36" stroke="#f7f7f7" stroke-width="3" stroke-linecap="round"/></g>
      <ellipse cx="42" cy="32" rx="16" ry="9" fill="rgba(255,255,255,0.08)"/>`;
    case 'cap': return `${ticks(t)}
      <circle cx="50" cy="52" r="36" fill="rgba(0,0,0,0.45)"/>
      <g class="rot"><circle cx="50" cy="50" r="36" fill="url(#g-dark)" stroke="rgba(255,255,255,0.14)"/>
        <circle cx="50" cy="50" r="24" fill="url(#${fill})" stroke="rgba(0,0,0,0.5)"/>
        <line x1="50" y1="15" x2="50" y2="34" stroke="#ffffff" stroke-width="3" stroke-linecap="round"/></g>`;
    default: return `<path d="${arcPath(44, A0, A1)}" fill="none" stroke="rgba(0,0,0,0.55)" stroke-width="6" stroke-linecap="round"/>
      <path class="varc" d="" fill="none" stroke="${sk.arc}" stroke-width="6" stroke-linecap="round" style="filter: drop-shadow(0 0 3px ${sk.arc})"/>
      <circle cx="50" cy="50" r="32" fill="url(#${fill})" stroke="rgba(255,255,255,0.14)"/>
      <g class="rot"><circle cx="50" cy="26" r="3.6" fill="#ffffff"/></g>`;
  }
}

// A control on a faceplate. onChange(v, phase) with phase "begin" | "move" | "end".
function makeKnob({ label, value = 0, text = '', size = 46, style, fill, sk, steps = 0, def = 0.5, hero = false, onChange, onLabel }) {
  const root = el('div', 'ctl' + (hero ? ' hero' : ''));
  const img = sk.img;
  root.innerHTML = img
    ? `<div class="iknob" style="width:${size}px;height:${size}px"><svg width="${size}" height="${size}" viewBox="0 0 100 100">${ticks(sk.tick || '#ccc', 45, 49.5, hero ? 21 : 11)}</svg>
         <img class="rotimg" src="skins/${img}" draggable="false" alt=""></div><div class="lab">${esc(label)}</div><div class="val"></div>`
    : `<svg width="${size}" height="${size}" viewBox="0 0 100 100">${knobSVG(style, fill, sk)}</svg>
    <div class="lab">${esc(label)}</div><div class="val"></div>`;
  const rot = root.querySelector('.rot'), varc = root.querySelector('.varc'), valEl = root.querySelector('.val');
  const rotImg = root.querySelector('.rotimg');
  let v = value;
  const snap = (x) => (steps > 2 && steps <= 24) ? Math.round(x * (steps - 1)) / (steps - 1) : x;
  const draw = () => {
    const deg = A0 + (A1 - A0) * v;
    if (rotImg) rotImg.style.transform = `rotate(${deg.toFixed(1)}deg)`;
    else rot.setAttribute('transform', `rotate(${deg.toFixed(1)} 50 50)`);
    if (varc) varc.setAttribute('d', v > 0.003 ? arcPath(44, A0, deg) : '');
  };
  root.set = (nv, nt) => { v = nv; draw(); if (nt != null) valEl.textContent = tidy(nt); };
  root.set(value, text);

  const svg = root.querySelector('.iknob') || root.querySelector('svg');
  svg.addEventListener('pointerdown', (e) => {
    e.preventDefault(); e.stopPropagation();
    svg.setPointerCapture(e.pointerId);
    let lastY = e.clientY, raw = v;
    root.dragging = true;
    onChange?.(v, 'begin');
    const move = (ev) => {
      const dy = lastY - ev.clientY; lastY = ev.clientY;
      raw = Math.min(1, Math.max(0, raw + dy * (ev.shiftKey ? 0.0012 : 0.0055)));
      const nv = snap(raw);
      if (nv !== v) { v = nv; draw(); onChange?.(v, 'move'); }
    };
    const up = () => {
      svg.removeEventListener('pointermove', move); svg.removeEventListener('pointerup', up); svg.removeEventListener('pointercancel', up);
      root.dragging = false;
      onChange?.(v, 'end');
    };
    svg.addEventListener('pointermove', move); svg.addEventListener('pointerup', up); svg.addEventListener('pointercancel', up);
  });
  svg.addEventListener('dblclick', () => { v = def; draw(); onChange?.(v, 'begin'); onChange?.(v, 'end'); });
  if (onLabel) root.querySelector('.lab').addEventListener('click', onLabel);
  return root;
}

function makeToggle({ label, onChange, onLabel }) {
  const root = el('div', 'ctl');
  root.innerHTML = `<div class="tog"><i></i></div><div class="lab">${esc(label)}</div><div class="val"></div>`;
  const tog = root.querySelector('.tog'), valEl = root.querySelector('.val');
  root.set = (v, t) => { tog.classList.toggle('on', v > 0.5); if (t != null) valEl.textContent = tidy(t); };
  tog.addEventListener('click', () => { const nv = tog.classList.contains('on') ? 0 : 1; root.set(nv); onChange?.(nv, 'begin'); onChange?.(nv, 'end'); });
  if (onLabel) root.querySelector('.lab').addEventListener('click', onLabel);
  return root;
}

// Some plugins report raw floats ("3.2000017"): show at most two decimals, keep any unit.
function tidy(t) {
  const m = /^(-?\d+\.\d{3,})(.*)$/.exec(String(t).trim());
  if (!m) return t;
  const n = parseFloat(m[1]);
  return (Math.abs(n) >= 100 ? n.toFixed(0) : Math.abs(n) >= 10 ? n.toFixed(1) : n.toFixed(2)) + m[2];
}

//==============================================================================
// Meters on the faceplates
const vuPolar = (r, deg) => { const t = (deg - 90) * Math.PI / 180; return [(100 + r * Math.cos(t)).toFixed(2), (78 + r * Math.sin(t)).toFixed(2)]; };
const vuArc = (r, a, b) => { const [x0, y0] = vuPolar(r, a), [x1, y1] = vuPolar(r, b); return `M ${x0} ${y0} A ${r} ${r} 0 0 1 ${x1} ${y1}`; };
function vuSVG() {
  let s = '';
  for (let i = 0; i <= 10; ++i) {
    const a = -48 + i * 9.6, red = i >= 8;
    const [x0, y0] = vuPolar(52, a), [x1, y1] = vuPolar(i % 2 ? 57 : 60, a);
    s += `<line x1="${x0}" y1="${y0}" x2="${x1}" y2="${y1}" stroke="${red ? '#b3261e' : '#2a2418'}" stroke-width="${i % 2 ? 1 : 1.6}"/>`;
  }
  return `<svg viewBox="0 0 200 72" preserveAspectRatio="xMidYMid meet">
    <path d="${vuArc(56, -48, 29)}" fill="none" stroke="#2a2418" stroke-width="1.2"/>
    <path d="${vuArc(56, 29, 48)}" fill="none" stroke="#b3261e" stroke-width="3"/>
    ${s}
    <text x="100" y="52" text-anchor="middle" font-family="Georgia, serif" font-size="13" font-weight="700" fill="#2a2418">VU</text>
    <g class="needle"><line x1="100" y1="78" x2="100" y2="14" stroke="#1a1510" stroke-width="1.4"/></g>
    <circle cx="100" cy="78" r="9" fill="#1a1510"/>
  </svg>`;
}

function scopePath(level, phase, w = 200, h = 58) {
  const amp = Math.max(0.04, Math.min(1, (level + 60) / 60)) * (h * 0.38);
  let d = `M 0 ${h / 2}`;
  for (let x = 0; x <= w; x += 4) {
    const y = h / 2 + Math.sin(x * 0.07 + phase) * amp * (0.55 + 0.45 * Math.sin(x * 0.013 + phase * 0.4)) * Math.sin(Math.PI * x / w);
    d += ` L ${x} ${y.toFixed(1)}`;
  }
  return d;
}

//==============================================================================
let api = null;
let state = null;
let live = null;
let browserTab = 'plugins';
let insertAt = -1;
const dragging = new Set();
const vuState = [];

async function makeBackend() {
  if (window.__JUCE__) {
    const juce = await import('./juce/index.js');
    return new Proxy({}, { get: (_, name) => (name === 'then' ? undefined : juce.getNativeFunction(name)) });
  }
  const mock = await import('./mock.js');
  return mock.createMockBackend();
}

//==============================================================================
// Top bar I/O
let inKnob, outKnob;
const IO_SKIN = { tick: '#59616c', arc: '#ff9836' };
function buildIO() {
  const mk = (which, label) => {
    const wrap = el('div', 'io-k');
    const knob = makeKnob({
      label, value: 0.5, size: 30, style: 'ring', fill: 'g-dark', sk: IO_SKIN, def: 0.5,
      onChange: (v) => { const db = Math.round((v * 48 - 24) * 10) / 10; knob.set(v, fmtDb(db)); api.setGain(which, db); },
    });
    const meter = el('div', 'meter', '<i></i>');
    wrap.append(knob, meter);
    knob.meter = meter.querySelector('i');
    $('io').append(wrap);
    return knob;
  };
  inKnob = mk('in', 'IN');
  outKnob = mk('out', 'OUT');
}
const fmtDb = (db) => (db > 0 ? '+' : '') + db.toFixed(1);
const meterPct = (db) => Math.max(0, Math.min(100, (db + 60) / 60 * 100));

//==============================================================================
// The chain
function renderChain() {
  const chain = $('chain');
  const scroll = chain.scrollLeft;
  chain.innerHTML = '';
  vuState.length = 0;

  const ins = (index) => {
    const d = el('div', 'ins');
    d.dataset.index = index;
    if (state.slots.length < state.maxSlots) {
      const b = el('button', null, '+');
      b.title = 'Insert a plugin here';
      b.addEventListener('click', () => openBrowser('plugins', index));
      d.append(b);
    }
    return d;
  };

  chain.append(ins(0));
  state.slots.forEach((s, i) => { chain.append(renderModule(s, i)); chain.append(ins(i + 1)); });
  for (let i = state.slots.length; i < state.maxSlots && i < Math.max(5, state.slots.length + 1); ++i) {
    const b = el('div', 'blank', `<div class="plus">+</div><div>ADD MODULE</div>`);
    b.addEventListener('click', () => openBrowser('plugins', -1));
    chain.append(b);
    if (i < state.maxSlots - 1) chain.append(el('div', 'ins'));
  }

  chain.scrollLeft = scroll;
  $('unitCount').textContent = `${state.slots.length} / ${state.maxSlots} MODULES`;
  $('rackNameText').textContent = state.rack.name || 'New Rack';
  if (live) applyLive(live);
}

function renderModule(s, i) {
  const sk = skinFor(s);
  const m = el('div', 'mod' + (s.missing ? ' missing' : ''));
  m.dataset.index = i;

  const head = el('div', 'mhead');
  const led = el('button', 'led');
  led.title = 'On / bypass';
  led.addEventListener('click', (e) => { e.stopPropagation(); const b = !m.classList.contains('bypassed'); m.classList.toggle('bypassed', b); api.setBypass(i, b); });
  head.append(led, el('div', 't', esc(s.plugin.replace(/^Nova /i, '').toUpperCase())));
  if (!s.missing) {
    const fs = el('button', 'fs', 'FULL SCREEN');
    fs.title = `Open ${s.plugin} at full size`;
    fs.addEventListener('click', (e) => { e.stopPropagation(); api.openPlugin(i); });
    head.append(fs);
  }
  const x = el('button', 'x', '✕');
  x.title = 'Remove';
  x.addEventListener('click', async (e) => {
    e.stopPropagation();
    if (await askConfirm(`Remove ${s.plugin}?`, 'Its current settings are lost unless you saved them as a preset.', 'REMOVE')) setState(await api.removeSlot(i));
  });
  head.append(x);
  head.addEventListener('pointerdown', (e) => { if (!e.target.closest('button')) startModuleDrag(e, m, i); });
  m.append(head);

  const plate = el('div', 'plate');
  for (const p of ['tl', 'tr', 'bl', 'br']) plate.append(el('i', 'screw ' + p));
  m.append(plate);

  if (s.missing) {
    plate.append(el('div', 'missing-msg', s.loading
      ? `Finding your Nova plugins…<br><br><b>${esc(s.plugin)}</b> loads as soon as it's found.`
      : `<b>${esc(s.plugin)}</b> isn't installed on this computer.<br><br>Its settings stay in the rack and come back when it's installed.`));
    return m;
  }

  for (const [k, v] of Object.entries({ '--panel': sk.panel, '--ink': sk.ink, '--ink-soft': sk.inkSoft, '--brand': sk.brand, '--label': sk.label,
    '--band': sk.band || 'transparent', '--brandglow': sk.brandglow || 'none' })) plate.style.setProperty(k, v);

  const nm = /^Nova\s+(.*)$/i.exec(s.plugin);
  plate.append(el('div', 'badge' + (sk.band ? ' band' : ''),
    `<div class="n1">NOVA</div><div class="n2">${esc((nm ? nm[1] : s.plugin).toUpperCase())}</div>${sk.sub ? `<div class="n3">${esc(sk.sub)}</div>` : ''}`));

  // Display: VU, LCD or scope, as the plugin itself has.
  const disp = sk.display;
  if (disp === 'tubes-vu')
    plate.append(sk.tubesImg ? el('div', 'tubes-img', `<img src="skins/${sk.tubesImg}" alt="">`) : el('div', 'tubes', '<i></i><i></i><i></i><i></i>'));
  if (sk.vuPair) {
    const vu = el('div', 'vu-pair', `<img src="skins/${sk.vuPair}" alt=""><i class="pn l"></i><i class="pn r"></i>`);
    plate.append(vu);
    m.pairNeedles = [...vu.querySelectorAll('.pn')];
  } else if (disp === 'vu' || disp === 'vu-amber' || disp === 'tubes-vu') {
    const vu = el('div', 'vu' + (disp === 'vu-amber' ? ' amber' : '') + (sk.vuBg ? ' img' : ''), vuSVG());
    if (sk.vuBg) {
      vu.style.backgroundImage = `url("skins/${sk.vuBg}")`;
      if (sk.vuFrame) vu.insertAdjacentHTML('beforeend', `<img class="vu-frame" src="skins/${sk.vuFrame}" alt="">`);
      if (sk.vuInset) vu.querySelector('svg').style.inset = sk.vuInset;
      if (sk.vuTint) vu.style.filter = sk.vuTint;
    }
    plate.append(vu);
    m.needle = vu.querySelector('.needle');
  }
  if (disp === 'lcd') { m.lcd = el('div', 'lcd', '<div class="l1"></div><div class="l2"></div>'); plate.append(m.lcd); }
  if (disp === 'scope') {
    const sc = el('div', 'scope', `<svg viewBox="0 0 200 58" preserveAspectRatio="none"><path d="" fill="none" stroke="${sk.arc}" stroke-width="1.6" style="filter: drop-shadow(0 0 4px ${sk.arc})"/></svg>`);
    plate.append(sc);
    m.scope = sc.querySelector('path');
  }

  // Controls: one hero knob, then a two-column grid. Units with a meter show fewer knobs.
  const controls = el('div', 'controls');
  const maxCtl = disp === 'tubes-vu' ? 3 : disp ? 5 : 7;
  m.knobs = [];
  s.face.slice(0, maxCtl).forEach((f, pos) => {
    const key = `${i}:${pos}`;
    const onChange = (v, phase) => {
      if (phase === 'begin') dragging.add(key);
      if (phase === 'end') dragging.delete(key);
      api.setParam(i, f.idx, v, phase);
    };
    const onLabel = (e) => openParamPicker(e, i, pos, true);
    const hero = pos === 0;
    const fill = hero ? sk.heroFill : (sk.caps ? sk.caps[pos % sk.caps.length] : sk.fill);
    const k = (f.toggle || f.steps === 2) && !hero
      ? makeToggle({ label: f.name, onChange, onLabel })
      : makeKnob({ label: f.name, size: hero ? (disp ? 76 : 96) : 52, style: sk.knob, fill, steps: f.steps, def: f.def, hero, onChange, onLabel,
                   sk: { ...sk, img: hero ? sk.hero : sk.knobs ? sk.knobs[(pos - 1) % sk.knobs.length] : null } });
    m.knobs.push(k);
    controls.append(k);
  });
  if (!s.face.length) controls.append(el('div', 'missing-msg', 'Open the preset menu to add controls.'));
  plate.append(controls);

  // Footer: preset picker + mix
  const foot = el('div', 'pfoot');
  const pick = el('button', 'ppick', esc(s.preset || 'Default'));
  pick.title = 'Presets for this module';
  pick.addEventListener('click', (e) => openModuleMenu(e, s, i));
  foot.append(pick);
  m.mixKnob = makeKnob({
    label: 'MIX', value: 1, size: 24, style: 'ring', fill: 'g-dark', sk: { ...sk, arc: '#e8eef6', tick: '#888' }, def: 1,
    onChange: (v, phase) => {
      const key = `${i}:mix`;
      if (phase === 'begin') dragging.add(key);
      if (phase === 'end') dragging.delete(key);
      m.mixKnob.set(v);
      api.setMix(i, v);
    },
  });
  m.mixKnob.querySelector('.val').remove();
  foot.append(m.mixKnob);
  plate.append(foot);
  return m;
}

//==============================================================================
// Live updates (20 Hz from C++)
let phase = 0;
function applyLive(d) {
  live = d;
  if (!state) return;
  phase += 0.35;
  $('dirty').textContent = d.dirty ? '●' : '';
  $('latency').textContent = d.latency > 0 ? `LATENCY ${d.latency} SMP` : '';
  if (inKnob && !inKnob.dragging) inKnob.set((d.inGain + 24) / 48, fmtDb(d.inGain));
  if (outKnob && !outKnob.dragging) outKnob.set((d.outGain + 24) / 48, fmtDb(d.outGain));
  if (inKnob) inKnob.meter.style.height = meterPct(d.in) + '%';
  if (outKnob) outKnob.meter.style.height = meterPct(d.out) + '%';

  const mods = $('chain').querySelectorAll('.mod');
  $('bypassAll').classList.toggle('on', d.slots.length > 0 && d.slots.every((s) => s.b));

  d.slots.forEach((s, i) => {
    const m = mods[i];
    if (!m) return;
    m.classList.toggle('bypassed', !!s.b);
    (m.knobs || []).forEach((k, pos) => {
      const kv = s.k[pos];
      if (kv && !dragging.has(`${i}:${pos}`) && !k.dragging) k.set(kv[0], kv[1]);
    });
    if (m.mixKnob && !dragging.has(`${i}:mix`) && !m.mixKnob.dragging) m.mixKnob.set(s.m);

    // VU ballistics: quick rise, slow fall.
    const level = s.b ? -80 : (s.p ?? -80);
    const prev = vuState[i] ?? -40;
    const target = Math.max(-40, Math.min(6, level + 8));
    const v = target > prev ? prev + (target - prev) * 0.55 : prev + (target - prev) * 0.18;
    vuState[i] = v;
    const needleDeg = -48 + (v + 20) / 23 * 77;
    if (m.needle) m.needle.setAttribute('transform', `rotate(${needleDeg.toFixed(1)} 100 78)`);
    if (m.pairNeedles) m.pairNeedles.forEach((n, k) => { n.style.transform = `rotate(${(needleDeg * 0.82 + (k ? 1.5 : -1.5)).toFixed(1)}deg)`; });
    if (m.scope) m.scope.setAttribute('d', scopePath(level, phase + i));
    if (m.lcd) {
      const f0 = state.slots[i]?.face?.[0];
      m.lcd.querySelector('.l1').textContent = (f0?.name || 'NOVA').toUpperCase();
      m.lcd.querySelector('.l2').textContent = s.k[0] ? tidy(s.k[0][1]) : '—';
    }
  });

  const scan = $('scan');
  scan.classList.toggle('on', !!d.scan);
  if (d.scan) {
    $('scanBar').style.width = Math.round(d.scan.progress * 100) + '%';
    $('scanText').textContent = d.scan.current ? `SCANNING ${d.scan.current.toUpperCase()}` : 'SCANNING NOVA PLUGINS';
  }
}

//==============================================================================
// Reorder: drag a module by its header
function startModuleDrag(e, mod, from) {
  e.preventDefault();
  const startX = e.clientX;
  let moved = false, target = from;
  const inserts = [...$('chain').querySelectorAll('.ins')].slice(0, state.slots.length + 1);
  const clear = () => inserts.forEach((x) => x.classList.remove('drop'));
  const move = (ev) => {
    if (!moved && Math.abs(ev.clientX - startX) < 6) return;
    moved = true;
    mod.classList.add('dragging');
    clear();
    let best = 0, bestD = Infinity;
    inserts.forEach((x, k) => { const r = x.getBoundingClientRect(); const dd = Math.abs(ev.clientX - (r.left + r.width / 2)); if (dd < bestD) { bestD = dd; best = k; } });
    inserts[best].classList.add('drop');
    target = best > from ? best - 1 : best;
  };
  const up = async () => {
    window.removeEventListener('pointermove', move);
    window.removeEventListener('pointerup', up);
    mod.classList.remove('dragging');
    clear();
    if (moved && target !== from) setState(await api.moveSlot(from, target));
  };
  window.addEventListener('pointermove', move);
  window.addEventListener('pointerup', up);
}

//==============================================================================
// Menus (positioned in the unscaled 1240 x 720 space)
function openMenu(anchorEvent, build) {
  const m = $('menu');
  m.innerHTML = '';
  build(m);
  m.classList.add('open');
  const s = scaleNow, app = $('app').getBoundingClientRect();
  const r = (anchorEvent.currentTarget || anchorEvent.target).getBoundingClientRect();
  const left = (r.left - app.left) / s, top = (r.bottom - app.top) / s, topAbove = (r.top - app.top) / s;
  const w = m.offsetWidth, h = m.offsetHeight;
  m.style.left = Math.max(6, Math.min(1240 - w - 6, left)) + 'px';
  m.style.top = (top + h + 6 > 720 ? Math.max(6, topAbove - h - 4) : top + 4) + 'px';
}
function closeMenu() { $('menu').classList.remove('open'); }
const menuHead = (m, t) => m.append(el('div', 'mh', esc(t)));
const menuSep = (m) => m.append(el('div', 'ms'));
function menuItem(m, label, onClick, opts = {}) {
  const it = el('div', 'mi' + (opts.current ? ' cur' : ''), `<span>${esc(label)}</span>`);
  it.addEventListener('click', (e) => { e.stopPropagation(); closeMenu(); onClick(); });
  if (opts.onDelete) {
    const x = el('span', 'mx', '✕');
    x.addEventListener('click', (e) => { e.stopPropagation(); closeMenu(); opts.onDelete(); });
    it.append(x);
  }
  m.append(it);
}

function openModuleMenu(e, s, i) {
  e.stopPropagation();
  const anchor = { currentTarget: e.currentTarget };
  openMenu(anchor, (m) => {
    const mine = state.modules.filter((x) => x.plugin === s.plugin);
    menuHead(m, 'MY PRESETS');
    if (!mine.length) m.append(el('div', 'mi', '<span style="color:#7d8590">None saved yet</span>'));
    mine.forEach((p) => menuItem(m, p.name, async () => result(await api.loadModule(i, p.name)), {
      current: p.name === s.preset,
      onDelete: async () => { if (await askConfirm(`Delete "${p.name}"?`, 'This preset is removed from every rack.', 'DELETE')) setState(await api.deleteModule(s.plugin, p.name)); },
    }));
    if (s.programs?.length) {
      menuSep(m);
      menuHead(m, `${s.plugin.toUpperCase()} FACTORY`);
      s.programs.forEach((p, k) => menuItem(m, p, async () => result(await api.loadModule(i, '#' + k)), { current: p === s.preset }));
    }
    menuSep(m);
    menuItem(m, 'Save preset…', () => askName(`Save ${s.plugin} preset`, 'Saved presets show up for this plugin in every rack.',
      s.preset && !s.programs?.includes(s.preset) ? s.preset : '', (name) => api.saveModule(i, name)));
    if (s.face.length < 8) menuItem(m, 'Add a control…', () => openParamPicker(anchor, i, 99));
    menuItem(m, 'Reset controls to default', async () => setState(await api.setFace(i, -1, -1)));
  });
}

async function openParamPicker(e, slot, pos, canRemove = false) {
  e.stopPropagation?.();
  const anchor = { currentTarget: e.currentTarget };
  const params = await api.listParams(slot);
  openMenu(anchor, (m) => {
    if (canRemove) { menuItem(m, 'Remove this control', async () => setState(await api.setFace(slot, pos, -1))); menuSep(m); }
    menuHead(m, pos === 99 ? 'ADD A CONTROL' : 'SHOW ON THIS CONTROL');
    params.forEach((p) => menuItem(m, p.name, async () => setState(await api.setFace(slot, pos, p.idx))));
  });
}

function openRackMenu(e) {
  e.stopPropagation();
  openMenu(e, (m) => {
    menuItem(m, 'Save', () => saveRack());
    menuItem(m, 'Save as…', () => saveRackAs());
    menuItem(m, 'Browse rack presets…', () => openBrowser('racks'));
    menuSep(m);
    menuItem(m, 'New empty rack', async () => {
      if (!state.slots.length || await askConfirm('Start a new rack?', 'The current modules are removed.', 'NEW')) setState(await api.newRack());
    });
  });
}

async function saveRack() {
  if (state.rack.factory || !state.rack.name || state.rack.name === 'New Rack') return saveRackAs();
  result(await api.saveRack(state.rack.name));
  toast(`Saved "${state.rack.name}"`);
}
function saveRackAs() {
  askName('Save rack preset', 'Saves every module with its full settings. Rack presets are shared by every Nova Rack.',
    state.rack.factory ? state.rack.name + ' (mine)' : state.rack.name, (name) => api.saveRack(name));
}

//==============================================================================
// Browser
function openBrowser(tab, at = -1) {
  browserTab = tab || browserTab;
  insertAt = at;
  $('overlay').classList.add('open');
  $('search').value = '';
  $('insertNote').textContent = at >= 0 && at < state.slots.length ? `INSERTING AT SLOT ${at + 1}` : '';
  renderBrowser();
  setTimeout(() => $('search').focus(), 30);
}
function closeBrowser() { $('overlay').classList.remove('open'); }

function renderBrowser() {
  [...$('tabs').children].forEach((b) => b.classList.toggle('active', b.dataset.tab === browserTab));
  const body = $('browserBody');
  body.innerHTML = '';
  const q = $('search').value.trim().toLowerCase();
  const hit = (...xs) => !q || xs.some((x) => String(x || '').toLowerCase().includes(q));
  const installed = new Set(state.library.plugins.map((p) => p.name));
  const full = state.slots.length >= state.maxSlots;

  if (browserTab === 'plugins') {
    if (full) body.append(el('div', 'note', `The rack is full (${state.maxSlots} modules). Remove one to add another.`));
    if (!state.library.plugins.length)
      body.append(el('div', 'note', state.library.scanning ? 'Looking for your Nova plugins…' : 'No Nova plugins found in your VST3 folder. Install some, then press RESCAN.'));
    const groups = {};
    state.library.plugins.filter((p) => hit(p.name, p.category, SUBTITLES[p.name])).forEach((p) => (groups[p.category] ||= []).push(p));
    Object.keys(groups).sort().forEach((cat) => {
      body.append(el('div', 'gtitle', cat.toUpperCase()));
      const cards = el('div', 'cards');
      groups[cat].forEach((p) => {
        const sk = SKINS[PLUGIN_SKINS[p.name]];
        const c = el('div', 'card', `<div class="c1">${esc(p.name)}</div><div class="c2">${esc(SUBTITLES[p.name] || sk?.sub || p.category)}</div>`);
        c.style.setProperty('--c', sk && PLUGIN_SKINS[p.name] !== 'opto' ? sk.brand : colorFor(p.category));
        c.addEventListener('click', async () => { if (full) return; closeBrowser(); result(await api.addPlugin(p.name, insertAt)); });
        cards.append(c);
      });
      body.append(cards);
    });
  }

  if (browserTab === 'racks') {
    const groups = {};
    state.racks.filter((r) => hit(r.name, r.group, r.note, ...(r.chain || []))).forEach((r) => (groups[r.group || 'MY RACKS'] ||= []).push(r));
    const order = Object.keys(groups).sort((a, b) => (a === 'MY RACKS') - (b === 'MY RACKS') || a.localeCompare(b));
    if (!order.length) body.append(el('div', 'note', 'No rack presets match.'));
    order.forEach((g) => {
      body.append(el('div', 'gtitle', g));
      const cards = el('div', 'cards racks');
      groups[g].forEach((r) => {
        const chips = (r.chain || []).map((p) => `<span class="${installed.has(p) ? '' : 'missing'}">${esc(p.replace(/^Nova /, ''))}</span>`).join('');
        const c = el('div', 'card', `<div class="c1">${esc(r.name)}</div>${r.note ? `<div class="c2">${esc(r.note)}</div>` : ''}<div class="chips">${chips}</div>`);
        c.style.setProperty('--c', g.startsWith('ANALOG') ? '#dcb86a' : g.startsWith('VOCAL') ? '#ff9836' : g === 'MY RACKS' ? '#3ddc84' : '#3fd0c9');
        c.addEventListener('click', () => loadRack(r.name));
        if (!r.factory) {
          const del = el('span', 'del', '✕');
          del.addEventListener('click', async (e) => {
            e.stopPropagation();
            if (await askConfirm(`Delete "${r.name}"?`, 'This rack preset is removed for good.', 'DELETE')) { setState(await api.deleteRack(r.name)); renderBrowser(); }
          });
          c.append(del);
        }
        cards.append(c);
      });
      body.append(cards);
    });
  }

  if (browserTab === 'modules') {
    const groups = {};
    state.modules.filter((m) => hit(m.name, m.plugin)).forEach((m) => (groups[m.plugin] ||= []).push(m));
    if (!Object.keys(groups).length)
      body.append(el('div', 'note', 'Presets you save from a module\'s preset menu land here, for every Nova plugin in one place.<br>Click one to add that plugin to the rack already dialled in.'));
    Object.keys(groups).sort().forEach((plugin) => {
      body.append(el('div', 'gtitle', plugin.toUpperCase()));
      const cards = el('div', 'cards');
      const cat = state.library.plugins.find((p) => p.name === plugin)?.category;
      groups[plugin].forEach((mp) => {
        const c = el('div', 'card', `<div class="c1">${esc(mp.name)}</div><div class="c2">${esc(plugin)}${installed.has(plugin) ? '' : ' · not installed'}</div>`);
        c.style.setProperty('--c', colorFor(cat));
        c.addEventListener('click', async () => { if (full) return; closeBrowser(); result(await api.addModule(plugin, mp.name, insertAt)); });
        const del = el('span', 'del', '✕');
        del.addEventListener('click', async (e) => {
          e.stopPropagation();
          if (await askConfirm(`Delete "${mp.name}"?`, 'This preset is removed from every rack.', 'DELETE')) { setState(await api.deleteModule(plugin, mp.name)); renderBrowser(); }
        });
        c.append(del);
        cards.append(c);
      });
      body.append(cards);
    });
  }
}

async function loadRack(name) {
  if (live?.dirty && state.slots.length && !(await askConfirm('Load a new rack?', 'Your current rack has unsaved changes.', 'LOAD'))) return;
  closeBrowser();
  result(await api.loadRack(name));
}
function stepRack(dir) {
  const names = state.racks.map((r) => r.name);
  if (!names.length) return;
  const i = names.indexOf(state.rack.name);
  loadRack(names[(i + dir + names.length) % names.length]);
}

//==============================================================================
// Modals and toasts
function askName(title, text, initial, save) {
  const bg = $('modalBg'), input = $('modalInput');
  $('modalTitle').textContent = title;
  $('modalText').textContent = text;
  $('modalErr').textContent = '';
  $('modalOk').textContent = 'SAVE';
  input.style.display = '';
  input.value = initial || '';
  bg.classList.add('open');
  setTimeout(() => { input.focus(); input.select(); }, 30);
  const done = () => { bg.classList.remove('open'); $('modalOk').onclick = $('modalCancel').onclick = input.onkeydown = null; };
  const ok = async () => {
    const r = await save(input.value.trim());
    if (r?.error) { $('modalErr').textContent = r.error; return; }
    done();
    if (r?.state) setState(r.state);
  };
  $('modalOk').onclick = ok;
  $('modalCancel').onclick = done;
  input.onkeydown = (e) => { if (e.key === 'Enter') ok(); if (e.key === 'Escape') done(); };
}
function askConfirm(title, text, okLabel = 'OK') {
  return new Promise((resolve) => {
    const bg = $('modalBg');
    $('modalTitle').textContent = title;
    $('modalText').textContent = text;
    $('modalErr').textContent = '';
    $('modalInput').style.display = 'none';
    $('modalOk').textContent = okLabel;
    bg.classList.add('open');
    const finish = (v) => { bg.classList.remove('open'); $('modalInput').style.display = ''; $('modalOk').onclick = $('modalCancel').onclick = null; resolve(v); };
    $('modalOk').onclick = () => finish(true);
    $('modalCancel').onclick = () => finish(false);
  });
}
let toastTimer = 0;
function toast(msg) {
  const t = $('toast');
  t.textContent = msg;
  t.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove('show'), 4200);
}
function result(r) {
  if (!r) return;
  if (r.error && r.error !== 'missing') toast(r.error);
  if (r.state) setState(r.state);
}
function setState(s) {
  if (!s || !s.slots) return;
  state = s;
  renderChain();
  if ($('overlay').classList.contains('open')) renderBrowser();
  $('licenseBg').classList.toggle('open', !!(s.licenseRequired && !s.unlocked));
}

//==============================================================================
// Scale the 1240 x 720 design to the window.
let scaleNow = 1;
function fit() {
  scaleNow = Math.min(window.innerWidth / 1240, window.innerHeight / 720);
  $('app').style.transform = `scale(${scaleNow})`;
}

async function main() {
  fit();
  window.addEventListener('resize', fit);
  api = await makeBackend();
  buildIO();

  $('openBrowser').addEventListener('click', () => openBrowser('plugins', -1));
  $('rackName').addEventListener('click', () => openBrowser('racks'));
  $('prevRack').addEventListener('click', () => stepRack(-1));
  $('nextRack').addEventListener('click', () => stepRack(1));
  $('rackMenu').addEventListener('click', openRackMenu);
  $('bypassAll').addEventListener('click', () => {
    const all = !$('bypassAll').classList.contains('on');
    state.slots.forEach((_, i) => api.setBypass(i, all));
    $('bypassAll').classList.toggle('on', all);
  });

  $('tabs').addEventListener('click', (e) => { const t = e.target.dataset?.tab; if (t) { browserTab = t; renderBrowser(); } });
  $('search').addEventListener('input', renderBrowser);
  $('closeBrowser').addEventListener('click', closeBrowser);
  $('overlay').addEventListener('pointerdown', (e) => { if (e.target === $('overlay')) closeBrowser(); });
  $('rescan').addEventListener('click', async () => { setState(await api.rescan()); renderBrowser(); });
  document.addEventListener('pointerdown', (e) => { if (!$('menu').contains(e.target)) closeMenu(); });
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape') { closeMenu(); if (!$('modalBg').classList.contains('open')) closeBrowser(); } });

  $('licenseOk').addEventListener('click', async () => {
    const r = await api.activateLicense($('licenseKey').value);
    const msgs = { ok: '', invalid: 'That key isn\'t valid.', revoked: 'That key has been revoked.', seats: 'That key is already active on its maximum number of computers.', network: 'Couldn\'t reach the license server. Check your connection.', empty: 'Enter a key.' };
    $('licenseErr').textContent = msgs[r] ?? 'Activation failed.';
    if (r === 'ok') setState(await api.getState());
  });

  window.novaRackUpdate = applyLive;
  window.novaRackStateChanged = async () => setState(await api.getState());
  setState(await api.getState());
}

main();
