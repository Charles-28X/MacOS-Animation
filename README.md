# macOS Genie & Pop Animations

Bring the familiar macOS window animations to Windows.

This Windhawk mod recreates the way macOS handles window movement. It brings the Genie minimize effect, smooth window launches, and clean closing animations to your desktop.

Every animation has its own settings. You can adjust the effects to match your preferences.

## Features

### Minimize & Restore

* **Genie Effect:** Makes the window flow into its taskbar button, like the macOS Genie effect.
* **Scale Effect:** Shrinks the window directly towards its taskbar button.
* **Smart Targeting:** Finds the correct taskbar button through UI Automation and AppUserModelID matching.
* **Cursor Tracking:** Follows your cursor when you click a taskbar button.
* **Reverse Animation:** Restores minimized windows with the reverse Genie effect.

### Window Animations

* **App Launch:** Windows scale up and fade in when applications open.
* **Window Close:** Windows shrink slightly and fade out.
* **Genie Close:** Sends closing windows towards any screen edge.
* **Dialog Animation:** Gives dialog boxes a small spring effect.
* **Tray Animation:** Can animate applications that hide in the system tray.

### Visual Details

* Soft shadows during window animations.
* Windows 11 rounded corners.
* Smooth movement with cubic-bezier easing.
* Optional spring animation for window launches.
* Adjustable animation speed and duration.

## Animation Engines

Choose the rendering engine that works best on your system.

| Engine  | Technology                      | Description                                                 |
| ------- | ------------------------------- | ----------------------------------------------------------- |
| Modern  | Direct2D                        | Uses mesh warping for smooth Genie animations.              |
| GPU     | Direct3D 11 + DirectComposition | Uses GPU rendering, anti-aliasing, and VSync. Experimental. |
| Classic | GDI                             | Uses lightweight strip-based rendering.                     |

The GPU engine falls back to Modern when required components are unavailable.

## Customization

You can control almost every part of the animation.

* Set minimize and restore durations.
* Select separate easing curves.
* Choose between Genie and Scale effects.
* Change window launch and close styles.
* Adjust spring intensity and animation timing.
* Control the Genie depth inside the taskbar.
* Enable or disable shadows and rounded corners.
* Set the rendering engine and mesh resolution.
* Exclude applications that cause problems.

### Slow Motion

Want to see the animation more clearly?

Hold these keys while minimizing or restoring a window:

| Key          | Effect            |
| ------------ | ----------------- |
| Shift        | Slow motion       |
| Ctrl         | Slower animation  |
| Ctrl + Shift | Slowest animation |

You can also change the slow-motion factor in the settings.

## Multi-Monitor Support

The animation uses the taskbar on the monitor that contains the window.

When that monitor has no taskbar, the mod uses the primary monitor's taskbar.

It also supports auto-hide taskbars. The mod briefly reveals the taskbar before the window minimizes.

## Performance

The mod uses rendering resources only when an animation starts.

* Creates Direct2D during the first animation.
* Stores minimized-window snapshots in an LRU cache.
* Uses a configurable cache limit of 64 MB by default.
* Allows you to disable the cache.
* Joins worker threads when the mod unloads.
* Skips animations when Windows disables animation effects.

These measures help keep resource usage under control.

## Troubleshooting

**An application has broken animations?**

Add its executable to the **Excluded programs** setting.

Use this setting instead of Windhawk's process exclusion list. The animation can start in another process, such as Explorer.

**The animation looks different on another monitor?**

Some monitors do not provide the required backdrop. The mod uses an opaque backdrop in those cases.

**An application does not animate correctly?**

Some applications handle window creation and closing in unusual ways. Exclude the application and report the issue.

## Known Limitations

* Mica and acrylic backgrounds appear as static images during animations.
* Live background blur does not move with the animated window.
* Some secondary monitors can use an opaque animation backdrop.
* Certain applications may require exclusion.

## Credits

* **Potassiumuncher** — Direct2D Genie engine, UI Automation taskbar targeting, and auto-hide taskbar handling.
* **Abdullah Masood** — Original inspiration and MacOS Minimize Animation mod.

## License

MIT License.
