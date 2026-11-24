// Nova Match panel: load a reference vocal and your vocal, match, A/B, load the chain into the rack.
// Talks to the processor through the match* native functions (see PluginEditor.cpp).

const STAGE_COLORS = ['#5fe39a', '#9c7dff', '#9c7dff', '#9c7dff', '#ffb86b', '#9c7dff', '#7cd1ff', '#7cd1ff'];
const SHORT = { 'Nova Pitch': 'PITCH', 'Nova Desk EQ': 'DESK EQ', 'Nova Royal 670': 'ROYAL 670', 'Nova Silk': 'SILK',
                'Nova Tape': 'TAPE', 'Nova Doubler': 'DOUBLER', 'Nova Delay': 'DELAY', 'Nova Space': 'SPACE' };

const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const fmtTime = (s) => `${Math.floor(s / 60)}:${String(Math.floor(s % 60)).padStart(2, '0')}`;

export function createMatchPanel({ root, button, api, toast, onRackChanged }) {
  let st = null;
  let open = false;
  let poll = null;
  let strength = 0.85;
  const locked = new Array(8).fill(false);
  const hasApi = typeof api.matchState === 'function' || (api && api.matchState);

  root.innerHTML = `
    <div class="mv-body">
      <div class="mv-card">
        <div class="mv-h">Reference vocal <span>the sound you want</span></div>
        <div id="mvRef"></div>
        <div class="mv-h" style="margin-top:2px">Your vocal <span>capture or choose a file</span></div>
        <div id="mvRaw"></div>
        <div class="mv-h" style="margin-top:2px">Tone curve <span>reference vs you</span></div>
        <canvas id="mvSpec" width="560" height="140" style="height:64px"></canvas>
        <div class="mv-legend"><span><i style="background:#9c7dff"></i>Reference</span><span><i style="background:#8b9099"></i>Raw</span><span><i style="background:#5fe39a"></i>Matched</span></div>
      </div>
      <div class="mv-card mv-score">
        <div class="mv-ring">
          <svg viewBox="0 0 214 214"><defs><linearGradient id="mvg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#b78dff"/><stop offset="1" stop-color="#7cd1ff"/></linearGradient></defs>
            <circle cx="107" cy="107" r="96" fill="none" stroke="rgba(255,255,255,.08)" stroke-width="6"/>
            <circle id="mvArc" cx="107" cy="107" r="96" fill="none" stroke="url(#mvg)" stroke-width="6" stroke-linecap="round" stroke-dasharray="0 603" style="filter:drop-shadow(0 0 8px rgba(156,125,255,.6));transition:stroke-dasharray 600ms ease"/></svg>
          <div><div class="mv-big" id="mvBig">--</div><div class="mv-sub">Match</div></div>
        </div>
        <div class="mv-before" id="mvBefore">Load a reference and your vocal, then press Match.</div>
        <div class="mv-progress" id="mvProg" style="display:none"><i id="mvProgBar" style="width:0%"></i></div>
        <div class="mv-status" id="mvStatus"></div>
        <div class="mv-ab" id="mvAB">
          <button data-p="1">REFERENCE</button><button data-p="2">YOUR RAW</button><button data-p="3">MATCHED</button><button data-p="0">■ STOP</button>
        </div>
        <div class="mv-slider">
          <div class="mv-h">Match strength <span id="mvStrengthText">85% · how far the chain goes toward the reference</span></div>
          <input type="range" min="0" max="100" value="85" id="mvStrength" />
        </div>
        <div class="mv-row-btns" style="justify-content:center;margin-top:6px">
          <button class="mv-btn primary" id="mvRun">✦ MATCH</button>
          <button class="mv-btn" id="mvLoad">LOAD CHAIN INTO RACK</button>
          <button class="mv-btn" id="mvSave">SAVE AS RACK PRESET</button>
        </div>
      </div>
      <div class="mv-card">
        <div class="mv-h">What it matched <span>lock a stage to keep your own sound</span></div>
        <div id="mvStages" style="display:flex;flex-direction:column;gap:12px;margin-top:4px"></div>
      </div>
    </div>
    <div class="mv-bottom">
      <div class="mv-card" style="padding:12px 14px;gap:8px">
        <div class="mv-h">Built chain <span>real Nova plugins, every knob editable once loaded</span></div>
        <div class="mv-chain" id="mvChain"><span class="mv-empty-chain">The chain appears here after matching.</span></div>
      </div>
      <div class="mv-card" style="padding:12px 14px;gap:6px">
        <div class="mv-h">Notes</div>
        <div class="mv-notes" id="mvNotes">Works best with an acapella reference (vocal only, no beat). Matching listens to tone, compression, saturation, S's, tuning, width, delay and reverb.</div>
      </div>
    </div>`;

  const $ = (id) => root.querySelector('#' + id);

  // ── Drawing ────────────────────────────────────────────────────────
  function wave(canvas, peaks, color) {
    const g = canvas.getContext('2d');
    g.clearRect(0, 0, canvas.width, canvas.height);
    if (!peaks || !peaks.length) return;
    const max = Math.max(0.0001, ...peaks);
    const w = canvas.width / peaks.length;
    g.fillStyle = color;
    peaks.forEach((p, i) => {
      const h = Math.max(2, (p / max) * canvas.height * 0.92);
      g.fillRect(i * w, (canvas.height - h) / 2, Math.max(1, w - 1), h);
    });
  }

  function spectrum() {
    const c = $('mvSpec'), g = c.getContext('2d'), W = c.width, H = c.height;
    g.clearRect(0, 0, W, H);
    g.strokeStyle = 'rgba(255,255,255,.06)';
    for (let k = 1; k < 4; k++) { g.beginPath(); g.moveTo(0, H * k / 4); g.lineTo(W, H * k / 4); g.stroke(); }
    g.fillStyle = '#5b6069'; g.font = '16px -apple-system, sans-serif';
    [['100', 3], ['1k', 13], ['10k', 22]].forEach(([l, b]) => g.fillText(l, (b / 23) * W - 8, H - 4));
    const r = st?.result;
    if (!r) return;
    const line = (arr, col, w) => {
      g.beginPath();
      arr.forEach((v, i) => {
        const x = (i / (arr.length - 1)) * W;
        const y = H * 0.45 - (Math.max(-30, Math.min(20, v)) / 30) * H * 0.55;
        i ? g.lineTo(x, y) : g.moveTo(x, y);
      });
      g.strokeStyle = col; g.lineWidth = w; g.stroke();
    };
    line(r.specRaw, '#8b9099', 2);
    line(r.specMatched, '#5fe39a', 2.5);
    line(r.specRef, '#9c7dff', 2.5);
  }

  function clipCard(clip, isRef) {
    const box = isRef ? $('mvRef') : $('mvRaw');
    const capturing = !isRef && st?.capturing;
    const busy = st?.running;
    if (!clip && !capturing) {
      box.innerHTML = `<div class="mv-drop empty">
          <div>${isRef ? 'Choose the vocal you want to sound like.<br>An acapella works best.' : 'Play your vocal track and press Capture,<br>or choose a file.'}</div>
          <div class="mv-row-btns" style="justify-content:center">
            ${isRef ? '' : '<button class="mv-btn rec" data-act="capture">● CAPTURE 30 s</button>'}
            <button class="mv-btn" data-act="choose" ${busy ? 'disabled' : ''}>CHOOSE FILE</button>
          </div></div>`;
    } else if (capturing) {
      box.innerHTML = `<div class="mv-drop">
          <div class="mv-fname">Recording the rack's input… ${st.captureSeconds.toFixed(1)} s</div>
          <div class="mv-meta">Play your vocal track. Up to ${Math.round(st.captureMax)} s.</div>
          <div class="mv-row-btns"><button class="mv-btn rec on" data-act="stop">■ STOP</button></div></div>`;
    } else {
      box.innerHTML = `<div class="mv-drop">
          <div class="mv-fname" title="${esc(clip.name)}">${esc(clip.name)}</div>
          <div class="mv-meta">${fmtTime(clip.seconds)} · ${Math.round(clip.rate / 100) / 10} kHz · ${clip.channels > 1 ? 'stereo' : 'mono'}</div>
          <canvas width="560" height="60" style="height:28px"></canvas>
          <div class="mv-row-btns" style="align-items:center">
            ${clip.key ? `<span class="mv-tag">KEY: ${esc(clip.key)}</span><span style="flex:1"></span>` : '<span style="flex:1"></span>'}
            ${isRef ? '' : `<button class="mv-btn rec" data-act="capture" ${busy ? 'disabled' : ''}>● RECAPTURE</button>`}
            <button class="mv-btn" data-act="choose" ${busy ? 'disabled' : ''}>CHOOSE FILE</button>
          </div></div>`;
      wave(box.querySelector('canvas'), clip.peaks, isRef ? '#9c7dff' : '#8b9099');
    }
    box.querySelectorAll('[data-act]').forEach((b) => b.addEventListener('click', async () => {
      const act = b.dataset.act;
      if (act === 'choose') {
        const r = await api.matchChooseFile(isRef ? 'ref' : 'raw');
        if (r?.error) toast(r.error);
        setState(r?.state ?? r);
      } else if (act === 'capture') setState(await api.matchCapture(true));
      else if (act === 'stop') setState(await api.matchCapture(false));
    }));
  }

  function render() {
    if (!st) return;
    // Only rebuild the input cards when something about them changed
    const key = JSON.stringify([st.ref?.name, st.ref?.seconds, st.raw?.name, st.raw?.seconds, st.capturing,
                                st.capturing ? Math.floor(st.captureSeconds * 2) : 0, st.running]);
    if (key !== render.lastKey) { render.lastKey = key; clipCard(st.ref, true); clipCard(st.raw, false); }

    const r = st.result;
    const score = r ? Math.round(r.matched) : null;
    $('mvBig').innerHTML = score == null ? '--' : `${score}<small>%</small>`;
    $('mvArc').setAttribute('stroke-dasharray', `${((score ?? 0) / 100) * 603} 603`);
    $('mvBefore').innerHTML = r ? `Your raw vocal: <b>${Math.round(r.raw)}%</b> → with the matched chain: <b>${score}%</b>`
                                : (st.ref && st.raw ? 'Ready. Press Match.' : 'Load a reference and your vocal, then press Match.');

    $('mvProg').style.display = st.running ? '' : 'none';
    $('mvProgBar').style.width = `${Math.round((st.progress || 0) * 100)}%`;
    const statusIsError = !st.running && !r && st.status && !/^Starting|^Matched/.test(st.status);
    $('mvStatus').innerHTML = st.status ? `<span class="${statusIsError ? 'mv-err' : ''}">${esc(st.status)}</span>` : '';

    $('mvRun').textContent = st.running ? '■ CANCEL' : (r ? '↻ MATCH AGAIN' : '✦ MATCH');
    $('mvRun').disabled = !st.running && !(st.ref && st.raw);
    $('mvLoad').disabled = !r || st.running;
    $('mvSave').disabled = !r || st.running;

    root.querySelectorAll('#mvAB button').forEach((b) => {
      const p = Number(b.dataset.p);
      b.classList.toggle('on', p !== 0 && st.preview === p);
      b.disabled = (p === 1 && !st.ref) || (p === 2 && !st.raw) || (p === 3 && !r) || st.running;
    });

    // Stages
    $('mvStages').innerHTML = st.stages.map((s, i) => {
      const before = r ? r.rawScores[i] : 0, after = r ? r.matchedScores[i] : 0;
      const used = r?.used?.[i];
      return `<div class="mv-stage">
        <div><div class="n">${esc(s.name)}</div><div class="p">${used ? esc(s.plugin) : (r ? (locked[i] ? 'kept yours' : 'not needed') : esc(s.plugin))}</div></div>
        <div class="mv-bar"><i style="width:${after}%;background:${STAGE_COLORS[i]};box-shadow:0 0 8px ${STAGE_COLORS[i]}"></i>${r ? `<b style="left:${before}%" title="before: ${Math.round(before)}%"></b>` : ''}</div>
        <div class="mv-pct">${r ? Math.round(after) + '%' : ''}</div>
        <button class="mv-lock ${locked[i] ? 'on' : ''}" data-lock="${i}" title="${locked[i] ? 'Locked: Match leaves this alone' : 'Lock to keep your own sound here'}">${locked[i] ? '🔒' : ''}</button>
      </div>`;
    }).join('');
    $('mvStages').querySelectorAll('[data-lock]').forEach((b) => b.addEventListener('click', () => {
      const i = Number(b.dataset.lock); locked[i] = !locked[i]; render();
    }));

    // Chain + notes
    $('mvChain').innerHTML = r && r.chain.length
      ? r.chain.map((c, i) => `${i ? '<span class="mv-arrow">›</span>' : ''}<div class="mv-mod"><div class="t">${SHORT[c.plugin] || esc(c.plugin)}</div><div class="v" title="${esc(c.summary)}">${esc(c.summary)}</div></div>`).join('')
      : `<span class="mv-empty-chain">${r ? 'Your vocal already matches; nothing needed.' : 'The chain appears here after matching.'}</span>`;
    if (r) $('mvNotes').textContent = r.notes || 'No big differences found.';
    spectrum();
  }

  function setState(s) {
    if (!s || typeof s !== 'object' || !s.stages) return;
    st = s;
    render();
  }

  async function refresh() {
    try { setState(await api.matchState()); } catch (e) { /* the editor is closing */ }
  }

  // ── Controls ───────────────────────────────────────────────────────
  $('mvRun').addEventListener('click', async () => {
    if (st?.running) { setState(await api.matchCancel()); return; }
    const r = await api.matchRun(locked);
    if (r?.error) toast(r.error);
    setState(r?.state ?? r);
  });
  root.querySelectorAll('#mvAB button').forEach((b) => b.addEventListener('click', async () => {
    const p = Number(b.dataset.p);
    setState(await api.matchPreview(st?.preview === p ? 0 : p));
  }));
  $('mvStrength').addEventListener('input', () => {
    strength = Number($('mvStrength').value) / 100;
    $('mvStrengthText').textContent = `${Math.round(strength * 100)}% · how far the chain goes toward the reference`;
    api.matchStrength(strength);
  });
  $('mvLoad').addEventListener('click', async () => {
    const r = await api.matchLoadChain(strength);
    if (r?.error) toast(r.error); else toast('Matched chain loaded into the rack.');
    onRackChanged(r?.state ?? r);
  });
  $('mvSave').addEventListener('click', async () => {
    const r = await api.matchLoadChain(strength);
    if (r?.error) { toast(r.error); return; }
    onRackChanged(r?.state ?? r, true);   // index.js opens the "save rack" dialog
  });

  function setOpen(v) {
    open = v;
    root.classList.toggle('open', v);
    button.classList.toggle('on', v);
    clearInterval(poll);
    if (v) { refresh(); poll = setInterval(refresh, 250); }
    else if (st?.preview) api.matchPreview(0);
  }
  button.addEventListener('click', () => setOpen(!open));

  return { setOpen, isOpen: () => open };
}
