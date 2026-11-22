import { getSliderState, getToggleState, getComboBoxState, getNativeFunction } from "./juce/index.js";

function hasNative() {
  return typeof window.__JUCE__ !== "undefined";
}

function callNative(name) {
  var args = Array.prototype.slice.call(arguments, 1);
  if (!hasNative()) return Promise.resolve(null);
  var fn = getNativeFunction(name);
  return fn.apply(null, args);
}

// ---------------------------------------------------------------------
// Stage scale-to-fit - #stage is a fixed 1086x362 design canvas, uniformly
// scaled via CSS transform to whatever size the actual window is (same
// technique as the rest of the catalog - never flex-centering, which
// breaks at non-native aspect ratios).
// ---------------------------------------------------------------------
(function () {
  var lastW = -1, lastH = -1;
  function fitStage() {
    var w = document.documentElement.clientWidth || window.innerWidth || 0;
    var h = document.documentElement.clientHeight || window.innerHeight || 0;
    if (w <= 0 || h <= 0) return;
    if (w === lastW && h === lastH) return;
    lastW = w; lastH = h;
    var scale = Math.min(w / 1086, h / 362);
    window.__novaVelvetScale = scale || 1;
    var stage = document.getElementById("stage");
    if (stage) stage.style.transform = "scale(" + scale + ")";
  }
  fitStage();
  window.addEventListener("resize", fitStage);
  setInterval(fitStage, 300);
})();

var KNOB_MIN_ANGLE = -135, KNOB_MAX_ANGLE = 135;

function angleForNorm(norm) {
  return KNOB_MIN_ANGLE + Math.max(0, Math.min(1, norm)) * (KNOB_MAX_ANGLE - KNOB_MIN_ANGLE);
}

// ---------------------------------------------------------------------
// Locked knob-scale spec (authoritative - overrides the AI mockup's own
// numbers, which have a couple of duplicated/malformed values, Threshold
// especially). Tick POSITIONS for the printed scale are computed from
// this spec's own min/max + "linear" or "log" rule, independent of the
// SliderState's skew - this is deliberately not the same curve as the
// pointer's own rotation (state.getNormalisedValue()) when a param's
// skew doesn't match a pure log (e.g. Attack/Release/SC-HPF use skew=0.4
// for a natural-feeling drag, but the printed scale must show a true log
// scale per spec).
// `reverse: true` means the scale runs clockwise from the param's MAX to
// its MIN (Threshold only - turning clockwise = lower threshold = more
// compression, the natural physical-hardware feel), so both the printed
// ticks and the knob pointer's own rotation/drag direction are flipped.
// ---------------------------------------------------------------------
function scaleNorm(value, min, max, isLog) {
  if (isLog) {
    var t = (Math.log(value) - Math.log(min)) / (Math.log(max) - Math.log(min));
    return Math.max(0, Math.min(1, t));
  }
  return Math.max(0, Math.min(1, (value - min) / (max - min)));
}

