// Launcher UI logic. Loads the config JSON from the backend, renders the four
// tabs from window.TABS, writes edits back into `config`, and saves the whole
// object (preserving the kbd/gamepad keymap sections the UI does not yet edit).
const { invoke } = window.__TAURI__.core;

let config = {};        // the full config object (incl. kbd/gamepad)
let activeTab = "General";

// Assets-tab state
let baseRom = null;                 // absolute path to the base US ROM
let languageRoms = [];              // [{ path, lang }]

const statusEl = () => document.getElementById("status");

let statusTimer = 0;

// Show a footer status message; with `clearAfterMs`, it disappears on its own
// (any newer message cancels the pending clear).
function setStatus(msg, kind, clearAfterMs) {
  clearTimeout(statusTimer);
  if (clearAfterMs) statusTimer = setTimeout(() => setStatus(""), clearAfterMs);
  const el = statusEl();
  el.textContent = msg || "";
  el.className = "status" + (kind ? " " + kind : "");
}

// --- rendering --------------------------------------------------------------

function makeRow(label, control) {
  const row = document.createElement("div");
  row.className = "row";
  const l = document.createElement("label");
  l.className = "field-label";
  l.textContent = label;
  row.appendChild(l);
  row.appendChild(control);
  return row;
}

// key -> function that re-reads config into its rendered <select>, so code can
// change a dropdown's value (see setSelect).
let selectRefreshers = {};

function setSelect(key, value) {
  config[key] = value;
  selectRefreshers[key]?.();
}

// enable_msu: 0 off, 1 PCM, 3 PCM Deluxe, 5 Opuz, 7 Opuz Deluxe (bit 4 = Opuz,
// bit 2 = Deluxe). PCM tracks are 44.1 kHz and OPUZ 48 kHz, so the output rate
// follows the MSU mode.
const MSU_OPUZ = 4, MSU_DELUXE = 2;

function matchAudioFreqToMsu() {
  const mode = config.enable_msu;
  if (mode) setSelect("audio_freq", mode & MSU_OPUZ ? 48000 : 44100);
}

// After encoding: use the Opuz variant of the current MSU mode (Deluxe kept).
function switchMsuToOpuz() {
  setSelect("enable_msu", 1 | MSU_OPUZ | (config.enable_msu & MSU_DELUXE));
  matchAudioFreqToMsu();
}

function renderField(field) {
  if (field.type === "section") {
    const h = document.createElement("h3");
    h.className = "section-h";
    h.textContent = field.label;
    return h;
  }
  if (field.type === "msu") return renderMsuField();

  let control;
  if (field.type === "bool") {
    control = document.createElement("input");
    control.type = "checkbox";
    control.checked = !!config[field.key];
    control.addEventListener("change", () => { config[field.key] = control.checked; });
  } else if (field.type === "flag") {
    control = document.createElement("input");
    control.type = "checkbox";
    control.checked = (config[field.key] & field.bit) !== 0;
    control.addEventListener("change", () => {
      if (control.checked) config[field.key] |= field.bit;
      else config[field.key] &= ~field.bit;
      config[field.key] >>>= 0;   // keep unsigned 32-bit
    });
  } else if (field.type === "int") {
    control = document.createElement("input");
    control.type = "number";
    control.min = "0";
    control.value = config[field.key] ?? 0;
    control.addEventListener("input", () => { config[field.key] = parseInt(control.value || "0", 10); });
  } else if (field.type === "text") {
    control = document.createElement("input");
    control.type = "text";
    control.value = config[field.key] ?? "";
    if (field.placeholder) control.placeholder = field.placeholder;
    control.addEventListener("input", () => { config[field.key] = control.value; });
  } else if (field.type === "file") {
    control = renderFileControl(field);
  } else if (field.type === "select") {
    control = document.createElement("select");
    for (const [value, text] of field.options) {
      const opt = document.createElement("option");
      opt.value = String(value);
      opt.textContent = text;
      control.appendChild(opt);
    }
    const show = () => { control.value = String(config[field.key] ?? field.options[0][0]); };
    show();
    selectRefreshers[field.key] = show;
    control.addEventListener("change", () => {
      config[field.key] = parseInt(control.value, 10);
      if (field.key === "enable_msu") matchAudioFreqToMsu();
    });
  }
  return makeRow(field.label, control);
}

