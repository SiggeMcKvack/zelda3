#include "config_writer.h"
#include "launcher_ui.h"
#include "config.h"
#include "features.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// Initialize Config with default values matching zelda3.ini template
void ConfigWriter_InitDefaults(Config *config) {
  memset(config, 0, sizeof(Config));

  // General defaults
  config->autosave = false;
  config->display_perf_title = false;
  config->disable_frame_delay = false;

  // Graphics defaults  config->window_width = 0;  // Auto
  config->window_height = 0;  // Auto
  config->window_scale = 3;
  config->fullscreen = 0;  // Windowed
  config->ignore_aspect_ratio = false;
  config->output_method = kOutputMethod_SDL;
  config->linear_filtering = false;
  config->new_renderer = true;
  config->enhanced_mode7 = true;
  config->no_sprite_limits = true;
  config->extended_aspect_ratio = 1;  // extend_y, 16:9
  config->extend_y = true;

  // Sound defaults
  config->enable_audio = true;
  config->audio_freq = 44100;
  config->audio_channels = 2;
  config->audio_samples = 512;
  config->enable_msu = 0;  // false
  config->resume_msu = true;
  config->msuvolume = 100;

  // Features defaults (all off by default)
  config->features0 = 0;

  // Paths (NULL = not set)
  config->link_graphics = NULL;
  config->shader = NULL;
  config->msu_path = NULL;
  config->language = NULL;
  config->memory_buffer = NULL;
}

// Helper to write formatted line to file
static bool WriteLine(FILE *f, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int result = vfprintf(f, fmt, args);
  va_end(args);

  if (result < 0) {
    LogError("ConfigWriter: Failed to write to file");
    return false;
  }
  return true;
}

