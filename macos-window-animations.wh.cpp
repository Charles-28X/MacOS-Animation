// ==WindhawkMod==
// @id              macos-window-animations
// @name            macOS Window Animations (Close / Open / Genie)
// @description     macOS-style window animations: shrink-and-fade close, pop-in open, and Genie / Scale minimize, with soft shadow and rounded corners
// @version         2.0.0
// @author          Charles-28X
// @github          https://github.com/Charles-28X
// @include         *
// @exclude         csrss.exe
// @exclude         dwm.exe
// @exclude         winlogon.exe
// @exclude         LogonUI.exe
// @exclude         fontdrvhost.exe
// @exclude         SearchHost.exe
// @exclude         StartMenuExperienceHost.exe
// @exclude         ShellExperienceHost.exe
// @exclude         TextInputHost.exe
// @license         MIT
// ==/WindhawkMod==

// ============================================================================
//  Author profile : https://github.com/Charles-28X
//  Related repo   : https://github.com/Charles-28X/MacOS-Animation
// ============================================================================

// ==WindhawkModReadme==
/*
# macOS Window Animations

Close, open and minimize animations modelled on macOS.

| Action   | Effect |
|----------|--------|
| Close    | The window shrinks slightly and fades out, keeping shadow and rounded corners. |
| Open     | The window pops in from ~92% size while fading in (experimental, off by default). |
| Minimize | **Genie**: the window is sucked into the taskbar through a curved funnel. **Scale**: the window shrinks into the taskbar. |

## How it works (reverse-engineering notes)

macOS does not animate the real window. The WindowServer takes a snapshot and
applies a transform to it:

* Close / open: a uniform scale (~0.92-0.95) plus an opacity ramp, roughly
  0.15-0.25 s, with a decelerating curve. The window keeps its shadow.
* Genie: private `CGSSetWindowWarp` applies a mesh warp. In practice it is a
  per-scanline horizontal squeeze. Each row has its own arrival time at the
  Dock, rows nearest the Dock arrive first, and the sides follow a sigmoid
  (S-shaped) curve. Compiz "Magic Lamp" and KWin "Magic Lamp" use the same idea.

This mod copies that approach in user mode:

1. The window is captured with `PrintWindow`.
2. The picture gets rounded corners and a synthetic soft shadow.
3. A click-through layered overlay shows the picture and a software
   renderer (integer bilinear, per-scanline) plays the warp.
4. Windows' own transition is disabled for that window, so the two
   animations never stack.
5. The mod hooks `DestroyWindow`, `ShowWindow` and `DefWindowProc`
   (`WM_CLOSE`, `WM_SYSCOMMAND/SC_MINIMIZE`), so it also catches windows
   that Windows closes or minimizes internally.

## Notes and limits

* The UI thread of the animated window waits for the animation to finish,
  so the process cannot exit mid-animation.
* Nothing is animated when "Animation effects" is off in Windows settings,
  in remote sessions, or for menus, tooltips, tool windows and the shell.
* Genie needs a taskbar on the bottom or top edge. Left/right taskbars use the
  Scale effect.
* The genie aims at the taskbar centre (or below the window). Windows exposes no
  public API for individual taskbar button positions.
* Restoring a minimized window uses the normal Windows animation, because a
  minimized window has no live picture to warp.
* Open animation: the window is briefly cloaked while its first picture is
  captured. If cloaking or the capture fails, the window just opens normally.
  Turn it off if an app misbehaves.
* Add games, anti-cheat software and misbehaving apps to "Excluded programs".

Author: https://github.com/Charles-28X
Dataset repo: https://github.com/Charles-28X/MacOS-Animation
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- closeEnabled: true
  $name: Close animation
- closeDuration: 180
  $name: Close duration (ms)
  $description: 60-1000. macOS is around 150-200.
- closeScale: 95
  $name: Close shrink to (%)
  $description: 50-100. 100 means fade only.
- openEnabled: false
  $name: Open animation (experimental)
  $description: Pop-in when a window is shown. Cloaks the window while capturing it, so turn it off if an app misbehaves.
- openDuration: 220
  $name: Open duration (ms)
- openScale: 92
  $name: Open starts at (%)
  $description: 50-100.
- minimizeStyle: genie
  $name: Minimize style
  $options:
  - genie: Genie (macOS)
  - scale: Scale
  - off: Off (Windows default)
- minimizeDuration: 420
  $name: Minimize duration (ms)
  $description: 100-2000.
- genieTarget: center
  $name: Genie target
  $options:
  - center: Taskbar centre
  - window: Straight below the window
- shadow: true
  $name: Draw window shadow
- animateHide: true
  $name: Also animate windows that are hidden instead of destroyed
  $description: Needed for Chrome, Electron and tray apps that hide their window when you press X.
- hookDefWindowProc: true
  $name: Catch default close and minimize handling
  $description: Also animates windows that Windows closes or minimizes internally (File Explorer, most native apps).
- excludedPrograms: [vgc.exe, LeagueClient.exe, VALORANT.exe, EasyAntiCheat.exe, BEService.exe]
  $name: Excluded programs
  $description: Executable names where the mod should not load (for example games with anti-cheat).
*/
// ==/WindhawkModSettings==

#include <dwmapi.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
#ifndef SPI_GETCLIENTAREAANIMATION
#define SPI_GETCLIENTAREAANIMATION 0x1042
#endif

// ---- Late-bound imports: no -l linker flags required ----
static decltype(&DwmGetWindowAttribute) g_pDwmGetWindowAttribute;
static decltype(&DwmSetWindowAttribute) g_pDwmSetWindowAttribute;
static decltype(&DwmFlush) g_pDwmFlush;
static decltype(&CreateDIBSection) g_pCreateDIBSection;
static decltype(&CreateCompatibleDC) g_pCreateCompatibleDC;
static decltype(&SelectObject) g_pSelectObject;
static decltype(&DeleteDC) g_pDeleteDC;
static decltype(&DeleteObject) g_pDeleteObject;

