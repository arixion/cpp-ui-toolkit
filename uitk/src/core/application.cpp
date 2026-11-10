#include "uitk/core/application.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

namespace uitk {

namespace {
Application* g_current = nullptr;
using Clock = std::chrono::steady_clock;

#ifdef _WIN32
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif
} // namespace

struct Application::Impl {
    bool running = false;
    int  exit_code = 0;
    LoopMode mode = LoopMode::OnDemand;

#ifdef _WIN32
    HANDLE wake = nullptr;    // auto-reset event: post() / quit() from any thread
    HANDLE timer = nullptr;   // waitable timer for the next due app timer (high resolution if possible)
#else
    int wake_fd = -1;
#endif

    std::vector<EventSource*> sources;

    std::mutex tasks_mutex;
    std::vector<std::function<void()>> tasks;

    struct Timer {
        Clock::time_point due;
        std::chrono::milliseconds interval;
        bool repeat;
        std::function<void()> fn;
    };
    std::map<TimerId, Timer> timers;
    TimerId next_timer = 1;

    void run_tasks() {
        std::vector<std::function<void()>> batch;
        { std::lock_guard lk(tasks_mutex); batch.swap(tasks); }
        for (auto& t : batch) t();
    }

    void run_timers() {
        auto now = Clock::now();
        std::vector<TimerId> due;
        for (auto& [id, t] : timers) if (t.due <= now) due.push_back(id);
        for (TimerId id : due) {
            auto it = timers.find(id);
            if (it == timers.end()) continue;              // cancelled by an earlier timer
            auto fn = it->second.fn;
            if (it->second.repeat) it->second.due = now + it->second.interval;
            else timers.erase(it);
            fn();
        }
    }

    bool has_tasks() { std::lock_guard lk(tasks_mutex); return !tasks.empty(); }

    // Time until the next timer is due; nullopt if there is none.
    std::optional<Clock::duration> next_timer_in() {
        if (timers.empty()) return std::nullopt;
        auto next = std::min_element(timers.begin(), timers.end(),
            [](auto& a, auto& b) { return a.second.due < b.second.due; })->second.due;
        return std::max(Clock::duration::zero(), next - Clock::now());
    }

    void wake_up() {
#ifdef _WIN32
        if (wake) SetEvent(wake);
#else
        uint64_t one = 1;
        [[maybe_unused]] auto n = ::write(wake_fd, &one, sizeof one);
#endif
    }

