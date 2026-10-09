// Data for the Keyboard and Gamepad tabs.
//
// The config JSON already carries kbd/gamepad objects (1:1 with the C keymap
// globals); these tabs edit those in place. Bindings are stored as the exact
// strings the game's config parser expects (SDL key names / SDL GameController
// button & axis names), so the maps below translate browser input into them.

// SNES control order (matches kControlNames in the C core).
window.CONTROL_NAMES = ["Up", "Down", "Left", "Right", "Select", "Start", "A", "B", "X", "Y", "L", "R"];

const slots = (n) => Array.from({ length: n }, (_, i) => "Slot " + (i + 1));

// Groups rendered for each device. kind=array -> indexed array key; kind=scalars
// -> individual string keys. Keys match the kbd/gamepad JSON objects.
window.KEYMAP_GROUPS = [
  { title: "Controls", kind: "array", key: "controls", labels: window.CONTROL_NAMES },
  { title: "Load state", kind: "array", key: "load", labels: slots(10) },
  { title: "Save state", kind: "array", key: "save", labels: slots(10) },
  { title: "Replay", kind: "array", key: "replay", labels: slots(10) },
  { title: "Cheats", kind: "scalars", items: [
    ["cheat_life", "Life / Magic"],
    ["cheat_keys", "Keys"],
    ["cheat_walkthrough", "Walk through walls"],
  ] },
  { title: "System", kind: "scalars", items: [
    ["clear_keylog", "Clear key log"],
    ["stop_replay", "Stop replay"],
    ["fullscreen", "Toggle fullscreen"],
    ["reset", "Reset"],
    ["pause", "Pause"],
    ["pause_dimmed", "Pause (dimmed)"],
    ["turbo", "Turbo"],
    ["replay_turbo", "Replay turbo"],
    ["window_bigger", "Window bigger"],
    ["window_smaller", "Window smaller"],
    ["volume_up", "Volume up"],
    ["volume_down", "Volume down"],
    ["display_perf", "Toggle perf display"],
    ["toggle_renderer", "Toggle renderer"],
  ] },
];

// KeyboardEvent.code -> SDL key name (SDL_GetKeyName output the game expects)
// for keys whose name doesn't depend on the keyboard layout.
window.KEY_TO_SDL = {
  ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left", ArrowRight: "Right",
  Enter: "Return", Escape: "Escape", Space: "Space",
  Tab: "Tab", Backspace: "Backspace", Delete: "Delete", Insert: "Insert",
  Home: "Home", End: "End", PageUp: "PageUp", PageDown: "PageDown",
  ShiftLeft: "Left Shift", ShiftRight: "Right Shift",
  ControlLeft: "Left Ctrl", ControlRight: "Right Ctrl",
  AltLeft: "Left Alt", AltRight: "Right Alt",
  MetaLeft: "Left GUI", MetaRight: "Right GUI", CapsLock: "CapsLock",
  NumpadEnter: "Keypad Enter", NumpadAdd: "Keypad +", NumpadSubtract: "Keypad -",
  NumpadMultiply: "Keypad *", NumpadDivide: "Keypad /", NumpadDecimal: "Keypad .",
};

// US-layout characters by physical position: only a fallback for when
// Shift/Alt changed the character the key produces.
const PUNCT_BY_CODE = {
  Minus: "-", Equal: "=", BracketLeft: "[", BracketRight: "]", Backslash: "\\",
  Semicolon: ";", Quote: "'", Backquote: "`", Comma: ",", Period: ".", Slash: "/",
};

// Translate a KeyboardEvent into an SDL key name.
window.keyEventToSdl = function (e) {
  const c = e.code, k = e.key;
  if (/^Digit[0-9]$/.test(c)) return c.slice(5);           // SDL keeps the number row as digits on every layout
  if (/^F([1-9]|1[0-9]|2[0-4])$/.test(c)) return c;        // F1..F24
  if (/^Numpad[0-9]$/.test(c)) return "Keypad " + c.slice(6);
  if (window.KEY_TO_SDL[c]) return window.KEY_TO_SDL[c];
  // Character keys: the game resolves single-character names to SDL keycodes,
  // which follow the layout, so use the character the key produces (on AZERTY
  // the key labelled A is "A", even though its e.code is KeyQ).
  if (!e.altKey && /^[a-z]$/i.test(k)) return k.toUpperCase();
  if (!e.altKey && !e.shiftKey && k.length === 1 && k > " " && k <= "~") return k;
  if (/^Key[A-Z]$/.test(c)) return c.slice(3);             // non-Latin layouts: SDL falls back to US letters
  return PUNCT_BY_CODE[c] || (k.length === 1 ? k.toUpperCase() : k);
};

// Modifier keys: captured as a prefix ("Shift+F1") when another key follows,
// or bound on their own when released alone. Prefix names match config.c.
window.MODIFIER_CODES = new Set([
  "ShiftLeft", "ShiftRight", "ControlLeft", "ControlRight", "AltLeft", "AltRight", "MetaLeft", "MetaRight",
]);
window.modifierPrefix = (e) =>
  (e.ctrlKey ? "Ctrl+" : "") + (e.altKey ? "Alt+" : "") + (e.shiftKey ? "Shift+" : "");

// W3C "standard mapping" gamepad button index -> button name config.c's
// ParseGamepadButtonName accepts. 6/7 are triggers -> L2/R2. The game has no
// stick-axis bindings, so axes are not captured.
window.GAMEPAD_BUTTON_TO_SDL = [
  "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "Back", "Start",
  "L3", "R3", "DpadUp", "DpadDown", "DpadLeft", "DpadRight", "Guide",
];
