# Lessons Learned: Disk Info Tab — Page Switching and WM_RETHINK

## Context

AmigaDiskBench v2.3.7 Disk Info tab uses a `page.gadget` on the right-hand side
to switch between three views: `PAGE_INIT` (welcome), `PAGE_DRIVE` (physical disk
details), and `PAGE_PARTITION` (partition/volume details). Switching is done at
runtime by calling `SetGadgetAttrs(PAGE_Current, N)` in `UpdateDetailsPage()`.

Between builds 1116 and 1125 a series of DSI crashes were investigated and fixed.
This document records the root causes and the correct patterns.

---

## Problem 1: WM_RETHINK from inside WM_HANDLEINPUT causes DSI crash

### Symptom
DSI exception in `kernel+0x00053520` (`lwzx` — font/text rendering load).
Stack: `button.gadget → layout.gadget × N → clicktab.gadget → window.class →
intuition → UpdateDetailsPage()`.
`DAR: 0xDD19xxxx` — garbage address, not NULL.

### Root Cause
`UpdateDetailsPage()` was calling `IIntuition->IDoMethod(ui.win_obj, WM_RETHINK)`
immediately after `SetGadgetAttrs(PAGE_Current, N)`.

`UpdateDetailsPage()` is called from `HandleGUIEvent()` which is called from inside
the `WM_HANDLEINPUT` loop in `StartGUI()`. At this point, `intuition` is already
mid-flight processing a gadget input event (the ListBrowser node click or tab
switch). Calling `WM_RETHINK` re-enters the layout system recursively, corrupting
render state — most visibly the `GA_Text` pointer inside `button.gadget` that is
used for font metrics, causing a page fault in the kernel text renderer.

### Why it only appeared on some clicks
The crash was reliably triggered by clicking "Fixed Drives" (a category node that
has neither a drive nor a partition). That path immediately jumped to
`SetGadgetAttrs(PAGE_Current, PAGE_INIT)` + `WM_RETHINK`. Other paths that first
updated gadget text strings before switching pages happened to crash less reliably
because they completed more work before the re-entrant rethink.

### Fix
**Defer `WM_RETHINK` until after `WM_HANDLEINPUT` returns.**

Pattern used:

In `gui.h` `GUIState`:
```c
BOOL diskinfo_rethink_pending;
```

In `UpdateDetailsPage()` — replace every `WM_RETHINK` call with:
```c
ui.diskinfo_rethink_pending = TRUE;
```

In `StartGUI()` event loop, after the `WM_HANDLEINPUT` while loop:
```c
if (ui.diskinfo_rethink_pending) {
    ui.diskinfo_rethink_pending = FALSE;
    IIntuition->IDoMethod(ui.win_obj, WM_RETHINK);
}
```

This is safe because `WM_RETHINK` is now called from the application's own context,
not from within an intuition callback chain.

---

## Problem 2: Deferred WM_RETHINK fires during startup → crash on program open

### Symptom
DSI crash immediately on launch, at `StartGUI()+0xf14`. Same kernel crash site.
Stack shows `window.class → layout → button.gadget → kernel` without any
`UpdateDetailsPage` in the trace.

### Root Cause
`RefreshDiskInfoTree()` is called at startup (before the event loop) to pre-populate
the tree. It calls `UpdateDetailsPage(NULL)` which set `diskinfo_rethink_pending =
TRUE`. On the very first `Wait()` return (which is the window's initial `WMHI_ACTIVE`
or similar), `WM_RETHINK` was fired before the window had completed its first full
layout pass. The layout gadgets' internal state was not yet valid.

### Fix
Clear `diskinfo_rethink_pending` right before entering the event loop:
```c
/* Discard any pending rethink set during init (e.g. from RefreshDiskInfoTree).
   The window is not yet in steady state; user interaction will trigger it. */
ui.diskinfo_rethink_pending = FALSE;
```

This ensures only user-triggered tree selection events (which arrive via
`WM_HANDLEINPUT` after the window is fully open and interactive) can set the flag.

---

## Problem 3: RefreshGList does not redraw page.gadget children

### Symptom (intermediate fix attempt)
After removing `WM_RETHINK` entirely (build 1122–1123), calling
`IIntuition->RefreshGList((struct Gadget *)ui.diskinfo_pages, ui.window, NULL, 1)`
did not cause the right panel to update. Clicking partitions showed no change.

### Root Cause
`RefreshGList(gadget, win, NULL, 1)` with count=1 redraws the gadget's own bounding
region, but `page.gadget` is embedded inside `layout.gadget`. The layout system
owns the render pass for all its child gadgets; a `RefreshGList` targeted at only
the page gadget does not propagate down through the layout hierarchy to repaint the
active page's content.

`WM_RETHINK` is the correct (and only reliable) mechanism for requesting a full
re-layout/redraw of the window's gadget tree when the active page changes. It must
simply be called at the right time (outside `WM_HANDLEINPUT`).

---

## General Rules for page.gadget in ReAction

1. **`SetGadgetAttrs(PAGE_Current, N)`** switches the internal page index only.
   It does NOT repaint. The screen is unchanged until a redraw is triggered.

2. **`WM_RETHINK`** is the correct way to force a full re-layout and repaint.
   It recalculates all gadget positions and redraws every visible gadget.

3. **Never call `WM_RETHINK` from inside a `WM_HANDLEINPUT` processing loop.**
   Use a deferred flag pattern (as above) to call it after `WM_HANDLEINPUT` returns.

4. **`RefreshGList` is NOT a substitute for `WM_RETHINK`** when gadgets are
   embedded in a `layout.gadget`. Use `RefreshGList` only for isolated gadgets
   that the layout does not manage (e.g. a standalone `SpaceObject` render hook).

5. **Discard deferred rethink flags set during startup init**, before entering the
   event loop. The window is not in steady state until `WM_OPEN` + first `Wait()`
   completes.

---

## Files Modified (v2.3.7.1116 → v2.3.7.1125)

| File | Change |
|---|---|
| `src/gui_info.c` | Removed WM_RETHINK; set `diskinfo_rethink_pending` flag instead; added `RefreshGList` calls (later removed); underscore sanitization; `"Partition:"/"Volume:"` prefix in value string; NULL guards on drive gadget attrs; DosEnvec size for unmounted partitions |
| `src/engine_diskinfo.c` | DosEnvec geometry read for unmounted partitions; `GetDosTypeString` 4th-byte ASCII fix |
| `src/gui.c` | Deferred `WM_RETHINK` block after `WM_HANDLEINPUT` loop; `diskinfo_rethink_pending = FALSE` before event loop |
| `include/gui.h` | Added `diskinfo_rethink_pending` to `GUIState`; removed obsolete `diskinfo_part_vol_row_label` |
| `include/version.h` | BUILD bumped 1116 → 1125 |
| `include/debug.h` | Toggled `DEBUG_ENABLED` 0→1→0 during investigation |
