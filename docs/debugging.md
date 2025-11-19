# Debugging Zelda3

[Home](index.md) > Debugging

Comprehensive guide to debugging Zelda3, including built-in tools, workflows, and techniques for troubleshooting issues.

## Table of Contents

- [Built-in Debug Tools](#built-in-debug-tools)
- [Debug Console Output](#debug-console-output)
- [Snapshot System](#snapshot-system)
- [Verification Mode](#verification-mode)
- [Debug Keys and Cheats](#debug-keys-and-cheats)
- [Debug Builds](#debug-builds)
- [Debugging Workflows](#debugging-workflows)
- [GDB/LLDB Usage](#gdblldb-usage)
- [Common Debugging Scenarios](#common-debugging-scenarios)
- [Platform-Specific Debugging](#platform-specific-debugging)

## Built-in Debug Tools

Zelda3 includes several built-in debugging features that don't require recompilation:

### Snapshot System
The most powerful debugging tool - save/load game state with input history replay capabilities. See [Snapshot System](#snapshot-system) below for details.

### Turbo Mode
- **Key:** Tab
- Fast-forward gameplay to quickly test sequences or reach specific game states
- Useful for reproducing bugs that occur later in gameplay

### Performance Display
- **Key:** F
- Shows FPS and renderer performance metrics
- Helps identify performance bottlenecks

### Renderer Switching
- **Key:** R
- Toggle between fast and slow renderer
- Useful for debugging rendering issues

### Cheats
Various debug cheats to quickly set up test conditions. See [Debug Keys and Cheats](#debug-keys-and-cheats) for full list.

## Debug Console Output

### Event-Driven State Tracking

In debug builds (`CMAKE_BUILD_TYPE=Debug`), Zelda3 automatically logs game state changes to the console:

```
[DEBUG] Module changed: 9 -> 7 (Overworld)
[DEBUG] Room changed: 0x002 -> 0x018
[DEBUG] Entered dungeon (room 0x001)
[DEBUG] Sprite count: 8 -> 12
```

**What Gets Tracked:**
- Main module transitions (title screen, overworld, dungeon, etc.)
- Submodule changes within a module
- Room/area transitions
- Indoor/outdoor state changes
- Active sprite count changes

**Implementation Details:**
- Defined in `src/debug_state.h` and `src/debug_state.c`
- Only active when `kDebugFlag` is set (debug builds, not release)
- Uses `DebugState` struct to track previous frame state
- Called once per frame in `ZeldaRunFrame()` after game logic
- Zero runtime cost in release builds (compiled out via `#if kDebugFlag`)

**Usage:**
```c
// In your debugging code
#if kDebugFlag
DebugState_LogSnapshot("Before boss fight");
#endif
```

### Logging System

Zelda3 uses a centralized logging system (`src/logging.h`):

```c
#include "logging.h"

LogError("Failed to load asset: %s", filename);
LogWarn("Feature disabled: %s", feature_name);
LogInfo("Loaded config from: %s", config_path);
LogDebug("Frame counter: %d", frame_counter);
```

**Log Levels:**
- `LOG_ERROR` - Critical errors (always shown)
- `LOG_WARN` - Warnings (shown by default)
- `LOG_INFO` - Informational messages (shown by default)
- `LOG_DEBUG` - Debug details (hidden by default)

**Controlling Output:**
Set the `ZELDA3_LOG_LEVEL` environment variable:
```bash
export ZELDA3_LOG_LEVEL=DEBUG  # Show all messages
export ZELDA3_LOG_LEVEL=INFO   # Show info/warn/error (default)
export ZELDA3_LOG_LEVEL=WARN   # Show only warnings and errors
export ZELDA3_LOG_LEVEL=ERROR  # Show only errors
```

**Platform Notes:**
- ANSI colors automatically enabled for TTY output
- Android: Colors disabled (logcat doesn't support ANSI)
- Android: Use `adb logcat | grep Zelda3` to filter logs
- Switch/embedded: Logs to stderr

## Snapshot System

The snapshot system saves complete game state including input history, enabling deterministic replay and debugging.

### Basic Usage

**Saving Snapshots:**
- **Shift+F1 through Shift+F10** - Save to slot 1-10

**Loading Snapshots:**
- **F1 through F10** - Load from slot 1-10

**Replaying Snapshots:**
- **Ctrl+F1 through Ctrl+F10** - Replay from slot 1-10
  - Game will play back all recorded inputs from that point
  - Useful for verifying behavior consistency
  - Press **L** to stop replay early

### Snapshot Files

Snapshots are saved to disk as `.sav` files in the working directory:
```
slot1.sav
slot2.sav
...
slot10.sav
```

**What's Saved:**
- Complete RAM state (`g_ram` buffer)
- VRAM state
- SRAM state (save data)
- Joypad input history
- Frame counter
- Module/submodule state

### Replay Mode

When replaying a snapshot (Ctrl+F1-F10):
1. Game state restored to snapshot point
2. All subsequent inputs played back automatically
3. Game executes deterministically
4. Can enable turbo mode (T key) during replay
5. Can verify behavior matches original recording

**Use Cases:**
- **Bug Reproduction:** Save just before a bug occurs, replay to reproduce
- **Testing Fixes:** Replay after fixing to verify behavior
- **Performance Testing:** Replay with performance display (F key)
- **Regression Testing:** Ensure changes don't break existing gameplay

### Dungeon Playthroughs

Pre-recorded dungeon playthroughs for testing:
- **1-9 keys** - Load dungeon playthrough
- **Ctrl+1-9** - Replay dungeon in turbo mode

### Snapshot Management

**Clear Input History:**
- **K key** - Clears all input history from joypad log
- Useful when you want to save a state without replay data

**Stop Replay:**
- **L key** - Stop replaying a snapshot and regain control

### Turbo Replay Mode

- **T key** - Toggle replay turbo mode
- Replays execute as fast as possible
- Useful for quickly verifying long sequences

## Verification Mode

Verification mode runs the original SNES ROM code side-by-side with the C implementation and compares RAM state after each frame.

### Usage

```bash
# Run with verification enabled
./zelda3 path/to/zelda3.sfc

# With specific snapshot
./zelda3 path/to/zelda3.sfc --load-state slot1.sav
```

### How It Works

1. Loads original SNES ROM into emulator (`snes/` code)
2. Executes both C implementation and SNES emulator
3. Compares RAM state after each frame
4. Reports any mismatches

**Emulated Components:**
- 65816 CPU (`snes/cpu.c`)
- PPU (`snes/ppu.c`)
- DMA (`snes/dma.c`)
- SPC-700 audio CPU (`snes/spc.c`)
- DSP (`snes/dsp.c`)

### Interpreting Results

**Match (Good):**
```
Frame 1000: RAM state matches
```

**Mismatch (Needs Investigation):**
```
Frame 1000: RAM mismatch at offset 0x22
Expected: 0x0100
Got:      0x0102
```

**Common Mismatch Causes:**
- Enhanced features enabled (expected - feature flags change RAM)
- Timing differences in input handling
- Bug in C implementation
- Floating point differences

### When to Use Verification

- **Implementing New Features:** Verify original behavior first
- **Fixing Bugs:** Confirm fix matches original when bug fix disabled
- **Reverse Engineering:** Understand original behavior
- **Regression Testing:** Ensure changes don't break compatibility

## Debug Keys and Cheats

Full list of debug keys (from `src/main.c`):

### Gameplay Cheats

| Key | Action | Description |
|-----|--------|-------------|
| W | Fill Health/Magic | Restore full health and magic |
| Shift+W | Fill Rupees/Bombs/Arrows | Max out consumable items |
| O | Set Dungeon Key to 1 | Give small key for current dungeon |

### Development Tools

| Key | Action | Description |
|-----|--------|-------------|
| F | Show Performance | Display FPS and renderer stats |
| R | Toggle Renderer | Switch between fast/slow renderer |
| P | Pause (with dim) | Pause game with screen dimming |
| Shift+P | Pause (no dim) | Pause game without screen effect |

### State Management

| Key | Action | Description |
|-----|--------|-------------|
| F1-F10 | Load Snapshot | Load game state from slot |
| Shift+F1-F10 | Save Snapshot | Save game state to slot |
| Ctrl+F1-F10 | Replay Snapshot | Replay inputs from snapshot |
| K | Clear Input History | Remove all recorded inputs |
| L | Stop Replay | Exit replay mode |

### Speed Control

| Key | Action | Description |
|-----|--------|-------------|
| Tab | Turbo Mode | Fast-forward gameplay |
| T | Toggle Replay Turbo | Enable/disable turbo during replay |

### Window Management

| Key | Action | Description |
|-----|--------|-------------|
| Alt+Enter | Toggle Fullscreen | Switch fullscreen/windowed |
| Ctrl+Up | Increase Window Size | Make window larger |
| Ctrl+Down | Decrease Window Size | Make window smaller |

### Testing

| Key | Action | Description |
|-----|--------|-------------|
| Ctrl+E | Reset | Restart game |
| 1-9 | Load Dungeon Playthrough | Load pre-recorded dungeon |
| Ctrl+1-9 | Replay Dungeon (Turbo) | Fast replay of dungeon |

## Debug Builds

### Building for Debug

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

### Debug vs Release

**Debug Build Features:**
- `kDebugFlag = 1` (enables debug code)
- Debug console output (state tracking)
- No compiler optimizations (`-O0`)
- Debug symbols (`-g`)
- Assertions enabled
- Larger binary size
- Slower execution

**Release Build Features:**
- `kDebugFlag = 0` (disables debug code)
- No console output
- Full optimizations (`-O2` or `-O3`)
- Stripped symbols
- Assertions disabled via `NDEBUG`
- Smaller binary size
- Faster execution

### Debug Flag Usage in Code

```c
#include "types.h"

#if kDebugFlag
  // Debug-only code
  printf("Debug: link_x_coord = %d\n", link_x_coord);
  DebugState_LogSnapshot("checkpoint");
#endif

// Debug assertions
#if kDebugFlag
  assert(link_x_coord < 4096);
#endif
```

## Debugging Workflows

### Bug Reproduction Workflow

1. **Identify Bug:** Notice incorrect behavior during gameplay
2. **Save Snapshot:** Press Shift+F1 just before bug occurs
3. **Reproduce:** Press F1 to reload, verify bug happens consistently
4. **Replay:** Press Ctrl+F1 to auto-replay and watch closely
5. **Investigate:** Use debug build + logging to understand cause

### Feature Development Workflow

1. **Save Baseline:** Save snapshot before implementing feature
2. **Implement:** Write feature code
3. **Test:** Load snapshot and test feature in various scenarios
4. **Verify:** Replay snapshots to ensure deterministic behavior
5. **Compare:** Use verification mode to check compatibility

### Performance Investigation Workflow

1. **Capture Slow Section:** Save snapshot when performance drops
2. **Enable Display:** Press F to show FPS
3. **Replay:** Ctrl+F1 to replay with performance monitoring
4. **Profile:** Use external profiler (see GDB/LLDB section)
5. **Optimize:** Make changes and compare FPS
6. **Verify:** Ensure optimization doesn't break behavior

### Regression Testing Workflow

1. **Collect Snapshots:** Save key gameplay moments
2. **Make Changes:** Implement fixes or features
3. **Replay All:** Use Ctrl+F1-F10 to replay each snapshot
4. **Verify:** Ensure all replays complete without issues
5. **Check RAM:** Use verification mode for critical changes

## GDB/LLDB Usage

### Basic GDB Setup

```bash
# Build with debug symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# Run under GDB
gdb ./zelda3
(gdb) run

# With arguments
(gdb) run path/to/zelda3.sfc
```

### Useful GDB Commands

```gdb
# Set breakpoint
(gdb) break Zelda_RunFrame
(gdb) break player.c:1234

# Conditional breakpoint
(gdb) break main.c:200 if frame_counter > 1000

# Watch variable changes
(gdb) watch link_x_coord

# Print expressions
(gdb) print link_x_coord
(gdb) print *(uint16*)(g_ram+0x22)

# Continue execution
(gdb) continue
(gdb) step
(gdb) next

# Backtrace
(gdb) backtrace
(gdb) frame 2
```

### LLDB Setup (macOS)

```bash
# Run under LLDB
lldb ./zelda3
(lldb) run

# Set breakpoint
(lldb) breakpoint set --name Zelda_RunFrame
(lldb) breakpoint set --file player.c --line 1234

# Watch memory
(lldb) watchpoint set expression -- link_x_coord

# Print
(lldb) print link_x_coord
(lldb) expression *(uint16*)(g_ram+0x22)
```

### Debugging Crashes

```bash
# Get core dump
ulimit -c unlimited
./zelda3
# After crash
gdb ./zelda3 core

# Analyze
(gdb) backtrace
(gdb) frame 0
(gdb) info locals
(gdb) print variable_name
```

### Memory Debugging

**Valgrind (Linux):**
```bash
valgrind --leak-check=full ./zelda3
```

**AddressSanitizer:**
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-fsanitize=address"
cmake --build .
./zelda3
```

**UndefinedBehaviorSanitizer:**
```bash
cmake .. -DCMAKE_C_FLAGS="-fsanitize=undefined"
cmake --build .
./zelda3
```

## Common Debugging Scenarios

### Link Not Moving

**Symptoms:** Link frozen in place despite input

**Debug Steps:**
1. Check `link_state` variable
2. Check `link_disable_sprite_interaction`
3. Verify input handler receiving events
4. Check if module/submodule allows movement
5. Use debug console to see module state

**GDB Investigation:**
```gdb
(gdb) break Player_Main
(gdb) run
(gdb) print link_state
(gdb) print link_x_coord
(gdb) watch link_x_coord
```

### Sprite Not Appearing

**Symptoms:** Expected enemy/NPC doesn't spawn

**Debug Steps:**
1. Verify sprite should spawn in current room (check room data)
2. Check sprite count: `sprite_count` variable
3. Check sprite state array
4. Enable debug output to see sprite initialization
5. Compare with verification mode

**Debug Build Investigation:**
```c
#if kDebugFlag
LogDebug("Sprite %d state: %d, type: 0x%02x", i, sprite_state[i], sprite_type[i]);
#endif
```

### Graphics Corruption

**Symptoms:** Garbled or incorrect graphics

**Debug Steps:**
1. Check VRAM state
2. Verify PPU rendering
3. Test different renderer (R key)
4. Check palette data
5. Verify asset loading

**Renderer Debug:**
- Press R to switch renderer (OpenGL vs SDL)
- Press F to check rendering performance
- Check console for OpenGL errors

### Audio Issues

**Symptoms:** No sound, crackling, or wrong music

**Debug Steps:**
1. Check SDL audio initialization
2. Verify SPC file loaded correctly
3. Check audio mutex on Android
4. Verify MSU audio path (if using MSU)
5. Check volume settings in zelda3.ini

**Audio Debug (Android):**
```bash
adb logcat | grep -i audio
```

### Save Data Corruption

**Symptoms:** Save file not loading or wrong data

**Debug Steps:**
1. Check SRAM buffer state
2. Verify save/load code paths
3. Compare SRAM with snapshot
4. Check file I/O errors
5. Verify platform file API

**SRAM Investigation:**
```c
#if kDebugFlag
LogDebug("SRAM checksum: 0x%04x", CalculateSramChecksum());
#endif
```

### Performance Issues

**Symptoms:** Low FPS, stuttering

**Debug Steps:**
1. Press F to show FPS
2. Profile with external tools (gprof, Instruments, perf)
3. Check hot paths: `ZeldaRunFrame()`, PPU rendering
4. Verify no debug code in hot paths
5. Test release build vs debug build

**Linux Profiling:**
```bash
# Build with profiling
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_FLAGS="-pg"
cmake --build .

# Run and profile
./zelda3
gprof ./zelda3 gmon.out > analysis.txt
```

**macOS Profiling:**
```bash
# Use Instruments
xcode-select --install
instruments -t "Time Profiler" ./zelda3
```

## Platform-Specific Debugging

### Linux

**Tools:**
- GDB for debugging
- Valgrind for memory issues
- `perf` for profiling
- `strace` for system call tracing

**Console Output:**
```bash
./zelda3 2>&1 | tee debug.log
```

### macOS

**Tools:**
- LLDB for debugging
- Instruments for profiling
- Console.app for system logs

**Xcode Integration:**
```bash
# Generate Xcode project
cmake .. -G Xcode
open zelda3.xcodeproj
```

### Windows

**Tools:**
- Visual Studio debugger
- WinDbg for advanced debugging
- Visual Studio Profiler

**Visual Studio:**
```cmd
cmake .. -G "Visual Studio 17 2022"
# Open zelda3.sln in Visual Studio
# Press F5 to debug
```

**Debug Output:**
- Use OutputDebugString for Windows-specific logging
- View in DebugView or Visual Studio Output window

### Android

**Tools:**
- Android Studio debugger
- adb logcat
- Android Studio Profiler

**Debugging:**
```bash
# View logs
adb logcat | grep Zelda3

# Attach debugger
# In Android Studio: Run > Attach Debugger to Android Process

# Get backtrace on crash
adb logcat | grep "DEBUG"
```

**JNI Debugging:**
```bash
# Build debug APK
cd android
./gradlew assembleDebug

# Install and run
adb install app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.yourpackage.zelda3/.MainActivity

# Debug native code
ndk-gdb --start
```

**Performance:**
```bash
# Profile with simpleperf
adb shell simpleperf record -p $(adb shell pidof com.yourpackage.zelda3)
```

### Nintendo Switch

**Debugging:**
- Use nxlink for console output
- Test on actual hardware (emulators may behave differently)
- Check for Switch-specific issues (memory limits, performance)

```bash
# Run with console output
nxlink -s zelda3.nro
```

## Additional Resources

- **Architecture:** See [architecture.md](architecture.md) for code structure
- **Contributing:** See [contributing.md](contributing.md) for development guidelines
- **Building:** See [BUILDING.md](../BUILDING.md) for build instructions
- **Troubleshooting:** See [troubleshooting.md](troubleshooting.md) for common issues

## Tips and Best Practices

1. **Always Save Snapshots:** Before investigating a bug, save a snapshot for reproducibility
2. **Use Debug Builds:** Enable `kDebugFlag` for detailed console output
3. **Start Simple:** Use built-in tools (snapshots, cheats) before external debuggers
4. **Check Logs:** Many issues show warnings in console output
5. **Compare with Original:** Use verification mode to understand correct behavior
6. **Test in Release:** Some bugs only appear with optimizations enabled
7. **Platform Testing:** Test on actual hardware, not just emulators
8. **Replay Testing:** Use snapshot replay to verify fixes don't break determinism
9. **Profiling:** Don't guess at performance issues - measure with profiler
10. **Document Findings:** Keep notes on bugs and fixes for future reference
