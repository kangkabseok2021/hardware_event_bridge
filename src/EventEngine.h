#pragma once
#include "Event.h"
#include <functional>
#include <mutex>
#include <vector>

// Observer pattern: listeners register a callback; the engine dispatches
// every emitted Event to all registered listeners without coupling.
using EventListener = std::function<void(const Event&)>;

class EventEngine {
public:
    void subscribe(EventListener listener) {
        std::lock_guard<std::mutex> lk(mu_);
        listeners_.push_back(std::move(listener));
    }

    // Called by device drivers — safe to call from any thread.
    void emit(Event event) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& l : listeners_) l(event);
    }

    size_t listenerCount() const {
        std::lock_guard<std::mutex> lk(mu_);
        return listeners_.size();
    }

private:
    mutable std::mutex       mu_;
    std::vector<EventListener> listeners_;
};
