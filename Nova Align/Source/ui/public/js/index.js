(function () {
  "use strict";

  function hasNative() {
    return typeof window.getNativeFunction === "function";
  }

  function callNative(name) {
    var args = Array.prototype.slice.call(arguments, 1);
    if (!hasNative()) {
      console.warn("Nova Align: native bridge unavailable, ignoring call to " + name);
      return Promise.resolve(null);
    }
    var fn = window.getNativeFunction(name);
    return fn.apply(null, args);
  }

  // ---------------------------------------------------------------------
  // Slider wiring (align_strength / preserve_transients / smoothing /
  // phrase_sensitivity / max_shift_ms) — each backed by a WebSliderRelay of
  // the same param id on the C++ side.
  // ---------------------------------------------------------------------
  function formatSliderValue(paramId, scaledValue) {
    if (paramId === "max_shift_ms") return Math.round(scaledValue) + " ms";
    return Math.round(scaledValue) + "%";
  }

  function wireSlider(row) {
    var paramId = row.getAttribute("data-param");
    var input = row.querySelector(".hslider");
    var valueEl = row.querySelector(".sliderval");
    if (!paramId || !input) return;

    var state = hasNative() ? window.getSliderState(paramId) : null;

    function setFill(scaledValue, min, max) {
      var pct = max > min ? ((scaledValue - min) / (max - min)) * 100 : 0;
      input.style.setProperty("--fill", pct + "%");
    }

    function applyScaled(scaledValue) {
      input.value = String(scaledValue);
      if (valueEl) valueEl.textContent = formatSliderValue(paramId, scaledValue);
      setFill(scaledValue, parseFloat(input.min), parseFloat(input.max));
    }

    if (state) {
      // Reflect backend -> UI whenever the parameter changes (host automation,
      // preset recall, or our own pushes below).
      state.addListener(function () {
        applyScaled(state.getScaledValue());
      });
      // Initial paint once properties/value arrive.
      setTimeout(function () { applyScaled(state.getScaledValue()); }, 60);
    } else {
      applyScaled(parseFloat(input.value));
    }

    input.addEventListener("input", function () {
      var scaled = parseFloat(input.value);
      applyScaled(scaled);
      if (state) {
        var norm = (scaled - state.properties.start) / (state.properties.end - state.properties.start);
        state.setNormalisedValue(norm);
      }
    });
    input.addEventListener("pointerdown", function () { if (state) state.sliderDragStarted(); });
    input.addEventListener("pointerup", function () { if (state) state.sliderDragEnded(); });
  }

  // ---------------------------------------------------------------------
  // Align Mode segmented control — backed by the align_mode choice param.
  // ---------------------------------------------------------------------
  function wireAlignMode() {
    var seg = document.getElementById("alignModeSeg");
    if (!seg) return;
    var buttons = Array.prototype.slice.call(seg.querySelectorAll("button"));
    var state = hasNative() ? window.getSliderState("align_mode") : null;
    var numModes = 3;

    function setActive(index) {
      buttons.forEach(function (b) {
        b.classList.toggle("active", parseInt(b.getAttribute("data-mode"), 10) === index);
      });
    }

    if (state) {
      state.addListener(function () {
        var index = Math.round(state.getNormalisedValue() * (numModes - 1));
        setActive(index);
      });
      setTimeout(function () {
        setActive(Math.round(state.getNormalisedValue() * (numModes - 1)));
      }, 60);
    }

    buttons.forEach(function (b) {
      b.addEventListener("click", function () {
        var index = parseInt(b.getAttribute("data-mode"), 10);
        setActive(index);
        if (state) state.setNormalisedValue(index / (numModes - 1));
      });
    });
  }

  // ---------------------------------------------------------------------
  // Placeholder waveform rendering — a real peak-extracted waveform is
  // Phase 3 work; for now a loaded lane gets a deterministic decorative bar
  // pattern (seeded by filename, so it isn't just static/flat) so the UI
  // reads clearly as "loaded" vs "empty" per the mockup.
  // ---------------------------------------------------------------------
  function seededRandom(seedStr) {
    var seed = 0;
    for (var i = 0; i < seedStr.length; i++) seed = (seed * 31 + seedStr.charCodeAt(i)) >>> 0;
    return function () {
      seed = (seed * 1664525 + 1013904223) >>> 0;
      return seed / 4294967296;
    };
  }

  function drawWaveform(laneId, color, seedText) {
    var lane = document.getElementById(laneId);
    if (!lane) return;
    var canvas = lane.querySelector("canvas");
    var empty = lane.querySelector(".waveempty");
    if (!canvas) return;

    var dpr = window.devicePixelRatio || 1;
    var w = lane.clientWidth, h = lane.clientHeight;
    canvas.width = w * dpr;
    canvas.height = h * dpr;
    var ctx = canvas.getContext("2d");
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, w, h);

    if (!seedText) {
      if (empty) empty.style.display = "block";
      return;
    }
    if (empty) empty.style.display = "none";

    var rand = seededRandom(seedText);
    var bars = Math.max(24, Math.floor(w / 6));
    var barW = w / bars;
    var midY = h / 2;

    ctx.fillStyle = color;
    for (var i = 0; i < bars; i++) {
      var envelope = 0.35 + 0.65 * Math.sin((i / bars) * Math.PI * 3.1 + rand() * 2) * 0.5 + 0.5 * rand() * 0.5;
      var amp = Math.max(0.06, Math.min(1, envelope)) * (h * 0.42);
      var x = i * barW + barW * 0.18;
      var bw = barW * 0.64;
      ctx.globalAlpha = 0.9;
      ctx.fillRect(x, midY - amp, bw, amp * 2);
    }
    ctx.globalAlpha = 1;
    ctx.strokeStyle = "rgba(255,255,255,0.06)";
    ctx.beginPath();
    ctx.moveTo(0, midY);
    ctx.lineTo(w, midY);
    ctx.stroke();
  }

  // ---------------------------------------------------------------------
  // File loading (Guide / Target), Align, Render, Preview
  // ---------------------------------------------------------------------
  var lastGuideName = "";
  var lastTargetName = "";
  var hasAlignedResult = false;
  var statusPollHandle = null;
  var previewingWhich = null; // "guide" | "target" | "aligned" | null

  function setStatus(text, kind) {
    var el = document.getElementById("statusLine");
    if (!el) return;
    el.textContent = text;
    el.className = "statusline" + (kind ? " " + kind : "");
  }

  function refreshRenderButtonState() {
    var renderBtn = document.getElementById("renderBtn");
    if (renderBtn) renderBtn.disabled = !hasAlignedResult;
  }

  function setBusyVisual(isBusy) {
    var alignBtn = document.getElementById("alignBtn");
    if (alignBtn) alignBtn.classList.toggle("busy", !!isBusy);
  }

  function pollAlignStatus() {
    if (statusPollHandle) return;
    statusPollHandle = setInterval(function () {
      callNative("getAlignStatus").then(function (status) {
        if (!status) return;
        setBusyVisual(status.busy);
        if (status.busy) {
          setStatus("Aligning …");
          return;
        }
        clearInterval(statusPollHandle);
        statusPollHandle = null;
        hasAlignedResult = !!status.hasAligned;
        refreshRenderButtonState();
        if (hasAlignedResult) {
          setStatus("Aligned. Hit play in your DAW to hear it, or Render to save a file.", "ok");
          drawWaveform("waveAligned", "#d8d8de", "aligned:" + lastTargetName + ":" + Date.now());
        } else {
          setStatus("Alignment failed — check that Guide and Target share the same sample rate.", "error");
        }
      });
    }, 400);
  }

  function applyGuideLoaded(result) {
    lastGuideName = result.fileName || "";
    var sub = document.getElementById("guideSub");
    if (sub) sub.textContent = lastGuideName;
    drawWaveform("waveGuide", "#d9ae52", lastGuideName);
    setStatus("Guide loaded: " + lastGuideName);
  }

  function applyTargetLoaded(result) {
    lastTargetName = result.fileName || "";
    var sub = document.getElementById("targetSub");
    if (sub) sub.textContent = lastTargetName;
    drawWaveform("waveTarget", "#4a90e2", lastTargetName);
    setStatus("Target loaded: " + lastTargetName);
  }

  // ---------------------------------------------------------------------
  // Capture (VocALign-style, non-ARA): arm a slot, press play in the DAW,
  // and it records whatever's arriving at that slot's input bus (Guide =
  // sidechain, Target = the plugin's main input) until you stop it.
  // ---------------------------------------------------------------------
  var capturePollHandle = null;

  function formatSeconds(s) {
    var total = Math.floor(s || 0);
    var m = Math.floor(total / 60);
    var sec = total % 60;
    return m + ":" + (sec < 10 ? "0" : "") + sec;
  }

  function setRecButtonState(which, state, seconds) {
    var btn = document.getElementById(which === "guide" ? "recGuideBtn" : "recTargetBtn");
    if (!btn) return;
    btn.classList.toggle("armed", state === "armed");
    btn.classList.toggle("capturing", state === "capturing");
    if (state === "armed") btn.innerHTML = "&#9679; ARMED";
    else if (state === "capturing") btn.innerHTML = "&#9679; " + formatSeconds(seconds);
    else btn.innerHTML = "&#9679; REC";
  }

  function stopCapturePolling() {
    if (capturePollHandle) {
      clearInterval(capturePollHandle);
      capturePollHandle = null;
    }
  }

  function pollCaptureStatus() {
    if (capturePollHandle) return;
    capturePollHandle = setInterval(function () {
      callNative("getCaptureStatus").then(function (status) {
        if (!status) return;
        if (status.armedSlot === "none") {
          setRecButtonState("guide", "idle");
          setRecButtonState("target", "idle");
          stopCapturePolling();
          return;
        }
        var otherSlot = status.armedSlot === "guide" ? "target" : "guide";
        setRecButtonState(otherSlot, "idle");
        if (status.capturing) {
          setRecButtonState(status.armedSlot, "capturing", status.capturedSeconds);
          setStatus("Capturing " + status.armedSlot + " — " + formatSeconds(status.capturedSeconds) + ". Click REC again to stop.");
        } else {
          setRecButtonState(status.armedSlot, "armed");
          setStatus("Armed to capture " + status.armedSlot + " — press play in your DAW.");
        }
      });
    }, 300);
  }

  function onRecToggle(which) {
    var btn = document.getElementById(which === "guide" ? "recGuideBtn" : "recTargetBtn");
    var isActive = btn && (btn.classList.contains("armed") || btn.classList.contains("capturing"));

    if (isActive) {
      callNative("stopCapture").then(function (result) {
        setRecButtonState("guide", "idle");
        setRecButtonState("target", "idle");
        stopCapturePolling();
        if (result && result.success) {
          if (result.guideCaptured) applyGuideLoaded({ fileName: result.guideName });
          if (result.targetCaptured) applyTargetLoaded({ fileName: result.targetName });
          if (result.guideCaptured && result.targetCaptured)
            setStatus("Captured guide and target in one pass. Press ALIGN.", "ok");
          else if (result.targetCaptured && lastGuideName)
            setStatus("Captured target. Press ALIGN.", "ok");
          else if (result.targetCaptured)
            setStatus("Captured target. No guide linked - set Nova Align on the guide track to GUIDE, or load a guide file.", "ok");
          else
            setStatus("Captured guide.", "ok");
        } else {
          setStatus((result && result.error) || "Capture stopped — nothing was captured.", "error");
        }
      });
      return;
    }

    var armFn = which === "guide" ? "armCaptureGuide" : "armCaptureTarget";
    callNative(armFn).then(function () {
      setRecButtonState(which, "armed", 0);
      pollCaptureStatus();
      setStatus("Armed to capture " + which + " — press play in your DAW.");
    });
  }

  function onSetGuide() {
    callNative("loadGuideFile").then(function (result) {
      if (result && result.success) applyGuideLoaded(result);
    });
  }

  function onSetTarget() {
    callNative("loadTargetFile").then(function (result) {
      if (result && result.success) applyTargetLoaded(result);
    });
  }

  // ---------------------------------------------------------------------
  // Drag-and-drop: drop an audio file (from Finder, or a DAW clip that
  // exposes itself as a real OS file drag) straight onto a lane. We read the
  // file's bytes in JS (no reliance on the WebView exposing a real
  // filesystem path) and hand them to native code as base64, which writes a
  // temp file and validates it's real audio before accepting it.
  // ---------------------------------------------------------------------
  function arrayBufferToBase64(buffer) {
    var bytes = new Uint8Array(buffer);
    var binary = "";
    var chunkSize = 0x8000;
    for (var i = 0; i < bytes.length; i += chunkSize) {
      var chunk = bytes.subarray(i, i + chunkSize);
      binary += String.fromCharCode.apply(null, chunk);
    }
    return btoa(binary);
  }

  function wireDropZone(elId, nativeFnName, onLoaded) {
    var el = document.getElementById(elId);
    if (!el) return;

    el.addEventListener("dragover", function (e) {
      e.preventDefault();
      if (e.dataTransfer) e.dataTransfer.dropEffect = "copy";
      el.classList.add("dragover");
    });
    el.addEventListener("dragenter", function (e) {
      e.preventDefault();
      el.classList.add("dragover");
    });
    el.addEventListener("dragleave", function () {
      el.classList.remove("dragover");
    });
    el.addEventListener("drop", function (e) {
      e.preventDefault();
      el.classList.remove("dragover");

      var files = e.dataTransfer && e.dataTransfer.files;
      if (!files || files.length === 0) {
        setStatus("That didn't look like a file drop — try dragging from Finder or the browser instead.", "error");
        return;
      }

      var file = files[0];
      setStatus("Loading " + file.name + " …");

      file.arrayBuffer().then(function (buf) {
        var base64 = arrayBufferToBase64(buf);
        return callNative(nativeFnName, base64, file.name);
      }).then(function (result) {
        if (result && result.success) onLoaded(result);
        else setStatus((result && result.error) || "Could not load dropped file.", "error");
      }).catch(function (err) {
        setStatus("Could not read dropped file.", "error");
        console.warn(err);
      });
    });
  }

  function onAlign() {
    if (!lastGuideName || !lastTargetName) {
      setStatus("Load both Guide and Target first.", "error");
      return;
    }
    setStatus("Aligning …");
    setBusyVisual(true);
    callNative("runAlign").then(function () {
      pollAlignStatus();
    });
  }

  function onRender() {
    if (!hasAlignedResult) return;
    callNative("renderAlignedAs").then(function (result) {
      if (result && result.success) {
        setStatus("Rendered to " + result.path, "ok");
      } else if (result && result.error !== "Cancelled") {
        setStatus(result.error || "Render failed.", "error");
      }
    });
  }

  function setPreviewButtonPlaying(isPlaying) {
    var btn = document.getElementById("previewBtn");
    if (!btn) return;
    btn.classList.toggle("playing", isPlaying);
    btn.innerHTML = isPlaying ? "&#9632; STOP" : "&#9654; PREVIEW";
  }

  function onPreviewToggle() {
    if (previewingWhich) {
      callNative("previewStop").then(function () {
        previewingWhich = null;
        setPreviewButtonPlaying(false);
      });
      return;
    }

    var which = hasAlignedResult ? "aligned" : (lastTargetName ? "target" : (lastGuideName ? "guide" : null));
    if (!which) {
      setStatus("Nothing loaded to preview yet.", "error");
      return;
    }

    callNative("previewPlay", which).then(function (result) {
      if (result && result.success) {
        previewingWhich = which;
        setPreviewButtonPlaying(true);
      } else {
        setStatus((result && result.error) || "Preview failed.", "error");
      }
    });
  }

  // ---------------------------------------------------------------------
  // Linked instances: TARGET / GUIDE role and link letter
  // ---------------------------------------------------------------------
  var GROUP_LETTERS = ["A", "B", "C", "D"];
  var currentRole = "target";

  function applyLinkStatus(s) {
    if (!s) return;
    currentRole = s.role;
    var app = document.querySelector(".app");
    if (app) app.classList.toggle("guidemode", s.role === "guide");

    Array.prototype.forEach.call(document.querySelectorAll("#roleSeg button"), function (b) {
      b.classList.toggle("active", b.getAttribute("data-role") === s.role);
    });
    Array.prototype.forEach.call(document.querySelectorAll("#groupSeg button"), function (b) {
      b.classList.toggle("active", Number(b.getAttribute("data-group")) === s.group);
    });

    var letter = GROUP_LETTERS[s.group] || "A";
    var box = document.getElementById("linkStatus");
    var text = document.getElementById("linkStatusText");
    var cls = "linkstatus";
    var msg;

    if (s.role === "guide") {
      var others = s.targets;
      if (s.guides > 1) msg = "Another track is also GUIDE on link " + letter + " - pick a different letter";
      else if (s.guideRecording) { msg = "Recording guide for " + others + " target" + (others === 1 ? "" : "s"); cls += " rec"; }
      else if (others > 0) { msg = "Linked to " + others + " target" + (others === 1 ? "" : "s") + " on link " + letter; cls += " ok"; }
      else msg = "No target on link " + letter + " yet";

      var badge = document.getElementById("guideBadge");
      if (badge) badge.textContent = "GUIDE \u00b7 LINK " + letter;
      var state = document.getElementById("guideState");
      if (state) {
        state.className = "guidestate" + (s.guideRecording ? " rec" : "");
        state.textContent = s.guideRecording
          ? "\u25cf Recording guide \u2014 " + formatSeconds(s.guideSeconds)
          : (others > 0 ? "Ready \u2014 linked to " + others + " target" + (others === 1 ? "" : "s") : "Waiting for a target on link " + letter);
      }
    } else {
      if (s.guides > 0) {
        msg = "Guide linked" + (s.guideTrackName ? ": " + s.guideTrackName : "") + " \u2014 REC captures both tracks";
        cls += s.guideRecording ? " rec" : " ok";
      } else {
        msg = "No guide linked \u2014 put Nova Align on your guide track and set it to GUIDE (link " + letter + ")";
      }
    }

    if (box) box.className = cls;
    if (text) text.textContent = msg;
  }

  function pollLink() {
    callNative("getLinkStatus").then(applyLinkStatus);
  }

  function wireLinkBar() {
    Array.prototype.forEach.call(document.querySelectorAll("#roleSeg button"), function (b) {
      b.addEventListener("click", function () {
        callNative("setLinkRole", b.getAttribute("data-role")).then(applyLinkStatus);
      });
    });
    Array.prototype.forEach.call(document.querySelectorAll("#groupSeg button"), function (b) {
      b.addEventListener("click", function () {
        callNative("setLinkGroup", Number(b.getAttribute("data-group"))).then(applyLinkStatus);
      });
    });
    pollLink();
    setInterval(pollLink, 500);
  }

  function wireAdvancedToggle() {
    var btn = document.getElementById("advancedBtn");
    var panel = document.getElementById("advancedPanel");
    if (!btn || !panel) return;
    btn.addEventListener("click", function () {
      var isHidden = panel.hasAttribute("hidden");
      if (isHidden) panel.removeAttribute("hidden");
      else panel.setAttribute("hidden", "");
      btn.innerHTML = isHidden ? "ADVANCED &#8963;" : "ADVANCED &#8964;";
    });
  }

  function init() {
    Array.prototype.forEach.call(document.querySelectorAll(".sliderrow"), wireSlider);
    wireAlignMode();
    wireAdvancedToggle();
    wireLinkBar();

    var setGuideBtn = document.getElementById("setGuideBtn");
    var setTargetBtn = document.getElementById("setTargetBtn");
    var recGuideBtn = document.getElementById("recGuideBtn");
    var recTargetBtn = document.getElementById("recTargetBtn");
    var alignBtn = document.getElementById("alignBtn");
    var renderBtn = document.getElementById("renderBtn");
    var previewBtn = document.getElementById("previewBtn");

    if (setGuideBtn) setGuideBtn.addEventListener("click", onSetGuide);
    if (setTargetBtn) setTargetBtn.addEventListener("click", onSetTarget);
    if (recGuideBtn) recGuideBtn.addEventListener("click", function () { onRecToggle("guide"); });
    if (recTargetBtn) recTargetBtn.addEventListener("click", function () { onRecToggle("target"); });
    if (alignBtn) alignBtn.addEventListener("click", onAlign);
    if (renderBtn) renderBtn.addEventListener("click", onRender);
    if (previewBtn) previewBtn.addEventListener("click", onPreviewToggle);

    wireDropZone("waveGuide", "dropGuideFile", applyGuideLoaded);
    wireDropZone("waveTarget", "dropTargetFile", applyTargetLoaded);

    refreshRenderButtonState();
    drawWaveform("waveGuide", "#d9ae52", null);
    drawWaveform("waveTarget", "#4a90e2", null);
    drawWaveform("waveAligned", "#d8d8de", null);

    // Restore state on reopen (project already had Guide/Target loaded).
    callNative("getAlignStatus").then(function (status) {
      if (!status) return;
      if (status.guideLoaded) {
        lastGuideName = status.guideName || "";
        document.getElementById("guideSub").textContent = lastGuideName;
        drawWaveform("waveGuide", "#d9ae52", lastGuideName);
      }
      if (status.targetLoaded) {
        lastTargetName = status.targetName || "";
        document.getElementById("targetSub").textContent = lastTargetName;
        drawWaveform("waveTarget", "#4a90e2", lastTargetName);
      }
      hasAlignedResult = !!status.hasAligned;
      refreshRenderButtonState();
      if (status.busy) {
        setBusyVisual(true);
        pollAlignStatus();
      } else if (hasAlignedResult) {
        setStatus("Aligned. Hit play in your DAW to hear it, or Render to save a file.", "ok");
        drawWaveform("waveAligned", "#d8d8de", "aligned:" + lastTargetName);
      }
    });

    window.addEventListener("resize", function () {
      drawWaveform("waveGuide", "#d9ae52", lastGuideName || null);
      drawWaveform("waveTarget", "#4a90e2", lastTargetName || null);
      drawWaveform("waveAligned", "#d8d8de", hasAlignedResult ? ("aligned:" + lastTargetName) : null);
    });
  }

  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", init);
  else
    init();
})();
