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
  'Nova Autotune': ['Retune Speed', 'Amount', 'Tolerance', 'Key', 'Scale', 'Formant Preserve'],
  'Nova Glue': ['Threshold', 'Ratio', 'Attack', 'Release', 'Makeup', 'Glue', 'Drive'],
};
const RACKS = [
  ['Analog Tour', 'DEMO', 'More faceplates', ['Nova Glow', 'Nova Glue', 'Nova Desk EQ', 'Nova Console', 'Nova Punch', 'Nova Tone']],
  ['Studio Tour', 'DEMO', 'One of each faceplate style', ['Nova 7even', 'Nova Royal 670', 'Nova Velvet', 'Nova Silk', 'Nova Tape', 'Nova Space']],
  ['Golden Era Chain', 'ANALOG • VOCAL', '1073-style pre and EQ, vari-mu glue, Pultec air, then tape', ['Nova 7even', 'Nova Royal 670', 'Nova Tone', 'Nova Tape']],
  ['Conejo Malo', 'VOCAL • LATIN', 'Deep baritone kept full, mid-forward bite, quick tune, short slap and plate',
    ['Nova Autotune', 'Nova Desk EQ', 'Nova Level', 'Nova Silk', 'Nova Heat', 'Nova Delay', 'Nova Space']],
  ['Vocal Starter', 'VOCAL • STARTER', 'Clean lead chain', ['Nova Autotune', 'Nova Desk EQ', 'Nova Level', 'Nova Silk', 'Nova Space']],
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

  return {
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
