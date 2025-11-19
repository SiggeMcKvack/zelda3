# Contributing to Zelda3

[Home](index.md) > Contributing

Thank you for your interest in contributing to Zelda3! This guide will help you understand the project structure, code standards, and contribution workflow.

## Table of Contents

- [Getting Started](#getting-started)
- [Code Style Guidelines](#code-style-guidelines)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Testing Changes](#testing-changes)
- [Submitting Pull Requests](#submitting-pull-requests)
- [Issue Reporting](#issue-reporting)
- [Community Guidelines](#community-guidelines)
- [Additional Resources](#additional-resources)

## Getting Started

Before contributing, please:

1. **Read the documentation:**
   - [README.md](../README.md) - Project overview
   - [ARCHITECTURE.md](../ARCHITECTURE.md) - Code architecture
   - [development.md](development.md) - Development workflows
   - [BUILDING.md](../BUILDING.md) - Build instructions

2. **Join the community:**
   - Discord: https://discord.gg/AJJbJAzNNJ
   - Discuss ideas before implementing major changes

3. **Set up your environment:**
   - Follow [Development Setup](#development-setup)
   - Build successfully before making changes

4. **Understand the project goals:**
   - Preserve original game behavior
   - Maintain replay compatibility
   - Keep enhancements optional and disabled by default
   - Support multiple platforms

## Code Style Guidelines

Zelda3 follows a specific code style inherited from the reverse-engineering process. Consistency is important for maintainability.

### Naming Conventions

**Functions:**
- Module-prefixed: `Module_FunctionName()` (e.g., `Player_HandleMovement()`)
- Or camelCase: `ModuleFunctionName()` (e.g., `PlayerHandleMovement()`)
- Use descriptive names from disassembly when available

```c
// Good
void Player_HandleMovement(void);
void Sprite_InitializeEnemy(int sprite_id);

// Avoid
void move(void);
void init(int id);
```

**Variables:**

Global variables use `g_` prefix:
```c
// Good
uint8 *g_ram;
int g_frame_counter;

// Avoid
uint8 *ram;
int frameCounter;
```

RAM macros use descriptive names (may have `g_` prefix if they conflict with platform symbols):
```c
// In variables.h
#define link_x_coord (*(uint16*)(g_ram+0x22))
#define frame_counter (*(uint8*)(g_ram+0x1A))

// With g_ prefix to avoid Windows conflicts
#define g_r10 (*(uint16*)(g_ram+0x10))
```

Local variables use snake_case:
```c
// Good
int sprite_count;
uint16 room_id;

// Avoid
int spriteCount;
int RoomId;
```

**Constants:**
```c
// Use kConstantName
enum {
  kMaxSprites = 16,
  kScreenWidth = 256,
  kPpuExtraLeftRight = 96,
};

// Or for flags
#define kFeatures0_SwitchLR 2
```

**Enums:**
```c
// Anonymous enums for constants
enum {
  kConstant1 = 1,
  kConstant2 = 2,
};

// Named enums for types
typedef enum SpriteType {
  kSpriteType_Soldier = 0x41,
  kSpriteType_Octorok = 0x0D,
} SpriteType;
```

### Code Organization

**File Structure:**
```c
// Header file (module.h)
#ifndef ZELDA3_MODULE_H_
#define ZELDA3_MODULE_H_

#include "types.h"

// Type definitions
typedef struct ModuleState {
  uint16 x, y;
} ModuleState;

// Function declarations
void Module_Initialize(void);
void Module_Update(void);

#endif  // ZELDA3_MODULE_H_
```

```c
// Source file (module.c)
#include "module.h"
#include "variables.h"

// Static (file-local) functions
static void Module_Internal(void) {
  // Implementation
}

// Public functions
void Module_Initialize(void) {
  // Implementation
}
```

**Include Order:**
1. Own header (if applicable)
2. Project headers
3. System headers

```c
#include "player.h"          // Own header
#include "sprite.h"          // Project headers
#include "variables.h"
#include "types.h"
#include <stdio.h>           // System headers
#include <stdint.h>
```

### Memory Access

**Always use RAM macros:**
```c
// Good
link_x_coord = new_x;
uint16 x = link_x_coord;

// Bad - direct g_ram access
*(uint16*)(g_ram+0x22) = new_x;
```

**Access patterns:**
```c
// Read-modify-write
link_x_coord += velocity_x;

// Conditional
if (link_state == 0) {
  link_state = 1;
}

// Array access (when macro defines array)
sprite_x[i] = 100;
```

### Platform-Specific Code

Use semantic macros from `src/platform_detect.h`:

```c
#include "platform_detect.h"

#ifdef PLATFORM_ANDROID
  // Android-specific code
  #include <android/log.h>
  __android_log_print(ANDROID_LOG_DEBUG, "Zelda3", "Message");
#elif defined(PLATFORM_WINDOWS)
  // Windows-specific code
  #include <windows.h>
#elif defined(PLATFORM_MACOS)
  // macOS-specific code
#elif defined(PLATFORM_LINUX)
  // Linux-specific code
#endif
```

Available macros:
- Platforms: `PLATFORM_WINDOWS`, `PLATFORM_MACOS`, `PLATFORM_LINUX`, `PLATFORM_ANDROID`, `PLATFORM_SWITCH`
- Compilers: `COMPILER_MSVC`, `COMPILER_GCC`, `COMPILER_CLANG`
- Architectures: `ARCH_X64`, `ARCH_X86`, `ARCH_ARM64`, `ARCH_ARM32`

### Comments

```c
// Single-line comments for brief explanations
link_x_coord = 0;  // Reset position

// Multi-line comments for complex logic
/*
 * This function handles the complex interaction between
 * sprite collision and player state transitions.
 */

// Document non-obvious behavior
// SNES bug: Original game has off-by-one error here
// We preserve it for compatibility
if (x < 255) {  // Not <= 255
  // ...
}
```

### Error Handling

Use the logging system from `src/logging.h`:

```c
#include "logging.h"

// Error messages
if (!LoadAssets()) {
  LogError("Failed to load assets from: %s", path);
  return false;
}

// Warnings
if (invalid_config) {
  LogWarn("Invalid config value '%s', using default", value);
}

// Debug info
#if kDebugFlag
LogDebug("Sprite %d initialized at (%d, %d)", id, x, y);
#endif
```

### Debug Code

Use `kDebugFlag` to gate debug-only code:

```c
#include "types.h"

#if kDebugFlag
  // Debug-only code (compiled out in release builds)
  LogDebug("Module changed: %d -> %d", prev_module, main_module_index);
  assert(link_x_coord < 4096);
#endif
```

## Development Setup

### 1. Fork and Clone

```bash
# Fork on GitHub first
git clone https://github.com/YOUR_USERNAME/zelda3.git
cd zelda3
git remote add upstream https://github.com/snesrev/zelda3.git
```

### 2. Install Dependencies

See [BUILDING.md](../BUILDING.md) for platform-specific instructions.

**Quick setup (Linux/macOS):**
```bash
# Install SDL2 and Opus
sudo apt install libsdl2-dev libopus-dev cmake  # Ubuntu/Debian
brew install sdl2 opus cmake                     # macOS

# Install Python dependencies
python3 -m pip install -r requirements.txt
```

### 3. Extract Assets

```bash
# Place zelda3.sfc (US version) in project root
python3 assets/restool.py --extract-from-rom
```

### 4. Build Debug Version

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

### 5. Verify Setup

```bash
# Run the game
./zelda3

# You should see the title screen and be able to play
```

## Making Changes

### Branch Workflow

```bash
# Update your fork
git checkout master
git pull upstream master

# Create feature branch
git checkout -b feature/my-feature-name

# Make changes...
git add .
git commit -m "Add feature: description"

# Push to your fork
git push origin feature/my-feature-name
```

### Commit Messages

Write clear, descriptive commit messages:

**Good:**
```
Add L/R item switching feature

- Define kFeatures0_SwitchLR flag in features.h
- Implement item cycling in player.c
- Add configuration option in zelda3.ini
- Update documentation
```

**Avoid:**
```
fixed bug
wip
update
```

**Format:**
- First line: Brief summary (50 chars or less)
- Blank line
- Detailed description (if needed)
- List specific changes

### Adding Features

See [development.md](development.md#adding-features) for detailed workflow. Quick checklist:

1. **Define feature flag** (`src/features.h`):
   ```c
   enum {
     // ... existing flags
     kFeatures0_MyFeature = 65536,  // Next power of 2
   };
   ```

2. **Store in RAM** (next unused offset):
   ```c
   // features.h
   #define my_feature_state (*(uint8*)(g_ram+0x659))
   ```

3. **Add INI option** (`zelda3.ini`):
   ```ini
   [Features]
   EnableMyFeature = 0
   ```

4. **Parse in config.c:**
   ```c
   if (strcmp(key, "EnableMyFeature") == 0) {
     if (value[0] == '1') {
       g_wanted_zelda_features |= kFeatures0_MyFeature;
     }
   }
   ```

5. **Implement feature** (relevant module):
   ```c
   if (enhanced_features0 & kFeatures0_MyFeature) {
     // Feature implementation
   }
   ```

6. **Test thoroughly** (see [Testing Changes](#testing-changes))

### Fixing Bugs

1. **Reproduce the bug:**
   - Save snapshot before bug occurs (Shift+F1)
   - Document steps to reproduce

2. **Investigate:**
   - Use debug build with console output
   - Check relevant code in affected module
   - Compare with verification mode if needed

3. **Fix the bug:**
   - Make minimal changes to fix the issue
   - Preserve original behavior (or gate fix with feature flag)

4. **Test the fix:**
   - Verify bug is fixed
   - Test with snapshot replay
   - Ensure no regressions

5. **Document the fix:**
   - Explain the bug in commit message
   - Add comments if behavior is non-obvious

### Compatibility Considerations

**Original Bugs:**
- Some SNES bugs are preserved for replay compatibility
- If fixing an original bug, gate it with a feature flag
- Document why the bug is preserved

```c
// Original SNES bug: Off-by-one error in boundary check
// We preserve this for replay compatibility
if (enhanced_features0 & kFeatures0_MiscBugFixes) {
  // Fixed behavior
  if (x <= 255) { /* ... */ }
} else {
  // Original buggy behavior
  if (x < 255) { /* ... */ }
}
```

**Replay Compatibility:**
- Changes to game logic may break replay files
- Test with existing snapshots
- Document breaking changes in commit message

**Enhanced Features:**
- Must be optional (disabled by default)
- Must be toggleable via zelda3.ini
- Must not affect gameplay when disabled

## Testing Changes

### 1. Manual Testing

```bash
# Build and run
cd build
cmake --build .
./zelda3

# Test your changes
# Try various scenarios
# Save snapshots at key points (Shift+F1-F10)
```

### 2. Snapshot Testing

```bash
# Create test snapshots before your changes
./zelda3
# Play to interesting points, save with Shift+F1-F10

# After changes, replay to verify behavior
./zelda3
# Press Ctrl+F1-F10 to replay each snapshot
# Verify gameplay is correct
```

### 3. Verification Mode

```bash
# Test against original ROM (if logic changes)
./zelda3 path/to/zelda3.sfc

# Should see "RAM state matches" messages
# Mismatches indicate potential issues
```

### 4. Cross-Platform Testing

Test on multiple platforms if possible:
- Linux
- macOS
- Windows
- Android (if changes affect mobile)

### 5. Performance Testing

```bash
# Release build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Run with performance display
./zelda3
# Press F to show FPS
# Monitor for performance regressions
```

### 6. Code Quality Checks

```bash
# Build with warnings
cmake .. -DENABLE_WERROR=ON
cmake --build .

# Should build without warnings
```

## Submitting Pull Requests

### Before Submitting

**Checklist:**
- [ ] Code follows style guidelines
- [ ] Changes are well-tested (manual + snapshot testing)
- [ ] No compiler warnings
- [ ] Documentation updated (if needed)
- [ ] Commit messages are clear
- [ ] Feature is optional (if adding new feature)
- [ ] No regressions in existing functionality
- [ ] Works in both debug and release builds

### Pull Request Process

1. **Push your branch:**
   ```bash
   git push origin feature/my-feature-name
   ```

2. **Create PR on GitHub:**
   - Go to your fork on GitHub
   - Click "Pull Request"
   - Select your feature branch
   - Fill out PR template (if available)

3. **Write a good PR description:**

   ```markdown
   ## Summary
   Brief description of what this PR does.

   ## Changes
   - Added X feature
   - Fixed Y bug
   - Updated Z documentation

   ## Testing
   - Tested on Linux/Windows/macOS
   - Verified with snapshot replay
   - Checked with verification mode
   - No performance regression

   ## Related Issues
   Fixes #123
   Related to #456
   ```

4. **Respond to feedback:**
   - Address reviewer comments
   - Push updates to the same branch
   - Be open to suggestions

5. **Wait for merge:**
   - Maintainers will review and merge
   - May request changes
   - Be patient - review takes time

### PR Guidelines

**Do:**
- Keep PRs focused on one feature/fix
- Write clear descriptions
- Test thoroughly
- Respond to feedback promptly
- Follow up if PR goes stale

**Don't:**
- Mix unrelated changes in one PR
- Submit untested code
- Ignore reviewer feedback
- Force-push after review (unless requested)
- Argue excessively - maintainers have final say

## Issue Reporting

### Before Opening an Issue

1. **Search existing issues:**
   - Someone may have already reported it
   - Check both open and closed issues

2. **Verify the issue:**
   - Test with latest code
   - Try with clean build
   - Reproduce consistently

3. **Gather information:**
   - Platform and OS version
   - Build configuration (Debug/Release)
   - Steps to reproduce
   - Expected vs actual behavior
   - Screenshots/videos if applicable

### Creating a Good Issue

**Bug Report Template:**

```markdown
**Bug Description:**
Clear description of what's wrong.

**Steps to Reproduce:**
1. Launch game
2. Go to specific location
3. Perform action X
4. Bug occurs

**Expected Behavior:**
What should happen.

**Actual Behavior:**
What actually happens.

**Environment:**
- Platform: Linux / macOS / Windows / Android
- OS Version: Ubuntu 22.04 / macOS 13.0 / Windows 11
- Build Type: Debug / Release
- Commit: abc123 or version number

**Additional Context:**
- Snapshot file (if available)
- Console output
- Screenshots
- Any relevant configuration
```

**Feature Request Template:**

```markdown
**Feature Description:**
Clear description of the proposed feature.

**Use Case:**
Why is this feature useful?
What problem does it solve?

**Proposed Implementation:**
High-level idea of how this could work.

**Compatibility Considerations:**
Should this be optional?
Does it affect replays?
Platform-specific concerns?

**Alternatives Considered:**
Other ways to achieve similar goals.
```

### Issue Etiquette

**Do:**
- Be respectful and professional
- Provide detailed information
- Follow up with requested details
- Close issue if resolved
- Thank contributors

**Don't:**
- Demand immediate fixes
- Post "+1" comments (use reactions)
- Go off-topic
- Bump issues excessively
- Be rude or aggressive

## Community Guidelines

### Code of Conduct

- **Be respectful:** Treat everyone with respect and kindness
- **Be patient:** Contributors volunteer their time
- **Be constructive:** Provide helpful feedback
- **Be inclusive:** Welcome newcomers
- **Be professional:** Keep discussions on-topic

### Communication Channels

**Discord:** https://discord.gg/AJJbJAzNNJ
- Real-time discussion
- Help with development
- Coordinate larger changes
- Community chat

**GitHub Issues:**
- Bug reports
- Feature requests
- Technical discussions

**GitHub Discussions:**
- General questions
- Ideas and proposals
- Showcase your work

**Pull Requests:**
- Code review
- Implementation discussion
- Focused on specific changes

### Getting Help

**For development questions:**
1. Read the documentation ([development.md](development.md), [ARCHITECTURE.md](../ARCHITECTURE.md))
2. Search existing issues/PRs
3. Ask on Discord
4. Open a GitHub Discussion

**For bug reports:**
1. Verify it's a bug (see [troubleshooting.md](troubleshooting.md))
2. Search existing issues
3. Open a new issue with details

**For feature ideas:**
1. Discuss on Discord first
2. Check if it aligns with project goals
3. Open a GitHub Discussion
4. Create detailed feature request

## Additional Resources

### Documentation

- **[README.md](../README.md)** - Project overview and quick start
- **[BUILDING.md](../BUILDING.md)** - Detailed build instructions
- **[ARCHITECTURE.md](../ARCHITECTURE.md)** - Code architecture and design
- **[CHANGELOG.md](../CHANGELOG.md)** - Recent changes and history
- **[development.md](development.md)** - Development workflows and patterns
- **[debugging.md](debugging.md)** - Debugging tools and techniques
- **[architecture.md](architecture.md)** - Technical architecture details

### External Resources

- **Original Project:** https://github.com/snesrev/zelda3
- **Android Fork:** https://github.com/Waterdish/zelda3-android
- **Disassembly:** https://github.com/spannerisms/zelda3 (reference)
- **SNES Documentation:** Various SNES development docs
- **Discord Community:** https://discord.gg/AJJbJAzNNJ

### Learning Resources

**Understanding the Codebase:**
1. Read [ARCHITECTURE.md](../ARCHITECTURE.md) for high-level overview
2. Study small modules first (e.g., `src/hud.c`)
3. Use verification mode to understand original behavior
4. Ask questions on Discord

**Reverse Engineering:**
- Variable names come from community disassembly
- Function behavior matches original SNES code
- Not always "clean" code - preserves original structure

**SNES Hardware:**
- PPU (Picture Processing Unit) - graphics rendering
- SPC-700 - audio processor
- 65816 CPU - main processor
- Understanding these helps with verification mode

## Recognition

All contributors are valued and appreciated! Your contributions help preserve and enhance this classic game for everyone to enjoy.

**Ways to Contribute:**
- Code (features, fixes, optimizations)
- Documentation (guides, examples, corrections)
- Testing (bug reports, platform testing)
- Community (help others, answer questions)
- Assets (shaders, configurations)

Thank you for contributing to Zelda3!

---

**Questions?** Join us on Discord: https://discord.gg/AJJbJAzNNJ
