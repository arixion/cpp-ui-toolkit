// Milestone 0 (Linux/Wayland): xdg-toplevel, no decorations, compositor-drawn shadow.
// Build: wayland-scanner + wayland-client (see CMake for protocol generation).
#include <wayland-client.h>
#include <wayland-egl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_surface *surface;
static struct xdg_wm_base *xdg_wm_base;
static struct xdg_toplevel *xdg_toplevel;

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
    xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = { .ping = xdg_wm_base_ping };

static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0)
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    else if (strcmp(interface, "xdg_wm_base") == 0)
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove
};

int main() {
    display = wl_display_connect(NULL);
    if (!display) { fprintf(stderr, "cannot connect to wayland display\n"); return 1; }
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    surface = wl_compositor_create_surface(compositor);
    xdg_toplevel = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    // xdg_surface listener omitted for brevity; real impl must handle configure.
    wl_surface_commit(surface);
    wl_display_roundtrip(display);

    while (wl_display_dispatch(display) != -1) {}
    wl_display_disconnect(display);
    return 0;
}
