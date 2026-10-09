// Declarative description of the Graphics / Sound / Features tabs, matching the
// original GTK launcher's tab placement, option set, order, and labels.
// (General is the asset-management tab, rendered specially; Keyboard/Gamepad too.)
//
// Field types:
//   bool   -> checkbox bound to a boolean config key
//   int    -> number input
//   text   -> text input
//   file   -> text input + Browse… button; filter: { name, extensions } for the dialog
//   select -> dropdown; options: [[value,label],...] (numeric values)
//   flag   -> checkbox toggling one bit of an integer key (config.features0)
//   section-> a header row (Features tab grouping)
//   opus   -> the MSU -> OPUZ encoder block (Sound tab)
window.TABS = {
  Graphics: [
    { key: "output_method", label: "Output Method", type: "select",
      options: [[0, "SDL"], [1, "SDL-Software"], [2, "OpenGL"], [3, "OpenGL ES"], [4, "Vulkan"]] },
    { key: "window_width", label: "Window Width (0 = auto)", type: "int" },
    { key: "window_height", label: "Window Height (0 = auto)", type: "int" },
    { key: "window_scale", label: "Window Scale (used in auto mode)", type: "int" },
    { key: "fullscreen", label: "Display Mode", type: "select",
      options: [[0, "Windowed"], [1, "Borderless Fullscreen"], [2, "Fullscreen"]] },
    { key: "extended_aspect_ratio", label: "Aspect Ratio", type: "select",
      options: [[0, "4:3"], [1, "16:9"], [2, "16:10"], [3, "18:9"], [4, "Custom"]] },
    { key: "custom_aspect_w", label: "Custom Aspect Width", type: "int" },
    { key: "custom_aspect_h", label: "Custom Aspect Height", type: "int" },
    { key: "ignore_aspect_ratio", label: "Stretch to fill window (ignore aspect ratio)", type: "bool" },
    { key: "extend_y", label: "Extend render height (224 lines → 240 lines)", type: "bool" },
    { key: "linear_filtering", label: "Use linear filtering for smoother pixels", type: "bool" },
    { key: "new_renderer", label: "Use optimized SNES PPU renderer", type: "bool" },
    { key: "enhanced_mode7", label: "Display the world map with higher resolution (Enhanced Mode 7)", type: "bool" },
    { key: "no_sprite_limits", label: "Disable SNES sprite limit (8 sprites per scanline)", type: "bool" },
    { key: "display_perf_title", label: "Show FPS in window title", type: "bool" },
    { key: "disable_frame_delay", label: "Disable frame delay (for 60Hz displays only)", type: "bool" },
    { key: "features0", bit: 65536, label: "Dim flashes (accessibility - lessens flashing effects)", type: "flag" },
    { key: "shader", label: "Shader File", type: "file",
      filter: { name: "Shader", extensions: ["glsl", "glslp", "slang", "slangp"] } },
    { key: "link_graphics", label: "Link Sprite File", type: "file",
      filter: { name: "Link sprite", extensions: ["zspr"] } },
  ],
  Sound: [
    { key: "enable_audio", label: "Enable Audio", type: "bool" },
    { key: "audio_channels", label: "Audio Channels", type: "select", options: [[1, "Mono"], [2, "Stereo"]] },
    { key: "audio_freq", label: "Audio Frequency", type: "select",
      options: [[11025, "11025"], [22050, "22050"], [32000, "32000"],
                [44100, "44100 (Use with PCM MSU)"], [48000, "48000 (Use with OPUZ MSU)"]] },
    { key: "audio_samples", label: "Audio Samples", type: "select",
      options: [[256, "256"], [512, "512"], [1024, "1024"], [2048, "2048"], [4096, "4096"]] },
    { key: "enable_msu", label: "MSU", type: "select",
      options: [[0, "Disabled"], [1, "PCM"], [3, "PCM Deluxe"], [5, "Opuz"], [7, "Opuz Deluxe"]] },
    { key: "msuvolume", label: "MSU Volume (0-100)", type: "int" },
    { key: "resume_msu", label: "Resume MSU position when re-entering overworld area", type: "bool" },
    { key: "msu_path", label: "MSU Folder / path prefix", type: "text" },
    { type: "opus" },
  ],
  Features: [
    { type: "section", label: "Save" },
    { key: "autosave", label: "Autosave on quit and reload on start", type: "bool" },
    { type: "section", label: "Controls" },
    { key: "features0", bit: 2, label: "Item switching with L/R shoulder buttons", type: "flag" },
    { key: "features0", bit: 32768, label: "Limit L/R item switching to first 4 items only", type: "flag" },
    { key: "features0", bit: 4, label: "Allow Link to turn while dashing", type: "flag" },
    { type: "section", label: "Gameplay" },
    { key: "features0", bit: 128, label: "Skip intro on any keypress", type: "flag" },
    { key: "features0", bit: 8, label: "Allow magic mirror to warp to the Dark World", type: "flag" },
    { key: "features0", bit: 16, label: "Collect items (hearts, rupees) with sword", type: "flag" },
    { key: "features0", bit: 512, label: "Allow more active bombs (4 instead of 2)", type: "flag" },
    { key: "features0", bit: 2048, label: "Increase rupee capacity to 9999", type: "flag" },
    { key: "features0", bit: 8192, label: "Cancel bird travel by pressing X", type: "flag" },
    { key: "break_pots_min_sword", label: "Break pots with sword", type: "select",
      options: [[0, "Disabled"], [1, "Level 1 (Wooden)"], [2, "Level 2 (Master)"],
                [3, "Level 3 (Tempered)"], [4, "Level 4 (Golden)"]] },
    { type: "section", label: "Interface" },
    { key: "features0", bit: 256, label: "Highlight maxed items in yellow", type: "flag" },
    { key: "features0", bit: 64, label: "Disable low health beep sound", type: "flag" },
    { type: "section", label: "Bug Fixes" },
    { key: "features0", bit: 4096, label: "Fix misc bugs from original game", type: "flag" },
    { key: "features0", bit: 16384, label: "Fix bugs that change gameplay", type: "flag" },
    { type: "section", label: "Experimental" },
    { key: "features0", bit: 131072, label: "Pokemode", type: "flag" },
    { key: "features0", bit: 262144, label: "Princess Zelda helps in battle", type: "flag" },
  ],
};