#define DwmGetWindowAttribute g_pDwmGetWindowAttribute
#define DwmSetWindowAttribute g_pDwmSetWindowAttribute
#define DwmFlush g_pDwmFlush
#define CreateDIBSection g_pCreateDIBSection
#define CreateCompatibleDC g_pCreateCompatibleDC
#define SelectObject g_pSelectObject
#define DeleteDC g_pDeleteDC
#define DeleteObject g_pDeleteObject

static bool LoadImports() {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    HMODULE gdi = LoadLibraryW(L"gdi32.dll");
    if (!dwm || !gdi) return false;
#define WH_LOAD(var, mod, name) ((var = (decltype(var))GetProcAddress(mod, name)) != nullptr)
    return WH_LOAD(g_pDwmGetWindowAttribute, dwm, "DwmGetWindowAttribute") &&
           WH_LOAD(g_pDwmSetWindowAttribute, dwm, "DwmSetWindowAttribute") &&
           WH_LOAD(g_pDwmFlush, dwm, "DwmFlush") &&
           WH_LOAD(g_pCreateDIBSection, gdi, "CreateDIBSection") &&
           WH_LOAD(g_pCreateCompatibleDC, gdi, "CreateCompatibleDC") &&
           WH_LOAD(g_pSelectObject, gdi, "SelectObject") &&
           WH_LOAD(g_pDeleteDC, gdi, "DeleteDC") &&
           WH_LOAD(g_pDeleteObject, gdi, "DeleteObject");
#undef WH_LOAD
}

