#include "uitk/core/application.h"

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <map>
#include <mutex>
#include <vector>

namespace uitk {

namespace {
Application* g_current = nullptr;
using Clock = std::chrono::steady_clock;
}

struct Application::Impl {
    bool running = false;
    int  exit_code = 0;
    int  wake_fd = -1;
    LoopMode mode = LoopMode::OnDemand;

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

    int poll_timeout_ms() {
        { std::lock_guard lk(tasks_mutex); if (!tasks.empty()) return 0; }
        if (timers.empty()) return -1;
        auto next = std::min_element(timers.begin(), timers.end(),
            [](auto& a, auto& b) { return a.second.due < b.second.due; })->second.due;
        // Round up (never wake early), without the blanket +1 ms that delayed every timer.
        auto ms = std::chrono::ceil<std::chrono::milliseconds>(next - Clock::now()).count();
        return (int)std::clamp<long long>(ms, 0, 60'000);
    }
};

Application::Application() : impl_(std::make_unique<Impl>()) {
    if (g_current) fprintf(stderr, "uitk: warning: more than one Application\n");
    g_current = this;
    impl_->wake_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
}

Application::~Application() {
    if (impl_->wake_fd >= 0) close(impl_->wake_fd);
    if (g_current == this) g_current = nullptr;
}

Application* Application::current() { return g_current; }

int Application::run() {
    Impl& d = *impl_;
    d.running = true;
    std::vector<pollfd> fds;
    while (d.running) {
        d.run_tasks();
        d.run_timers();
        if (!d.running) break;

        for (EventSource* s : std::vector<EventSource*>(d.sources)) s->prepare();
        if (!d.running) break;

        auto sources = d.sources;   // snapshot: dispatch may add/remove sources
        fds.clear();
        fds.push_back({d.wake_fd, POLLIN, 0});
        for (EventSource* s : sources) fds.push_back({s->fd(), POLLIN, 0});

        int r = ::poll(fds.data(), fds.size(), d.poll_timeout_ms());
        if (r < 0 && errno != EINTR) { perror("uitk: poll"); d.exit_code = 1; break; }

        if (r > 0 && (fds[0].revents & POLLIN)) {
            uint64_t v; while (::read(d.wake_fd, &v, sizeof v) > 0) {}
        }
        for (size_t i = 0; i < sources.size(); ++i) {
            // A source removed by an earlier dispatch must not be touched.
            if (std::find(d.sources.begin(), d.sources.end(), sources[i]) == d.sources.end()) continue;
            sources[i]->dispatch(r > 0 ? fds[i + 1].revents : 0);
        }
    }
    d.running = false;
    return d.exit_code;
}

void Application::quit(int exit_code) {
    impl_->exit_code = exit_code;
    impl_->running = false;
    uint64_t one = 1;
    [[maybe_unused]] auto n = ::write(impl_->wake_fd, &one, sizeof one);
}

bool Application::running() const { return impl_->running; }

void Application::post(std::function<void()> task) {
    { std::lock_guard lk(impl_->tasks_mutex); impl_->tasks.push_back(std::move(task)); }
    uint64_t one = 1;
    [[maybe_unused]] auto n = ::write(impl_->wake_fd, &one, sizeof one);
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
