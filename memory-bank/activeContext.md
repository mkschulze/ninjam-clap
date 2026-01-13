# Active Context - JamWide Plugin

## Current Session Focus

**Date:** 2026-01-13  
**Phase:** 6 - Beta Release  
**Status:** 🔄 macOS keyboard input bug discovered (REAPER)

## Latest Build: r120

### What's Working
- ✅ Plugin loads in GarageBand, Logic Pro (AU), REAPER (CLAP/VST3) - macOS
- ✅ Plugin loads in Bitwig Studio, REAPER (CLAP/VST3) - Windows
- ✅ **Multi-format builds**: CLAP, VST3, Audio Unit v2 (macOS + Windows)
- ✅ **Windows build system**: Visual Studio 2022/2026, MSBuild, PowerShell install script
- ✅ **Windows keyboard input**: Message hook + dummy EDIT control (spacebar/Caps Lock fixed)
- ✅ **Windows IME support**: Japanese/Chinese/Korean keyboard input
- ✅ **GitHub Actions CI/CD**: Automated builds for Windows and macOS
- ✅ Connection to public NINJAM servers
- ✅ Server browser with **live usernames** (autosong.ninjam.com)
- ✅ License agreement dialog (single-click fix)
- ✅ BPM/BPI display and voting via chat
- ✅ Remote users with volume/pan/mute/solo
- ✅ Chat room with message history and timestamps
- ✅ Visual timing guide with color zones
- ✅ VU meters for all channels
- ✅ Default audio quality: 256 kbps (highest)
- ✅ Anonymous login support
- ✅ Window size 800x1200 for AU (Logic/GarageBand compatibility)

### Recent Changes (v0.117-v0.120)
| Version | Change |
|---------|--------|
| v0.120 | Pulled Windows bug fix, verified macOS build works |
| v0.119 | Windows: Message hook prevents DAW accelerators during text input |
| v0.117 | Windows: Dummy EDIT control + IME/focus forwarding + null guards |
| v0.108 | UI: Transmit toggle now visible (layout fix) |
| v0.107 | Fix: License dialog responds to single click |

### Known Issues
| Issue | Platform | Status |
|-------|----------|--------|
| **Spacebar triggers transport instead of text input** | macOS (REAPER) | 🔴 Active - see plan-reaper-macos-input.md |
| Spacebar works in GarageBand AU but not REAPER | macOS | REAPER intercepts keys at window level |
| Bitwig Beta 11 plugin scan | macOS | Bitwig bug - localhost connection refused |
| AU resize in Logic/GarageBand | macOS | Apple limitation - use fixed 800x1200 size |
| `NinjamView` class not renamed to `JamWideView` | macOS | Low priority cleanup |

## CI/CD

GitHub Actions builds on every tag push:
- **macOS**: Universal binary (x86_64 + arm64)
- **Windows**: x64 with Visual Studio 2022
- Artifacts uploaded to GitHub Releases
# ~/Library/Audio/Plug-Ins/CLAP/
# ~/Library/Audio/Plug-Ins/VST3/
# ~/Library/Audio/Plug-Ins/Components/
```

**Windows:** `install-win.ps1` script builds and installs to:
```powershell
# %LOCALAPPDATA%\Programs\Common\CLAP\
# %LOCALAPPDATA%\Programs\Common\VST3\
```

### Logging Macros
| Macro | Description |
|-------|-------------|
| `NLOG(...)` | Always logs (errors, status changes) |
| `NLOG_VERBOSE(...)` | Only in dev builds (per-frame debug) |

## New Files Added (r90-r92)

| File | Purpose |
|------|---------|
| `src/plugin/clap_entry_export.cpp` | CLAP entry export shim for clap-wrapper |
| `tools/check_imgui_ids.py` | ImGui ID hygiene checker script |

## Key Architecture (clap-wrapper)

| Component | Description |
|-----------|-------------|
| `ninjam-impl` | Static library with all plugin code |
| `clap_entry_export.cpp` | Tiny export file recompiled per format |
| `make_clapfirst_plugins()` | CMake function that generates all formats |
| `memory-bank/plan-visual-latency-guide.md` | Timing guide implementation plan |
| `release.sh` | Automated release script |

## Key Architectural Features

| Feature | Description |
|---------|-------------|
| **Command Queue** | UI sends UiCommand to run thread via cmd_queue |
| **Chat Queue** | Run thread pushes ChatMessage to UI via chat_queue |
| **Transient Detection** | Audio thread detects peaks for timing guide |
| **Anonymous Login** | Auto-prefix "anonymous:" for public server compatibility |

## Priority Actions for Next Session

1. **Fix macOS keyboard input in REAPER** - Implement plan in `plan-reaper-macos-input.md`
   - Use local event monitor to intercept keys when `io.WantTextInput` is true
   - Detect REAPER host via bundle ID `com.cockos.reaper`
   - Forward consumed events to ImGui's `KeyEventResponder`
2. **Rename `NinjamView` to `JamWideView`** - Cleanup in `gui_macos.mm`
3. **Timing guide polish** - Add tooltips, reset button, color-coded dots per offset
4. **End-to-end audio test** - Connect when other musicians are online

## Key Files

| File | Purpose |
|------|---------|
| `src/threading/run_thread.cpp` | Main network thread - processes commands, handles chat, anonymous login |
| `src/threading/ui_command.h` | UiCommand variant types (UI→Run thread) |
| `src/ui/ui_chat.cpp` | Chat room panel with message history |
| `src/ui/ui_latency_guide.cpp` | Visual timing guide with beat grid |
| `src/ui/ui_remote.cpp` | Remote users panel reads NJClient under `client_mutex` |
| `src/plugin/clap_entry.cpp` | Audio processing with transient detection |
| `src/debug/logging.h` | Logging macros |
| `release.sh` | Automated release workflow |

## Build Commands

```bash
# Build and install (increments build number)
./install.sh

# Current build: r67 (DEV BUILD)
# Installs to: ~/Library/Audio/Plug-Ins/CLAP/JamWide.clap
```

## Debug Logging

```bash
# Watch live log
tail -f /tmp/jamwide.log

# Clear log before test
: > /tmp/jamwide.log
```

## Test Server

```bash
# Public server
ninbot.com:2049  user: anonymous

# Local test server
/Users/cell/dev/ninjam/ninjam/server/ninjamsrv /tmp/ninjam-test.cfg
```