namespace {

// DWM attribute ids, defined locally because MinGW headers can lack them.
constexpr DWORD kDwmTransitionsForceDisabled = 3;
constexpr DWORD kDwmExtendedFrameBounds = 9;
constexpr DWORD kDwmCloak = 13;
constexpr DWORD kDwmCloaked = 14;
constexpr DWORD kDwmCornerPreference = 33;  // Windows 11 only

constexpr wchar_t kOverlayClass[] = L"WhMacAnimOverlay";

enum class MinStyle { Off, Scale, Genie };

struct {
    bool closeOn = true;
    int closeMs = 180;
    double closeScale = 0.95;
    bool openOn = false;
    int openMs = 220;
    double openScale = 0.92;
    MinStyle minStyle = MinStyle::Genie;
    int minMs = 420;
    bool genieNearWindow = false;
    bool shadow = true;
    bool animateHide = true;
    bool hookDefProc = true;
    std::vector<std::wstring> excluded;
} g_cfg;

thread_local bool t_busy = false;  // re-entrancy guard (per thread)

SRWLOCK g_classLock = SRWLOCK_INIT;
bool g_classRegistered = false;

using SetThreadDpiAwarenessContext_t = HANDLE(WINAPI*)(HANDLE);
using GetDpiForWindow_t = UINT(WINAPI*)(HWND);
SetThreadDpiAwarenessContext_t g_pSetThreadDpi = nullptr;
GetDpiForWindow_t g_pGetDpiForWindow = nullptr;

using DestroyWindow_t = decltype(&DestroyWindow);
DestroyWindow_t DestroyWindow_Original;
using ShowWindow_t = decltype(&ShowWindow);
ShowWindow_t ShowWindow_Original;
using DefProc_t = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
DefProc_t DefWindowProcW_Original;
DefProc_t DefWindowProcA_Original;

// ============================================================================
//  Pure maths and software renderer (no Win32 calls)
// ============================================================================

inline float Sat(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
inline float SmoothStep(float x) {
    x = Sat(x);
    return x * x * (3.f - 2.f * x);
}
inline double EaseOutCubic(double t) {
    double u = 1.0 - t;
    return 1.0 - u * u * u;
}
inline double EaseInOutSine(double t) {
    return -(std::cos(3.14159265358979323846 * t) - 1.0) / 2.0;
}

// Linear interpolation of two premultiplied ARGB pixels, f in 0..256.
inline uint32_t Lerp8(uint32_t a, uint32_t b, uint32_t f) {
    const uint32_t rb =
        (((a & 0x00FF00FFu) * (256u - f) + (b & 0x00FF00FFu) * f) >> 8) & 0x00FF00FFu;
    const uint32_t ag = ((((a >> 8) & 0x00FF00FFu) * (256u - f)) +
                         (((b >> 8) & 0x00FF00FFu) * f)) &
                        0xFF00FF00u;
    return rb | ag;
}

// Scales all four channels of a premultiplied pixel, a in 0..256.
inline uint32_t Mul8(uint32_t c, uint32_t a) {
    const uint32_t rb = (((c & 0x00FF00FFu) * a) >> 8) & 0x00FF00FFu;
    const uint32_t ag = (((c >> 8) & 0x00FF00FFu) * a) & 0xFF00FF00u;
    return rb | ag;
}

// One output scanline: which source row to sample and where the span sits.
struct RowMap {
    float srcY = 0.f;  // continuous source row (pixel centres at .0)
    float xl = 0.f;    // output span, pixel units
    float xr = 0.f;
    float cov = 0.f;   // vertical coverage 0..1 (0 = skip the row)
};

// Draws one scanline: bilinear horizontal stretch of source row `m.srcY`
// into [xl, xr), with anti-aliased left and right edges.
void RenderRow(uint32_t* dst, int bw, const uint32_t* src, int aw, int ah, const RowMap& m) {
    if (m.cov <= 0.f || m.xr - m.xl < 0.25f) return;
    const float sy = std::clamp(m.srcY, 0.f, (float)(ah - 1));
    const int iy = (int)sy;
    const int iy1 = std::min(iy + 1, ah - 1);
    const uint32_t fy = (uint32_t)((sy - (float)iy) * 256.f);
    const uint32_t* r0 = src + (size_t)iy * aw;
    const uint32_t* r1 = src + (size_t)iy1 * aw;

    const float step = (float)aw / (m.xr - m.xl);
    const int xs = std::max(0, (int)std::floor(m.xl));
    const int xe = std::min(bw - 1, (int)std::floor(m.xr - 0.001f));
    if (xe < xs) return;
    const float maxU = (float)(aw - 1);
    float u = ((float)xs + 0.5f - m.xl) * step - 0.5f;

    for (int x = xs; x <= xe; x++, u += step) {
        const float uc = u < 0.f ? 0.f : (u > maxU ? maxU : u);
        const int ix = (int)uc;
        const int ix1 = std::min(ix + 1, aw - 1);
        const uint32_t fx = (uint32_t)((uc - (float)ix) * 256.f);
        uint32_t px = Lerp8(Lerp8(r0[ix], r0[ix1], fx), Lerp8(r1[ix], r1[ix1], fx), fy);

        float c = m.cov;
        if (x == xs || x == xe) {
            const float l = std::max((float)x, m.xl);
            const float r = std::min((float)x + 1.f, m.xr);
            c *= Sat(r - l);
        }
        if (c < 0.996f) px = Mul8(px, (uint32_t)(c * 256.f));
        dst[x] = px;
    }
}

// Uniform scale `s` of the whole picture around the centre (cx, cy).
void BuildAffine(std::vector<RowMap>& rows, int bh, float cx, float cy, float aw, float ah,
                 float s) {
    const float w = aw * s, h = ah * s;
    const float left = cx - w * 0.5f, top = cy - h * 0.5f;
    for (int y = 0; y < bh; y++) {
        const float t0 = std::max((float)y, top);
        const float t1 = std::min((float)y + 1.f, top + h);
        if (t1 <= t0) {
            rows[y].cov = 0.f;
            continue;
        }
        const float yc = (t0 + t1) * 0.5f;
        rows[y] = RowMap{(yc - top) / s - 0.5f, left, left + w, std::min(1.f, t1 - t0)};
    }
}

// Genie geometry in a "canonical" frame where the dock lies BELOW the window
// (y grows toward the dock). For a dock above the window the frame is flipped.
struct GenieGeom {
    float ax = 0, ay = 0, aw = 0, ah = 0;  // picture rectangle
    float dx = 0, dy = 0;                  // dock point (dy >= ay + ah)
    float hwMin = 0;                       // half width of the funnel at the dock
    bool flip = false;
};

// The mesh-warp model:
//  * Source row t (0 = far from the dock, 1 = nearest) has its own progress
//    q(t, p) = clamp((p - k (1 - t)) / (1 - k)). Rows nearest the dock leave
//    first, the far edge leaves last.
//  * A row slides toward the dock and narrows to the dock width with a
//    smoothstep, which gives the S-shaped sides of the genie funnel.
// p is the (already eased) animation progress in 0..1.
void BuildGenie(std::vector<RowMap>& rows, std::vector<float>& yb, int bh, const GenieGeom& g,
                float p) {
    for (auto& r : rows) r.cov = 0.f;
    const int n = (int)g.ah;
    if (n < 2) return;
    constexpr float k = 0.5f;
    const float cx0 = g.ax + g.aw * 0.5f;
    const float hw0 = g.aw * 0.5f;
    auto qOf = [&](float t) { return Sat((p - k * (1.f - t)) / (1.f - k)); };

    yb.resize((size_t)n + 1);
    for (int i = 0; i <= n; i++) {
        const float t = (float)i / (float)n;
        const float ys = g.ay + t * g.ah;
        float v = ys + (g.dy - ys) * SmoothStep(qOf(t));
        if (i > 0 && v < yb[i - 1]) v = yb[i - 1];  // keep it monotonic
        yb[i] = v;
    }

    int idx = 0;
    for (int y = 0; y < bh; y++) {
        const float yc = (float)y + 0.5f;
        if (yc < yb[0] || yc >= yb[n]) continue;
        while (idx < n - 1 && yb[idx + 1] <= yc) idx++;
        const float span = yb[idx + 1] - yb[idx];
        const float frac = span > 1e-4f ? (yc - yb[idx]) / span : 0.5f;
        const float s = (float)idx + frac;  // continuous source row
        const float e = SmoothStep(qOf(s / (float)n));
        const float xc = cx0 + (g.dx - cx0) * e;
        const float hw = hw0 + (g.hwMin - hw0) * e;
        RowMap m{s - 0.5f, xc - hw, xc + hw, 1.f};
        int oy = y;
        if (g.flip) {
            oy = bh - 1 - y;
            m.srcY = (float)(n - 1) - m.srcY;
        }
        rows[oy] = m;
    }
}

// Two-pass box blur of a single-channel image (used for the soft shadow).
void BoxBlur(std::vector<uint8_t>& a, int w, int h, int r) {
    if (r < 1) return;
    std::vector<uint8_t> tmp(a.size());
    const int d = 2 * r + 1;

    for (int y = 0; y < h; y++) {
        const uint8_t* src = &a[(size_t)y * w];
        uint8_t* dst = &tmp[(size_t)y * w];
        int sum = 0;
        for (int x = -r; x <= r; x++) sum += (x >= 0 && x < w) ? src[x] : 0;
        for (int x = 0; x < w; x++) {
            dst[x] = (uint8_t)(sum / d);
            int add = x + r + 1, sub = x - r;
            if (add < w) sum += src[add];
            if (sub >= 0) sum -= src[sub];
        }
    }
    for (int x = 0; x < w; x++) {
        int sum = 0;
        for (int y = -r; y <= r; y++) sum += (y >= 0 && y < h) ? tmp[(size_t)y * w + x] : 0;
        for (int y = 0; y < h; y++) {
            a[(size_t)y * w + x] = (uint8_t)(sum / d);
            int add = y + r + 1, sub = y - r;
            if (add < h) sum += tmp[(size_t)add * w + x];
            if (sub >= 0) sum -= tmp[(size_t)sub * w + x];
        }
    }
}

// ============================================================================
//  Win32 helpers
// ============================================================================

// A 32-bit top-down DIB section with its own memory DC.
struct Surface {
    HDC dc = nullptr;
    HBITMAP bmp = nullptr;
    HGDIOBJ old = nullptr;
    void* bits = nullptr;

    Surface() = default;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    bool Create(HDC ref, int w, int h) {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        bmp = CreateDIBSection(ref, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bmp) return false;
        dc = CreateCompatibleDC(ref);
        if (!dc) return false;
        old = SelectObject(dc, bmp);
        return true;
    }

    void Free() {
        if (dc) {
            if (old) SelectObject(dc, old);
            DeleteDC(dc);
        }
        if (bmp) DeleteObject(bmp);
        dc = nullptr;
        bmp = nullptr;
        old = nullptr;
        bits = nullptr;
    }

    ~Surface() { Free(); }
};

bool EnsureOverlayClass() {
    bool ok = true;
    AcquireSRWLockExclusive(&g_classLock);
    if (!g_classRegistered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = DefWindowProcW;  // user32 stays loaded, so this is safe
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kOverlayClass;
        ok = RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        g_classRegistered = ok;
    }
    ReleaseSRWLockExclusive(&g_classLock);
    return ok;
}

bool IsCloaked(HWND hwnd) {
    DWORD cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, kDwmCloaked, &cloaked, sizeof(cloaked))) &&
           cloaked;
}

