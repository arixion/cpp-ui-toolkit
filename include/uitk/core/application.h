// Event loop: waits on registered event sources (display connections), runs timers and posted
// tasks. Linux: poll() over file descriptors. Windows: MsgWaitForMultipleObjects, which also
// dispatches the thread's window messages.
#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

namespace uitk {

// An event source the loop waits on (e.g. a Wayland or X11 display connection).
class EventSource {
public:
#ifdef _WIN32
    using Handle = void*;   // a waitable HANDLE (event, waitable timer, ...)
#else
    using Handle = int;     // a file descriptor, polled for input
#endif
    virtual ~EventSource() = default;
    virtual Handle handle() const = 0;
    // Called before every wait. Must flush pending output and dispatch already-queued events.
    virtual void prepare() {}
    // Called after every wait; `ready` is non-zero if the handle was signalled (POSIX: revents).
    virtual void dispatch(short ready) = 0;
};

enum class LoopMode {
    OnDemand,     // windows repaint only after request_redraw() (apps, editors)
    Continuous,   // windows repaint every frame, paced by the compositor (games)
};

using TimerId = uint64_t;

class Application {
public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    static Application* current();

    int  run();
    void quit(int exit_code = 0);
    bool running() const;

    // Thread-safe: queue a task to run on the loop thread.
    void post(std::function<void()> task);

    // Runs due timers and posted tasks now. For platform code that spins a nested message loop
    // the main loop cannot see (Windows: interactive move/resize, menus), so the app keeps going.
    void process_pending();

    TimerId add_timer(std::chrono::milliseconds delay, std::function<void()> fn, bool repeat = false);
    void    cancel_timer(TimerId id);

    void add_source(EventSource* src);
    void remove_source(EventSource* src);

    void     set_loop_mode(LoopMode m);
    LoopMode loop_mode() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace uitk
