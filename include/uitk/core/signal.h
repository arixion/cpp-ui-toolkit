// Signal/slot: the one event-callback mechanism for every control (clicked, toggled, changed...).
//
//   Signal<> clicked;
//   auto id = clicked.connect([] { ... });
//   clicked.emit();            // or clicked()
//   clicked.disconnect(id);
//
// Emission is re-entrancy safe: slots may connect/disconnect (even themselves) while emitting.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace uitk {

using ConnectionId = uint64_t;

template <class... Args>
class Signal {
public:
    using Slot = std::function<void(Args...)>;

    ConnectionId connect(Slot slot) {
        ConnectionId id = ++next_id_;
        slots_->push_back({id, std::make_shared<Slot>(std::move(slot))});
        return id;
    }

    void disconnect(ConnectionId id) {
        auto& v = *slots_;
        for (auto it = v.begin(); it != v.end(); ++it)
            if (it->id == id) { v.erase(it); return; }
    }

    void disconnect_all() { slots_->clear(); }
    bool empty() const { return slots_->empty(); }

    void emit(Args... args) const {
        // Snapshot: changes made by slots take effect from the next emission.
        auto snapshot = *slots_;
        for (auto& e : snapshot) (*e.fn)(args...);
    }
    void operator()(Args... args) const { emit(args...); }

private:
    struct Entry { ConnectionId id; std::shared_ptr<Slot> fn; };
    std::shared_ptr<std::vector<Entry>> slots_ = std::make_shared<std::vector<Entry>>();
    ConnectionId next_id_ = 0;
};

} // namespace uitk