// Checks that do not depend on whether the window is shown right now.
bool BaseEligible(HWND hwnd) {
    if (t_busy || !hwnd || !IsWindow(hwnd)) return false;
    if (GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId()) return false;

    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (style & WS_CHILD) return false;
    if (ex & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) return false;
    if (((style & WS_CAPTION) != WS_CAPTION) && !(style & WS_THICKFRAME)) return false;
    if (GetAncestor(hwnd, GA_PARENT) != GetDesktopWindow()) return false;

    BOOL anim = TRUE;  // respect the Windows "Animation effects" switch
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim, 0);
    if (!anim || GetSystemMetrics(SM_REMOTESESSION)) return false;

    WCHAR cls[64] = {};
    GetClassNameW(hwnd, cls, 64);
    static const wchar_t* const kSkip[] = {
        L"#32768", L"tooltips_class32", L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
        L"Progman", L"WorkerW", L"NotifyIconOverflowWindow", L"ForegroundStaging",
        L"Windows.UI.Core.CoreWindow", L"XamlExplorerHostIslandWindow",
        L"TopLevelWindowForOverflowXamlIsland", L"Xaml_WindowedPopupClass",
        L"MultitaskingViewFrame", L"TaskListThumbnailWnd", L"TaskListOverlayWnd",
        kOverlayClass};
    for (const wchar_t* s : kSkip)
        if (wcscmp(cls, s) == 0) return false;
    return true;
}

// A window that is on screen and can be captured now (close / hide / minimize).
bool ShouldAnimateVisible(HWND hwnd) {
    return BaseEligible(hwnd) && IsWindowVisible(hwnd) && !IsIconic(hwnd) && !IsCloaked(hwnd);
}

// A hidden window that is about to be shown (open).
bool ShouldAnimateOpen(HWND hwnd) {
    return BaseEligible(hwnd) && !IsWindowVisible(hwnd) && !IsIconic(hwnd) && !IsCloaked(hwnd);
}

bool CanMinimize(HWND hwnd) {
    return (GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_MINIMIZEBOX) != 0;
}

// Rough "is this capture empty?" test, used before the open animation.
bool LooksBlank(const uint32_t* px, int w, int h) {
    if (!px || w < 8 || h < 8) return true;
    const uint32_t first = px[0] & 0x00FCFCFCu;
    for (int sy = 1; sy < 16; sy++) {
        for (int sx = 1; sx < 16; sx++) {
            const uint32_t v = px[(size_t)(h * sy / 16) * w + (w * sx / 16)] & 0x00FCFCFCu;
            if (v != first) return false;
        }
    }
    return true;
}

// ---- Dock (taskbar) target -------------------------------------------------

enum class Edge { Bottom, Top, Left, Right };

struct Dock {
    POINT pt = {};
    Edge edge = Edge::Bottom;
    int half = 22;  // half the width of a taskbar button
};

bool FindTaskbarRect(HMONITOR mon, RECT* out) {
    RECT r;
    HWND h = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (h && GetWindowRect(h, &r) && MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST) == mon) {
        *out = r;
        return true;
    }
    HWND s = nullptr;
    while ((s = FindWindowExW(nullptr, s, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        if (GetWindowRect(s, &r) && MonitorFromRect(&r, MONITOR_DEFAULTTONEAREST) == mon) {
            *out = r;
            return true;
        }
    }
    return false;
}

Dock ComputeDock(HWND hwnd, const RECT& fr, UINT dpi) {
    Dock d;
    d.half = MulDiv(22, dpi, 96);

    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);

    RECT tb;
    if (!FindTaskbarRect(mon, &tb)) {  // auto-hidden or missing: assume a bottom bar
        tb = {mi.rcMonitor.left, mi.rcMonitor.bottom - MulDiv(48, dpi, 96), mi.rcMonitor.right,
              mi.rcMonitor.bottom};
    }
    const int tbw = tb.right - tb.left, tbh = tb.bottom - tb.top;
    const int mcx = (mi.rcMonitor.left + mi.rcMonitor.right) / 2;
    const int mcy = (mi.rcMonitor.top + mi.rcMonitor.bottom) / 2;
    const int winCx = (fr.left + fr.right) / 2;
    const int winCy = (fr.top + fr.bottom) / 2;

    if (tbw >= tbh) {
        d.edge = ((tb.top + tb.bottom) / 2 >= mcy) ? Edge::Bottom : Edge::Top;
        int x = (tb.left + tb.right) / 2;
        if (g_cfg.genieNearWindow)
            x = std::clamp(winCx, (int)tb.left + d.half, std::max((int)tb.left + d.half, (int)tb.right - d.half));
        d.pt.x = x;
        d.pt.y = (d.edge == Edge::Bottom) ? tb.top + tbh * 45 / 100 : tb.bottom - tbh * 45 / 100;
    } else {
        d.edge = ((tb.left + tb.right) / 2 >= mcx) ? Edge::Right : Edge::Left;
        d.pt.y = std::clamp(winCy, (int)tb.top + d.half, std::max((int)tb.top + d.half, (int)tb.bottom - d.half));
        d.pt.x = (d.edge == Edge::Left) ? tb.right - tbw * 45 / 100 : tb.left + tbw * 45 / 100;
    }
    return d;
}

