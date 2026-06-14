/**
 * Nova Chord — Main UI Script
 * Vanilla JS, no framework. Communicates with JUCE via:
 *   - WebSliderRelay parameters (key, scale, style) — managed automatically by JUCE's JS
 *   - getNativeFunction() for imperative calls (triggerChord, addToProgression, etc.)
 *   - window.receiveDSP() pushed from C++ timer at 30 Hz
 *
 * Depends on: js/juce/check_native_interop.js, js/juce/index.js, js/fallback-boot.js
 * (all loaded as plain <script> tags before this file)
 */

(function () {
  'use strict';

  // ── Native function bindings ───────────────────────────────────────────────
  // These are bound lazily on first use so that the JUCE backend has time to
  // inject __JUCE__ into the page.
  var _nativeTriggerChord          = null;
  var _nativeTriggerProgChord      = null;
  var _nativeAddToProgression      = null;
  var _nativeRemoveFromProgression = null;
  var _nativeClearProgression      = null;
  var _nativeSetApiKey             = null;
  var _nativeGenerateFromPrompt    = null;
  var _nativeExportMidi            = null;

  function getNative(name) {
    return (typeof getNativeFunction === 'function') ? getNativeFunction(name) : function () { return Promise.resolve(null); };
  }

  function ensureNatives() {
    if (!_nativeTriggerChord)          _nativeTriggerChord          = getNative('triggerChord');
    if (!_nativeTriggerProgChord)      _nativeTriggerProgChord      = getNative('triggerProgressionChord');
    if (!_nativeAddToProgression)      _nativeAddToProgression      = getNative('addToProgression');
    if (!_nativeRemoveFromProgression) _nativeRemoveFromProgression = getNative('removeFromProgression');
    if (!_nativeClearProgression)      _nativeClearProgression      = getNative('clearProgression');
    if (!_nativeSetApiKey)             _nativeSetApiKey             = getNative('setApiKey');
    if (!_nativeGenerateFromPrompt)    _nativeGenerateFromPrompt    = getNative('generateFromPrompt');
    if (!_nativeExportMidi)            _nativeExportMidi            = getNative('exportMidi');
  }

  // ── State ──────────────────────────────────────────────────────────────────
  var currentSuggestions = [];
  var currentProgression = [];
  var playingIndex     = -1;
  var playingProgIndex = -1;

  // ── DOM refs ───────────────────────────────────────────────────────────────
  var keySelect       = document.getElementById('key-select');
  var scaleSelect     = document.getElementById('scale-select');
  var styleSelect     = document.getElementById('style-select');
  var detectDot       = document.getElementById('detect-dot');
  var detectedNote    = document.getElementById('detected-note');
  var suggestionsGrid = document.getElementById('suggestions-grid');
  var progressionList = document.getElementById('progression-list');
  var progEmpty       = document.getElementById('prog-empty');
  var promptInput     = document.getElementById('prompt-input');
  var generateBtn     = document.getElementById('generate-btn');
  var apiKeyInput     = document.getElementById('api-key-input');
  var aiStatus        = document.getElementById('ai-status');
  var clearBtn        = document.getElementById('clear-btn');
  var exportBtn       = document.getElementById('export-btn');

  // ── Parameter selects — sync with JUCE WebSliderRelay ─────────────────────
  function bindSlider(id, el) {
    if (!window.__JUCE__ || !window.__JUCE__.sliderState) return;
    var state = window.__JUCE__.sliderState[id];
    if (!state) return;
    // Sync UI from JUCE
    state.valueChangedEvent.addListener(function () {
      el.value = Math.round(state.getValue());
    });
    // Sync JUCE from UI
    el.addEventListener('change', function () {
      state.setValue(parseFloat(el.value));
    });
  }

  function bindAllSliders() {
    bindSlider('key',   keySelect);
    bindSlider('scale', scaleSelect);
    bindSlider('style', styleSelect);
  }

  // Try immediately and again after JUCE has had time to inject
  bindAllSliders();
  setTimeout(bindAllSliders, 300);
  setTimeout(bindAllSliders, 1000);

  // Fallback: plain change listeners that call native function directly
  keySelect.addEventListener('change', function () {
    if (!window.__JUCE__) return; // handled by slider binding above
  });

  // ── Suggestion rendering ───────────────────────────────────────────────────
  function renderSuggestions(suggestions) {
    currentSuggestions = suggestions || [];
    suggestionsGrid.innerHTML = '';

    var count = currentSuggestions.length || 6;

    for (var i = 0; i < 6; i++) {
      var chord = currentSuggestions[i];
      var btn = document.createElement('div');
      btn.className = 'chord-btn';
      if (i === playingIndex) btn.classList.add('playing');

      if (chord) {
        btn.innerHTML =
          '<span class="chord-name">' + escHtml(chord.name) + '</span>' +
          '<div class="chord-actions">' +
          '  <span class="chord-action-btn" data-action="play" data-idx="' + i + '">&#9654; Play</span>' +
          '  <span class="chord-action-btn" data-action="add"  data-idx="' + i + '">+ Add</span>' +
          '</div>';

        (function (idx) {
          btn.addEventListener('click', function (e) {
            var action = e.target.dataset && e.target.dataset.action;
            if (action === 'add') {
              doAddToProgression(idx);
            } else {
              doPlayChord(idx);
            }
          });
        })(i);
      } else {
        btn.innerHTML = '<span class="chord-name" style="color:var(--text-dim)">—</span>';
      }

      suggestionsGrid.appendChild(btn);
    }
  }

  function doPlayChord(index) {
    ensureNatives();
    _nativeTriggerChord(index);
    flashSuggestion(index);
  }

  function doAddToProgression(index) {
    if (currentProgression.length >= 8) return;
    ensureNatives();
    _nativeAddToProgression(index);
  }

  function flashSuggestion(index) {
    playingIndex = index;
    renderSuggestions(currentSuggestions);
    setTimeout(function () {
      playingIndex = -1;
      renderSuggestions(currentSuggestions);
    }, 600);
  }

  // ── Progression rendering ──────────────────────────────────────────────────
  function renderProgression(progression) {
    currentProgression = progression || [];
    progressionList.innerHTML = '';

    if (currentProgression.length === 0) {
      progressionList.appendChild(progEmpty);
      return;
    }

    for (var i = 0; i < currentProgression.length; i++) {
      var chord = currentProgression[i];
      var item = document.createElement('div');
      item.className = 'prog-item';
      if (i === playingProgIndex) item.classList.add('playing');

      item.innerHTML =
        '<span class="prog-num">' + (i + 1) + '</span>' +
        '<span class="prog-name">' + escHtml(chord.name) + '</span>' +
        '<span class="prog-remove" data-idx="' + i + '" title="Remove">&times;</span>';

      (function (idx) {
        item.addEventListener('click', function (e) {
          if (e.target.classList.contains('prog-remove')) {
            doRemoveFromProgression(parseInt(e.target.dataset.idx));
          } else {
            doPlayProgressionChord(idx);
          }
        });
      })(i);

      progressionList.appendChild(item);
    }
  }

  function doPlayProgressionChord(index) {
    ensureNatives();
    _nativeTriggerProgChord(index);
    flashProgression(index);
  }

  function doRemoveFromProgression(index) {
    ensureNatives();
    _nativeRemoveFromProgression(index);
  }

  function flashProgression(index) {
    playingProgIndex = index;
    renderProgression(currentProgression);
    setTimeout(function () {
      playingProgIndex = -1;
      renderProgression(currentProgression);
    }, 600);
  }

  // ── Progression actions ────────────────────────────────────────────────────
  clearBtn.addEventListener('click', function () {
    ensureNatives();
    _nativeClearProgression();
  });

  exportBtn.addEventListener('click', function () {
    ensureNatives();
    _nativeExportMidi();
  });

  // ── AI generation ──────────────────────────────────────────────────────────
  apiKeyInput.addEventListener('blur', function () {
    var key = apiKeyInput.value.trim();
    if (key) {
      ensureNatives();
      _nativeSetApiKey(key);
    }
  });

  generateBtn.addEventListener('click', function () {
    var prompt = promptInput.value.trim();
    if (!prompt) {
      setStatus('Please enter a style prompt first.', 'warn');
      return;
    }
    var key = apiKeyInput.value.trim();
    if (!key) {
      setStatus('Please enter your Anthropic API key.', 'warn');
      return;
    }

    ensureNatives();
    _nativeSetApiKey(key);
    setStatus('Generating progression…', 'info');
    generateBtn.disabled = true;

    _nativeGenerateFromPrompt(prompt);

    // Re-enable after timeout; progression update arrives via receiveDSP
    setTimeout(function () {
      generateBtn.disabled = false;
      setStatus('Done — check progression below.', 'ok');
    }, 10000);
  });

  function setStatus(msg, type) {
    aiStatus.textContent = msg;
    aiStatus.style.color = type === 'ok'   ? 'var(--green)'
                         : type === 'warn' ? 'var(--yellow)'
                         :                   'var(--text-dim)';
  }

  // ── DSP receive (pushed from C++ at 30 Hz) ────────────────────────────────
  window.receiveDSP = function (data) {
    // Detection indicator
    if (data.hasInput) {
      detectDot.classList.add('active');
      detectedNote.textContent = data.rootName || '—';
    } else {
      detectDot.classList.remove('active');
      detectedNote.textContent = '—';
    }

    if (data.suggestions) renderSuggestions(data.suggestions);
    if (data.progression)  renderProgression(data.progression);
  };

  // ── Utility ───────────────────────────────────────────────────────────────
  function escHtml(str) {
    return String(str)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  // ── Initial render ────────────────────────────────────────────────────────
  renderSuggestions([]);
  renderProgression([]);

})();
