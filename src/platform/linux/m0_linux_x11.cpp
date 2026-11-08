// Milestone 0 (Linux/X11): borderless window with compositor shadow + rounded region hint.
// Build: g++ m0_linux_x11.cpp -o m0_linux_x11 -lX11 -lXext
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cstdio>
#include <cstdlib>

int main() {
    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) { fprintf(stderr, "Cannot open display\\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window root = RootWindow(dpy, scr);
    int w = 900, h = 600;
    XSetWindowAttributes swa{};
    swa.override_redirect = True;
    swa.background_pixel = BlackPixel(dpy, scr);
    Window win = XCreateWindow(dpy, root, 100, 100, w, h, 0,
        CopyFromParent, InputOutput, CopyFromParent,
        CWOverrideRedirect | CWBackPixel, &swa);
    // Hint compositor to draw shadow (best-effort; WM/compositor dependent).
    // _NET_WM_WINDOW_TYPE
    Atom net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom net_wm_window_type_normal = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    XChangeProperty(dpy, win, net_wm_window_type, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&net_wm_window_type_normal, 1);
    XMapWindow(dpy, win);
    XFlush(dpy);
    fprintf(stderr, "mapped window %lu\\n", (unsigned long)win);
    fflush(stderr);

    XEvent ev;
    while (true) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) break;
    }
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