// ============================================================================
//  The animation
// ============================================================================

class Anim {
   public:
    enum Kind { Close, Open, Minimize };

    Anim() = default;
    Anim(const Anim&) = delete;
    Anim& operator=(const Anim&) = delete;
    ~Anim() { End(); }

    // Open animation only: the window was cloaked by the caller; End() un-cloaks it.
    void MarkCloaked(HWND hwnd) {
        m_target = hwnd;
        m_cloaked = true;
    }

    bool Prepare(HWND hwnd, Kind kind) {
        t_busy = true;
        m_started = true;
        m_target = hwnd;
        m_kind = kind;
        if (g_pSetThreadDpi) m_oldDpi = g_pSetThreadDpi((HANDLE)(INT_PTR)-4);  // PER_MONITOR_AWARE_V2

        RECT wr, fr;
        if (!GetWindowRect(hwnd, &wr)) return false;
        if (FAILED(DwmGetWindowAttribute(hwnd, kDwmExtendedFrameBounds, &fr, sizeof(fr)))) fr = wr;
        if (fr.left < wr.left || fr.top < wr.top || fr.right > wr.right || fr.bottom > wr.bottom)
            fr = wr;

        const int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
        const int fw = fr.right - fr.left, fh = fr.bottom - fr.top;
        if (fw < 120 || fh < 60 || (long long)fw * fh > 8500000LL) return false;  // up to ~4K

        const UINT dpi = g_pGetDpiForWindow ? g_pGetDpiForWindow(hwnd) : 96;
        const bool zoomed = IsZoomed(hwnd) != FALSE;
        const int pad = (g_cfg.shadow && !zoomed) ? MulDiv(48, dpi, 96) : 0;
        const int ow = fw + 2 * pad, oh = fh + 2 * pad;

        // Corner radius (Windows 11 only; square when maximized).
        int radius = 0;
        DWORD pref = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, kDwmCornerPreference, &pref, sizeof(pref))) &&
            !zoomed) {
            if (pref == 1) radius = 0;
            else if (pref == 3) radius = MulDiv(4, dpi, 96);
            else radius = MulDiv(8, dpi, 96);
        }

        HDC screen = GetDC(nullptr);
        struct ScreenRelease {
            HDC dc;
            ~ScreenRelease() { ReleaseDC(nullptr, dc); }
        } screenRelease{screen};

        // 1) Capture the window.
        Surface cap;
        if (!cap.Create(screen, ww, wh)) return false;
        if (!PrintWindow(hwnd, cap.dc, PW_RENDERFULLCONTENT)) return false;
        const uint32_t* capPx = (const uint32_t*)cap.bits;
        const int ox = fr.left - wr.left, oy = fr.top - wr.top;
        if (kind == Open && LooksBlank(capPx + (size_t)oy * ww + ox, fw, fh)) return false;

        // 2) Premultiplied artwork: shadow + window with rounded corners.
        if (!m_art.Create(screen, ow, oh)) return false;
        uint32_t* art = (uint32_t*)m_art.bits;

        auto cov = [&](int x, int y) -> int {
            if (radius <= 0) return 255;
            float dx = 0, dy = 0;
            if (x < radius) dx = radius - (x + 0.5f);
            else if (x >= fw - radius) dx = (x + 0.5f) - (fw - radius);
            if (y < radius) dy = radius - (y + 0.5f);
            else if (y >= fh - radius) dy = (y + 0.5f) - (fh - radius);
            if (dx <= 0 || dy <= 0) return 255;
            float c = radius - std::sqrt(dx * dx + dy * dy) + 0.5f;
            return c <= 0 ? 0 : (c >= 1 ? 255 : (int)(c * 255 + 0.5f));
        };
        auto covRow = [&](int y, uint8_t* out) {
            memset(out, 255, fw);
            if (radius > 0 && (y < radius || y >= fh - radius)) {
                for (int x = 0; x < radius; x++) {
                    out[x] = (uint8_t)cov(x, y);
                    out[fw - 1 - x] = (uint8_t)cov(fw - 1 - x, y);
                }
            }
        };
        std::vector<uint8_t> rowCov(fw);

        if (pad > 0) {
            std::vector<uint8_t> mask((size_t)ow * oh, 0);
            const int offY = pad / 4;  // light comes from above
            for (int y = 0; y < fh; y++) {
                covRow(y, rowCov.data());
                memcpy(&mask[(size_t)(pad + offY + y) * ow + pad], rowCov.data(), fw);
            }
            const int br = std::max(1, pad / 3);
            BoxBlur(mask, ow, oh, br);
            BoxBlur(mask, ow, oh, br);
            for (size_t i = 0; i < mask.size(); i++)
                art[i] = (uint32_t)(mask[i] * 38 / 100) << 24;  // black, ~38% max opacity
        }

        for (int y = 0; y < fh; y++) {
            covRow(y, rowCov.data());
            const uint32_t* s = capPx + (size_t)(oy + y) * ww + ox;
            uint32_t* d = art + (size_t)(pad + y) * ow + pad;
            for (int x = 0; x < fw; x++) {
                const uint32_t c = rowCov[x];
                const uint32_t px = s[x];
                if (c == 255) {
                    d[x] = 0xFF000000u | (px & 0x00FFFFFFu);
                } else if (c != 0) {
                    uint32_t r = ((px >> 16) & 0xFF) * c / 255;
                    uint32_t g = ((px >> 8) & 0xFF) * c / 255;
                    uint32_t b = (px & 0xFF) * c / 255;
                    uint32_t sa = d[x] >> 24;  // shadow alpha underneath
                    uint32_t a = c + sa * (255 - c) / 255;
                    d[x] = (a << 24) | (r << 16) | (g << 8) | b;
                }
            }
        }
        cap.Free();

        // 3) Geometry of the overlay for this kind of animation.
        m_aw = ow;
        m_ah = oh;
        const RECT A = {fr.left - pad, fr.top - pad, fr.right + pad, fr.bottom + pad};
        if (kind == Minimize) {
            const Dock dock = ComputeDock(hwnd, fr, dpi);
            m_genie = g_cfg.minStyle == MinStyle::Genie &&
                      (dock.edge == Edge::Bottom || dock.edge == Edge::Top);
            long dxs = dock.pt.x, dys = dock.pt.y;
            long L, T, R, B;
            if (m_genie) {
                const bool top = dock.edge == Edge::Top;
                // The dock point must lie beyond the picture so rows stay in order.
                if (top) dys = std::min(dys, A.top - 2L);
                else dys = std::max(dys, A.bottom + 2L);
                L = std::min((long)A.left, dxs - dock.half - 2);
                R = std::max((long)A.right, dxs + dock.half + 2);
                T = top ? dys - 2 : A.top;
                B = top ? A.bottom : dys + 2;
                m_geo.flip = top;
                m_geo.ax = (float)(A.left - L);
                m_geo.aw = (float)ow;
                m_geo.ah = (float)oh;
                m_geo.ay = top ? (float)(B - A.bottom) : (float)(A.top - T);
                m_geo.dx = (float)(dxs - L);
                m_geo.dy = top ? (float)(B - dys) : (float)(dys - T);
                m_geo.hwMin = std::max(3.f, (float)dock.half * 0.6f);
            } else {
                L = std::min((long)A.left, dxs - 2);
                R = std::max((long)A.right, dxs + 2);
                T = std::min((long)A.top, dys - 2);
                B = std::max((long)A.bottom, dys + 2);
                m_cx0 = (float)((A.left + A.right) * 0.5 - L);
                m_cy0 = (float)((A.top + A.bottom) * 0.5 - T);
                m_cx1 = (float)(dxs - L);
                m_cy1 = (float)(dys - T);
                m_sEnd = std::max(0.04f, (float)(2 * dock.half) / (float)fw);
            }
            m_pos = {L, T};
            m_bw = (int)(R - L);
            m_bh = (int)(B - T);
        } else {
            m_pos = {A.left, A.top};
            m_bw = ow;
            m_bh = oh;
            m_cx0 = ow * 0.5f;
            m_cy0 = oh * 0.5f;
        }
        if (m_bw < 1 || m_bh < 1 || (long long)m_bw * m_bh > 24000000LL) return false;
        if (!m_frame.Create(screen, m_bw, m_bh)) return false;
        m_rows.assign((size_t)m_bh, RowMap());

        // 4) Overlay window, placed just above the target window in z-order.
        if (!EnsureOverlayClass()) return false;
        m_overlay = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            kOverlayClass, L"", WS_POPUP, m_pos.x, m_pos.y, m_bw, m_bh, nullptr, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        if (!m_overlay) return false;

        Frame(0.0);
        HWND prev = GetWindow(hwnd, GW_HWNDPREV);
        HWND after = prev ? prev
                          : ((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) ? HWND_TOPMOST
                                                                                    : HWND_TOP);
        SetWindowPos(m_overlay, after, m_pos.x, m_pos.y, m_bw, m_bh,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);

        // 5) Turn off Windows' own transition, then wait one composition so the
        //    overlay is on screen before the real window changes.
        BOOL on = TRUE;
        DwmSetWindowAttribute(hwnd, kDwmTransitionsForceDisabled, &on, sizeof(on));
        m_transDisabled = true;
        DwmFlush();
        return true;
    }

    void Play() {
        if (!m_overlay) return;
        LARGE_INTEGER freq, t0, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t0);
        const double dur = (double)DurationMs();
        for (;;) {
            QueryPerformanceCounter(&now);
            const double ms = (double)(now.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart;
            const double t = ms / dur;
            if (t >= 1.0 || ms > dur * 3.0) break;
            Frame(t);
            if (FAILED(DwmFlush())) Sleep(4);
        }
        if (m_kind == Open) {  // exact final pixels before the real window is un-cloaked
            Frame(1.0);
            DwmFlush();
        }
    }

   private:
    int DurationMs() const {
        switch (m_kind) {
            case Close: return g_cfg.closeMs;
            case Open: return g_cfg.openMs;
            default: return g_cfg.minMs;
        }
    }

    // Renders animation time t (0..1) into the overlay.
    void Frame(double t) {
        double alpha = 1.0;
        switch (m_kind) {
            case Close: {
                const double s = 1.0 - (1.0 - g_cfg.closeScale) * EaseOutCubic(t);
                alpha = 1.0 - EaseInOutSine(t);
                BuildAffine(m_rows, m_bh, m_cx0, m_cy0, (float)m_aw, (float)m_ah, (float)s);
                break;
            }
            case Open: {
                const double s = g_cfg.openScale + (1.0 - g_cfg.openScale) * EaseOutCubic(t);
                alpha = EaseOutCubic(std::min(1.0, t / 0.65));
                BuildAffine(m_rows, m_bh, m_cx0, m_cy0, (float)m_aw, (float)m_ah, (float)s);
                break;
            }
            case Minimize: {
                const float p = (float)EaseInOutSine(t);
                if (m_genie) {
                    BuildGenie(m_rows, m_yb, m_bh, m_geo, p);
                    alpha = 1.0 - SmoothStep((p - 0.86f) / 0.14f);
                } else {
                    const float s = 1.f + (m_sEnd - 1.f) * p;
                    const float cx = m_cx0 + (m_cx1 - m_cx0) * p;
                    const float cy = m_cy0 + (m_cy1 - m_cy0) * p;
                    BuildAffine(m_rows, m_bh, cx, cy, (float)m_aw, (float)m_ah, s);
                    alpha = 1.0 - SmoothStep((p - 0.7f) / 0.3f);
                }
                break;
            }
        }

        uint32_t* frame = (uint32_t*)m_frame.bits;
        memset(frame, 0, (size_t)m_bw * m_bh * 4);
        const uint32_t* art = (const uint32_t*)m_art.bits;
        for (int y = 0; y < m_bh; y++)
            RenderRow(frame + (size_t)y * m_bw, m_bw, art, m_aw, m_ah, m_rows[y]);

        POINT ptSrc = {0, 0};
        SIZE sz = {m_bw, m_bh};
        BLENDFUNCTION bf = {AC_SRC_OVER, 0,
                            (BYTE)std::clamp((int)std::lround(alpha * 255.0), 0, 255),
                            AC_SRC_ALPHA};
        UpdateLayeredWindow(m_overlay, nullptr, &m_pos, &sz, m_frame.dc, &ptSrc, 0, &bf,
                            ULW_ALPHA);
    }

    void End() {
        if (m_cloaked) {  // open animation: reveal the real window under the overlay
            if (IsWindow(m_target)) {
                BOOL off = FALSE;
                DwmSetWindowAttribute(m_target, kDwmCloak, &off, sizeof(off));
                if (m_overlay) DwmFlush();
            }
            m_cloaked = false;
        }
        if (m_overlay) {
            DestroyWindow_Original(m_overlay);  // original: avoids re-entering our hook
            m_overlay = nullptr;
        }
        if (m_transDisabled) {
            if (IsWindow(m_target)) {  // hidden-not-destroyed: restore its normal transitions
                BOOL off = FALSE;
                DwmSetWindowAttribute(m_target, kDwmTransitionsForceDisabled, &off, sizeof(off));
            }
            m_transDisabled = false;
        }
        m_art.Free();
        m_frame.Free();
        if (m_oldDpi && g_pSetThreadDpi) g_pSetThreadDpi(m_oldDpi);
        m_oldDpi = nullptr;
        if (m_started) t_busy = false;
        m_started = false;
    }

    HWND m_target = nullptr;
    HWND m_overlay = nullptr;
    HANDLE m_oldDpi = nullptr;
    Kind m_kind = Close;
    bool m_started = false;
    bool m_transDisabled = false;
    bool m_cloaked = false;
    bool m_genie = false;
    int m_aw = 0, m_ah = 0;  // artwork size
    int m_bw = 0, m_bh = 0;  // overlay (frame) size
    POINT m_pos = {};
    float m_cx0 = 0, m_cy0 = 0, m_cx1 = 0, m_cy1 = 0, m_sEnd = 0.1f;
    GenieGeom m_geo;
    std::vector<RowMap> m_rows;
    std::vector<float> m_yb;
    Surface m_art, m_frame;
};

