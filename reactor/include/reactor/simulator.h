#pragma once

#include "reactor/constants.h"
#include "reactor/coordinator.h"

#include <memory>

namespace reactor {

class Simulator {
public:
    explicit Simulator(SimulatorConfig config = {});
    ~Simulator();

    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;

    void start();
    void stop();

    // Commands (thread-safe, applied on next tick)
    void move_control_rods(double target_percent);
    void set_pump_flow_rate(double kg_per_sec);

    // Queries (thread-safe, reads latest completed state)
    ReactorState get_reactor_state() const;
    uint64_t get_tick_count() const;

private:
    std::unique_ptr<Coordinator> coordinator_;
};

} // namespace reactor
