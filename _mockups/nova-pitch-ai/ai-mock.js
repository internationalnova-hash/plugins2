// Mockup overlay: shows the proposed AI Harmony mode on the real Nova Pitch screen.
(function () {
  const css = `
  .aiPanel { display:flex; flex-direction:column; gap:10px; }
  .aiChord { display:flex; align-items:center; gap:14px; padding:12px 14px; border-radius:12px; background:rgba(156,125,255,.08); border:1px solid rgba(156,125,255,.35); }
  .aiChord .lbl { font-size:9.5px; letter-spacing:.18em; color:#8a8a9c; font-weight:700; }
  .aiChord .big { font-size:30px; font-weight:800; letter-spacing:-.02em; color:#f1f1f7; line-height:1; }
  .aiChord .big small { font-size:13px; color:#b79bff; font-weight:700; margin-left:6px; }
  .aiProg { display:flex; gap:5px; margin-left:auto; align-items:center; }
  .aiProg span { font-size:11px; padding:4px 8px; border-radius:6px; border:1px solid #2a2a3a; color:#8a8a9c; font-weight:600; }
  .aiProg span.now { color:#f1f1f7; border-color:#9c7dff; background:rgba(156,125,255,.18); }
  .aiProg i { color:#555566; font-style:normal; font-size:10px; }
  .aiRole { width:100%; }
  .aiNote { font-weight:700; color:#f1f1f7 !important; white-space:nowrap; font-size:13px; }
  .aiNote small { color:#8a8a9c; font-weight:500; margin-left:4px; }
  .aiCtl { display:grid; grid-template-columns: 1fr 1fr 1fr; gap:10px; padding:10px 12px; border-radius:12px; border:1px solid #23232f; background:#12121a; }
  .aiCtl .c { display:flex; flex-direction:column; gap:6px; }
  .aiCtl .t { display:flex; justify-content:space-between; font-size:10px; letter-spacing:.12em; color:#8a8a9c; font-weight:700; }
  .aiCtl .t b { color:#f1f1f7; letter-spacing:0; font-weight:600; }
  .aiCtl .bar { height:4px; border-radius:2px; background:#262634; position:relative; }
  .aiCtl .bar i { position:absolute; left:0; top:0; bottom:0; border-radius:2px; background:#9c7dff; box-shadow:0 0 8px rgba(156,125,255,.45); }
  .aiCtl .ends { display:flex; justify-content:space-between; font-size:9.5px; color:#555566; }
  .aiBadge { font-size:9px; font-weight:800; letter-spacing:.1em; color:#0a0a10; background:#b79bff; padding:2px 6px; border-radius:4px; margin-left:8px; vertical-align:2px; }
  `;
  const st = document.createElement("style"); st.textContent = css; document.head.appendChild(st);

  function apply() {
    const tab = document.getElementById("tabHarm"); if (tab) tab.click();
    for (let k = 0; k < 3 && !/ON/.test(document.getElementById("harmPowerLabel").textContent); k++) document.getElementById("harmOnBtn").click();
    document.getElementById("harmPowerLabel").textContent = "HARMONY ON";
    document.getElementById("harmPowerLabel").style.color = "#6ee7a8";
    document.getElementById("harmOnBtn").classList.add("is-active");
    // mode switch: SCALE | AI | MIDI
    const seg = document.getElementById("modeSeg");
    seg.innerHTML = '<button data-mode="0">SCALE</button><button data-mode="2" class="is-active">AI</button><button data-mode="1">MIDI</button>';
    seg.querySelectorAll("button").forEach(b => { b.style.cssText = ""; if (b.dataset.mode === "2") b.classList.add("is-active"); });
    document.getElementById("modeHint").textContent = "AI: listens to your melody, works out the chords and writes each part.";

    const col = document.querySelector(".partsCol");
    const rows = [
      ["#6ee7a8", "PART 1", "High", "B4", "↑3rd", "-4.0 dB"],
      ["#ffb86b", "PART 2", "Low",  "E4", "↓4th", "-6.0 dB"],
      ["#ff7eb6", "PART 3", "Octave", "G3", "↓8ve", "-8.0 dB"],
      ["#7cd1ff", "PART 4", "Top",  "--", "",     "-6.0 dB"],
    ];
    col.innerHTML = `<div class="secHead">AI Harmony <span class="aiBadge">AI</span> <span>· follows your melody</span></div>
      <div class="aiPanel">
        <div class="aiChord"><div><div class="lbl">CHORD</div><div class="big">Em<small>vi</small></div></div>
          <div class="aiProg"><span>C</span><i>›</i><span>G</span><i>›</i><span>D</span><i>›</i><span class="now">Em</span></div></div>
        ${rows.map(([c, n, role, note, rel, lev], i) => `
          <div class="part"><span class="sw" style="background:${c};box-shadow:0 0 8px ${c}"></span><div class="pname">${n}</div>
          <div class="partInt"><button class="intervalBtn aiRole" style="width:100%"><span>${role}</span><small>›</small></button></div>
          <div class="pnote aiNote">${note}<small>${rel}</small></div><div class="plev">${lev}</div>
          <button class="powerBtn ${i < 3 ? "is-active" : ""}" aria-label="Part on">⏻</button></div>`).join("")}
        <div class="aiCtl">
          <div class="c"><div class="t">COLOUR <b>Triads</b></div><div class="bar"><i style="width:30%"></i></div><div class="ends"><span>Triads</span><span>7ths · 9ths</span></div></div>
          <div class="c"><div class="t">MOVEMENT <b>Smooth</b></div><div class="bar"><i style="width:35%"></i></div><div class="ends"><span>Smooth</span><span>Bold</span></div></div>
          <div class="c"><div class="t">SPREAD <b>Close</b></div><div class="bar"><i style="width:25%"></i></div><div class="ends"><span>Close</span><span>Open</span></div></div>
        </div>
      </div>`;
  }
  setTimeout(apply, 700);
})();