bool SafePrepare(Anim& anim, HWND hwnd, Anim::Kind kind) {
    try {
        return anim.Prepare(hwnd, kind);
    } catch (...) {
        return false;
    }
}

// ============================================================================
//  Hooks
// ============================================================================

bool IsMinimizeCmd(int cmd) {
    return cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED || cmd == SW_SHOWMINNOACTIVE ||
           cmd == SW_FORCEMINIMIZE;
}

bool IsShowCmd(int cmd) {
    return cmd == SW_SHOWNORMAL || cmd == SW_SHOW || cmd == SW_SHOWDEFAULT ||
           cmd == SW_SHOWMAXIMIZED;
}

BOOL OpenWithAnimation(HWND hWnd, int cmd) {
    BOOL on = TRUE;
    if (FAILED(DwmSetWindowAttribute(hWnd, kDwmCloak, &on, sizeof(on))))
        return ShowWindow_Original(hWnd, cmd);  // cloaking not allowed: plain open

    Anim anim;
    anim.MarkCloaked(hWnd);  // from here on ~Anim always un-cloaks
    BOOL ret = ShowWindow_Original(hWnd, cmd);
    if (IsWindowVisible(hWnd)) {
        RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        if (SafePrepare(anim, hWnd, Anim::Open)) anim.Play();
    }
    return ret;
}

