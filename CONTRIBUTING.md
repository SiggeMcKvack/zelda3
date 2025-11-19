# Contributing to Zelda3

Thank you for your interest in contributing to Zelda3! This guide will help you get started.

## Quick Start

1. **Fork the repository** on GitHub
2. **Read the guides:**
   - [docs/contributing.md](docs/contributing.md) - Detailed contribution guidelines
   - [docs/development.md](docs/development.md) - Developer workflows and patterns
   - [docs/architecture.md](docs/architecture.md) - Technical architecture
3. **Set up your development environment:**
   ```bash
   git clone https://github.com/YOUR_USERNAME/zelda3.git
   cd zelda3
   # Install dependencies (see BUILDING.md)
   python3 assets/restool.py --extract-from-rom
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   cmake --build . -j$(nproc)
   ```
4. **Make your changes** on a feature branch
5. **Test thoroughly** (see [docs/contributing.md#testing-changes](docs/contributing.md#testing-changes))
6. **Submit a pull request**

## What to Contribute

We welcome contributions of all kinds:

- **Bug fixes** - Fix issues in game logic, rendering, audio, etc.
- **New features** - Add optional enhancements (must be toggleable)
- **Platform support** - Port to new platforms
- **Performance improvements** - Optimize hot paths
- **Documentation** - Improve guides, add examples, fix typos
- **Testing** - Report bugs, test on different platforms
- **Code quality** - Refactoring, cleanup, better error handling

## Guidelines

### Code Style

Zelda3 follows specific naming conventions from the reverse-engineering process:

- **Functions:** `Module_FunctionName()` or `ModuleFunctionName()`
- **Global variables:** `g_variable_name`
- **Constants:** `kConstantName`
- **RAM macros:** Use accessors from `variables.h`

See [docs/contributing.md#code-style-guidelines](docs/contributing.md#code-style-guidelines) for complete style guide.

### Compatibility

**Important:** This project preserves the original SNES game behavior for replay compatibility.

- **Original behavior must be preserved** (or gated behind optional feature flags)
- **Enhanced features must be optional** and disabled by default
- **Test with snapshot replay** to ensure deterministic behavior
- **Avoid breaking changes** to save files or replay compatibility

### Project Structure

```
zelda3/
├── src/              # Core game code
│   ├── player.c      # Link's movement/actions
│   ├── sprite_main.c # Enemy AI
│   ├── dungeon.c     # Dungeon logic
│   └── ...
├── snes/             # SNES hardware emulation
├── assets/           # Asset extraction tools
├── android/          # Android build system
├── docs/             # Documentation
└── build/            # Build directory (generated)
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed architecture overview.

## Development Resources

### Documentation

- **[BUILDING.md](BUILDING.md)** - Build instructions for all platforms
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - High-level architecture overview
- **[docs/architecture.md](docs/architecture.md)** - Detailed technical architecture
- **[docs/contributing.md](docs/contributing.md)** - Complete contribution guidelines
- **[docs/development.md](docs/development.md)** - Development workflows and patterns
- **[docs/debugging.md](docs/debugging.md)** - Debugging tools and techniques
- **[CHANGELOG.md](CHANGELOG.md)** - Recent changes and history

### Development Workflow

**Typical workflow:**
1. Create feature branch: `git checkout -b feature/my-feature`
2. Make changes in debug build (`CMAKE_BUILD_TYPE=Debug`)
3. Test with built-in tools (snapshots, debug keys)
4. Test in release build (`CMAKE_BUILD_TYPE=Release`)
5. Commit with clear message
6. Push and create pull request

See [docs/development.md](docs/development.md) for detailed workflows.

### Testing Your Changes

Before submitting a PR:

- [ ] Build in debug mode (no warnings)
- [ ] Build in release mode (no warnings)
- [ ] Run the game and test your changes manually
- [ ] Test with snapshot replay (F1-F10 keys)
- [ ] Verify original behavior preserved (if applicable)
- [ ] Test with feature enabled AND disabled (if applicable)
- [ ] Check for performance regressions (F key to show FPS)

See [docs/contributing.md#testing-changes](docs/contributing.md#testing-changes) for comprehensive testing guide.

## Pull Request Process

1. **Update documentation** if needed (README, CHANGELOG, docs/)
2. **Write clear commit messages** describing what and why
3. **Fill out PR description** with:
   - Summary of changes
   - Testing performed
   - Any breaking changes
   - Related issues
4. **Respond to review feedback** promptly and constructively
5. **Wait for approval** from maintainers

See [docs/contributing.md#submitting-pull-requests](docs/contributing.md#submitting-pull-requests) for complete PR guidelines.

## Reporting Issues

Found a bug? Have a feature idea? We'd love to hear about it!

**Before opening an issue:**
1. Search existing issues (open and closed)
2. Test with latest code
3. Verify with clean build
4. Gather details (platform, steps to reproduce, etc.)

**Open an issue with:**
- Clear description
- Steps to reproduce (for bugs)
- Expected vs actual behavior
- Platform and build information
- Screenshots/videos if helpful
- Snapshot file if applicable (for bugs)

See [docs/contributing.md#issue-reporting](docs/contributing.md#issue-reporting) for issue templates and guidelines.

## Getting Help

**For development questions:**
- Read [docs/development.md](docs/development.md) and [docs/architecture.md](docs/architecture.md)
- Search existing issues and pull requests
- Ask on [Discord](https://discord.gg/AJJbJAzNNJ)
- Open a GitHub Discussion

**For build problems:**
- See [BUILDING.md](BUILDING.md) troubleshooting section
- Check [docs/troubleshooting.md](docs/troubleshooting.md)
- Ask on Discord

**For debugging help:**
- See [docs/debugging.md](docs/debugging.md) for tools and techniques
- Use built-in debug tools (snapshots, debug keys)
- Ask on Discord

## Community Guidelines

We strive to maintain a welcoming and inclusive community:

- **Be respectful** - Treat everyone with kindness and respect
- **Be patient** - Contributors volunteer their time
- **Be constructive** - Provide helpful, actionable feedback
- **Be inclusive** - Welcome newcomers and help them learn
- **Be professional** - Keep discussions on-topic and productive

## Recognition

All contributors are valued and appreciated! Your contributions help preserve and enhance this classic game for everyone to enjoy.

**Contributors:**
- Check [GitHub contributors](https://github.com/snesrev/zelda3/graphs/contributors) for full list
- Original project by [snesrev](https://github.com/snesrev)
- Android port integration by [Waterdish](https://github.com/Waterdish/zelda3-android)
- Community disassembly efforts by [spannerisms](https://github.com/spannerisms/zelda3) and others

## External Resources

- **Original Project:** https://github.com/snesrev/zelda3
- **Android Fork:** https://github.com/Waterdish/zelda3-android
- **Discord Community:** https://discord.gg/AJJbJAzNNJ
- **Disassembly Reference:** https://github.com/spannerisms/zelda3

## License

This project is licensed under the MIT license. See [LICENSE.txt](LICENSE.txt) for details.

By contributing, you agree that your contributions will be licensed under the MIT License.

---

**Ready to contribute?** Check out the [detailed contributing guide](docs/contributing.md) and join us on [Discord](https://discord.gg/AJJbJAzNNJ)!

Thank you for helping make Zelda3 better!
