// Browser-preview backend (no JUCE). Not shipped in the plugin.
import { MOCK_FACES } from './mock-data.js';

const cat = (n) => {
  const s = n.toLowerCase(), any = (...w) => w.some((x) => s.includes(x));
  if (any('tune', 'pitch', 'harmony', 'chord', 'bend', 'align')) return 'Pitch';
  if (any('space', 'delay', 'aura', 'motion', 'doubler', 'glow')) return 'Space';
  if (any('eq', 'curve', 'tone', 'desk')) return 'EQ';
  if (any('tape', 'heat', 'velvet', 'silk', 'console', '7even', '670')) return 'Color';
  if (any('meter')) return 'Utility';
  if (any('master', 'apex', 'clip', 'translate')) return 'Master';
  if (any('punch', 'glue', 'ride', 'clean', 'level')) return 'Dynamics';
  if (any('vox', 'voice', 'vocal')) return 'Vocal';
  return 'Effect';
};

const CURATED = {
  'Nova 7even': ['Preamp Gain', 'Character', 'HPF', 'Low Gain', 'Mid Frequency', 'Mid Gain', 'High Shelf (12k)', 'Output Trim'],
  'Nova Royal 670': ['Input', 'AC Threshold', 'Time Constant', 'Tube Saturation', 'Transformer', 'Output'],
  'Nova Tone': ['Low Frequency', 'Low Boost', 'Low Attenuation', 'High Boost Frequency', 'High Boost', 'High Attenuation', 'Output Gain'],
  'Nova Tape': ['Input', 'Tape', 'Tape Type', 'IPS', 'Wow / Flutter', 'Noise', 'Output'],
  'Nova Velvet': ['Threshold', 'Ratio', 'Attack', 'Release', 'Gain', 'Analog Mode', 'Tube Drive'],
  'Nova Silk': ['Smooth', 'Focus', 'Air Preserve', 'Body', 'Output', 'Magic'],
  'Nova Space': ['Space', 'Decay', 'Pre-Delay', 'Damping', 'Width', 'Nova Mode', 'Mix'],
  'Nova Desk EQ': ['HPF Frequency', 'Low Gain', 'Low Mid Frequency', 'Low Mid Gain', 'High Mid Frequency', 'High Mid Gain', 'High Gain', 'Drive'],
  'Nova Pitch': ['Retune Speed', 'Humanize', 'Key', 'Scale', 'Natural Vibrato', 'Formant Warp', 'Transients', 'Output'],
  'Nova Glue': ['Threshold', 'Ratio', 'Attack', 'Release', 'Makeup', 'Glue', 'Drive'],
};
const RACKS = [
  ['Analog Tour', 'DEMO', 'More faceplates', ['Nova Glow', 'Nova Glue', 'Nova Desk EQ', 'Nova Console', 'Nova Punch', 'Nova Tone']],
  ['Studio Tour', 'DEMO', 'One of each faceplate style', ['Nova 7even', 'Nova Royal 670', 'Nova Velvet', 'Nova Silk', 'Nova Tape', 'Nova Space']],
  ['Golden Era Chain', 'ANALOG • VOCAL', '1073-style pre and EQ, vari-mu glue, Pultec air, then tape', ['Nova 7even', 'Nova Royal 670', 'Nova Tone', 'Nova Tape']],
  ['Conejo Malo', 'VOCAL • LATIN', 'Deep baritone kept full, mid-forward bite, quick tune, short slap and plate',
    ['Nova Pitch', 'Nova Desk EQ', 'Nova Level', 'Nova Silk', 'Nova Heat', 'Nova Delay', 'Nova Space']],
  ['Vocal Starter', 'VOCAL • STARTER', 'Clean lead chain', ['Nova Pitch', 'Nova Desk EQ', 'Nova Level', 'Nova Silk', 'Nova Space']],
];