BOOL WINAPI DestroyWindow_Hook(HWND hWnd) {
    Anim anim;
    bool on = false;
    if (g_cfg.closeOn && ShouldAnimateVisible(hWnd)) on = SafePrepare(anim, hWnd, Anim::Close);
    BOOL ret = DestroyWindow_Original(hWnd);
    if (on && ret) anim.Play();
    return ret;
}

BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    if (nCmdShow == SW_HIDE) {
        if (!g_cfg.closeOn || !g_cfg.animateHide || !ShouldAnimateVisible(hWnd))
            return ShowWindow_Original(hWnd, nCmdShow);
        Anim anim;
        const bool on = SafePrepare(anim, hWnd, Anim::Close);
        BOOL ret = ShowWindow_Original(hWnd, nCmdShow);
        if (on && !IsWindowVisible(hWnd)) anim.Play();
        return ret;
    }

    if (IsMinimizeCmd(nCmdShow)) {
        if (g_cfg.minStyle == MinStyle::Off || !CanMinimize(hWnd) || !ShouldAnimateVisible(hWnd))
            return ShowWindow_Original(hWnd, nCmdShow);
        Anim anim;
        const bool on = SafePrepare(anim, hWnd, Anim::Minimize);
        BOOL ret = ShowWindow_Original(hWnd, nCmdShow);
        if (on && IsIconic(hWnd)) anim.Play();
        return ret;
    }

    if (g_cfg.openOn && IsShowCmd(nCmdShow) && ShouldAnimateOpen(hWnd))
        return OpenWithAnimation(hWnd, nCmdShow);

    return ShowWindow_Original(hWnd, nCmdShow);
}

