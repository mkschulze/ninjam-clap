# Win32 Text Input Fix Plan (Dummy EDIT Focus + Forwarding)

Goal: prevent DAW shortcuts (spacebar, etc.) while typing in ImGui text fields on Win32, without breaking ImGui input.

## Summary
- Use a focusable Win32 EDIT control to signal "text input active" to the host.
- Subclass the EDIT control to forward key/char messages into ImGui.
- Switch focus only on state transitions (no per-frame focus thrash).
- Add a GUI-thread message hook to intercept host accelerators before they run.

## Implementation Plan

1) Add dummy EDIT control to `GuiContextWin32`
   - Add `HWND dummy_edit_` member.
   - Add `WNDPROC orig_edit_proc_` member to store the original EDIT WndProc.
   - Create it in `set_parent()`:
     - Style: `WS_CHILD | WS_TABSTOP | WS_VISIBLE` (not hidden).
     - Size: 1x1, optionally positioned offscreen (e.g., -10,-10) to avoid any visual artifact.
     - Parent: plugin window `hwnd_`.
   - Store the original EDIT WndProc via `SetWindowLongPtr(GWLP_WNDPROC, ...)`.
   - Store `GuiContextWin32*` on the EDIT via `SetWindowLongPtr(GWLP_USERDATA, ...)`.

2) Subclass the EDIT control to forward input to ImGui
   - Implement `static LRESULT CALLBACK dummy_edit_proc(...)`.
   - In the proc, retrieve `GuiContextWin32*` via `GetWindowLongPtr(GWLP_USERDATA)`.
   - Call `ImGui::SetCurrentContext(ctx->imgui_ctx_)` before forwarding.
   - For keyboard/text messages (WM_KEYDOWN/UP, WM_SYSKEYDOWN/UP, WM_CHAR, WM_SYSCHAR):
     - Call `ImGui_ImplWin32_WndProcHandler(ctx->hwnd_, msg, wParam, lParam)` using the main plugin window HWND (not the EDIT HWND).
     - Return 0 to suppress default EDIT behavior.
   - Optional: handle `VK_TAB` in WM_KEYDOWN to prevent focus from leaving the EDIT.
   - For other messages, call the original EDIT WndProc.

3) Transition-based focus management
   - Track `wants_text_input` in `GuiContextWin32` (bool).
   - After `ui_render_frame()` in `render()`:
     - Read `io.WantTextInput`.
     - If it changed from false -> true:
       - `SetFocus(dummy_edit_)` if not already focused.
     - If it changed from true -> false:
       - `SetFocus(hwnd_)` (or restore previous focus if tracked).
   - Avoid calling `SetFocus()` every frame.

4) Cleanup
   - On `hide()` or `cleanup()`:
     - If the dummy edit has focus, restore focus to `hwnd_`.
     - Destroy the EDIT control.
   - Remove the message hook if installed (see step 5).
   - Remove unused atomics (`wants_text_input` / `prev_wants_text_input`) from `jamwide_plugin.h` and any related logic.
   - Remove `wants_keyboard_input_` message blocking in the main WndProc if no longer needed.

5) Add a GUI-thread message hook (required for Bitwig/Reaper)
   - Install a hook on the GUI thread in `set_parent()`:
     - `SetWindowsHookEx(WH_GETMESSAGE, ..., nullptr, GetCurrentThreadId())`.
   - In the hook proc:
     - If `wants_text_input_` is true and the message is key/char/syskey/IME:
       - Neutralize it before the host’s accelerator handling (e.g., set `msg->message = WM_NULL`).
     - Otherwise, pass through with `CallNextHookEx`.
   - Remove the hook in `cleanup()`/`hide()`.

6) Final flow (implemented)
- When ImGui wants text input, focus shifts to the dummy EDIT control.
- A GUI-thread WH_GETMESSAGE hook intercepts key messages, routes them through
  TranslateMessage/DispatchMessage to the dummy EDIT (which forwards to ImGui),
  then neutralizes the original message so the host never sees it.
- When text input ends or the editor hides, focus returns to the main window and
  the hook is removed.

7) Test checklist
   - With text input active:
     - Spacebar does not trigger DAW transport.
     - Caps Lock works in ImGui text fields.
   - With no text input active:
     - DAW shortcuts work normally.
   - Hide/show editor:
     - Focus returns to DAW, no shortcut capture.

## Notes
- The EDIT control must be focusable and visible (not zero-sized and not hidden).
- Forwarding key/char messages is required; otherwise ImGui won’t receive text input while the EDIT has focus.