// ---------------------------------------------------------------------
// Radial tick + number scale drawn around a knob, matching the locked
// spec exactly: `spec.majors` is an explicit [{value, label}] array in
// CLOCKWISE order (label overrides the printed text - used for "OFF"/
// "MAX"); position comes from `scaleNorm(value, spec.min, spec.max,
// spec.log)`, then flipped (1-norm) when `spec.reverse` is set. Minor
// ticks are interpolated evenly between each consecutive pair of majors
// (not globally even), since log-spaced majors are not evenly spaced by
// angle themselves.
// ---------------------------------------------------------------------
function renderKnobScale(svgEl, spec, small) {
  if (!svgEl) return;
  var box = 100, c = box / 2;
  var tickInnerR = small ? 30 : 36, tickOuterR = small ? 40 : 43, numberR = small ? 47 : 48.5;
  var tickStroke = small ? 2.2 : 1.9;
  var fontSize = small ? 10 : 10.5;
  var minorPerMajor = spec.minorPerMajor === undefined ? 3 : spec.minorPerMajor;

  svgEl.setAttribute("viewBox", "0 0 " + box + " " + box);
  svgEl.innerHTML = "";

  function visNorm(value) {
    var n = scaleNorm(value, spec.min, spec.max, spec.log);
    return spec.reverse ? 1 - n : n;
  }

  function drawLine(norm, r1, r2, stroke, width) {
    var angle = angleForNorm(norm);
    var rad = (Math.PI / 180) * angle;
    var sin = Math.sin(rad), cos = Math.cos(rad);
    var line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", c + r1 * sin); line.setAttribute("y1", c - r1 * cos);
    line.setAttribute("x2", c + r2 * sin); line.setAttribute("y2", c - r2 * cos);
    line.setAttribute("stroke", stroke);
    line.setAttribute("stroke-width", String(width));
    line.setAttribute("stroke-linecap", "round");
    svgEl.appendChild(line);
    return angle;
  }

  var majorNorms = spec.majors.map(function (m) { return visNorm(m.value); });

  // Minor ticks, evenly interpolated between each consecutive pair of majors.
  if (minorPerMajor > 0) {
    for (var i = 0; i < majorNorms.length - 1; i++) {
      for (var k = 1; k <= minorPerMajor; k++) {
        var t = k / (minorPerMajor + 1);
        var mn = majorNorms[i] + (majorNorms[i + 1] - majorNorms[i]) * t;
        drawLine(mn, tickInnerR, tickOuterR - 2, "rgba(238,233,221,0.55)", small ? 1.4 : 1.1);
      }
    }
  }

  spec.majors.forEach(function (m, i) {
    var angle = drawLine(majorNorms[i], tickInnerR, tickOuterR, "#f5f1e6", tickStroke);
    if (spec.noNumbers) return;
    var rad = (Math.PI / 180) * angle;
    var sin = Math.sin(rad), cos = Math.cos(rad);
    var tx = c + numberR * sin, ty = c - numberR * cos;
    var text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tx); text.setAttribute("y", ty);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("dominant-baseline", "middle");
    text.setAttribute("font-size", String(fontSize));
    text.setAttribute("font-weight", "600");
    text.setAttribute("font-family", "Arial, sans-serif");
    text.setAttribute("fill", "#e9e4d6");
    text.textContent = m.label;
    svgEl.appendChild(text);
  });
}

// ---------------------------------------------------------------------
// Rotary knob - drag vertically to change, backed by a real SliderState.
// The printed scale (spec.majors/min/max/log/reverse) is independent of
// the pointer's own rotation, which always tracks the real automation
// curve (state.getNormalisedValue(), respecting the param's true skew) -
// except that `spec.reverse` flips the pointer's visual angle AND drag
// direction too, so the knob's physical feel matches the printed scale
// (Threshold: turning clockwise lowers the raw value/raises compression).
// ---------------------------------------------------------------------
function wireKnob(groupEl, opts) {
  if (!groupEl) return null;
  var imageEl = groupEl.querySelector(".knob-image, .knob-small-image");
  var svgEl = groupEl.querySelector(".scale-svg");
  var small = imageEl && imageEl.classList.contains("knob-small-image");
  var reverse = !!(opts.spec && opts.spec.reverse);

  var state = hasNative() ? getSliderState(opts.name) : null;

  function drawScale() {
    if (!state || !opts.spec) return;
    renderKnobScale(svgEl, opts.spec, small);
  }

  function applyVisual() {
    var rawNorm = state ? state.getNormalisedValue() : 0.5;
    var visNorm = reverse ? 1 - rawNorm : rawNorm;
    if (imageEl) imageEl.style.transform = "rotate(" + angleForNorm(visNorm) + "deg)";
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    state.propertiesChangedEvent.addListener(function () { drawScale(); applyVisual(); });
    setTimeout(function () { drawScale(); applyVisual(); }, 60);
  } else {
    drawScale();
    applyVisual();
  }

  var dragging = false, startY = 0, startNorm = 0;
  var dragRangePx = 140;

  groupEl.addEventListener("pointerdown", function (e) {
    if (!state) return;
    dragging = true;
    startY = e.clientY;
    startNorm = state.getNormalisedValue();
    try { groupEl.setPointerCapture(e.pointerId); } catch (err) {}
    state.sliderDragStarted();
  });
  groupEl.addEventListener("pointermove", function (e) {
    if (!dragging || !state) return;
    var scale = window.__novaVelvetScale || 1;
    var deltaNorm = ((startY - e.clientY) / scale) / dragRangePx;
    if (reverse) deltaNorm = -deltaNorm;
    state.setNormalisedValue(Math.max(0, Math.min(1, startNorm + deltaNorm)));
    applyVisual();
  });
  function endDrag(e) {
    if (!dragging) return;
    dragging = false;
    // WKWebView has been observed NOT auto-releasing pointer capture on
    // pointerup the way the spec requires (unlike this same code's
    // Chromium-based preview) - left stuck, a captured element can keep
    // swallowing pointer/click events site-wide after the drag ends. Cheap
    // and correct regardless of the exact cause.
    if (e) { try { groupEl.releasePointerCapture(e.pointerId); } catch (err) {} }
    if (state) state.sliderDragEnded();
  }
  groupEl.addEventListener("pointerup", endDrag);
  groupEl.addEventListener("pointercancel", endDrag);

  groupEl.addEventListener("dblclick", function () {
    if (!state || opts.defaultNorm === undefined) return;
    state.sliderDragStarted();
    state.setNormalisedValue(opts.defaultNorm);
    applyVisual();
    state.sliderDragEnded();
  });

  return { refresh: applyVisual };
}

