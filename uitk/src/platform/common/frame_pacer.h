// Frame scheduling for platforms without compositor frame callbacks (X11, Win32).
//
// Frames land on a fixed grid at the display refresh rate. Targets are absolute, so timer
// latency never accumulates into dropped frames; when idle the next slot is already due, so a
// redraw after input happens at once. Requests coalesce until the slot fires.
#pragma once
#include <chrono>
#include <functional>
#include <memory>

#include "uitk/core/application.h"

namespace uitk::platform {

class FramePacer {
public:
    using Clock = std::chrono::steady_clock;

    FramePacer(Application& app, std::function<void()> on_slot)
        : app_(app), on_slot_(std::move(on_slot)), alive_(std::make_shared<bool>(true)) {}
    ~FramePacer() {
        *alive_ = false;
        if (timer_) app_.cancel_timer(timer_);
    }
    FramePacer(const FramePacer&) = delete;
    FramePacer& operator=(const FramePacer&) = delete;

    void set_refresh_rate(double hz) {
        if (hz >= 20 && hz <= 1000) period_ = std::chrono::nanoseconds((long long)(1e9 / hz));
    }
    Clock::duration period() const { return period_; }

    // Calls on_slot at the next frame slot (once, however often it is requested before then).
    void request() {
        if (pending_) return;
        pending_ = true;
        auto now = Clock::now();
        auto alive = alive_;
        auto fire = [this, alive] {
            if (!*alive) return;
            timer_ = 0;
            pending_ = false;
            on_slot_();
        };
        if (next_ > now) timer_ = app_.add_timer(std::chrono::ceil<std::chrono::milliseconds>(next_ - now), fire);
        else app_.post(fire);
    }
    bool pending() const { return pending_; }

    // Call when a frame is actually drawn: advances the grid to the following slot.
    void frame_drawn() {
        auto now = Clock::now();
        if (next_ + period_ < now) next_ = now;   // idle or stalled: resync the grid
        next_ += period_;
    }

private:
    Application& app_;
    std::function<void()> on_slot_;
    std::shared_ptr<bool> alive_;   // posted tasks cannot be cancelled; they check this
    TimerId timer_ = 0;
    bool pending_ = false;
    Clock::time_point next_{};
    Clock::duration period_ = std::chrono::nanoseconds(16'666'667);
};

} // namespace uitk::platform
