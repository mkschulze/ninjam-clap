# Plan: JUCE Port for JamWide

## Goal

Port JamWide from ImGui to JUCE for a more polished, professional UI while keeping the battle-tested NJClient core and threading architecture.

## Why JUCE?

| Advantage | Description |
|-----------|-------------|
| **Mature ecosystem** | 20+ years of audio plugin development |
| **Documentation** | Extensive tutorials, API docs, active forum |
| **LookAndFeel** | Easy theming without custom drawing code |
| **Components** | Built-in sliders, buttons, tables, text editors |
| **Cross-platform** | Single codebase for macOS, Windows, Linux |
| **Plugin formats** | VST2, VST3, AU, AAX, LV2, CLAP (via wrapper), Standalone |
| **Community** | Quick answers, lots of examples |

## License Consideration

- **GPL version** - Free, requires open-source distribution (we're already GPL-2.0 ✅)
- **Commercial license** - One-time purchase if closed-source needed
- **Recommendation** - Use GPL JUCE, matches our existing license

---

## Architecture

### What We Keep

```
src/core/          → NJClient, netmsg, mpb, njmisc (unchanged)
src/threading/     → run_thread, spsc_ring, ui_command (unchanged)
src/net/           → server_list fetcher (unchanged)
wdl/               → JNetLib, WDL utilities (unchanged)
```

### What We Replace

| Current (ImGui) | JUCE Replacement |
|-----------------|------------------|
| `gui_macos.mm` | JUCE handles platform |
| `gui_win32.cpp` | JUCE handles platform |
| `ui_main.cpp` | `PluginEditor.cpp` |
| `ui_chat.cpp` | `ChatComponent.cpp` |
| `ui_remote.cpp` | `MixerComponent.cpp` |
| `ui_status.cpp` | `StatusBar.cpp` |
| `ui_local.cpp` | `LocalChannelComponent.cpp` |
| `ui_latency_guide.cpp` | `TimingGuideComponent.cpp` |
| Metal/D3D11 backends | JUCE OpenGL/Metal/D3D |
| ImGui widgets | JUCE Component classes |
| clap-wrapper | JUCE plugin wrapper |

### Threading Model (Unchanged)

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Audio Thread   │     │   Run Thread    │     │   UI Thread     │
│  (JUCE)         │     │   (NJClient)    │     │   (JUCE)        │
│                 │     │                 │     │                 │
│ processBlock()  │     │ Run() loop      │     │ Components      │
│      │          │     │ chat_queue ─────┼────►│ repaint()       │
│      ▼          │     │ ◄───── cmd_queue│◄────┤ button clicks   │
│ AudioProc()     │     │                 │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

---

## Project Structure

```
JamWide/
├── CMakeLists.txt                    # Updated for JUCE
├── libs/
│   └── JUCE/                         # JUCE as submodule
├── Source/
│   ├── PluginProcessor.cpp           # Audio processing
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp              # Main UI container
│   ├── PluginEditor.h
│   ├── Components/
│   │   ├── ConnectionPanel.h/cpp     # Server, username, password
│   │   ├── StatusBar.h/cpp           # Connected, BPM/BPI, beat
│   │   ├── ChatPanel.h/cpp           # Message list, input field
│   │   ├── LocalChannelPanel.h/cpp   # Local channel controls
│   │   ├── MasterPanel.h/cpp         # Master + metronome
│   │   ├── MixerPanel.h/cpp          # Remote users grid
│   │   ├── ServerBrowser.h/cpp       # Server list table
│   │   └── TimingGuide.h/cpp         # Beat alignment visualization
│   ├── LookAndFeel/
│   │   ├── JamWideLookAndFeel.h/cpp  # Custom theming
│   │   └── Colors.h                  # Color palette
│   └── Core/                         # Symlink to existing src/core
├── Resources/
│   ├── fonts/
│   │   └── Inter-Regular.ttf         # Embedded font
│   └── images/
│       └── logo.png
└── wdl/                              # Keep existing WDL
```

---

## Component Design

### PluginProcessor

```cpp
class JamWideProcessor : public juce::AudioProcessor {
    std::unique_ptr<NJClient> client;
    std::shared_ptr<JamWidePlugin> plugin;  // Reuse existing state struct
    std::thread run_thread;
    
    void processBlock(AudioBuffer<float>&, MidiBuffer&) override {
        // Call client->AudioProc() 
    }
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override {
        // Initialize NJClient
    }
};
```

### PluginEditor

```cpp
class JamWideEditor : public juce::AudioProcessorEditor, public Timer {
    StatusBar statusBar;
    ConnectionPanel connectionPanel;
    ChatPanel chatPanel;
    LocalChannelPanel localPanel;
    MasterPanel masterPanel;
    MixerPanel mixerPanel;
    ServerBrowser serverBrowser;
    TimingGuide timingGuide;
    
    void timerCallback() override {
        // Poll queues, update UI
    }
    
    void resized() override {
        // Layout components
    }
};
```

### ChatPanel

```cpp
class ChatPanel : public Component {
    TextEditor messageList;      // Read-only, shows history
    TextEditor inputField;       // User types here
    TextButton sendButton;
    
    void sendMessage() {
        // Push SendChatCommand to queue
    }
};
```

### MixerPanel

```cpp
class MixerPanel : public Component {
    OwnedArray<ChannelStrip> channelStrips;
    
    void updateFromNJClient() {
        // Read remote users under mutex, update strips
    }
};

class ChannelStrip : public Component {
    Label nameLabel;
    Slider volumeSlider;
    Slider panSlider;
    TextButton muteButton;
    TextButton soloButton;
    LevelMeter meter;
};
```

---

## Custom LookAndFeel

```cpp
class JamWideLookAndFeel : public juce::LookAndFeel_V4 {
public:
    JamWideLookAndFeel() {
        // Dark theme colors
        setColour(ResizableWindow::backgroundColourId, Colour(0xff1a1a2e));
        setColour(Slider::thumbColourId, Colour(0xff4a9eff));
        setColour(TextButton::buttonColourId, Colour(0xff2d2d44));
        // ... more colors
    }
    
    void drawRotarySlider(...) override {
        // Custom knob drawing
    }
    
    void drawLinearSlider(...) override {
        // Custom fader drawing
    }
};
```

### Color Palette

```cpp
namespace Colors {
    const Colour background    = Colour(0xff1a1a2e);
    const Colour panel         = Colour(0xff252538);
    const Colour accent        = Colour(0xff4a9eff);
    const Colour accentHover   = Colour(0xff6ab0ff);
    const Colour text          = Colour(0xffe0e0e0);
    const Colour textDim       = Colour(0xff808080);
    const Colour meterGreen    = Colour(0xff00c853);
    const Colour meterYellow   = Colour(0xffffd600);
    const Colour meterRed      = Colour(0xffff1744);
    const Colour connected     = Colour(0xff00e676);
    const Colour disconnected  = Colour(0xffff5252);
}
```

---

## Implementation Phases

### Phase 1: Project Setup (4h)
- [ ] Add JUCE as git submodule
- [ ] Create CMakeLists.txt for JUCE
- [ ] Set up basic PluginProcessor/Editor
- [ ] Verify builds on macOS and Windows

### Phase 2: Audio Integration (4h)
- [ ] Wire processBlock() to AudioProc()
- [ ] Initialize NJClient in prepareToPlay()
- [ ] Start/stop run thread with plugin lifecycle
- [ ] Verify audio passthrough works

### Phase 3: Basic UI Shell (8h)
- [ ] Create main layout with panels
- [ ] Implement ConnectionPanel (server/user/pass inputs)
- [ ] Implement StatusBar (connection status, BPM/BPI)
- [ ] Connect/disconnect button working

### Phase 4: Chat Panel (6h)
- [ ] Message display with timestamps
- [ ] Input field with send button
- [ ] Wire to chat_queue
- [ ] Auto-scroll on new messages

### Phase 5: Mixer Panel (10h)
- [ ] Remote users display
- [ ] Channel strips with volume/pan
- [ ] Mute/solo buttons
- [ ] VU meters (custom component)
- [ ] Master + metronome controls

### Phase 6: Server Browser (6h)
- [ ] TableListBox with server data
- [ ] Fetch from autosong.ninjam.com
- [ ] "Use" button to populate connection
- [ ] Refresh button

### Phase 7: Timing Guide (4h)
- [ ] Custom paint() for beat grid
- [ ] Transient dots display
- [ ] Sensitivity slider
- [ ] Early/late indicator

### Phase 8: Theming & Polish (6h)
- [ ] Custom LookAndFeel class
- [ ] Embed custom font
- [ ] Hover states, animations
- [ ] Window resizing behavior

### Phase 9: Testing (8h)
- [ ] Test in REAPER (macOS/Windows)
- [ ] Test in Ableton Live
- [ ] Test in Logic Pro / GarageBand (AU)
- [ ] Test in Bitwig Studio
- [ ] Multi-instance testing
- [ ] State save/load testing

---

## CMake Integration

```cmake
cmake_minimum_required(VERSION 3.22)
project(JamWide VERSION 2.0.0)

add_subdirectory(libs/JUCE)

juce_add_plugin(JamWide
    COMPANY_NAME "JamWide"
    PLUGIN_MANUFACTURER_CODE JMWD
    PLUGIN_CODE JWDE
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "JamWide"
    BUNDLE_ID "com.jamwide.client"
)

target_sources(JamWide PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Components/ConnectionPanel.cpp
    # ... more sources
    
    # Existing core (symlinked or copied)
    src/core/njclient.cpp
    src/core/netmsg.cpp
    src/core/mpb.cpp
    src/core/njmisc.cpp
)

target_link_libraries(JamWide
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
)
```

---

## Migration Strategy

### Option A: New Branch (Recommended)
1. Create `juce-port` branch
2. Build JUCE version alongside ImGui
3. Merge when feature-complete
4. Release as v2.0.0

### Option B: Gradual Replacement
1. Add JUCE as additional build target
2. Replace one panel at a time
3. More complex, not recommended

---

## Estimated Timeline

| Phase | Hours | Calendar |
|-------|-------|----------|
| Setup + Audio | 8h | Day 1-2 |
| Basic UI | 8h | Day 3-4 |
| Chat + Mixer | 16h | Day 5-8 |
| Server + Timing | 10h | Day 9-11 |
| Polish + Testing | 14h | Day 12-14 |
| **Total** | **~56h** | **~2-3 weeks** |

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| JUCE learning curve | Start with tutorials, copy patterns |
| NJClient integration issues | Keep existing threading, just swap UI |
| Platform differences | JUCE handles this |
| Performance regression | Profile if needed, JUCE is efficient |
| Binary size increase | Acceptable trade-off (~5-10 MB) |

---

## Success Criteria

- [ ] All current features working
- [ ] Polished, professional appearance
- [ ] Same or better performance
- [ ] Works in all tested DAWs
- [ ] Easy to extend with new features
- [ ] Maintainable codebase

---

## References

- [JUCE Documentation](https://juce.com/learn/documentation)
- [JUCE Tutorials](https://juce.com/learn/tutorials)
- [JUCE Forum](https://forum.juce.com/)
- [JUCE GitHub](https://github.com/juce-framework/JUCE)
- [Audio Plugin CMake Template](https://github.com/eyalamirmusic/JUCECmakeRepoPrototype)