// Text input + Browse… button for a path-valued key. `field.filter` is
// { name, extensions } for the native file dialog.
function renderFileControl(field) {
  const wrap = document.createElement("div");
  wrap.className = "file-control";
  const input = document.createElement("input");
  input.type = "text";
  const show = (value) => {
    input.value = value;
    input.title = value;
    input.scrollLeft = input.scrollWidth;   // keep the filename end in view
  };
  show(config[field.key] ?? "");
  input.addEventListener("input", () => { config[field.key] = input.value; input.title = input.value; });

  const btn = document.createElement("button");
  btn.className = "btn";
  btn.textContent = "Browse…";
  btn.addEventListener("click", async () => {
    const path = await invoke("pick_file", {
      filterName: field.filter.name,
      extensions: field.filter.extensions,
    });
    if (!path) return;
    config[field.key] = path;
    show(path);
  });

  wrap.append(input, btn);
  return wrap;
}

function pickRom() {
  return invoke("pick_file", { filterName: "SNES ROM", extensions: ["sfc", "smc", "fig", "bin"] });
}

function renderTabs() {
  const tabsEl = document.getElementById("tabs");
  const panelsEl = document.getElementById("panels");
  tabsEl.innerHTML = "";
  panelsEl.innerHTML = "";
  selectRefreshers = {};

  // General (asset management) first, then the schema tabs, then input tabs.
  const tabNames = ["General", ...Object.keys(window.TABS), "Keyboard", "Gamepad"];
  for (const name of tabNames) {
    const btn = document.createElement("button");
    btn.className = "tab" + (name === activeTab ? " active" : "");
    btn.textContent = name;
    btn.addEventListener("click", () => selectTab(name));
    tabsEl.appendChild(btn);

    const panel = document.createElement("div");
    panel.className = "panel" + (name === activeTab ? " active" : "");
    panel.dataset.tab = name;
    // Attach before rendering: the General/Sound renderers look up their
    // elements by id.
    panelsEl.appendChild(panel);
    if (name === "General") {
      renderGeneralPanel(panel);
    } else if (name === "Keyboard") {
      renderKeymapPanel(panel, "kbd");
    } else if (name === "Gamepad") {
      renderKeymapPanel(panel, "gamepad");
    } else {
      for (const field of window.TABS[name]) panel.appendChild(renderField(field));
    }
  }

  // Sliding underline under the active tab. Placed once without a transition,
  // then enabled so later tab switches animate.
  const indicator = document.createElement("div");
  indicator.className = "tab-indicator";
  tabsEl.appendChild(indicator);
  moveIndicator();
  void indicator.offsetWidth;   // commit the initial position before enabling transitions
  indicator.classList.add("animated");
}

function moveIndicator() {
  const active = document.querySelector(".tab.active");
  const indicator = document.querySelector(".tab-indicator");
  if (!active || !indicator) return;
  indicator.style.width = active.offsetWidth + "px";
  indicator.style.transform = `translateX(${active.offsetLeft}px)`;
}

// --- Keyboard / Gamepad tabs ------------------------------------------------

let capturing = null;   // active capture state, to cancel/replace

function renderKeymapPanel(panel, device) {
  const dev = config[device] || (config[device] = {});
  const hint = document.createElement("div");
  hint.className = "assets-status";
  hint.textContent = device === "kbd"
    ? "Click a binding, then press a key. Right-click to clear."
    : "Click a binding, then press a button/stick on your controller. Right-click to clear.";
  panel.appendChild(hint);

  for (const group of window.KEYMAP_GROUPS) {
    const h = document.createElement("h3");
    h.className = "section-h";
    h.textContent = group.title;
    panel.appendChild(h);

    if (group.kind === "array") {
      const arr = dev[group.key] || (dev[group.key] = []);
      group.labels.forEach((label, i) => {
        panel.appendChild(bindingRow(device, label,
          () => arr[i] || "",
          (v) => { arr[i] = v; }));
      });
    } else {
      for (const [key, label] of group.items) {
        panel.appendChild(bindingRow(device, label,
          () => dev[key] || "",
          (v) => { dev[key] = v; }));
      }
    }
  }
}

