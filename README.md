# MacOS-Animation
 # macOS Window Animations for Windows

Bring the visual feel of macOS window transitions to Windows.

**macOS Window Animations** is a Windhawk mod that recreates macOS-inspired close, open, and minimize animations using window capture, layered overlays, and a custom software renderer.

The goal is to reproduce the characteristic macOS experience while working within the Windows desktop environment.

## Features

| Action       | Animation       | Description                                                                                            |
| ------------ | --------------- | ------------------------------------------------------------------------------------------------------ |
| **Close**    | Shrink & Fade   | The window smoothly scales down and fades out while retaining rounded corners and a soft shadow.       |
| **Open**     | Pop-In          | The window appears from approximately 92% scale while fading in. Experimental and disabled by default. |
| **Minimize** | Genie           | The window collapses toward the taskbar through a curved, per-scanline warp inspired by macOS.         |
| **Minimize** | Scale           | The window smoothly shrinks toward the taskbar.                                                        |
| **Restore**  | Windows Default | Restoring a minimized window uses the native Windows animation.                                        |

### Animation Preview

The animations are configurable, including their duration, scale, shadow, and minimize style.

## How It Works

### 1. Understanding the macOS Approach

macOS window transitions do not simply move the original application window. The WindowServer uses a visual representation of the window and applies transformations to it.

The effects can be approximated as follows:

**Close / Open**

* Uniform scaling, typically around 0.92–0.95.
* Opacity transitions.
* Short durations, approximately 150–250 ms.
* Decelerating animation curves.
* Window shadows retained during the transition.

**Genie Minimize**

The macOS Genie effect uses a private WindowServer API, `CGSSetWindowWarp`, to apply a mesh deformation.

Its characteristic behavior can be approximated using:

* Per-scanline horizontal compression.
* Different arrival times for individual rows.
* Rows closer to the Dock reaching it first.
* Sigmoid-based deformation for the window edges.

Similar concepts have appeared in Compiz Magic Lamp and KWin Magic Lamp.

### 2. Windows Implementation

This mod recreates these effects in user mode rather than modifying the Windows compositor.

The animation pipeline works as follows:

1. **Window Capture:** Captures the window using `PrintWindow`.
2. **Visual Processing:** Applies rounded corners and generates a synthetic soft shadow.
3. **Overlay Rendering:** Displays the captured image through a click-through layered overlay.
4. **Animation Engine:** Uses a software renderer with integer bilinear interpolation and per-scanline processing to produce the Genie deformation.
5. **Transition Control:** Disables the original Windows transition for the animated window to prevent overlapping animations.
6. **Window Event Hooks:** Intercepts `DestroyWindow`, `ShowWindow`, and `DefWindowProc` to detect close, minimize, and visibility events.

The hooks also allow the mod to catch certain windows that Windows closes or minimizes internally.

## Configuration

All settings can be managed through Windhawk.

| Setting                | Default        | Description                                                    |
| ---------------------- | -------------- | -------------------------------------------------------------- |
| Close animation        | Enabled        | Enables the close transition.                                  |
| Close duration         | 180 ms         | Controls how long the closing animation lasts.                 |
| Close scale            | 95%            | Determines the final scale before the window disappears.       |
| Open animation         | Disabled       | Enables the experimental opening transition.                   |
| Open duration          | 220 ms         | Controls the opening animation duration.                       |
| Open scale             | 92%            | Determines the initial scale of the opening window.            |
| Minimize style         | Genie          | Selects Genie, Scale, or the native Windows animation.         |
| Minimize duration      | 420 ms         | Controls the minimize animation duration.                      |
| Genie target           | Taskbar centre | Determines the approximate destination of the Genie animation. |
| Window shadow          | Enabled        | Draws a synthetic shadow around the captured window.           |
| Animate hidden windows | Enabled        | Also animates windows that hide instead of being destroyed.    |
| Hook DefWindowProc     | Enabled        | Detects default Windows close and minimize handling.           |
| Excluded programs      | Configurable   | Prevents the mod from loading in specified applications.       |

### Available Minimize Styles

**Genie (macOS)**
Creates a curved deformation that compresses the window toward the taskbar.

**Scale**
Shrinks the window toward the taskbar without the Genie deformation.

**Off**
Disables the custom minimize animation and uses the native Windows behavior.

### Excluded Programs

Applications can be excluded by executable name.

The default exclusion list includes:

```text
vgc.exe
LeagueClient.exe
VALORANT.exe
EasyAntiCheat.exe
BEService.exe
```

This is particularly useful for games, anti-cheat software, and applications that may conflict with window hooks or overlays.

## Compatibility & Limitations

There are some important implementation constraints:

* **Animation effects:** Animations are disabled when Windows' Animation Effects setting is turned off.
* **Remote sessions:** Custom animations are not performed in remote sessions.
* **Excluded windows:** Menus, tooltips, tool windows, and shell windows are not animated.
* **Taskbar position:** Genie works with taskbars positioned along the top or bottom edge. Left- and right-aligned taskbars use the Scale effect instead.
* **Genie destination:** The animation targets the approximate taskbar centre or a position below the window. Windows does not expose a public API for obtaining individual taskbar button positions.
* **Restore animation:** Restoring a minimized window uses the native Windows animation because the original live window image is no longer available for deformation.
* **UI thread:** The animated window's UI thread waits for the animation to finish. This prevents the process from exiting before the transition completes.
* **Capture limitations:** Some applications may not produce a usable image through `PrintWindow`.

### Experimental Open Animation

The opening animation is disabled by default.

To capture the initial window image, the mod briefly cloaks the window. If cloaking or image capture fails, the window opens normally.

Some applications may not behave correctly with this process. Disable the opening animation if you encounter application-specific issues.

## Performance & Rendering

The mod performs its animations using captured window images and a software-based rendering pipeline.

The Genie effect uses per-scanline deformation rather than simply scaling the entire window. This allows different portions of the captured image to reach the taskbar at different times.

Performance and visual results may vary depending on the application, window contents, and capture behavior.

## Credits & Links

**Author:** [Charles-28X](https://github.com/Charles-28X)


---

*Inspired by macOS window animations and implemented for the Windows desktop using Windhawk.*