// ---------------------------------------------------------------------
// 3-position image switch (Time Select, Analog Mode) - backed by a
// 3-choice ComboBox. Image swaps per position; labels highlight to match.
// ---------------------------------------------------------------------
function wireSwitch3(groupEl, name, imageEl, images) {
  if (!groupEl || !imageEl) return;
  var state = hasNative() ? getComboBoxState(name) : null;
  var labels = groupEl.querySelectorAll(".switch-options span");

  function applyVisual() {
    var idx = state ? state.getChoiceIndex() : 0;
    imageEl.src = images[idx] || images[0];
    labels.forEach(function (l) {
      l.classList.toggle("active", parseInt(l.getAttribute("data-index"), 10) === idx);
    });
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  labels.forEach(function (l) {
    l.addEventListener("click", function () {
      if (!state) return;
      state.setChoiceIndex(parseInt(l.getAttribute("data-index"), 10));
      applyVisual();
    });
  });

  var dragging = false, startY = 0, startIdx = 0;
  imageEl.addEventListener("pointerdown", function (e) {
    if (!state) return;
    dragging = true; startY = e.clientY; startIdx = state.getChoiceIndex();
    try { imageEl.setPointerCapture(e.pointerId); } catch (err) {}
  });
  imageEl.addEventListener("pointermove", function (e) {
    if (!dragging || !state) return;
    var scale = window.__novaVelvetScale || 1;
    var deltaSteps = Math.round(((startY - e.clientY) / scale) / 24);
    var newIdx = Math.max(0, Math.min(2, startIdx + deltaSteps));
    if (newIdx !== state.getChoiceIndex()) { state.setChoiceIndex(newIdx); applyVisual(); }
  });
  function endDrag(e) {
    dragging = false;
    // See the matching comment in wireKnob's endDrag - WKWebView has been
    // observed not auto-releasing pointer capture on pointerup, which can
    // leave this image swallowing clicks meant for the adjacent labels.
    if (e) { try { imageEl.releasePointerCapture(e.pointerId); } catch (err) {} }
  }
  imageEl.addEventListener("pointerup", endDrag);
  imageEl.addEventListener("pointercancel", endDrag);
}

// ---------------------------------------------------------------------
// 2-position image switch (Stereo Link) - backed by a bool WebToggleRelay.
// ---------------------------------------------------------------------
function wireSwitch2(groupEl, name, imageEl, images) {
  if (!groupEl || !imageEl) return;
  var state = hasNative() ? getToggleState(name) : null;
  var labels = groupEl.querySelectorAll(".switch-options span");

  function applyVisual() {
    var on = state ? state.getValue() : true;
    imageEl.src = on ? images[1] : images[0];
    labels.forEach(function (l) {
      l.classList.toggle("active", (parseInt(l.getAttribute("data-index"), 10) === 1) === on);
    });
  }

  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  function toggle() {
    if (!state) return;
    state.setValue(!state.getValue());
    applyVisual();
  }
  imageEl.addEventListener("click", toggle);
  labels.forEach(function (l) { l.addEventListener("click", toggle); });
}

// ---------------------------------------------------------------------
// Tube glow crossfade - real, not decorative: baseline comes from the
// Tube Drive parameter, nudged forward by actual gain-reduction activity
// so the glow visibly responds to processing, not just the knob position.
// ---------------------------------------------------------------------
var tubeDriveNorm01 = 0.25;
var recentActivity01 = 0;

function applyTubeVisual() {
  var intensity = Math.max(0, Math.min(1, tubeDriveNorm01 * 0.7 + recentActivity01 * 0.3));
  var stops = [
    { id: "tube-off-img", at: 0.0 },
    { id: "tube-idle-img", at: 0.33 },
    { id: "tube-active-img", at: 0.66 },
    { id: "tube-strong-img", at: 1.0 },
  ];
  stops.forEach(function (s, i) {
    var el = document.getElementById(s.id);
    if (!el) return;
    var prev = stops[i - 1];
    var next = stops[i + 1];
    var opacity = 0;
    if (intensity <= s.at) {
      opacity = prev ? Math.max(0, 1 - (s.at - intensity) / (s.at - prev.at)) : 1;
    } else {
      opacity = next ? Math.max(0, 1 - (intensity - s.at) / (next.at - s.at)) : 1;
    }
    el.style.opacity = String(opacity);
    el.classList.toggle("tube-visible", opacity > 0.01);
  });
}

function wireTubeDrive() {
  var state = hasNative() ? getSliderState("tube_drive") : null;
  function applyVisual() {
    tubeDriveNorm01 = state ? state.getNormalisedValue() : 0.25;
    applyTubeVisual();
  }
  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();
}

// ---------------------------------------------------------------------
// VU meter - real output-level telemetry, -20..+3 VU scale matching the
// mockup and the engine's NovaVUBallistics (-18dBFS = 0VU) convention.
// ---------------------------------------------------------------------
var VU_MIN = -20, VU_MAX = 3, VU_RED_FROM = 0;
var VU_TICKS = [-20, -10, -7, -5, -3, -1, 0, 1, 2, 3];
var NEEDLE_MIN_ANGLE = -42, NEEDLE_MAX_ANGLE = 42;

// Full rebuild (previous version hand-placed each label's radius/font as
// one-off patches, which is exactly what made it look "crooked" - a real
// meter's ticks and numbers all derive from ONE pivot and ONE radius).
//
// VU_TICK_POS is each major value's position as a FRACTION (0..1) along
// the sweep - not linear-in-dB, not even-by-index. Real VU faces compress
// the low end (-20..-10 spans 10dB in a small fraction of the arc) and
// give progressively more room approaching 0dB, since that's the
// perceptually/functionally important region; the +1..+3 red region gets
// its own comparable density. angleForPos() turns a fraction into a
// sweep angle; angleForVu() (below) interpolates the real telemetry value
// between whichever two ticks it falls between, using these SAME
// fractions, so the needle always agrees with the printed numbers.
var VU_TICK_POS = {
  "-20": 0.00, "-10": 0.15, "-7": 0.30, "-5": 0.44, "-3": 0.56,
  "-1": 0.66, "0": 0.74, "1": 0.82, "2": 0.90, "3": 1.00,
};
// 0/+1/+2/+3 still end up the densest cluster on the dial (real VU
// ballistics) even after giving them the most room the layout allows -
// a slightly smaller font here (applied below) is what actually keeps
// all four legible, same fix already proven on the knob scales.
var VU_SMALL_FONT = { "0": 1, "1": 1, "2": 1, "3": 1 };

function angleForPos(pos) {
  return NEEDLE_MIN_ANGLE + pos * (NEEDLE_MAX_ANGLE - NEEDLE_MIN_ANGLE);
}

function angleForTickIndex(i) {
  return angleForPos(VU_TICK_POS[String(VU_TICKS[i])]);
}

// The needle interpolates within whichever pair of ticks the real value
// falls between, using their real VU_TICK_POS fractions - never a
// separate linear-dB formula, which would visibly disagree with the
// printed numbers.
function angleForVu(value) {
  var v = Math.max(VU_MIN, Math.min(VU_MAX, value));
  for (var i = 0; i < VU_TICKS.length - 1; i++) {
    var a = VU_TICKS[i], b = VU_TICKS[i + 1];
    if (v >= a && v <= b) {
      var segT = (v - a) / (b - a);
      var angA = angleForTickIndex(i), angB = angleForTickIndex(i + 1);
      return angA + segT * (angB - angA);
    }
  }
  return angleForTickIndex (v <= VU_TICKS[0] ? 0 : VU_TICKS.length - 1);
}

// Pivot (cx,cy) sits BELOW the visible cream face, near the housing's
// lower bezel - a real VU needle emerges from beneath the glass and
// sweeps a long, shallow arc, it doesn't pivot from the middle of the
// visible scale. r/cy are solved (not guessed) from the actual face
// geometry (#vu-face is 90% width, top-anchored at 51%, native ratio
// 528:243 after the asset's own stray-sliver crop) so the arc's top
// clears the requested 20-25%-down safe line and its left/right
// extremes land at the requested 10%/90%-of-face-width safe columns:
//   faceW = 0.90*187 = 168.3, faceH = faceW/(528/243) = 77.4
//   faceLeft = 93.5 - faceW/2 = 9.35, faceTop = 0.51*113 - faceH/2 = 18.9
//   scale left/right = faceLeft + {0.10,0.90}*faceW = 26.2 / 160.8
//   scale top = faceTop + 0.225*faceH = 36.3
// cx stays the horizontal midpoint of the left/right targets (which is
// also the face's own horizontal center); r and cy solve the top-clears
// and side-reach constraints simultaneously against the +-42deg sweep.
// NOTE: VU_CY was originally solved purely from the face-geometry safe
// -area targets (see comment above) and came out to 137 - but that pivot
// (and the needle's own length/position, which must match it) sits well
// below #vu-assembly's own 113-unit height, and the needle DIV isn't
// clipped by any ancestor, so at 137 the needle visibly speared through
// the "NOVA AUDIO / VELVET" brandline text below the housing (verified
// via screenshot - a real regression, not a guess). #vu-brandline starts
// 19 units below vu-assembly's bottom edge (`bottom:-19px`), so cy must
// stay within vu-assembly's own box with margin. 112 is the practical
// floor: still visibly lower than every earlier version of this scale,
// still reads as "emerging from beneath the bezel" (the housing's own
// opaque lower portion covers the pivot itself, z-index 6 over 3), but
// keeps the needle's own DIV bottom edge safely above the brandline.
var VU_CX = 93.5, VU_CY = 112, VU_R = 75.7;

function renderVuScale() {
  var svg = document.getElementById("vu-scale-svg");
  if (!svg) return;
  svg.setAttribute("viewBox", "0 0 187 113");
  svg.innerHTML = "";
  var cx = VU_CX, cy = VU_CY, r = VU_R;

  function polar(radius, angleDeg) {
    var rad = (Math.PI / 180) * (angleDeg - 90);
    return [cx + radius * Math.cos(rad), cy + radius * Math.sin(rad)];
  }
  function drawLine(p1, p2, stroke, width) {
    var line = document.createElementNS("http://www.w3.org/2000/svg", "line");
    line.setAttribute("x1", p1[0]); line.setAttribute("y1", p1[1]);
    line.setAttribute("x2", p2[0]); line.setAttribute("y2", p2[1]);
    line.setAttribute("stroke", stroke);
    line.setAttribute("stroke-width", String(width));
    line.setAttribute("stroke-linecap", "round");
    svg.appendChild(line);
  }

  // Minor ticks: one at the angular midpoint between every consecutive
  // pair of majors - restrained (short, low-contrast), just enough to
  // read as a manufactured scale without fighting the numbers for room.
  for (var m = 0; m < VU_TICKS.length - 1; m++) {
    var posLo = VU_TICK_POS[String(VU_TICKS[m])], posHi = VU_TICK_POS[String(VU_TICKS[m + 1])];
    var midPos = (posLo + posHi) / 2;
    var midAngle = angleForPos(midPos);
    var minorIsRed = midPos >= VU_TICK_POS["0"];
    drawLine(polar(r, midAngle), polar(r - 4, midAngle),
      minorIsRed ? "rgba(156,47,38,0.6)" : "rgba(74,63,40,0.6)", 0.9);
  }

  // Major ticks + numbers - both derived from the SAME (cx,cy,r) polar
  // geometry as the arc and minor ticks above, so everything converges
  // on one pivot (the success test: rays from every major tick back to
  // (cx,cy) all coincide, because they're literally drawn that way).
  VU_TICKS.forEach(function (t, i) {
    var angleDeg = angleForTickIndex(i);
    var isRed = t >= VU_RED_FROM;
    var color = isRed ? "#b1372c" : "#3a2f18";

    drawLine(polar(r + 1, angleDeg), polar(r - 9, angleDeg), color, 1.6);

    var tp = polar(r - 17, angleDeg);
    var text = document.createElementNS("http://www.w3.org/2000/svg", "text");
    text.setAttribute("x", tp[0]); text.setAttribute("y", tp[1]);
    text.setAttribute("text-anchor", "middle");
    text.setAttribute("dominant-baseline", "middle");
    text.setAttribute("font-size", VU_SMALL_FONT[String(t)] ? "6.6" : "8.2");
    text.setAttribute("font-weight", "700");
    text.setAttribute("font-family", "Arial, sans-serif");
    text.setAttribute("fill", color);
    text.textContent = (t > 0 ? "+" : "") + t;
    svg.appendChild(text);
  });

  var nText = document.createElementNS("http://www.w3.org/2000/svg", "text");
  nText.setAttribute("x", cx - 26); nText.setAttribute("y", 68);
  nText.setAttribute("text-anchor", "middle");
  nText.setAttribute("font-size", "22");
  nText.setAttribute("font-weight", "700");
  nText.setAttribute("fill", "rgba(58,47,24,0.45)");
  nText.textContent = "N";
  svg.appendChild(nText);
}

window.updateTelemetry = function (data) {
  if (!data) return;
  var needle = document.getElementById("vu-needle");
  if (needle) needle.style.transform = "rotate(" + angleForVu(data.outputVu) + "deg)";

  var illum = document.getElementById("vu-illum");
  if (illum) {
    var t = Math.max(0, Math.min(1, (data.outputVu - VU_MIN) / (VU_MAX - VU_MIN)));
    illum.style.opacity = String(0.4 + 0.35 * t);
  }

  recentActivity01 = Math.max(0, Math.min(1, Math.abs(data.grDb || 0) / 12));
  applyTubeVisual();
};

// ---------------------------------------------------------------------
// Bypass - wired to the settings menu (mockup has no dedicated bypass
// jewel; Bypass lives behind the settings/menu buttons instead).
// ---------------------------------------------------------------------
function wireBypass() {
  var state = hasNative() ? getToggleState("bypass") : null;
  function applyVisual() {
    var bypassed = state ? state.getValue() : false;
    var stage = document.getElementById("stage");
    if (stage) stage.classList.toggle("bypassed", bypassed);
  }
  if (state) {
    state.valueChangedEvent.addListener(applyVisual);
    setTimeout(applyVisual, 60);
  } else applyVisual();

  var menuItem = document.getElementById("menu-bypass");
  if (menuItem) {
    menuItem.addEventListener("click", function () {
      if (!state) return;
      state.setValue(!state.getValue());
      applyVisual();
      closeSettingsMenu();
    });
  }
}

function closeSettingsMenu() {
  var menu = document.getElementById("settings-menu");
  if (menu) menu.classList.remove("open");
}

function wireSettingsMenu() {
  var btn = document.getElementById("btn-settings");
  var menuBtn = document.getElementById("btn-menu");
  var menu = document.getElementById("settings-menu");
  if (!menu) return;

  function toggleMenu(anchorEl) {
    var isOpen = menu.classList.contains("open");
    if (isOpen) { closeSettingsMenu(); return; }
    var stage = document.getElementById("stage");
    var stageRect = stage.getBoundingClientRect();
    var anchorRect = anchorEl.getBoundingClientRect();
    var scale = window.__novaVelvetScale || 1;
    menu.style.left = ((anchorRect.left - stageRect.left) / scale) + "px";
    menu.style.top = ((anchorRect.bottom - stageRect.top) / scale + 4) + "px";
    menu.classList.add("open");
  }

  if (btn) btn.addEventListener("click", function (e) { e.stopPropagation(); toggleMenu(btn); });
  if (menuBtn) menuBtn.addEventListener("click", function (e) { e.stopPropagation(); toggleMenu(menuBtn); });
  document.addEventListener("click", function (e) {
    if (menu.classList.contains("open") && !menu.contains(e.target)) closeSettingsMenu();
  });

  var saveAs = document.getElementById("menu-save-as");
  if (saveAs) saveAs.addEventListener("click", function () {
    closeSettingsMenu();
    doSavePreset();
  });
}

// ---------------------------------------------------------------------
// Presets - factory + user (saved to disk natively), browsed via
// prev/next header buttons.
// ---------------------------------------------------------------------
var presetNames = ["Default"];
var presetIndex = 0;

function refreshPresetLabel() {
  var el = document.getElementById("preset-name-text");
  if (el) el.textContent = presetNames[presetIndex] || "Default";
}

function doSavePreset() {
  var name = window.prompt("Save preset as:", "");
  if (!name) return;
  callNative("savePreset", name).then(function () {
    callNative("listPresets").then(function (result) {
      if (Array.isArray(result) && result.length)
        presetNames = result.map(function (p) { return p.name; });
      callNative("getCurrentPresetIndex").then(function (idx) {
        if (typeof idx === "number") presetIndex = idx;
        refreshPresetLabel();
      });
    });
  });
}

function initPresets() {
  callNative("listPresets").then(function (result) {
    if (Array.isArray(result) && result.length) {
      presetNames = result.map(function (p) { return p.name; });
      callNative("getCurrentPresetIndex").then(function (idx) {
        presetIndex = typeof idx === "number" ? idx : 0;
        refreshPresetLabel();
      });
    } else {
      refreshPresetLabel();
    }
  });

  var prevBtn = document.getElementById("btn-prev");
  var nextBtn = document.getElementById("btn-next");
  var saveBtn = document.getElementById("btn-save");

  if (prevBtn) prevBtn.addEventListener("click", function () {
    callNative("loadPreviousPreset").then(function (idx) {
      if (typeof idx === "number") presetIndex = idx;
      refreshPresetLabel();
    });
  });
  if (nextBtn) nextBtn.addEventListener("click", function () {
    callNative("loadNextPreset").then(function (idx) {
      if (typeof idx === "number") presetIndex = idx;
      refreshPresetLabel();
    });
  });
  if (saveBtn) saveBtn.addEventListener("click", doSavePreset);
}

// ---------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------
function fmtPlain(decimals) {
  return function (v) { return decimals > 0 ? v.toFixed(decimals) : String(Math.round(v)); };
}
function fmtSigned(decimals) {
  return function (v) {
    var s = decimals > 0 ? Math.abs(v).toFixed(decimals) : String(Math.round(Math.abs(v)));
    return (v > 0 ? "+" : v < 0 ? "-" : "") + s;
  };
}
// Builds a spec.majors array: [{value, label}], label from formatFn unless
// overridden (used for "OFF"/"MAX" - real automation values that print as
// a word instead of a number, per the locked knob-scale spec).
function majorsFrom(values, formatFn, overrides) {
  return values.map(function (v) {
    var label = (overrides && Object.prototype.hasOwnProperty.call(overrides, v)) ? overrides[v] : formatFn(v);
    return { value: v, label: label };
  });
}

function init() {
  // Locked knob-scale spec (authoritative - see project memory/plan for
  // the full document): every major-tick value/order/log-vs-linear rule
  // and the Threshold reverse-direction convention comes from that spec,
  // not from the AI-generated mockup's own (partly malformed) numbers.
  wireKnob(document.querySelector('[data-param="threshold"]'), {
    name: "threshold",
    spec: {
      min: -40, max: 10, log: false, reverse: true, minorPerMajor: 4,
      majors: majorsFrom([10, -10, -20, -30, -40], fmtSigned(0), { 10: "OFF" }),
    },
  });
  wireKnob(document.querySelector('[data-param="ratio"]'), {
    name: "ratio",
    spec: { min: 2, max: 10, log: false, reverse: false, minorPerMajor: 1, majors: majorsFrom([2, 3, 4, 5, 6, 8, 10], fmtPlain(0)) },
  });
  wireKnob(document.querySelector('[data-param="attack"]'), {
    name: "attack",
    spec: {
      min: 0.5, max: 300, log: true, reverse: false, minorPerMajor: 2,
      majors: majorsFrom([0.5, 1, 3, 10, 30, 100, 300], function (v) { return v < 1 ? v.toFixed(1) : String(Math.round(v)); }),
    },
  });
  wireKnob(document.querySelector('[data-param="release"]'), {
    name: "release",
    spec: {
      min: 0.05, max: 10, log: true, reverse: false, minorPerMajor: 2,
      majors: majorsFrom([0.05, 0.1, 0.3, 1, 3, 10], function (v) { return v < 1 ? v.toFixed(2).replace(/0$/, "") : String(v); }),
    },
  });
  wireKnob(document.querySelector('[data-param="gain"]'), {
    name: "gain",
    spec: { min: 0, max: 30, log: false, reverse: false, minorPerMajor: 4, majors: majorsFrom([0, 10, 20, 30], fmtSigned(0), { 0: "OFF" }) },
  });
  wireKnob(document.querySelector('[data-param="scHpf"]'), {
    name: "sc_hpf",
    spec: { min: 20, max: 400, log: true, reverse: false, minorPerMajor: 2, majors: majorsFrom([20, 50, 100, 200, 400], fmtPlain(0), { 20: "OFF" }) },
  });
  wireKnob(document.querySelector('[data-param="mix"]'), {
    name: "mix",
    spec: { min: 0, max: 100, log: false, reverse: false, minorPerMajor: 4, majors: majorsFrom([0, 25, 50, 75, 100], fmtPlain(0)) },
  });
  wireKnob(document.querySelector('[data-param="output"]'), {
    name: "output",
    spec: { min: -10, max: 10, log: false, reverse: false, minorPerMajor: 4, majors: majorsFrom([-10, -5, 0, 5, 10], fmtSigned(0)) },
  });
  wireKnob(document.querySelector('[data-param="tubeDrive"]'), {
    name: "tube_drive",
    spec: {
      min: 0, max: 100, log: false, reverse: false, minorPerMajor: 4, noNumbers: true,
      majors: majorsFrom([0, 25, 50, 75, 100], fmtPlain(0), { 0: "OFF", 100: "MAX" }),
    },
  });
  wireTubeDrive();

  wireSwitch3(
    document.querySelector('[data-switch="timeSelect"]'), "time_select",
    document.getElementById("timeSelect-image"),
    ["img/time_select_position_top@2x.png", "img/time_select_position_center@2x.png", "img/time_select_position_bottom@2x.png"]
  );
  wireSwitch3(
    document.querySelector('[data-switch="analogMode"]'), "analog_mode",
    document.getElementById("analogMode-image"),
    ["img/analog_mode_position_top@2x.png", "img/analog_mode_position_center@2x.png", "img/analog_mode_position_bottom@2x.png"]
  );
  wireSwitch2(
    document.querySelector('[data-switch="stereoLink"]'), "stereo_link",
    document.getElementById("stereoLink-image"),
    ["img/stereo_link_off@2x.png", "img/stereo_link_on@2x.png"]
  );

  renderVuScale();
  wireBypass();
  wireSettingsMenu();
  initPresets();
}

if (document.readyState === "loading")
  document.addEventListener("DOMContentLoaded", init);
else
  init();