function bindingRow(device, label, get, set) {
  const row = document.createElement("div");
  row.className = "row";
  const l = document.createElement("label");
  l.className = "field-label";
  l.textContent = label;

  const btn = document.createElement("button");
  btn.className = "btn binding-btn";
  const refresh = () => {
    btn.textContent = get() || "(unset)";
    btn.classList.toggle("unset", !get());
  };
  refresh();

  btn.addEventListener("click", () => startCapture(device, btn, (v) => { set(v); refresh(); }));
  btn.addEventListener("contextmenu", (e) => { e.preventDefault(); set(""); refresh(); });

  row.appendChild(l);
  row.appendChild(btn);
  return row;
}

function cancelCapture() {
  if (!capturing) return;
  capturing.cleanup();
  capturing.btn.classList.remove("capturing");
  capturing = null;
}

function startCapture(device, btn, assign) {
  cancelCapture();
  btn.classList.add("capturing");
  const prev = btn.textContent;
  btn.textContent = "Press…";

  if (device === "kbd") {
    const onKey = (e) => {
      e.preventDefault();
      const name = window.keyEventToSdl(e);
      finish(name);
    };
    window.addEventListener("keydown", onKey, true);
    capturing = { btn, cleanup: () => window.removeEventListener("keydown", onKey, true) };
  } else {
    const baseline = snapshotGamepad();      // ignore already-held inputs
    let raf = 0;
    const tick = () => {
      const hit = detectGamepad(baseline);
      if (hit) { finish(hit); return; }
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    capturing = { btn, cleanup: () => cancelAnimationFrame(raf) };
  }

  function finish(value) {
    cancelCapture();
    if (value) assign(value); else btn.textContent = prev;
  }
}

function activePad() {
  const pads = navigator.getGamepads ? navigator.getGamepads() : [];
  return Array.from(pads).find((p) => p && p.connected) || null;
}

// Snapshot currently-held buttons/axes so capture ignores them.
function snapshotGamepad() {
  const pad = activePad();
  if (!pad) return null;
  return {
    buttons: pad.buttons.map((b) => b.value > 0.5 || b.pressed),
    axes: Array.from(pad.axes),
  };
}

// Return the SDL name of the first input newly active since `baseline`, else null.
function detectGamepad(baseline) {
  const pad = activePad();
  if (!pad) return null;
  for (let i = 0; i < pad.buttons.length; i++) {
    const pressed = pad.buttons[i].value > 0.5 || pad.buttons[i].pressed;
    const was = baseline && baseline.buttons[i];
    if (pressed && !was && window.GAMEPAD_BUTTON_TO_SDL[i]) {
      return window.GAMEPAD_BUTTON_TO_SDL[i];
    }
  }
  for (let a = 0; a < pad.axes.length; a++) {
    const v = pad.axes[a];
    const wasV = baseline ? baseline.axes[a] : 0;
    if (Math.abs(v) > 0.6 && Math.abs(wasV) <= 0.6 && window.GAMEPAD_AXIS_TO_SDL[a]) {
      return window.GAMEPAD_AXIS_TO_SDL[a] + (v > 0 ? "+" : "-");
    }
  }
  return null;
}

// --- General tab (asset management: status, language, ROM -> DAT) ------------

function renderGeneralPanel(panel) {
  panel.innerHTML = `
    <h3 class="section-h">Asset File</h3>
    <div id="dat-status" class="assets-status">Checking for assets…</div>

    <h3 class="section-h">Language</h3>
    <div class="row">
      <label class="field-label">Language</label>
      <select id="language-select"></select>
    </div>

    <h3 class="section-h">Create Asset File from ROM</h3>
    <div class="row">
      <label class="field-label">Base US ROM</label>
      <span id="base-rom-path" class="muted path">none selected</span>
      <button id="pick-base" class="btn">Browse…</button>
    </div>
    <div id="base-rom-info" class="muted assets-info"></div>
    <div class="row">
      <label class="field-label">Language ROMs (optional)</label>
      <button id="add-lang" class="btn">Browse…</button>
    </div>
    <ul id="lang-rom-list" class="lang-list"></ul>
    <div class="row row-end">
      <button id="create-dat" class="btn btn-primary">Create Asset File</button>
    </div>`;

  panel.querySelector("#pick-base").addEventListener("click", pickBaseRom);
  panel.querySelector("#add-lang").addEventListener("click", addLanguageRom);
  panel.querySelector("#create-dat").addEventListener("click", createDat);
  panel.querySelector("#language-select").addEventListener("change", (e) => {
    config.language = e.target.value;   // "" = default
  });

  renderBaseRom();
  renderLangList();
  refreshDatStatus();   // also fills the language dropdown
}

// The Sound tab's MSU section ("msu" field): browse for the track folder,
// detect the filename prefix, and offer PCM -> OPUZ encoding when PCM tracks
// match. Edits config.msu_path, which the game reads as <folder>/<prefix>.
function renderMsuField() {
  let { folder, prefix } = splitMsuPath(config.msu_path || "");
  let scan = null;   // scan_msu_folder result for `folder`

  const frag = document.createDocumentFragment();
  const folderRow = document.createElement("div");
  folderRow.className = "row";
  folderRow.innerHTML = `
    <label class="field-label">MSU folder</label>
    <span class="muted path"></span>
    <button class="btn">Browse…</button>`;
  const folderText = folderRow.querySelector(".path");

  const prefixInput = document.createElement("input");
  prefixInput.type = "text";
  const prefixRow = makeRow("Track filename prefix", prefixInput);

  const info = document.createElement("div");
  info.className = "muted assets-info";

  const encodeBtn = document.createElement("button");
  encodeBtn.className = "btn btn-primary";
  encodeBtn.textContent = "Encode to OPUZ";
  const encodeRow = makeRow("Convert the PCM tracks to OPUZ", encodeBtn);

  const progress = document.createElement("div");
  progress.id = "opus-progress";
  progress.className = "progress";
  progress.hidden = true;
  progress.innerHTML = `<div id="opus-bar" class="progress-bar"></div>`;
  const msg = document.createElement("div");
  msg.id = "opus-msg";
  msg.className = "muted assets-info";

  frag.append(folderRow, prefixRow, info, encodeRow, progress, msg);

  const refresh = () => {
    setPathText(folderText, folder);
    if (!folder) folderText.textContent = "data folder";
    prefixInput.value = prefix;
    const match = scan && scan.ok && scan.prefixes.find((p) => p.prefix === prefix);
    if (!scan) info.textContent = "";
    else if (!scan.ok) info.textContent = "Folder not found. Browse to the folder with your MSU-1 tracks.";
    else if (match) info.textContent = `Found ${match.pcm} PCM and ${match.opuz} OPUZ tracks.`;
    else info.textContent = `No tracks named ${prefix || "<prefix>"}N.pcm or .opuz in this folder.`;
    encodeRow.hidden = !(match && match.pcm > 0);
  };
  const save = () => { config.msu_path = joinMsuPath(folder, prefix); };
  const rescan = async () => {
    scan = JSON.parse(await invoke("scan_msu_folder", { folder }));
    refresh();
  };

  folderRow.querySelector(".btn").addEventListener("click", async () => {
    const path = await invoke("pick_msu_folder");
    if (!path) return;
    folder = path;
    await rescan();
    // Keep the current prefix if it matches tracks here, else use the most common one.
    const known = scan.ok && scan.prefixes.some((p) => p.prefix === prefix);
    if (!known && scan.ok && scan.prefixes.length) prefix = scan.prefixes[0].prefix;
    save();
    refresh();
  });
  prefixInput.addEventListener("input", () => {
    prefix = prefixInput.value;
    save();
    refresh();
  });
  encodeBtn.addEventListener("click", async () => {
    await encodeOpus(folder, prefix);
    await rescan();
  });

  refresh();
  if (folder || prefix) rescan();
  return frag;
}

// "msu/alttp_msu-" -> { folder: "msu", prefix: "alttp_msu-" }
function splitMsuPath(path) {
  const i = Math.max(path.lastIndexOf("/"), path.lastIndexOf("\\"));
  return i < 0 ? { folder: "", prefix: path } : { folder: path.slice(0, i), prefix: path.slice(i + 1) };
}

function joinMsuPath(folder, prefix) {
  return folder ? folder.replace(/[\\/]+$/, "") + "/" + prefix : prefix;
}

// Show a path in a left-truncating `.path` span. The LRM keeps the leading "/"
// in place under the span's direction:rtl; the tooltip shows the full path.
function setPathText(el, path) {
  if (!el) return;
  el.textContent = path ? "\u200E" + path : "none selected";
  el.title = path || "";
}

async function encodeOpus(folder, prefix) {
  setOpusProgress(0, "Starting…");
  setStatus("Encoding MSU audio…", "");
  try {
    const result = JSON.parse(await invoke("encode_opus", { folder, prefix }));
    if (result.ok) {
      setOpusProgress(1, `Encoded ${result.encoded} / ${result.total} tracks`);
      setStatus("Opus encoding complete.", "ok");
      if (result.encoded > 0) {
        switchMsuToOpuz();
        await tidyPcm(folder, prefix, result.encoded);
      }
    } else {
      setStatus("Encoding failed: " + (result.error || "unknown"), "err");
    }
  } catch (e) {
    setStatus("Encoding failed: " + e, "err");
  }
}

// Ask (native dialog) whether to keep the source PCM files in a pcm/ subfolder
// or delete them; only tracks that now have an .opuz are touched.
async function tidyPcm(folder, prefix, encoded) {
  try {
    const { action, result } = JSON.parse(
      await invoke("finish_opus_encode", { folder, prefix, encoded }));
    if (action === "none") return;
    if (!result.ok) { setStatus("PCM cleanup failed: " + result.error, "err"); return; }
    setStatus(action === "moved"
      ? `Moved ${result.count} PCM files to the pcm subfolder.`
      : `Deleted ${result.count} PCM files.`, "ok");
  } catch (e) {
    setStatus("PCM cleanup failed: " + e, "err");
  }
}

function setOpusProgress(fraction, message) {
  const progress = document.getElementById("opus-progress");
  if (progress) progress.hidden = false;
  const bar = document.getElementById("opus-bar");
  const msg = document.getElementById("opus-msg");
  if (bar) bar.style.width = Math.round((fraction || 0) * 100) + "%";
  if (msg && message != null) msg.textContent = message;
}

function renderBaseRom() {
  setPathText(document.getElementById("base-rom-path"), baseRom);
}

function renderLangList() {
  const ul = document.getElementById("lang-rom-list");
  if (!ul) return;
  ul.innerHTML = "";
  languageRoms.forEach((r, i) => {
    const li = document.createElement("li");
    li.className = "lang-item";
    const tag = r.lang ? `[${r.lang}] ` : "";
    li.innerHTML = `<span>${tag}${r.path}</span>`;
    const rm = document.createElement("button");
    rm.className = "btn";
    rm.textContent = "Remove";
    rm.addEventListener("click", () => { languageRoms.splice(i, 1); renderLangList(); });
    li.appendChild(rm);
    ul.appendChild(li);
  });
}

async function refreshDatStatus() {
  const el = document.getElementById("dat-status");
  if (!el) return;
  try {
    const st = JSON.parse(await invoke("dat_status"));
    el.textContent = st.exists
      ? `Assets present — languages: ${st.languages.length ? st.languages.join(", ") : "us"}`
      : "No zelda3_assets.dat yet — create one from a ROM below.";
    el.className = "assets-status " + (st.exists ? "ok" : "warn");
    populateLanguages(st.exists ? st.languages : []);
  } catch (e) {
    el.textContent = "Could not read asset status: " + e;
  }
}

// Fill the Language dropdown with "(default)" + the languages present in the DAT.
function populateLanguages(languages) {
  const sel = document.getElementById("language-select");
  if (!sel) return;
  const langs = languages && languages.length ? languages : ["us"];
  const current = config.language || "";
  sel.innerHTML = "";
  const opts = [["", "(default)"], ...langs.map((l) => [l, l])];
  for (const [value, text] of opts) {
    const o = document.createElement("option");
    o.value = value;
    o.textContent = text;
    sel.appendChild(o);
  }
  sel.value = current;
}

async function pickBaseRom() {
  const path = await pickRom();
  if (!path) return;
  baseRom = path;
  renderBaseRom();
  const info = document.getElementById("base-rom-info");
  try {
    const id = JSON.parse(await invoke("identify_rom", { path }));
    if (!id.read) info.textContent = "Could not read that file.";
    else if (!id.valid) info.textContent = "Not a recognized ALttP ROM.";
    else info.textContent = `Recognized: ${id.lang_name} (${id.lang_code})`;
  } catch (e) {
    info.textContent = "Identify failed: " + e;
  }
}

async function addLanguageRom() {
  const path = await pickRom();
  if (!path) return;
  let lang = "";
  try {
    const id = JSON.parse(await invoke("identify_rom", { path }));
    if (id.valid) lang = id.lang_code;
  } catch (_) { /* ignore identify errors; backend re-validates */ }
  languageRoms.push({ path, lang });
  renderLangList();
}

async function createDat() {
  if (!baseRom) { setStatus("Select a base US ROM first.", "err"); return; }
  setStatus("Creating DAT… this can take a while.", "");
  try {
    const result = JSON.parse(await invoke("create_dat", {
      usRomPath: baseRom,
      languageRoms: languageRoms.map((r) => r.path),
    }));
    if (result.ok) {
      setStatus("DAT created (" + (result.languages.join(", ") || "us") + ").", "ok");
      refreshDatStatus();
    } else {
      setStatus("DAT creation failed: " + (result.error || "unknown"), "err");
    }
  } catch (e) {
    setStatus("DAT creation failed: " + e, "err");
  }
}

function selectTab(name) {
  if (name === activeTab) return;
  cancelCapture();
  const tabs = Array.from(document.querySelectorAll(".tab"));
  const from = tabs.findIndex((t) => t.textContent === activeTab);
  const to = tabs.findIndex((t) => t.textContent === name);
  activeTab = name;

  // The incoming panel slides in from the side of the tab being moved to.
  const panelsEl = document.getElementById("panels");
  panelsEl.style.setProperty("--slide-dir", to > from ? 1 : -1);
  panelsEl.scrollTop = 0;
  for (const t of tabs) t.classList.toggle("active", t.textContent === name);
  for (const p of document.querySelectorAll(".panel"))
    p.classList.toggle("active", p.dataset.tab === name);
  moveIndicator();
}

// --- backend actions --------------------------------------------------------

async function load() {
  try {
    const json = await invoke("load_config");
    config = JSON.parse(json);
    renderTabs();
    setStatus("Loaded configuration.", "ok");
  } catch (e) {
    setStatus("Load failed: " + e, "err");
  }
}

async function save() {
  try {
    const result = JSON.parse(await invoke("save_config", { config: JSON.stringify(config) }));
    if (result.ok) setStatus("Saved.", "ok");
    else setStatus("Save rejected: " + (result.error || "invalid config"), "err");
  } catch (e) {
    setStatus("Save failed: " + e, "err");
  }
}

async function validate() {
  try {
    const result = JSON.parse(await invoke("validate_config", { config: JSON.stringify(config) }));
    if (result.ok) setStatus("Configuration is valid.", "ok");
    else setStatus("Invalid: " + (result.error || "unknown"), "err");
  } catch (e) {
    setStatus("Validate failed: " + e, "err");
  }
}

async function reset() {
  try {
    const json = await invoke("reset_config");
    config = JSON.parse(json);
    renderTabs();
    setStatus("Reset to defaults.", "ok");
  } catch (e) {
    setStatus("Reset failed: " + e, "err");
  }
}

// Save the config, then launch the game detached.
async function saveAndLaunch() {
  try {
    const result = JSON.parse(await invoke("save_config", { config: JSON.stringify(config) }));
    if (!result.ok) { setStatus("Save rejected: " + (result.error || "invalid config"), "err"); return; }
    await invoke("launch_game");   // returns once the game process has started
    setStatus("Game launched.", "ok", 3000);
  } catch (e) {
    setStatus("Launch failed: " + e, "err");
  }
}

// --- init -------------------------------------------------------------------

window.addEventListener("DOMContentLoaded", () => {
  document.getElementById("btn-save").addEventListener("click", save);
  document.getElementById("btn-validate").addEventListener("click", validate);
  document.getElementById("btn-reset").addEventListener("click", reset);
  document.getElementById("btn-launch").addEventListener("click", saveAndLaunch);

  // Stream opus encode progress from the backend into the progress bar.
  window.__TAURI__.event.listen("opus-progress", (e) => {
    setOpusProgress(e.payload.fraction, e.payload.message);
  });

  load();
});
