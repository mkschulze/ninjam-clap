# Plan: Linux Port

## Goal
Add Linux support to JamWide, enabling CLAP and VST3 plugin builds for Linux DAWs (REAPER, Bitwig, Ardour, etc.).

## Current State
- macOS: Metal + ImGui ✅
- Windows: D3D11 + ImGui ✅
- Linux: Not implemented ❌

## Architecture Overview

```
┌─────────────────────────────────────────┐
│              DAW (Host)                 │
│  ┌───────────────────────────────────┐  │
│  │         Host Window (X11)         │  │
│  │  ┌─────────────────────────────┐  │  │
│  │  │   JamWide Plugin Window     │  │  │
│  │  │  ┌───────────────────────┐  │  │  │
│  │  │  │   OpenGL Context      │  │  │  │
│  │  │  │   + ImGui Rendering   │  │  │  │
│  │  │  └───────────────────────┘  │  │  │
│  │  └─────────────────────────────┘  │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘
```

## Implementation Steps

### Phase 1: GUI Backend (gui_linux.cpp)

**File:** `src/platform/gui_linux.cpp`

#### 1.1 X11 Window Embedding
```cpp
// CLAP provides parent window as void* (X11 Window ID)
// Create child window and embed into parent

#include <X11/Xlib.h>
#include <GL/glx.h>

struct LinuxGuiContext {
    Display* display;
    Window window;
    GLXContext gl_context;
    XVisualInfo* visual_info;
    Colormap colormap;
    int width, height;
};
```

#### 1.2 OpenGL + ImGui Setup
```cpp
// Initialize OpenGL 3.3+ context via GLX
// Setup ImGui with OpenGL3 backend
// Use imgui_impl_opengl3.cpp (already in libs/imgui)

ImGui_ImplOpenGL3_Init("#version 330");
```

#### 1.3 Event Handling
```cpp
// Process X11 events for mouse/keyboard
// Forward to ImGui via imgui_impl_x11.cpp or manual handling

while (XPending(display)) {
    XEvent event;
    XNextEvent(display, &event);
    // Handle KeyPress, KeyRelease, ButtonPress, ButtonRelease, MotionNotify
}
```

#### 1.4 Render Loop
```cpp
void gui_render(JamWidePlugin* plugin) {
    glXMakeCurrent(display, window, gl_context);
    
    ImGui_ImplOpenGL3_NewFrame();
    // No ImGui_Impl_X11_NewFrame - handle manually or use custom impl
    ImGui::NewFrame();
    
    ui_render_main(plugin);
    
    ImGui::Render();
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glXSwapBuffers(display, window);
}
```

### Phase 2: CMake Updates

**File:** `CMakeLists.txt`

```cmake
if(UNIX AND NOT APPLE)
    # Linux-specific sources
    target_sources(jamwide-impl PRIVATE
        src/platform/gui_linux.cpp
    )
    
    # Find and link X11, OpenGL
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    
    target_include_directories(jamwide-impl PRIVATE
        ${X11_INCLUDE_DIR}
        ${OPENGL_INCLUDE_DIRS}
    )
    
    target_link_libraries(jamwide-impl PRIVATE
        ${X11_LIBRARIES}
        ${OPENGL_LIBRARIES}
        GL
        GLX
    )
    
    # Add ImGui OpenGL3 backend
    target_sources(imgui PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/imgui/backends/imgui_impl_opengl3.cpp
    )
endif()
```

### Phase 3: GitHub Actions CI

**File:** `.github/workflows/build.yml`

Add Linux job:
```yaml
build-linux:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
      with:
        submodules: recursive
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y \
          build-essential \
          cmake \
          libx11-dev \
          libxrandr-dev \
          libxinerama-dev \
          libxcursor-dev \
          libxi-dev \
          libgl1-mesa-dev \
          libglu1-mesa-dev
    
    - name: Configure
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release
    
    - name: Build
      run: cmake --build build --config Release
    
    - name: Upload CLAP
      uses: actions/upload-artifact@v4
      with:
        name: JamWide-Linux-CLAP
        path: build/JamWide.clap
    
    - name: Upload VST3
      uses: actions/upload-artifact@v4
      with:
        name: JamWide-Linux-VST3
        path: build/JamWide.vst3
```

### Phase 4: Testing

#### 4.1 Local Testing (VM or real Linux)
```bash
# Install dependencies
sudo apt install build-essential cmake libx11-dev libgl1-mesa-dev

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Install
cp -r JamWide.clap ~/.clap/
cp -r JamWide.vst3 ~/.vst3/
```

#### 4.2 Test in DAWs
- REAPER (Linux version)
- Bitwig Studio
- Ardour
- Carla (plugin host)

## Files to Create/Modify

| File | Action | Description |
|------|--------|-------------|
| `src/platform/gui_linux.cpp` | Create | X11 + OpenGL + ImGui backend (~250 lines) |
| `CMakeLists.txt` | Modify | Add Linux platform detection and libs |
| `.github/workflows/build.yml` | Modify | Add Linux build job |

## Dependencies (Linux)

| Package | Purpose |
|---------|---------|
| `libx11-dev` | X11 window system |
| `libgl1-mesa-dev` | OpenGL |
| `libxrandr-dev` | X11 RandR extension (optional) |
| `libxinerama-dev` | X11 Xinerama extension (optional) |
| `libxcursor-dev` | X11 cursor support |
| `libxi-dev` | X11 input extension |

## Technical Considerations

### X11 vs Wayland
- **X11**: Mature, well-documented, easier plugin embedding
- **Wayland**: Modern, but plugin window embedding is complex
- **Recommendation**: Start with X11, add Wayland later via XWayland compatibility

### ImGui X11 Backend
ImGui doesn't have an official X11 backend. Options:
1. **Manual handling** - Process X11 events and feed to ImGui IO (simplest)
2. **GLFW backend** - Use GLFW for window/input, but complicates embedding
3. **SDL2 backend** - Similar to GLFW
4. **Custom imgui_impl_x11.cpp** - Several community implementations available

**Recommendation**: Manual X11 event handling (most control, no extra deps)

### Thread Safety
- X11 is not thread-safe by default
- Call `XInitThreads()` at startup if needed
- Or ensure all X11 calls happen on GUI thread (current pattern)

## Effort Estimate

| Task | Time |
|------|------|
| gui_linux.cpp skeleton | 1 hour |
| X11 window embedding | 2 hours |
| OpenGL context setup | 1 hour |
| ImGui integration | 1-2 hours |
| Event handling | 1-2 hours |
| CMake updates | 30 min |
| CI/CD setup | 30 min |
| Testing & debugging | 2-3 hours |
| **Total** | **8-12 hours** |

## Reference Code

### ReaNINJAM Linux (SWELL)
- Uses Cockos SWELL library for cross-platform GUI
- May be overly complex for our needs

### clap-wrapper Linux examples
- Check `clap-wrapper/src/detail/standalone/` for Linux patterns

### ImGui examples
- `libs/imgui/examples/example_glfw_opengl3/` - GLFW + OpenGL3
- Adapt for X11 direct usage

## Milestones

1. **M1**: Build compiles on Linux (no GUI)
2. **M2**: Plugin loads in DAW, shows blank window
3. **M3**: ImGui renders, basic interaction works
4. **M4**: Full functionality, keyboard/mouse works
5. **M5**: CI/CD produces Linux artifacts

## Open Questions

1. Should we support Wayland natively or rely on XWayland?
2. Do we need HiDPI/scaling support for Linux?
3. Should we package as .deb/.rpm or just .zip?