// Windows closes and minimizes most windows inside DefWindowProc, which never
// goes through the DestroyWindow / ShowWindow exports.
LRESULT DefProcImpl(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, DefProc_t orig) {
    if (msg == WM_CLOSE && g_cfg.closeOn) {
        if (!ShouldAnimateVisible(hWnd)) return orig(hWnd, msg, wParam, lParam);
        Anim anim;
        const bool on = SafePrepare(anim, hWnd, Anim::Close);
        LRESULT r = orig(hWnd, msg, wParam, lParam);
        if (on && !IsWindow(hWnd)) anim.Play();  // only if the window really was destroyed
        return r;
    }
    if (msg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_MINIMIZE &&
        g_cfg.minStyle != MinStyle::Off) {
        if (!CanMinimize(hWnd) || !ShouldAnimateVisible(hWnd))
            return orig(hWnd, msg, wParam, lParam);
        Anim anim;
        const bool on = SafePrepare(anim, hWnd, Anim::Minimize);
        LRESULT r = orig(hWnd, msg, wParam, lParam);
        if (on && IsIconic(hWnd)) anim.Play();
        return r;
    }
    return orig(hWnd, msg, wParam, lParam);
}

LRESULT WINAPI DefWindowProcW_Hook(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefProcImpl(h, m, w, l, DefWindowProcW_Original);
}
LRESULT WINAPI DefWindowProcA_Hook(HWND h, UINT m, WPARAM w, LPARAM l) {
    return DefProcImpl(h, m, w, l, DefWindowProcA_Original);
}

// ============================================================================
//  Settings
// ============================================================================

std::wstring GetStr(PCWSTR name) {
    PCWSTR s = Wh_GetStringSetting(name);
    std::wstring r = s ? s : L"";
    Wh_FreeStringSetting(s);
    return r;
}

void LoadSettings() {
    g_cfg.closeOn = Wh_GetIntSetting(L"closeEnabled") != 0;
    g_cfg.closeMs = std::clamp((int)Wh_GetIntSetting(L"closeDuration"), 60, 1000);
    g_cfg.closeScale = std::clamp((int)Wh_GetIntSetting(L"closeScale"), 50, 100) / 100.0;
    g_cfg.openOn = Wh_GetIntSetting(L"openEnabled") != 0;
    g_cfg.openMs = std::clamp((int)Wh_GetIntSetting(L"openDuration"), 60, 1000);
    g_cfg.openScale = std::clamp((int)Wh_GetIntSetting(L"openScale"), 50, 100) / 100.0;
    g_cfg.minMs = std::clamp((int)Wh_GetIntSetting(L"minimizeDuration"), 100, 2000);

    const std::wstring style = GetStr(L"minimizeStyle");
    g_cfg.minStyle = style == L"off" ? MinStyle::Off
                     : style == L"scale" ? MinStyle::Scale
                                         : MinStyle::Genie;
    g_cfg.genieNearWindow = GetStr(L"genieTarget") == L"window";

    g_cfg.shadow = Wh_GetIntSetting(L"shadow") != 0;
    g_cfg.animateHide = Wh_GetIntSetting(L"animateHide") != 0;
    g_cfg.hookDefProc = Wh_GetIntSetting(L"hookDefWindowProc") != 0;

    g_cfg.excluded.clear();
    for (int i = 0;; i++) {
        PCWSTR s = Wh_GetStringSetting(L"excludedPrograms[%d]", i);
        bool has = *s != L'\0';
        if (has) g_cfg.excluded.emplace_back(s);
        Wh_FreeStringSetting(s);
        if (!has) break;
    }
}

bool IsExcludedProcess() {
    WCHAR path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    const WCHAR* name = wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    for (const auto& e : g_cfg.excluded)
        if (_wcsicmp(name, e.c_str()) == 0) return true;
    return false;
}

}  // namespace

BOOL Wh_ModInit() {
    LoadSettings();
    if (IsExcludedProcess()) return FALSE;
    if (!LoadImports()) return FALSE;

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        g_pSetThreadDpi =
            (SetThreadDpiAwarenessContext_t)GetProcAddress(user32, "SetThreadDpiAwarenessContext");
        g_pGetDpiForWindow = (GetDpiForWindow_t)GetProcAddress(user32, "GetDpiForWindow");
    }

    Wh_SetFunctionHook((void*)DestroyWindow, (void*)DestroyWindow_Hook,
                       (void**)&DestroyWindow_Original);
    Wh_SetFunctionHook((void*)ShowWindow, (void*)ShowWindow_Hook, (void**)&ShowWindow_Original);
    if (g_cfg.hookDefProc) {
        Wh_SetFunctionHook((void*)DefWindowProcW, (void*)DefWindowProcW_Hook,
                           (void**)&DefWindowProcW_Original);
        Wh_SetFunctionHook((void*)DefWindowProcA, (void*)DefWindowProcA_Hook,
                           (void**)&DefWindowProcA_Original);
    }
    return TRUE;
}

void Wh_ModUninit() {
    AcquireSRWLockExclusive(&g_classLock);
    if (g_classRegistered) {
        UnregisterClassW(kOverlayClass, GetModuleHandleW(nullptr));
        g_classRegistered = false;
    }
    ReleaseSRWLockExclusive(&g_classLock);
}

BOOL Wh_ModSettingsChanged(BOOL* bReload) {
    *bReload = TRUE;  // reload so the exclusion list is re-evaluated per process
    return TRUE;
}
