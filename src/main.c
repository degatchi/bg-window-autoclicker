// AutoClicker (macOS + Windows 11) — list the active windows, pick one, and
// spam the Enter keystroke to that window until it is closed (or Ctrl+C).
//
// -------- Build (macOS) --------
//   cc -O2 -Wall -o autoclicker main.c -framework CoreGraphics -framework CoreFoundation
//   (or just: make)
//
// -------- Build (Windows 11) --------
//   MSVC:   cl /nologo /W3 /O2 main.c /Fe:autoclicker.exe user32.lib
//           (or just: build.bat)
//   MinGW:  gcc -O2 -Wall main.c -o autoclicker.exe -luser32
//
// Usage:
//   autoclicker [interval_ms] [--foreground]
//     interval_ms   delay between presses (default 200)
//     --foreground  Windows only: focus the window and synthesize a real
//                   keystroke, for apps/games that ignore posted messages.
//
// Permissions:
//   macOS  - needs Accessibility permission for the sending app
//            (System Settings -> Privacy & Security -> Accessibility).
//            Screen Recording is only needed to also read window titles.
//   Windows- no special permission for normal apps. To send into a window
//            running elevated (as admin), run this program elevated too.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <CoreGraphics/CoreGraphics.h>
#  include <CoreFoundation/CoreFoundation.h>
#  include <libproc.h>
#  include <unistd.h>
#  define KEY_RETURN 0x24  // kVK_Return
#endif

#define MAX_WINDOWS 512
#define NAME_LEN 256
#define PATH_BUF 4096

typedef struct {
#ifdef _WIN32
    HWND handle;       // target window handle
#else
    CGWindowID window_id;
#endif
    long pid;              // owning process id
    char owner[NAME_LEN];  // app name (macOS) or exe base name (Windows)
    char title[NAME_LEN];  // window title (may be empty)
} WindowInfo;

static volatile sig_atomic_t g_stop = 0;

// ---------------------------------------------------------------------------
// Windows implementation
// ---------------------------------------------------------------------------
#ifdef _WIN32

static BOOL WINAPI ctrl_handler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
        type == CTRL_CLOSE_EVENT) {
        g_stop = 1;
        return TRUE;
    }
    return FALSE;
}

static void install_stop_handler(void) {
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
}

static void sleep_ms(long ms) {
    Sleep((DWORD)ms);
}

// Full executable path of a process, e.g. "C:\\...\\chrome.exe".
static void proc_full_path(long pid, char *buf, size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return;
    }
    buf[0] = '\0';
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (h != NULL) {
        DWORD sz = (DWORD)buflen;
        if (!QueryFullProcessImageNameA(h, 0, buf, &sz)) {
            buf[0] = '\0';
        }
        CloseHandle(h);
    }
    if (buf[0] == '\0') {
        snprintf(buf, buflen, "(unknown)");
    }
}

static const char *base_name(const char *path) {
    const char *b = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '\\' || *p == '/') {
            b = p + 1;
        }
    }
    return b;
}

typedef struct {
    WindowInfo *out;
    int max;
    int count;
} EnumCtx;

static BOOL CALLBACK enum_cb(HWND hwnd, LPARAM lparam) {
    EnumCtx *ctx = (EnumCtx *)lparam;
    if (ctx->count >= ctx->max) {
        return FALSE;  // stop enumerating
    }
    // Approximate the Alt+Tab list: visible, titled, top-level, not a tool
    // window.
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    if (GetWindow(hwnd, GW_OWNER) != NULL) {
        return TRUE;
    }
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (ex & WS_EX_TOOLWINDOW) {
        return TRUE;
    }
    if (GetWindowTextLengthA(hwnd) <= 0) {
        return TRUE;
    }

    WindowInfo *w = &ctx->out[ctx->count];
    memset(w, 0, sizeof(*w));
    w->handle = hwnd;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    w->pid = (long)pid;

    GetWindowTextA(hwnd, w->title, (int)sizeof(w->title));

    char path[PATH_BUF];
    proc_full_path((long)pid, path, sizeof(path));
    snprintf(w->owner, sizeof(w->owner), "%.*s",
             (int)(sizeof(w->owner) - 1), base_name(path));

    ctx->count++;
    return TRUE;
}

static int list_windows(WindowInfo *out, int max) {
    EnumCtx ctx = {out, max, 0};
    EnumWindows(enum_cb, (LPARAM)&ctx);  // FALSE return = stopped early, not an error
    return ctx.count;
}

