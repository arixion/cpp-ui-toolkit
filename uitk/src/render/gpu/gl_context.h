// Platform GL contexts for the GPU backend. The Skia/Ganesh side (backend_gpu.cpp) is shared;
// only context creation and presenting differ: EGL on Linux (egl_context.cpp), WGL on Windows
// (wgl_context.cpp). Both render to the default framebuffer: RGBA8, 8-bit stencil, no MSAA.
#pragma once
#include <memory>
#include <string>

#include "include/core/SkRefCnt.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"

namespace uitk {

class Window;

namespace gpu {

class GlWindowContext {
public:
    virtual ~GlWindowContext() = default;
    // Makes the context current on the window's drawable, sized w x h physical px.
    // Returns false if this frame must be skipped (the window will ask again).
    virtual bool make_current(int w, int h) = 0;
    virtual void swap() = 0;
    virtual sk_sp<const GrGLInterface> interface() = 0;   // call while current
    virtual const char* api() const = 0;                   // "gles" / "gl", for descriptions
    virtual std::string renderer() const = 0;              // GL_RENDERER; call while current
};

class GlOffscreenContext {
public:
    virtual ~GlOffscreenContext() = default;
    virtual bool make_current() = 0;
    virtual sk_sp<const GrGLInterface> interface() = 0;
    virtual const char* api() const = 0;
    virtual std::string renderer() const = 0;
};

// Null when GL is unavailable (the renderer then falls back to CPU raster).
std::unique_ptr<GlWindowContext>    make_gl_window_context(Window& window);
std::unique_ptr<GlOffscreenContext> make_gl_offscreen_context();

} // namespace gpu
} // namespace uitk
