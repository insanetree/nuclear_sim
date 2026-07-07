#include "reactor/coordinator.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>

namespace reactor {

Coordinator::Coordinator(const SimulatorConfig& config)
    : config_(config)
    , control_rods_(config.reactivity)
    , reactor_core_(config.neutronics, config.fuel, config.reactivity)
    , coolant_loop_(config.coolant, config.fuel)
    , turbine_(config.turbine)
    , barrier_(kNumModules, [this]() noexcept { on_tick_complete(); }) {

    // Initialise steady-state in both buffers
    auto init_state = [&](ReactorState& s) {
        s.rod_position = 100.0;
        s.rod_target = 100.0;
        s.neutron_population = 1.0;
        // Steady-state precursor concentration: C = (β / Λ) * n / λ
        s.precursor_concentration =
            (config.neutronics.beta / config.neutronics.generation_time)
            / config.neutronics.lambda;
        s.reactivity = 0.0;
        s.thermal_power = config.neutronics.nominal_power;
        s.fuel_temperature = config.fuel.ref_temperature;
        s.coolant_inlet_temp = config.coolant.inlet_temperature;
        // Steady-state outlet: T_out = T_in + P / (m_dot * c_p)
        s.coolant_outlet_temp = config.coolant.inlet_temperature
            + config.neutronics.nominal_power
              / (config.coolant.nominal_flow_rate * config.coolant.specific_heat);
        s.pump_flow_rate = config.coolant.nominal_flow_rate;
        s.heat_removal_rate = config.neutronics.nominal_power;
        // Carnot-fraction efficiency at the nominal outlet temperature
        double nominal_efficiency = config.turbine.carnot_fraction
            * (1.0 - (config.turbine.condenser_temperature + 273.15)
                     / (s.coolant_outlet_temp + 273.15));
        s.electrical_power = nominal_efficiency * config.neutronics.nominal_power;
    };

    init_state(state_buffers_[0]);
    init_state(state_buffers_[1]);

    spdlog::info("Coordinator initialised — tick period: {:.3f}s, nominal power: {:.0f} MW",
                 config_.tick_period.count(), config_.neutronics.nominal_power / 1.0e6);
}

Coordinator::~Coordinator() {
    stop();
}

void Coordinator::start() {
    if (running_.exchange(true)) {
        return;
    }
    stopping_.store(false, std::memory_order_relaxed);

    spdlog::info("Starting reactor simulation");

    auto make_thread = [this](Module& module) {
        return std::jthread([this, &module]() {
            auto tick_start = std::chrono::steady_clock::now();
            auto tick_end = tick_start + config_.tick_period;
            while (true) {

                auto ri = read_index_.load(std::memory_order_acquire);
                const ReactorState& read = state_buffers_[ri];
                ReactorState& write = state_buffers_[1 - ri];

                module.tick(config_.tick_period, read, write);

                barrier_.arrive_and_wait();
                if (stopping_.load(std::memory_order_acquire)) {
                    break;
                }

                std::this_thread::sleep_until(tick_end);
                tick_end += config_.tick_period;
            }
        });
    };

    module_threads_[0] = make_thread(control_rods_);
    module_threads_[1] = make_thread(reactor_core_);
    module_threads_[2] = make_thread(coolant_loop_);
    module_threads_[3] = make_thread(turbine_);
}

void Coordinator::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    spdlog::info("Stopping reactor simulation at tick {}", tick_count_.load());

    for (auto& t : module_threads_) {
        t.request_stop();
    }
    // Unblock any threads waiting at the barrier
    // jthread destructor will join
    for (auto& t : module_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

ReactorState Coordinator::get_reactor_state() const {
    // Seqlock keyed on tick_count_: if no tick completed while we were copying,
    // read_index_ stayed put and the buffer we copied belongs to a single tick.
    ReactorState out;
    uint64_t t1;
    uint64_t t2;
    do {
        t1 = tick_count_.load(std::memory_order_acquire);
        out = state_buffers_[read_index_.load(std::memory_order_acquire)];
        t2 = tick_count_.load(std::memory_order_acquire);
    } while (t1 != t2);
    return out;
}

Commands& Coordinator::commands() {
    return commands_;
}

uint64_t Coordinator::tick_count() const {
    return tick_count_.load(std::memory_order_relaxed);
}

void Coordinator::on_tick_complete() noexcept {
    // Apply external commands to the newly written state
    std::size_t write_index = 1 - read_index_.load(std::memory_order_relaxed);
    state_buffers_[write_index].rod_target =
        commands_.rod_target.load(std::memory_order_relaxed);
    state_buffers_[write_index].pump_flow_rate = std::clamp(
        commands_.pump_flow_rate.load(std::memory_order_relaxed),
        1.0, config_.coolant.max_flow_rate);

    // Swap buffers
    read_index_.store(write_index, std::memory_order_release);
    tick_count_.fetch_add(1, std::memory_order_release);

    // Decide collectively whether this is the final tick. Running once per
    // barrier phase, this guarantees every module observes the same value and
    // therefore arrives at every phase, so shutdown cannot deadlock.
    stopping_.store(!running_.load(std::memory_order_relaxed), std::memory_order_relaxed);
}

} // namespace reactor