static bool window_is_open(const WindowInfo *w) {
    return IsWindow(w->handle) ? true : false;
}

static void send_enter(const WindowInfo *w, bool foreground) {
    if (foreground) {
        // Bring the window to the front and synthesize a hardware-level key
        // event. Works for apps that ignore posted messages, but it steals
        // focus, so the target should be the window you want focused.
        SetForegroundWindow(w->handle);
        Sleep(10);
        INPUT in[2];
        memset(in, 0, sizeof(in));
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wVk = VK_RETURN;
        in[1].type = INPUT_KEYBOARD;
        in[1].ki.wVk = VK_RETURN;
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
        return;
    }

    // Default: post to the focused control of the target window's thread,
    // without stealing focus (mirrors the macOS background behavior).
    HWND target = w->handle;
    DWORD cur = GetCurrentThreadId();
    DWORD tid = GetWindowThreadProcessId(w->handle, NULL);
    BOOL attached = FALSE;
    if (tid != cur) {
        attached = AttachThreadInput(cur, tid, TRUE);
    }
    HWND focus = GetFocus();
    if (focus != NULL) {
        target = focus;
    }

    UINT sc = MapVirtualKey(VK_RETURN, MAPVK_VK_TO_VSC);
    LPARAM down = (LPARAM)1 | ((LPARAM)sc << 16);
    LPARAM up = (LPARAM)1 | ((LPARAM)sc << 16) |
                ((LPARAM)1 << 30) | ((LPARAM)1 << 31);
    PostMessageA(target, WM_KEYDOWN, VK_RETURN, down);
    PostMessageA(target, WM_KEYUP, VK_RETURN, up);

    if (attached) {
        AttachThreadInput(cur, tid, FALSE);
    }
}

// ---------------------------------------------------------------------------
// macOS implementation
// ---------------------------------------------------------------------------
#else

static void handle_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

static void install_stop_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
}

static void sleep_ms(long ms) {
    usleep((useconds_t)(ms * 1000));
}

// Full executable path of a process, e.g. "/Applications/Safari.app/...".
static void proc_full_path(long pid, char *buf, size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return;
    }
    buf[0] = '\0';
    if (proc_pidpath((pid_t)pid, buf, (uint32_t)buflen) <= 0) {
        snprintf(buf, buflen, "(unknown)");
    }
}

static void cfstring_to_cstr(CFStringRef str, char *buf, size_t buflen) {
    if (buf == NULL || buflen == 0) {
        return;
    }
    buf[0] = '\0';
    if (str == NULL) {
        return;
    }
    CFStringGetCString(str, buf, (CFIndex)buflen, kCFStringEncodingUTF8);
}

static int list_windows(WindowInfo *out, int max) {
    CGWindowListOption opts =
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements;
    CFArrayRef windows = CGWindowListCopyWindowInfo(opts, kCGNullWindowID);
    if (windows == NULL) {
        return -1;
    }

    int count = 0;
    CFIndex n = CFArrayGetCount(windows);
    for (CFIndex i = 0; i < n && count < max; i++) {
        CFDictionaryRef dict = CFArrayGetValueAtIndex(windows, i);
        if (dict == NULL) {
            continue;
        }

        int layer = 0;
        CFNumberRef layer_ref = CFDictionaryGetValue(dict, kCGWindowLayer);
        if (layer_ref != NULL) {
            CFNumberGetValue(layer_ref, kCFNumberIntType, &layer);
        }
        if (layer != 0) {
            continue;  // skip menu bar, Dock, and other system UI
        }

        CFNumberRef id_ref = CFDictionaryGetValue(dict, kCGWindowNumber);
        CFNumberRef pid_ref = CFDictionaryGetValue(dict, kCGWindowOwnerPID);
        if (id_ref == NULL || pid_ref == NULL) {
            continue;
        }

        WindowInfo *w = &out[count];
        memset(w, 0, sizeof(*w));

        int64_t wid = 0;
        int64_t pid = 0;
        CFNumberGetValue(id_ref, kCFNumberSInt64Type, &wid);
        CFNumberGetValue(pid_ref, kCFNumberSInt64Type, &pid);
        w->window_id = (CGWindowID)wid;
        w->pid = (long)pid;

        cfstring_to_cstr(CFDictionaryGetValue(dict, kCGWindowOwnerName),
                         w->owner, sizeof(w->owner));
        cfstring_to_cstr(CFDictionaryGetValue(dict, kCGWindowName),
                         w->title, sizeof(w->title));

        count++;
    }

    CFRelease(windows);
    return count;
}

