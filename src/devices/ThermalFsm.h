#pragma once
#include <string>

// Four-state thermal FSM driven by CPU temperature thresholds.
// NORMAL → WARN → CRITICAL → SHUTDOWN
// Models production-grade thermal protection logic for edge hardware.
enum class ThermalState { NORMAL, WARN, CRITICAL, SHUTDOWN };

inline const char* thermalStateName(ThermalState s) {
    switch (s) {
        case ThermalState::NORMAL:   return "NORMAL";
        case ThermalState::WARN:     return "WARN";
        case ThermalState::CRITICAL: return "CRITICAL";
        case ThermalState::SHUTDOWN: return "SHUTDOWN";
    }
    return "UNKNOWN";
}

class ThermalFsm {
public:
    // Thresholds in °C
    static constexpr double WARN_C     = 60.0;
    static constexpr double CRITICAL_C = 70.0;
    static constexpr double SHUTDOWN_C = 80.0;
    static constexpr double HYSTERESIS = 3.0;  // prevents rapid oscillation

    ThermalState update(double temp_c) {
        switch (state_) {
            case ThermalState::NORMAL:
                if (temp_c >= SHUTDOWN_C)        state_ = ThermalState::SHUTDOWN;
                else if (temp_c >= CRITICAL_C)   state_ = ThermalState::CRITICAL;
                else if (temp_c >= WARN_C)        state_ = ThermalState::WARN;
                break;
            case ThermalState::WARN:
                if (temp_c >= SHUTDOWN_C)         state_ = ThermalState::SHUTDOWN;
                else if (temp_c >= CRITICAL_C)    state_ = ThermalState::CRITICAL;
                else if (temp_c < WARN_C - HYSTERESIS) state_ = ThermalState::NORMAL;
                break;
            case ThermalState::CRITICAL:
                if (temp_c >= SHUTDOWN_C)         state_ = ThermalState::SHUTDOWN;
                else if (temp_c < CRITICAL_C - HYSTERESIS) state_ = ThermalState::WARN;
                break;
            case ThermalState::SHUTDOWN:
                // Latch — manual reset required (operator intervention)
                break;
        }
        return state_;
    }

    ThermalState state() const { return state_; }
    void reset()               { state_ = ThermalState::NORMAL; }

private:
    ThermalState state_{ThermalState::NORMAL};
};