// Write [General] section
static bool WriteGeneralSection(FILE *f, const Config *config) {
  if (!WriteLine(f, "[General]\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# General Game Settings\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Automatically save state on quit and reload on start\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "Autosave = %d\n\n", config->autosave ? 1 : 0)) return false;

  if (!WriteLine(f, "# Display performance metrics in window title\n")) return false;
  if (!WriteLine(f, "# Shows FPS and frame timing information\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "DisplayPerfInTitle = %d\n\n", config->display_perf_title ? 1 : 0)) return false;

  if (!WriteLine(f, "# Disable the SDL_Delay that happens each frame\n")) return false;
  if (!WriteLine(f, "# Only enable if your display is exactly 60Hz for slightly better performance\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "DisableFrameDelay = %d\n\n", config->disable_frame_delay ? 1 : 0)) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Display Configuration\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Extended aspect ratio - widescreen support\n")) return false;
  if (!WriteLine(f, "# (default: 4:3, accepts: 4:3, 16:9, 16:10, 18:9)\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (!WriteLine(f, "# Options (comma-separated):\n")) return false;
  if (!WriteLine(f, "#   - Aspect ratio: 4:3 (original), 16:9, 16:10, or 18:9\n")) return false;
  if (!WriteLine(f, "#   - extend_y: Display 240 lines instead of 224 (add before aspect ratio)\n")) return false;
  if (!WriteLine(f, "#   - unchanged_sprites: Preserve original sprite spawn/die behavior for replay compatibility\n")) return false;
  if (!WriteLine(f, "#   - no_visual_fixes: Disable graphics glitch fixes (affects memory compare but not gameplay)\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (!WriteLine(f, "# Examples:\n")) return false;
  if (!WriteLine(f, "#   ExtendedAspectRatio = 16:9\n")) return false;
  if (!WriteLine(f, "#   ExtendedAspectRatio = extend_y, 16:9\n")) return false;
  if (!WriteLine(f, "#   ExtendedAspectRatio = 16:9, unchanged_sprites\n")) return false;

  // Extended aspect ratio
  if (!WriteLine(f, "ExtendedAspectRatio = ")) return false;
  if (config->extend_y) {
    if (!WriteLine(f, "extend_y, ")) return false;
  }

  // Map extended_aspect_ratio enum to string
  const char *aspect_str = "16:9";  // default
  switch (config->extended_aspect_ratio) {
    case 0: aspect_str = "4:3"; break;
    case 1: aspect_str = "16:9"; break;
    case 2: aspect_str = "16:10"; break;
    case 3: aspect_str = "18:9"; break;
  }
  if (!WriteLine(f, "%s\n\n", aspect_str)) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Language Settings\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Set which language to use\n")) return false;
  if (!WriteLine(f, "# Requires appropriate asset file created with restool.py\n")) return false;
  if (!WriteLine(f, "# (default: none/English, accepts: de, fr, etc.)\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (!WriteLine(f, "# To create assets for other languages:\n")) return false;
  if (!WriteLine(f, "#   python restool.py --extract-dialogue -r german.sfc\n")) return false;
  if (!WriteLine(f, "#   python restool.py --languages=de\n")) return false;
  if (!WriteLine(f, "#\n")) return false;

  if (config->language && *config->language) {
    if (!WriteLine(f, "Language = %s\n", config->language)) return false;
  } else {
    if (!WriteLine(f, "# Language = de\n")) return false;
  }

  if (!WriteLine(f, "\n\n")) return false;
  return true;
}

// Write [Graphics] section
static bool WriteGraphicsSection(FILE *f, const Config *config) {
  if (!WriteLine(f, "[Graphics]\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Window & Display Settings\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  // Window size
  if (!WriteLine(f, "# Window size in pixels\n")) return false;
  if (!WriteLine(f, "# (default: Auto, accepts: Auto or WidthxHeight like 1024x768)\n")) return false;
  if (!WriteLine(f, "# Auto calculates size based on WindowScale\n")) return false;
  if (config->window_width == 0 || config->window_height == 0) {
    if (!WriteLine(f, "WindowSize = Auto\n\n")) return false;

    if (!WriteLine(f, "# Window scale multiplier when WindowSize is Auto\n")) return false;
    if (!WriteLine(f, "# (default: 3, accepts: 1-10)\n")) return false;
    if (!WriteLine(f, "# 1=256x224, 2=512x448, 3=768x672, etc.\n")) return false;
    if (!WriteLine(f, "WindowScale = %d\n\n", config->window_scale)) return false;
  } else {
    if (!WriteLine(f, "WindowSize = %dx%d\n\n", config->window_width, config->window_height)) return false;
  }

  if (!WriteLine(f, "# Fullscreen mode\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1/2)\n")) return false;
  if (!WriteLine(f, "#   0 = Windowed\n")) return false;
  if (!WriteLine(f, "#   1 = Desktop fullscreen (recommended, borderless window)\n")) return false;
  if (!WriteLine(f, "#   2 = Fullscreen with mode change (may cause display switching)\n")) return false;
  if (!WriteLine(f, "Fullscreen = %d\n\n", config->fullscreen)) return false;

  if (!WriteLine(f, "# Ignore aspect ratio when scaling\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Enable to stretch image to fill window\n")) return false;
  if (!WriteLine(f, "IgnoreAspectRatio = %d\n\n", config->ignore_aspect_ratio ? 1 : 0)) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Rendering Settings\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  // Output method
  if (!WriteLine(f, "# Output rendering method\n")) return false;
  if (!WriteLine(f, "# (default: SDL, accepts: SDL, SDL-Software, OpenGL, OpenGL ES, Vulkan)\n")) return false;
  if (!WriteLine(f, "#   SDL          = Hardware-accelerated (recommended)\n")) return false;
  if (!WriteLine(f, "#   SDL-Software = Software rendering (better for Raspberry Pi)\n")) return false;
  if (!WriteLine(f, "#   OpenGL       = OpenGL 3.3+ (required for custom shaders)\n")) return false;
  if (!WriteLine(f, "#   OpenGL ES    = OpenGL ES (mobile/embedded)\n")) return false;
  if (!WriteLine(f, "#   Vulkan       = Vulkan 1.0 (cross-platform, requires Vulkan SDK or MoltenVK on macOS)\n")) return false;
  const char *output_method_str = "SDL";
  switch (config->output_method) {
    case kOutputMethod_SDL: output_method_str = "SDL"; break;
    case kOutputMethod_SDLSoftware: output_method_str = "SDL-Software"; break;
    case kOutputMethod_OpenGL: output_method_str = "OpenGL"; break;
    case kOutputMethod_OpenGL_ES: output_method_str = "OpenGL ES"; break;
    case kOutputMethod_Vulkan: output_method_str = "Vulkan"; break;
  }
  if (!WriteLine(f, "OutputMethod = %s\n\n", output_method_str)) return false;

  if (!WriteLine(f, "# Use linear filtering for smoother pixels\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Disable for crisp, pixelated look. Works with SDL and OpenGL.\n")) return false;
  if (!WriteLine(f, "LinearFiltering = %d\n\n", config->linear_filtering ? 1 : 0)) return false;

  if (!WriteLine(f, "# Use optimized SNES PPU renderer\n")) return false;
  if (!WriteLine(f, "# (default: 1, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Faster but potentially less accurate than original renderer\n")) return false;
  if (!WriteLine(f, "# See also: ToggleRenderer key (default: 'r') to switch at runtime\n")) return false;
  if (!WriteLine(f, "NewRenderer = %d\n\n", config->new_renderer ? 1 : 0)) return false;

  if (!WriteLine(f, "# Display the world map with higher resolution\n")) return false;
  if (!WriteLine(f, "# (default: 1, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Enhanced Mode 7 rendering for smoother map rotation\n")) return false;
  if (!WriteLine(f, "EnhancedMode7 = %d\n\n", config->enhanced_mode7 ? 1 : 0)) return false;

  if (!WriteLine(f, "# Remove SNES sprite limit (8 sprites per scanline)\n")) return false;
  if (!WriteLine(f, "# (default: 1, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Eliminates sprite flickering\n")) return false;
  if (!WriteLine(f, "NoSpriteLimits = %d\n\n", config->no_sprite_limits ? 1 : 0)) return false;

  if (!WriteLine(f, "# Recreate Virtual Console flash dimming\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Lessens flashing effects (accessibility feature)\n")) return false;
  if (!WriteLine(f, "DimFlashes = 0\n\n")) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Graphics Customization\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Custom Link sprite (ZSPR format)\n")) return false;
  if (!WriteLine(f, "# (default: none, accepts: path to .zspr file)\n")) return false;
  if (!WriteLine(f, "# Browse sprites: https://snesrev.github.io/sprites-gfx/snes/zelda3/link/\n")) return false;
  if (!WriteLine(f, "# Download: git clone https://github.com/snesrev/sprites-gfx.git\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (config->link_graphics && *config->link_graphics) {
    if (!WriteLine(f, "LinkGraphics = %s\n\n", config->link_graphics)) return false;
  } else {
    if (!WriteLine(f, "# LinkGraphics = sprites-gfx/snes/zelda3/link/sheets/megaman-x.2.zspr\n\n")) return false;
  }

  if (!WriteLine(f, "# GLSL shader (OpenGL output method only)\n")) return false;
  if (!WriteLine(f, "# (default: none, accepts: path to .glsl or .glslp file)\n")) return false;
  if (!WriteLine(f, "# Get shaders: git clone https://github.com/snesrev/glsl-shaders\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (config->shader && *config->shader) {
    if (!WriteLine(f, "Shader = %s\n\n", config->shader)) return false;
  } else {
    if (!WriteLine(f, "# Shader =\n\n")) return false;
  }

  if (!WriteLine(f, "\n")) return false;
  return true;
}

// Write [Sound] section
static bool WriteSoundSection(FILE *f, const Config *config) {
  if (!WriteLine(f, "[Sound]\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Audio Settings\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Enable audio output\n")) return false;
  if (!WriteLine(f, "# (default: 1, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "EnableAudio = %d\n\n", config->enable_audio ? 1 : 0)) return false;

  if (!WriteLine(f, "# Audio sample rate in Hz\n")) return false;
  if (!WriteLine(f, "# (default: 44100, accepts: 11025, 22050, 32000, 44100, 48000)\n")) return false;
  if (!WriteLine(f, "# Use 44100 for PCM MSU, 48000 for OPUZ MSU\n")) return false;
  if (!WriteLine(f, "AudioFreq = %d\n\n", config->audio_freq)) return false;

  if (!WriteLine(f, "# Number of audio channels\n")) return false;
  if (!WriteLine(f, "# (default: 2, accepts: 1=mono, 2=stereo)\n")) return false;
  if (!WriteLine(f, "AudioChannels = %d\n\n", config->audio_channels)) return false;

  if (!WriteLine(f, "# Audio buffer size in samples (power of 2)\n")) return false;
  if (!WriteLine(f, "# (default: 512, accepts: 256, 512, 1024, 2048, 4096)\n")) return false;
  if (!WriteLine(f, "# Lower = less latency but may cause crackling\n")) return false;
  if (!WriteLine(f, "# Higher = more latency but smoother playback\n")) return false;
  if (!WriteLine(f, "AudioSamples = %d\n\n", config->audio_samples)) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# MSU Audio (Custom Music)\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  // MSU enable
  if (!WriteLine(f, "# Enable MSU audio support for custom soundtracks\n")) return false;
  if (!WriteLine(f, "# (default: false, accepts: false, true, deluxe, opuz, deluxe-opuz)\n")) return false;
  if (!WriteLine(f, "#   false        = Disabled\n")) return false;
  if (!WriteLine(f, "#   true         = MSU PCM format (requires AudioFreq = 44100)\n")) return false;
  if (!WriteLine(f, "#   deluxe       = MSU Deluxe PCM\n")) return false;
  if (!WriteLine(f, "#   opuz         = OPUZ format (~10%% file size, requires AudioFreq = 48000)\n")) return false;
  if (!WriteLine(f, "#   deluxe-opuz  = MSU Deluxe OPUZ\n")) return false;
  const char *msu_str = "false";
  if (config->enable_msu & kMsuEnabled_Msu) {
    if (config->enable_msu & kMsuEnabled_MsuDeluxe) {
      if (config->enable_msu & kMsuEnabled_Opuz) {
        msu_str = "deluxe-opuz";
      } else {
        msu_str = "deluxe";
      }
    } else if (config->enable_msu & kMsuEnabled_Opuz) {
      msu_str = "opuz";
    } else {
      msu_str = "true";
    }
  }
  if (!WriteLine(f, "EnableMSU = %s\n\n", msu_str)) return false;

  if (!WriteLine(f, "# Path to MSU files (number and extension appended automatically)\n")) return false;
  if (!WriteLine(f, "# (default: msu/alttp_msu-, accepts: any path prefix)\n")) return false;
  if (!WriteLine(f, "# Example: msu/alttp_msu- loads msu/alttp_msu-1.pcm, msu/alttp_msu-2.pcm, etc.\n")) return false;
  if (config->msu_path && *config->msu_path) {
    if (!WriteLine(f, "MSUPath = %s\n\n", config->msu_path)) return false;
  } else {
    if (!WriteLine(f, "MSUPath = msu/alttp_msu-\n\n")) return false;
  }

  if (!WriteLine(f, "# Resume MSU position when re-entering overworld area\n")) return false;
  if (!WriteLine(f, "# (default: 1, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Remembers playback position for one area\n")) return false;
  if (!WriteLine(f, "ResumeMSU = %d\n\n", config->resume_msu ? 1 : 0)) return false;

  if (!WriteLine(f, "# MSU playback volume\n")) return false;
  if (!WriteLine(f, "# (default: 100%%, accepts: 0-100 with or without %% sign)\n")) return false;
  if (!WriteLine(f, "MSUVolume = %d%%\n\n", config->msuvolume)) return false;

  if (!WriteLine(f, "\n")) return false;
  return true;
}

// Write [Features] section
static bool WriteFeaturesSection(FILE *f, const Config *config) {
  if (!WriteLine(f, "[Features]\n")) return false;
  if (!WriteLine(f, "# ==============================================================================\n")) return false;
  if (!WriteLine(f, "# Enhanced Features\n")) return false;
  if (!WriteLine(f, "# ==============================================================================\n")) return false;
  if (!WriteLine(f, "# All features are disabled by default to preserve original game behavior.\n")) return false;
  if (!WriteLine(f, "# These features are optional enhancements and may affect replay compatibility.\n")) return false;
  if (!WriteLine(f, "# ==============================================================================\n\n")) return false;

  uint32 features = config->features0;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Control Enhancements\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Item switching with L/R shoulder buttons\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (!WriteLine(f, "# Enables:\n")) return false;
  if (!WriteLine(f, "#   - L/R to cycle through items\n")) return false;
  if (!WriteLine(f, "#   - Y+direction to reorder items in inventory\n")) return false;
  if (!WriteLine(f, "#   - Hold X/L/R in item menu to assign items to those buttons\n")) return false;
  if (!WriteLine(f, "#   - Select opens map when X is reassigned\n")) return false;
  if (!WriteLine(f, "#   - Select while paused to save/quit\n")) return false;
  if (!WriteLine(f, "ItemSwitchLR = %d\n\n", (features & kFeatures0_SwitchLR) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Limit ItemSwitchLR cycling to first 4 items only\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Requires ItemSwitchLR = 1\n")) return false;
  if (!WriteLine(f, "ItemSwitchLRLimit = %d\n\n", (features & kFeatures0_SwitchLRLimit) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Allow Link to turn while dashing\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Normally Link can only dash in cardinal directions\n")) return false;
  if (!WriteLine(f, "TurnWhileDashing = %d\n\n", (features & kFeatures0_TurnWhileDashing) ? 1 : 0)) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Gameplay Modifications\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Allow magic mirror to warp TO the Dark World\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Normally mirror only warps to Light World\n")) return false;
  if (!WriteLine(f, "MirrorToDarkworld = %d\n\n", (features & kFeatures0_MirrorToDarkworld) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Collect items (hearts, rupees) with sword\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Normally requires touching items\n")) return false;
  if (!WriteLine(f, "CollectItemsWithSword = %d\n\n", (features & kFeatures0_CollectItemsWithSword) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Break pots with level 2-4 sword\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Normally requires lifting or dashing\n")) return false;
  if (!WriteLine(f, "BreakPotsWithSword = %d\n\n", (features & kFeatures0_BreakPotsWithSword) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Allow more active bombs (4 instead of 2)\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "MoreActiveBombs = %d\n\n", (features & kFeatures0_MoreActiveBombs) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Increase rupee capacity to 9999 (instead of 999)\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "CarryMoreRupees = %d\n\n", (features & kFeatures0_CarryMoreRupees) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Cancel bird travel by pressing X\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "CancelBirdTravel = %d\n\n", (features & kFeatures0_CancelBirdTravel) ? 1 : 0)) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Interface & Quality of Life\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Disable low health warning beep\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "DisableLowHealthBeep = %d\n\n", (features & kFeatures0_DisableLowHealthBeep) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Skip intro sequence on any keypress\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Speeds up game start\n")) return false;
  if (!WriteLine(f, "SkipIntroOnKeypress = %d\n\n", (features & kFeatures0_SkipIntroOnKeypress) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Show max items with orange/yellow color in HUD\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Visual indicator when rupees/bombs/arrows are maxed\n")) return false;
  if (!WriteLine(f, "ShowMaxItemsInYellow = %d\n\n", (features & kFeatures0_ShowMaxItemsInYellow) ? 1 : 0)) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Bug Fixes\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Enable miscellaneous bug fixes from original game\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Fixes various minor bugs while preserving core gameplay\n")) return false;
  if (!WriteLine(f, "# See ARCHITECTURE.md for list of fixes\n")) return false;
  if (!WriteLine(f, "MiscBugFixes = %d\n\n", (features & kFeatures0_MiscBugFixes) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Enable game-changing bug fixes\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Fixes bugs that noticeably affect gameplay/mechanics\n")) return false;
  if (!WriteLine(f, "# May break replays from original game\n")) return false;
  if (!WriteLine(f, "GameChangingBugFixes = %d\n\n", (features & kFeatures0_GameChangingBugFixes) ? 1 : 0)) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Experimental Features\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# Pokemode - Pokemon-style monster capture system (EXPERIMENTAL)\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Capture enemies and NPCs with the Bug Net and store them in bottles\n")) return false;
  if (!WriteLine(f, "# Release captured sprites to fight alongside you as friendly companions\n")) return false;
  if (!WriteLine(f, "# Supported captures: Ravens, Vultures, Guards, Princess Zelda, followers, and more\n")) return false;
  if (!WriteLine(f, "# Some sprites become friendly AI that attack nearby enemies\n")) return false;
  if (!WriteLine(f, "# Bottles display a flute icon when containing captured sprites\n")) return false;
  if (!WriteLine(f, "# WARNING: Experimental feature, may affect gameplay balance and replays\n")) return false;
  if (!WriteLine(f, "Pokemode = %d\n\n", (features & kFeatures0_Pokemode) ? 1 : 0)) return false;

  if (!WriteLine(f, "# Princess Zelda follower mode (EXPERIMENTAL)\n")) return false;
  if (!WriteLine(f, "# (default: 0, accepts: 0/1)\n")) return false;
  if (!WriteLine(f, "# Allows Princess Zelda to become a follower companion outside normal story sequence\n")) return false;
  if (!WriteLine(f, "# Works in conjunction with Pokemode - can capture and release Zelda\n")) return false;
  if (!WriteLine(f, "# After healing Link at sanctuary, Zelda becomes a permanent follower\n")) return false;
  if (!WriteLine(f, "# WARNING: Experimental feature, may affect story progression and replays\n")) return false;
  if (!WriteLine(f, "PrincessZeldaHelps = %d\n\n", (features & kFeatures0_PrincessZeldaHelps) ? 1 : 0)) return false;

  if (!WriteLine(f, "\n")) return false;
  return true;
}

// Write [KeyMap] section
static bool WriteKeyMapSection(FILE *f, const Config *config) {
  (void)config;

  if (!WriteLine(f, "[KeyMap]\n")) return false;
  if (!WriteLine(f, "# ==============================================================================\n")) return false;
  if (!WriteLine(f, "# Keyboard Controls\n")) return false;
  if (!WriteLine(f, "# ==============================================================================\n")) return false;
  if (!WriteLine(f, "# Customize keyboard bindings for all game functions.\n")) return false;
  if (!WriteLine(f, "# Format: key names from SDL, supports modifiers (Shift+, Ctrl+, Alt+)\n")) return false;
  if (!WriteLine(f, "# ==============================================================================\n\n")) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Main Game Controls\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# SNES controller button mapping\n")) return false;
  if (!WriteLine(f, "# Order: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (!WriteLine(f, "# Default for QWERTY keyboards:\n")) return false;
  char *controls_str = LauncherUI_FormatControlString(g_kbd_controls);
  if (!WriteLine(f, "Controls = %s\n\n", controls_str)) { free(controls_str); return false; }
  free(controls_str);

  if (!WriteLine(f, "# Alternative layouts (uncomment to use):\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (!WriteLine(f, "# QWERTZ keyboards:\n")) return false;
  if (!WriteLine(f, "#Controls = Up, Down, Left, Right, Right Shift, Return, x, y, s, a, c, v\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (!WriteLine(f, "# AZERTY keyboards:\n")) return false;
  if (!WriteLine(f, "#Controls = Up, Down, Left, Right, Right Shift, Return, x, w, s, q, c, v\n\n")) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Save State Management\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  // Save states
  if (!WriteLine(f, "# Load save state (F1-F10)\n")) return false;
  char *load_str = LauncherUI_FormatControlString(g_kbd_load);
  if (!WriteLine(f, "Load =      %s\n\n", load_str)) { free(load_str); return false; }
  free(load_str);

  if (!WriteLine(f, "# Save state (Shift+F1 through Shift+F10)\n")) return false;
  char *save_str = LauncherUI_FormatControlString(g_kbd_save);
  if (!WriteLine(f, "Save = %s\n\n", save_str)) { free(save_str); return false; }
  free(save_str);

  if (!WriteLine(f, "# Replay state (Ctrl+F1 through Ctrl+F10)\n")) return false;
  char *replay_str = LauncherUI_FormatControlString(g_kbd_replay);
  if (!WriteLine(f, "Replay= %s\n\n", replay_str)) { free(replay_str); return false; }
  free(replay_str);

  if (!WriteLine(f, "# Load reference saves (uncomment to enable)\n")) return false;
  if (!WriteLine(f, "#LoadRef = 1,2,3,4,5,6,7,8,9,0,-,=,Backspace\n\n")) return false;

  if (!WriteLine(f, "# Replay reference saves (uncomment to enable)\n")) return false;
  if (!WriteLine(f, "#ReplayRef = Ctrl+1,Ctrl+2,Ctrl+3,Ctrl+4,Ctrl+5,Ctrl+6,Ctrl+7,Ctrl+8,Ctrl+9,Ctrl+0,Ctrl+-,Ctrl+=,Ctrl+Backspace\n\n")) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Cheat Keys (Development/Testing)\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  // Cheats
  if (!WriteLine(f, "# Refill health and magic to full\n")) return false;
  if (!WriteLine(f, "# Sets both health and magic to maximum (80 hearts, 80 magic)\n")) return false;
  if (g_kbd_cheat_life && *g_kbd_cheat_life)
    if (!WriteLine(f, "CheatLife = %s\n\n", g_kbd_cheat_life)) return false;

  if (!WriteLine(f, "# Set key count to 1\n")) return false;
  if (!WriteLine(f, "# Note: Sets key count to 1, does not add keys incrementally\n")) return false;
  if (g_kbd_cheat_keys && *g_kbd_cheat_keys)
    if (!WriteLine(f, "CheatKeys = %s\n\n", g_kbd_cheat_keys)) return false;

  if (!WriteLine(f, "# Give bombs, arrows, and rupees\n")) return false;
  if (!WriteLine(f, "# Gives 10 bombs, 10 arrows, and 100 rupees\n")) return false;
  if (!WriteLine(f, "# CheatEquipment = \n\n")) return false;

  if (!WriteLine(f, "# Walk through walls\n")) return false;
  if (!WriteLine(f, "# Toggles collision detection on/off\n")) return false;
  if (g_kbd_cheat_walkthrough && *g_kbd_cheat_walkthrough)
    if (!WriteLine(f, "CheatWalkThroughWalls = %s\n\n", g_kbd_cheat_walkthrough)) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Debug & Replay Tools\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  // System controls
  if (!WriteLine(f, "# Clear input recording log\n")) return false;
  if (!WriteLine(f, "# Clears the replay system's input log (used for debugging replays)\n")) return false;
  if (g_kbd_clear_keylog && *g_kbd_clear_keylog)
    if (!WriteLine(f, "ClearKeyLog = %s\n\n", g_kbd_clear_keylog)) return false;

  if (!WriteLine(f, "# Stop replay playback\n")) return false;
  if (!WriteLine(f, "# Stops the currently playing replay\n")) return false;
  if (g_kbd_stop_replay && *g_kbd_stop_replay)
    if (!WriteLine(f, "StopReplay = %s\n\n", g_kbd_stop_replay)) return false;

  if (!WriteLine(f, "# Toggle fullscreen\n")) return false;
  if (g_kbd_fullscreen && *g_kbd_fullscreen)
    if (!WriteLine(f, "Fullscreen = %s\n\n", g_kbd_fullscreen)) return false;

  if (!WriteLine(f, "# Reset game\n")) return false;
  if (g_kbd_reset && *g_kbd_reset)
    if (!WriteLine(f, "Reset = %s\n\n", g_kbd_reset)) return false;

  if (!WriteLine(f, "# Pause (dimmed - can see game)\n")) return false;
  if (g_kbd_pause_dimmed && *g_kbd_pause_dimmed)
    if (!WriteLine(f, "PauseDimmed = %s\n\n", g_kbd_pause_dimmed)) return false;

  if (!WriteLine(f, "# Pause (full pause)\n")) return false;
  if (g_kbd_pause && *g_kbd_pause)
    if (!WriteLine(f, "Pause = %s\n\n", g_kbd_pause)) return false;

  if (!WriteLine(f, "# Fast-forward (turbo)\n")) return false;
  if (g_kbd_turbo && *g_kbd_turbo)
    if (!WriteLine(f, "Turbo = %s\n\n", g_kbd_turbo)) return false;

  if (!WriteLine(f, "# Replay turbo mode\n")) return false;
  if (g_kbd_replay_turbo && *g_kbd_replay_turbo)
    if (!WriteLine(f, "ReplayTurbo = %s\n\n", g_kbd_replay_turbo)) return false;

  if (!WriteLine(f, "# Increase window size\n")) return false;
  if (g_kbd_window_bigger && *g_kbd_window_bigger)
    if (!WriteLine(f, "WindowBigger = %s\n\n", g_kbd_window_bigger)) return false;

  if (!WriteLine(f, "# Decrease window size\n")) return false;
  if (g_kbd_window_smaller && *g_kbd_window_smaller)
    if (!WriteLine(f, "WindowSmaller = %s\n\n", g_kbd_window_smaller)) return false;

  if (!WriteLine(f, "# Increase volume\n")) return false;
  if (g_kbd_volume_up && *g_kbd_volume_up)
    if (!WriteLine(f, "VolumeUp = %s\n\n", g_kbd_volume_up)) return false;

  if (!WriteLine(f, "# Decrease volume\n")) return false;
  if (g_kbd_volume_down && *g_kbd_volume_down)
    if (!WriteLine(f, "VolumeDown = %s\n\n", g_kbd_volume_down)) return false;

  if (!WriteLine(f, "\n")) return false;
  return true;
}

// Write [GamepadMap] section
static bool WriteGamepadMapSection(FILE *f, const Config *config) {
  (void)config;

  if (!WriteLine(f, "[GamepadMap]\n")) return false;
  if (!WriteLine(f, "# ==============================================================================\n")) return false;
  if (!WriteLine(f, "# Gamepad Controls\n")) return false;
  if (!WriteLine(f, "# ==============================================================================\n")) return false;
  if (!WriteLine(f, "# Configure physical gamepad/controller button mappings.\n")) return false;
  if (!WriteLine(f, "# ALL keyboard commands from [KeyMap] can be bound to gamepad buttons.\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (!WriteLine(f, "# Button names:\n")) return false;
  if (!WriteLine(f, "#   Face buttons: A, B, X, Y\n")) return false;
  if (!WriteLine(f, "#   D-pad: DpadUp, DpadDown, DpadLeft, DpadRight\n")) return false;
  if (!WriteLine(f, "#   Shoulders: L1/Lb, R1/Rb (bumpers), L2, R2 (triggers)\n")) return false;
  if (!WriteLine(f, "#   System: Start, Back, Guide\n")) return false;
  if (!WriteLine(f, "#   Thumbsticks: L3 (left stick click), R3 (right stick click)\n")) return false;
  if (!WriteLine(f, "#\n")) return false;
  if (!WriteLine(f, "# Modifiers: Use + to combine buttons (e.g., \"L1+A\" = hold L1 and press A)\n")) return false;
  if (!WriteLine(f, "# Multiple modifiers: \"L1+R1+A\" = hold both shoulders and press A\n")) return false;
  if (!WriteLine(f, "# ==============================================================================\n\n")) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Main Game Controls\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  if (!WriteLine(f, "# SNES controller button mapping for gamepad\n")) return false;
  if (!WriteLine(f, "# Order: Up, Down, Left, Right, Select, Start, A, B, X, Y, L, R\n")) return false;
  // Format the controls string
  char *controls_str = LauncherUI_FormatControlString(g_gamepad_controls);
  bool result = WriteLine(f, "Controls = %s\n\n", controls_str);
  free(controls_str);

  if (!result) return false;

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Quick Save/Load\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n\n")) return false;

  // Quick save/load bindings
  if (!WriteLine(f, "# Quick Save to slot 1 (L2+R3)\n")) return false;
  if (g_gamepad_save && *g_gamepad_save) {
    if (!WriteLine(f, "Save = %s\n\n", g_gamepad_save)) return false;
  } else {
    if (!WriteLine(f, "Save = L2+R3\n\n")) return false;  // default
  }

  if (!WriteLine(f, "# Quick Load from slot 1 (L2+L3)\n")) return false;
  if (g_gamepad_load && *g_gamepad_load) {
    if (!WriteLine(f, "Load = %s\n\n", g_gamepad_load)) return false;
  } else {
    if (!WriteLine(f, "Load = L2+L3\n\n")) return false;  // default
  }

  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Optional: Additional Gamepad Bindings\n")) return false;
  if (!WriteLine(f, "# ------------------------------------------------------------------------------\n")) return false;
  if (!WriteLine(f, "# Uncomment and customize any of these examples to add extra functionality\n")) return false;
  if (!WriteLine(f, "# to your gamepad. Useful for save states, turbo, cheats, etc.\n\n")) return false;

  if (!WriteLine(f, "# Save states (example: L2+face buttons for slots 1-4)\n")) return false;
  if (!WriteLine(f, "#Save = L2+A, L2+B, L2+X, L2+Y\n\n")) return false;

  if (!WriteLine(f, "# Load states (example: R2+face buttons for slots 1-4)\n")) return false;
  if (!WriteLine(f, "#Load = R2+A, R2+B, R2+X, R2+Y\n\n")) return false;

  if (!WriteLine(f, "# Replay states (example: L2+R2+face buttons)\n")) return false;
  if (!WriteLine(f, "#Replay = L2+R2+A, L2+R2+B, L2+R2+X, L2+R2+Y\n\n")) return false;

  if (!WriteLine(f, "# Turbo mode (hold for fast-forward)\n")) return false;
  if (!WriteLine(f, "#Turbo = L3\n\n")) return false;

  if (!WriteLine(f, "# Pause (dimmed, can still see game)\n")) return false;
  if (!WriteLine(f, "#PauseDimmed = Guide\n\n")) return false;

  if (!WriteLine(f, "# Reset game\n")) return false;
  if (!WriteLine(f, "#Reset = L1+R1+Start\n\n")) return false;

  if (!WriteLine(f, "# Fullscreen toggle\n")) return false;
  if (!WriteLine(f, "#Fullscreen = L3+R3\n\n")) return false;

  if (!WriteLine(f, "# Volume controls\n")) return false;
  if (!WriteLine(f, "#VolumeUp = DpadUp+L1\n")) return false;
  if (!WriteLine(f, "#VolumeDown = DpadDown+L1\n\n")) return false;

  if (!WriteLine(f, "# Cheats (example bindings)\n")) return false;
  if (!WriteLine(f, "#CheatLife = L2+R2+Start\n")) return false;
  if (!WriteLine(f, "#CheatKeys = L2+R2+Back\n")) return false;
  if (!WriteLine(f, "#CheatWalkThroughWalls = L1+R1+Back\n\n")) return false;

  if (!WriteLine(f, "# Toggle renderer (switch between PPU implementations)\n")) return false;
  if (!WriteLine(f, "#ToggleRenderer = L1+R1+Guide\n\n")) return false;

  if (!WriteLine(f, "# Display performance metrics\n")) return false;
  if (!WriteLine(f, "#DisplayPerf = L2+R2+Guide\n")) return false;

  if (!WriteLine(f, "\n")) return false;
  return true;
}

// Write complete INI file
bool ConfigWriter_Write(const char *path, const Config *config) {
  if (!path || !config) {
    LogError("ConfigWriter: NULL parameter");
    return false;
  }

  FILE *f = fopen(path, "w");
  if (!f) {
    LogError("ConfigWriter: Failed to open '%s' for writing", path);
    return false;
  }

  bool success = true;

  // Write header
  if (!WriteLine(f, "# ==============================================================================\n")) { success = false; goto cleanup; }
  if (!WriteLine(f, "# Zelda3 Configuration File\n")) { success = false; goto cleanup; }
  if (!WriteLine(f, "# ==============================================================================\n")) { success = false; goto cleanup; }
  if (!WriteLine(f, "# This file configures the Zelda3 reverse-engineered port of A Link to the Past.\n")) { success = false; goto cleanup; }
  if (!WriteLine(f, "#\n")) { success = false; goto cleanup; }
  if (!WriteLine(f, "# Note: zelda3.user.ini is loaded first if it exists, otherwise this file is used.\n")) { success = false; goto cleanup; }
  if (!WriteLine(f, "# You can use \"!include path/to/file.ini\" to include other config files.\n")) { success = false; goto cleanup; }
  if (!WriteLine(f, "#\n")) { success = false; goto cleanup; }
  if (!WriteLine(f, "# Boolean values accept: 0/1, true/false, yes/no, on/off\n")) { success = false; goto cleanup; }
  if (!WriteLine(f, "# ==============================================================================\n\n")) { success = false; goto cleanup; }

  // Write all sections
  if (!WriteGeneralSection(f, config)) { success = false; goto cleanup; }
  if (!WriteGraphicsSection(f, config)) { success = false; goto cleanup; }
  if (!WriteSoundSection(f, config)) { success = false; goto cleanup; }
  if (!WriteFeaturesSection(f, config)) { success = false; goto cleanup; }
  if (!WriteKeyMapSection(f, config)) { success = false; goto cleanup; }
  if (!WriteGamepadMapSection(f, config)) { success = false; goto cleanup; }

cleanup:
  fclose(f);

  if (success) {
    LogInfo("ConfigWriter: Wrote config to '%s'", path);
  } else {
    LogError("ConfigWriter: Failed to write config to '%s'", path);
  }

  return success;
}

// Validate Config structure
bool ConfigWriter_Validate(const Config *config, char *error_buf, size_t error_buf_size) {
  if (!config) {
    snprintf(error_buf, error_buf_size, "NULL config pointer");
    return false;
  }

  // Validate audio freq
  if (config->audio_freq != 11025 && config->audio_freq != 22050 &&
      config->audio_freq != 32000 && config->audio_freq != 44100 &&
      config->audio_freq != 48000) {
    snprintf(error_buf, error_buf_size, "Invalid audio frequency: %d", config->audio_freq);
    return false;
  }

  // Validate audio channels
  if (config->audio_channels < 1 || config->audio_channels > 2) {
    snprintf(error_buf, error_buf_size, "Invalid audio channels: %d", config->audio_channels);
    return false;
  }

  // Validate audio samples (must be power of 2)
  uint16 samples = config->audio_samples;
  if (samples < 128 || samples > 4096 || (samples & (samples - 1)) != 0) {
    snprintf(error_buf, error_buf_size, "Invalid audio samples: %d (must be power of 2, 128-4096)", samples);
    return false;
  }

  // Validate fullscreen mode
  if (config->fullscreen > 2) {
    snprintf(error_buf, error_buf_size, "Invalid fullscreen mode: %d", config->fullscreen);
    return false;
  }

  // Validate output method
  if (config->output_method > kOutputMethod_Vulkan) {
    snprintf(error_buf, error_buf_size, "Invalid output method: %d", config->output_method);
    return false;
  }

  // Validate extended aspect ratio
  if (config->extended_aspect_ratio > 3) {
    snprintf(error_buf, error_buf_size, "Invalid extended aspect ratio: %d", config->extended_aspect_ratio);
    return false;
  }

  // Validate MSU volume
  if (config->msuvolume > 100) {
    snprintf(error_buf, error_buf_size, "Invalid MSU volume: %d (must be 0-100)", config->msuvolume);
    return false;
  }

  return true;
}
