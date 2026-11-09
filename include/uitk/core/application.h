// Event loop: polls registered fd sources (display connections), runs timers and posted tasks.
#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

namespace uitk {

// A file-descriptor based event source (e.g. a Wayland or X11 display connection).
class EventSource {
public:
    virtual ~EventSource() = default;
    virtual int  fd() const = 0;
    // Called before every poll(). Must flush pending output and dispatch already-queued events.
    virtual void prepare() {}
    // Called after every poll() with the fd's revents (0 if it was not ready).
    virtual void dispatch(short revents) = 0;
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