static bool window_is_open(const WindowInfo *w) {
    CFArrayRef windows = CGWindowListCopyWindowInfo(
        kCGWindowListOptionIncludingWindow, w->window_id);
    if (windows == NULL) {
        return false;
    }
    bool open = CFArrayGetCount(windows) > 0;
    CFRelease(windows);
    return open;
}

static void send_enter(const WindowInfo *w, bool foreground) {
    (void)foreground;  // not used on macOS
    static CGEventSourceRef source = NULL;
    if (source == NULL) {
        source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    }
    CGEventRef down = CGEventCreateKeyboardEvent(source, KEY_RETURN, true);
    CGEventRef up = CGEventCreateKeyboardEvent(source, KEY_RETURN, false);
    if (down != NULL) {
        CGEventPostToPid((pid_t)w->pid, down);
        CFRelease(down);
    }
    if (up != NULL) {
        CGEventPostToPid((pid_t)w->pid, up);
        CFRelease(up);
    }
}

#endif  // _WIN32

// ---------------------------------------------------------------------------
// Shared code
// ---------------------------------------------------------------------------

static int read_int(const char *prompt, int min, int max, int fallback) {
    char line[64];
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        return fallback;
    }
    char *end = NULL;
    long v = strtol(line, &end, 10);
    if (end == line) {
        return fallback;
    }
    if (v < min) v = min;
    if (v > max) v = max;
    return (int)v;
}

int main(int argc, char **argv) {
    long interval_ms = 200;
    bool foreground = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--foreground") == 0 ||
            strcmp(argv[i], "-f") == 0) {
            foreground = true;
            continue;
        }
        char *end = NULL;
        long v = strtol(argv[i], &end, 10);
        if (end != argv[i] && *end == '\0' && v > 0) {
            interval_ms = v;
        } else {
            fprintf(stderr, "Usage: %s [interval_ms] [--foreground]\n", argv[0]);
            return 1;
        }
    }

    WindowInfo windows[MAX_WINDOWS];
    int count = list_windows(windows, MAX_WINDOWS);
    if (count < 0) {
        fprintf(stderr, "Failed to read the window list.\n");
#ifndef _WIN32
        fprintf(stderr, "Grant this terminal Screen Recording / Accessibility "
                        "permission and retry.\n");
#endif
        return 1;
    }
    if (count == 0) {
        fprintf(stderr, "No application windows found.\n");
        return 1;
    }

    printf("\nActive windows:\n");
    for (int i = 0; i < count; i++) {
        const char *title = windows[i].title[0] ? windows[i].title : "(no title)";
        char path[PATH_BUF];
        proc_full_path(windows[i].pid, path, sizeof(path));
        printf("  [%2d] %s  (pid %ld)\n", i, windows[i].owner, windows[i].pid);
        printf("       title: %s\n", title);
        printf("       proc:  %s\n", path);
    }

    int choice = read_int("\nSelect a window number: ", 0, count - 1, -1);
    if (choice < 0 || choice >= count) {
        fprintf(stderr, "Invalid selection.\n");
        return 1;
    }

    WindowInfo target = windows[choice];
    const char *ttitle = target.title[0] ? target.title : "(no title)";
    char target_path[PATH_BUF];
    proc_full_path(target.pid, target_path, sizeof(target_path));

    printf("\nSpamming Enter to:\n");
    printf("  app:   %s\n", target.owner);
    printf("  title: %s\n", ttitle);
    printf("  proc:  %s\n", target_path);
    printf("  pid:   %ld   interval: %ld ms%s\n", target.pid, interval_ms,
           foreground ? "   mode: foreground" : "");
    printf("\nPress Ctrl+C to stop; stops automatically when the window closes.\n\n");

    install_stop_handler();

    unsigned long sent = 0;
    while (!g_stop) {
        if (!window_is_open(&target)) {
            printf("\nWindow closed. Stopping after %lu keystroke(s).\n", sent);
            break;
        }
        send_enter(&target, foreground);
        sent++;
        printf("\rEnter sent: %lu", sent);
        fflush(stdout);
        sleep_ms(interval_ms);
    }

    if (g_stop) {
        printf("\nInterrupted. Sent %lu keystroke(s).\n", sent);
    }
    return 0;
}