    // Blocks until a source, a posted task, a timer or (Windows) a window message needs attention,
    // then dispatches. Returns false on a fatal error.
    bool wait_and_dispatch();
};

#ifdef _WIN32

bool Application::Impl::wait_and_dispatch() {
    auto srcs = sources;   // snapshot: dispatch may add/remove sources
    std::vector<HANDLE> handles;
    handles.push_back(wake);
    handles.push_back(timer);
    for (EventSource* s : srcs) handles.push_back(static_cast<HANDLE>(s->handle()));

    DWORD timeout = INFINITE;
    if (has_tasks()) {
        timeout = 0;
    } else if (auto in = next_timer_in()) {
        // Waitable timer in 100 ns units (negative = relative): sub-millisecond accuracy, unlike
        // the wait timeout, which is rounded to the ~15.6 ms system tick.
        auto hundred_ns = std::chrono::duration_cast<std::chrono::duration<long long, std::ratio<1, 10'000'000>>>(*in).count();
        if (hundred_ns <= 0) timeout = 0;
        else {
            LARGE_INTEGER due;
            due.QuadPart = -hundred_ns;
            if (!SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
                timeout = (DWORD)std::max<long long>(1, (hundred_ns + 9'999) / 10'000);
        }
    }

    // MWMO_INPUTAVAILABLE: also wake for messages that were already in the queue.
    DWORD r = MsgWaitForMultipleObjectsEx((DWORD)handles.size(), handles.data(), timeout, QS_ALLINPUT,
                                          MWMO_INPUTAVAILABLE);
    if (r == WAIT_FAILED) { fprintf(stderr, "uitk: MsgWaitForMultipleObjectsEx failed (%lu)\n", GetLastError()); return false; }

    // Window messages: dispatch everything queued (they invoke the window procedures).
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { running = false; exit_code = (int)msg.wParam; break; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    for (size_t i = 0; i < srcs.size(); ++i) {
        if (std::find(sources.begin(), sources.end(), srcs[i]) == sources.end()) continue;
        bool ready = WaitForSingleObject(static_cast<HANDLE>(srcs[i]->handle()), 0) == WAIT_OBJECT_0;
        srcs[i]->dispatch(ready ? 1 : 0);
    }
    return true;
}

#else

bool Application::Impl::wait_and_dispatch() {
    auto srcs = sources;   // snapshot: dispatch may add/remove sources
    std::vector<pollfd> fds;
    fds.push_back({wake_fd, POLLIN, 0});
    for (EventSource* s : srcs) fds.push_back({s->handle(), POLLIN, 0});

    int timeout = -1;
    if (has_tasks()) timeout = 0;
    else if (auto in = next_timer_in()) {
        // Round up (never wake early), without a blanket +1 ms that would delay every timer.
        auto ms = std::chrono::ceil<std::chrono::milliseconds>(*in).count();
        timeout = (int)std::clamp<long long>(ms, 0, 60'000);
    }

    int r = ::poll(fds.data(), fds.size(), timeout);
    if (r < 0 && errno != EINTR) { perror("uitk: poll"); return false; }

    if (r > 0 && (fds[0].revents & POLLIN)) {
        uint64_t v; while (::read(wake_fd, &v, sizeof v) > 0) {}
    }
    for (size_t i = 0; i < srcs.size(); ++i) {
        // A source removed by an earlier dispatch must not be touched.
        if (std::find(sources.begin(), sources.end(), srcs[i]) == sources.end()) continue;
        srcs[i]->dispatch(r > 0 ? fds[i + 1].revents : 0);
    }
    return true;
}

#endif

Application::Application() : impl_(std::make_unique<Impl>()) {
    if (g_current) fprintf(stderr, "uitk: warning: more than one Application\n");
    g_current = this;
#ifdef _WIN32
    impl_->wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    impl_->timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!impl_->timer) impl_->timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);   // before Windows 10 1803
#else
    impl_->wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
#endif
}

Application::~Application() {
#ifdef _WIN32
    if (impl_->wake) CloseHandle(impl_->wake);
    if (impl_->timer) CloseHandle(impl_->timer);
#else
    if (impl_->wake_fd >= 0) close(impl_->wake_fd);
#endif
    if (g_current == this) g_current = nullptr;
}

Application* Application::current() { return g_current; }

int Application::run() {
    Impl& d = *impl_;
    d.running = true;
    while (d.running) {
        d.run_tasks();
        d.run_timers();
        if (!d.running) break;

        for (EventSource* s : std::vector<EventSource*>(d.sources)) s->prepare();
        if (!d.running) break;

        if (!d.wait_and_dispatch()) { d.exit_code = 1; break; }
    }
    d.running = false;
    return d.exit_code;
}

void Application::process_pending() {
    impl_->run_tasks();
    impl_->run_timers();
}

void Application::quit(int exit_code) {
    impl_->exit_code = exit_code;
    impl_->running = false;
    impl_->wake_up();
}

bool Application::running() const { return impl_->running; }

void Application::post(std::function<void()> task) {
    { std::lock_guard lk(impl_->tasks_mutex); impl_->tasks.push_back(std::move(task)); }
    impl_->wake_up();
}

TimerId Application::add_timer(std::chrono::milliseconds delay, std::function<void()> fn, bool repeat) {
    TimerId id = impl_->next_timer++;
    impl_->timers[id] = {Clock::now() + delay, delay, repeat, std::move(fn)};
    return id;
}

void Application::cancel_timer(TimerId id) { impl_->timers.erase(id); }

void Application::add_source(EventSource* s) { impl_->sources.push_back(s); }

void Application::remove_source(EventSource* s) {
    auto& v = impl_->sources;
    v.erase(std::remove(v.begin(), v.end(), s), v.end());
}

void Application::set_loop_mode(LoopMode m) { impl_->mode = m; }
LoopMode Application::loop_mode() const { return impl_->mode; }

} // namespace uitk