const faceOf = (name) => {
  const all = MOCK_FACES[name] || [];
  const want = CURATED[name];
  if (!want) return all;
  return want.map((w) => all.find((p) => p[1] === w)).filter(Boolean).concat(all.filter((p) => !want.includes(p[1]))).slice(0, 8);
};

export function createMockBackend() {
  const plugins = Object.keys(MOCK_FACES).sort().map((name) => ({ name, category: cat(name), version: '1.0.0' }));
  let slots = [];
  let rack = { name: 'Golden Era Chain', dirty: false, factory: true };
  const modules = [{ plugin: 'Nova Tape', name: 'Warm Glue' }, { plugin: 'Nova Silk', name: 'Gentle De-Ess' }];
  let inGain = 0, outGain = 0;

  const mkSlot = (name) => ({
    plugin: name, missing: false, preset: '', category: cat(name), version: '1.0.0', bypass: false, mix: 1,
    programs: [],
    face: faceOf(name).map(([idx, pname, , steps]) => ({ idx, name: pname, steps, toggle: steps === 2, def: 0.5 })),
    values: faceOf(name).map(([, , text, steps]) => [steps === 2 ? (text === 'On' ? 1 : 0) : 0.3 + Math.random() * 0.4, text]),
  });
  const load = (name) => { const r = RACKS.find((x) => x[0] === name); slots = (r ? r[3] : []).map(mkSlot); rack = { name: r ? name : 'New Rack', dirty: false, factory: !!r }; };
  load('Studio Tour');
  slots[0].preset = 'Warm Vocal';

  const st = () => ({
    rack, maxSlots: 8, slots: slots.map(({ values, bypass, mix, ...s }) => s),
    library: { scanning: false, plugins },
    racks: RACKS.map(([name, group, note, chain]) => ({ name, group, note, chain, factory: true })),
    modules, licenseRequired: false, unlocked: true,
  });
  const ok = (r) => Promise.resolve(r);

  setInterval(() => {
    const t = Date.now() / 1000;
    window.novaRackUpdate?.({
      in: -14 + 6 * Math.sin(t * 2.1) - Math.random() * 4, out: -10 + 5 * Math.sin(t * 1.7) - Math.random() * 3,
      inGain, outGain, latency: 0, dirty: rack.dirty,
      slots: slots.map((s, i) => ({ k: s.values, b: s.bypass, m: s.mix, l: 0, p: -16 + 7 * Math.sin(t * 2.3 + i) - Math.random() * 5 })),
    });
  }, 50);

  // Nova Match demo (browser preview only)
  const pk = (seed) => { let r = seed; return Array.from({ length: 240 }, (_, i) => { r = (r * 9301 + 49297) % 233280; return (0.4 + 0.6 * r / 233280) * (0.2 + 0.8 * Math.abs(Math.sin(i / 18))); }); };
  const curve = (f) => Array.from({ length: 24 }, (_, i) => f(i / 23));
  const ref = (t) => 14 * Math.sin(t * 3.2) * 0.4 + 6 * Math.exp(-Math.pow((t - 0.7) / 0.08, 2)) - 30 * Math.max(0, t - 0.85) - 20 * Math.max(0, 0.12 - t) * 6;
  const matchDemo = {
    ref: { name: 'Ozuna – reference (acapella)', seconds: 42, rate: 48000, channels: 2, key: 'A MINOR', peaks: pk(7) },
    raw: { name: 'Captured from the track', seconds: 30, rate: 48000, channels: 2, key: 'A MINOR', peaks: pk(3) },
    capturing: false, captureSeconds: 0, captureMax: 30, running: false, progress: 1, status: 'Matched in 31 s · 268 test renders', preview: 3,
    stages: ['Tuning', 'Tone (EQ)', 'Dynamics', "Air & S's", 'Warmth', 'Width & doubles', 'Delay', 'Space (reverb)'].map((name, i) => ({
      name, plugin: ['Nova Pitch', 'Nova Desk EQ', 'Nova Royal 670', 'Nova Silk', 'Nova Tape', 'Nova Doubler', 'Nova Delay', 'Nova Space'][i] })),
    result: {
      raw: 29, matched: 88,
      rawScores: [96, 19, 13, 92, 45, 6, 4, 2], matchedScores: [97, 92, 98, 98, 98, 79, 64, 85],
      used: [false, true, true, false, true, false, true, true],
      specRef: curve(ref), specRaw: curve((t) => ref(t) + 6 * Math.exp(-Math.pow((t - 0.6) / 0.07, 2)) + 3 * Math.sin(t * 9)), specMatched: curve((t) => ref(t) + 0.8 * Math.sin(t * 13)),
      chain: [{ plugin: 'Nova Desk EQ', summary: 'Low -3.0 dB · Hi-mid +4.0 dB' }, { plugin: 'Nova Royal 670', summary: 'Input 6.0 · 1.2 sec' },
              { plugin: 'Nova Tape', summary: 'Vintage · Tape 15' }, { plugin: 'Nova Delay', summary: '333 ms · Mix 18' }, { plugin: 'Nova Space', summary: '1.2 s · Mix 28 %' }],
      notes: 'The reference has a noticeable reverb tail (-14.8 dB after phrases). It has an echo about 370 ms after each phrase. Your voice was 4.1 dB louder around 3.2 kHz than the reference.',
      renders: 268, seconds: 31,
    },
  };
  return {
    matchState: async () => matchDemo,
    matchChooseFile: async () => ({ error: '', state: matchDemo }),
    matchCapture: async () => matchDemo,
    matchRun: async () => ({ error: '', state: matchDemo }),
    matchCancel: async () => matchDemo,
    matchPreview: async (p) => { matchDemo.preview = p; return matchDemo; },
    matchStrength: async () => ({}),
    matchLoadChain: async () => ({ error: '' }),
    getState: () => ok(st()),
    addPlugin: (name) => { slots.push(mkSlot(name)); rack.dirty = true; return ok({ state: st() }); },
    removeSlot: (i) => { slots.splice(i, 1); rack.dirty = true; return ok(st()); },
    moveSlot: (f, t) => { const [s] = slots.splice(f, 1); slots.splice(t, 0, s); rack.dirty = true; return ok(st()); },
    setBypass: (i, b) => { slots[i].bypass = b; rack.dirty = true; return ok(); },
    setMix: (i, v) => { slots[i].mix = v; return ok(); },
    setGain: (w, db) => { if (w === 'out') outGain = db; else inGain = db; return ok(); },
    setParam: (i, p, v) => { const k = slots[i].face.findIndex((f) => f.idx === p); if (k >= 0) slots[i].values[k] = [v, (v * 100).toFixed(0) + '%']; rack.dirty = true; return ok(); },
    listParams: (i) => ok((MOCK_FACES[slots[i].plugin] || []).map(([idx, name]) => ({ idx, name }))),
    setFace: () => ok(st()),
    openPlugin: (i) => { alert(`Opens ${slots[i].plugin} at full size`); return ok(); },
    newRack: () => { load(''); return ok(st()); },
    loadRack: (n) => { load(n); return ok({ state: st() }); },
    saveRack: (n) => { rack = { name: n, dirty: false, factory: false }; return ok({ state: st() }); },
    deleteRack: () => ok(st()),
    loadModule: (i, n) => { slots[i].preset = n; return ok({ state: st() }); },
    saveModule: (i, n) => { modules.push({ plugin: slots[i].plugin, name: n }); slots[i].preset = n; return ok({ state: st() }); },
    deleteModule: () => ok(st()),
    addModule: (p, n) => { const s = mkSlot(p); s.preset = n; slots.push(s); return ok({ state: st() }); },
    rescan: () => ok(st()),
    activateLicense: () => ok('ok'),
  };
}
