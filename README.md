# macOS Window Animations — Exact Taskbar Button Edition


This mod replaces the default Windows window transitions with smooth, elegant, macOS-inspired animations:

- **Close**: the window gently shrinks and fades out.
- **Open**: the window pops in with a subtle, natural overshoot.
- **Minimize**: the window flows into the taskbar using a true genie-style funnel.
- **Exact taskbar targeting**: the genie now attempts to aim at the actual taskbar button of the app, not just the center of the taskbar.

The result should feel clean, fluid, and satisfying — like a native desktop animation.

---

## Features

### macOS-style close animation
When a window is closed, it shrinks slightly while fading out.

This gives a softer, more polished exit compared to the abrupt Windows close behavior.

### macOS-style open animation
When enabled, newly opened windows appear with a gentle pop-in effect.

The animation uses a custom easing curve with a small overshoot, similar to the lively feel of macOS window appearance.

> This feature is experimental and disabled by default because some apps may not render correctly before their first frame is ready.

### Genie minimize animation
When minimizing, the window is captured as a snapshot and warped into the taskbar through a curved funnel.

The effect is designed to feel like the classic macOS genie animation:

- The bottom of the window collapses into a neck.
- The window content flows downward.
- The animation accelerates naturally.
- The window disappears into the taskbar button area.

### Exact taskbar button targeting
The mod uses UI Automation to locate the taskbar button associated with the window being minimized.

This allows the genie animation to aim at the correct taskbar button instead of guessing.

If the exact button cannot be found, the mod falls back safely to:

- Taskbar center, or
- Straight below the window, depending on your setting.

### Soft shadow
The animation draws a soft synthetic shadow around the captured window.

This helps the animation feel more natural and prevents the window snapshot from looking flat.

### Rounded corners
On Windows 11, the mod respects the window’s rounded-corner preference where possible.

### High-quality rendering
The animation renderer uses:

- Mipmapped scaling
- Box filtering
- Anti-aliased edges
- Dirty-rectangle updates
- Multi-threaded rendering
- Frame pacing based on display refresh timing

This helps keep the animation smooth and reduces shimmering when the window is compressed.

### Shift slow motion
Like macOS, holding **Shift** while minimizing plays the animation in slow motion.

This can be disabled in the settings.

---

## Minimize styles

| Style | Description |
|---|---|
| Genie | macOS-style funnel animation. |
| Scale | Simple shrink-toward-taskbar animation. |
| Off | Uses normal Windows behavior. |

---

## Settings

### Close animation
Enables or disables the animated close effect.

### Close duration
How long the close animation lasts.

Recommended: **150–220 ms**

### Close shrink to
Final size of the window before it disappears.

Example:

- `95%` = subtle shrink
- `85%` = more visible shrink
- `100%` = fade only

### Open animation
Enables the pop-in animation when a window is shown.

Experimental. If an app flashes, misbehaves, or opens incorrectly, disable this option.

### Open duration
Duration of the open animation.

Recommended: **180–260 ms**

### Open starts at
Initial scale of the window when opening.

Recommended: **90–94%**

### Minimize style
Choose between:

- Genie
- Scale
- Off

### Minimize duration
Duration of the minimize animation.

Recommended: **400–550 ms**

For a more dramatic macOS-like feel, use around **500 ms**.

For a snappier feel, use around **350–420 ms**.

### Genie fallback target
Used only if the exact taskbar button cannot be found.

Options:

- **Taskbar centre**
- **Below window**

### Hold Shift for slow motion
Plays the minimize animation much slower while Shift is held.

### Draw shadow
Draws a soft shadow behind the animated window snapshot.

### Shadow strength
Controls how strong the synthetic shadow appears.

If you notice a visible shadow jump when the animation ends, adjust this setting until it matches your system shadow more closely.

### Animate hidden windows
Some apps hide their window instead of destroying it when closed.

This is common in:

- Chrome
- Electron apps
- Tray apps
- Messaging apps

Enable this if you want those windows to animate when hidden.

### Hook DefWindowProc
Allows the mod to catch minimize and close actions that are handled internally by Windows.

This improves compatibility with apps such as File Explorer and many native Win32 programs.

### Excluded programs
Processes listed here will not use the mod.

Useful for:

- Games
- Anti-cheat software
- Screen-capture tools
- Apps with custom rendering
- Misbehaving apps

Example:

```txt
game.exe
launcher.exe
anticheat.exe
```

---

## Recommended settings for the most macOS-like feel

For a clean, natural macOS-style experience:

| Setting | Recommended value |
|---|---|
| Close animation | Enabled |
| Close duration | 170–190 ms |
| Close shrink to | 93–96% |
| Open animation | Optional |
| Open duration | 210–230 ms |
| Open starts at | 92% |
| Minimize style | Genie |
| Minimize duration | 450–520 ms |
| Shadow | Enabled |
| Shadow strength | 35–55% |
| Shift slow motion | Enabled |

---

## Notes and limitations

- The animation uses a snapshot of the window. Live video, DRM-protected content, or GPU-exclusive surfaces may not capture correctly.
- Some apps may appear black or blank during capture. Those apps will automatically fall back to normal behavior when possible.
- Restoring a minimized window uses the normal Windows restore animation because a minimized window does not have a live visual snapshot to warp.
- If your taskbar is auto-hidden, the genie may target the hidden taskbar position.
- On vertical taskbars, the genie effect may fall back to the scale animation.
- The open animation is experimental because some apps need a moment before their first frame is rendered.
- If an app misbehaves, add it to the excluded programs list.

---

## Troubleshooting

### The genie does not hit the exact taskbar button
Possible reasons:

- The taskbar button does not expose a valid accessible name.
- The window title does not match the taskbar button name.
- The taskbar is hidden, moved, or heavily customized.
- The taskbar uses a third-party modification.
- The app is grouped in a way that prevents reliable detection.

The mod will automatically fall back to the selected fallback target.

### The animation flickers
Try:

- Lowering the shadow strength.
- Disabling open animation.
- Excluding the affected app.
- Disabling third-party taskbar mods temporarily.

### A window closes without animation
This can happen if:

- The window is too small.
- The window is a popup, tooltip, menu, or tool window.
- The capture failed.
- The app is excluded.
- Windows animation effects are disabled.
- The app hides itself in an unsupported way.

### The animation looks too fast or too slow
Adjust the duration settings.

For macOS-like motion:

- Close: around **180 ms**
- Open: around **220 ms**
- Minimize: around **450–500 ms**

---

## Why it feels smooth

This mod uses custom easing curves instead of simple linear motion.

- Close uses an accelerating fade/shrink.
- Open uses a soft overshoot curve.
- Genie uses a fluid travel curve that starts gently and accelerates into the dock.
- Edges are filtered to reduce aliasing.
- Frames are paced against the display refresh to avoid uneven motion.

The goal is not just animation, but animation that feels natural.

---

## Credits

Original animation engine and concept by **Charles-28X**.
**Github Repo**-https://github.com/Charles-28X/MacOS-Animation

Inspired by macOS window transitions and Dock behavior.

This enhanced version improves:

- Exact taskbar button targeting
- Compile compatibility
- Easing smoothness
- Open-animation clipping
- Fallback reliability
