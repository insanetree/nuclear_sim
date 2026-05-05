#pragma once

#include "reactor/reactor_state.h"

namespace reactor {

class Module {
public:
    virtual ~Module() = default;

    // Process one simulation tick.
    // dt    — time step in seconds
    // read  — previous tick's state (read-only)
    // write — current tick's state (write target)
    virtual void tick(double dt, const ReactorState& read, ReactorState& write) = 0;
};

} // namespace reactor
