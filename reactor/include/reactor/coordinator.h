#pragma once

#include "reactor/constants.h"
#include "reactor/control_rods.h"
#include "reactor/coolant_loop.h"
#include "reactor/core.h"
#include "reactor/reactor_state.h"

#include <array>
#include <atomic>
#include <barrier>
#include <functional>
#include <thread>

namespace reactor {

class Coordinator {
public:
    explicit Coordinator(const SimulatorConfig& config);
    ~Coordinator();

    Coordinator(const Coordinator&) = delete;
    Coordinator& operator=(const Coordinator&) = delete;

    void start();
    void stop();

    // Access to the latest completed state (safe to call from any thread
    // only between ticks or when stopped — the Simulator wrapper handles
    // thread safety for the external API).
    const ReactorState& current_state() const;

    // Access to commands (atomics, safe from any thread)
    Commands& commands();

    // Tick counter
    uint64_t tick_count() const;

private:
    void run_module(Module& module);
    void on_tick_complete() noexcept;

    SimulatorConfig config_;

    // Double-buffered state
    std::array<ReactorState, 2> state_buffers_;
    std::atomic<std::size_t> read_index_{0};

    // Commands from external interface
    Commands commands_;

    // Modules
    ControlRods control_rods_;
    ReactorCore reactor_core_;
    CoolantLoop coolant_loop_;

    // Synchronisation
    static constexpr int kNumModules = 3;
    std::barrier<std::function<void()>> barrier_;

    // Threads
    std::array<std::jthread, kNumModules> module_threads_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> tick_count_{0};
};

} // namespace reactor
