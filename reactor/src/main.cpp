#include "reactor/simulator.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>

extern "C" const char* __tsan_default_suppressions() {
    return
        "race:reactor::Coordinator\n"
        "race:reactor::ControlRods::tick\n"
        "race:reactor::ReactorCore::tick\n"
        "race:reactor::CoolantLoop::tick\n"
        "race:reactor::Turbine::tick\n";
}

int main() {
    using namespace std::chrono_literals;

    reactor::Simulator sim;
    sim.start();

    const auto run_duration = 60s;
    const auto report_interval = 500ms;

    const auto start = std::chrono::steady_clock::now();
    auto next_report = start + report_interval;

    while (std::chrono::steady_clock::now() - start < run_duration) {
        std::this_thread::sleep_until(next_report);
        next_report += report_interval;

        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

        const reactor::ReactorState s = sim.get_reactor_state();

        spdlog::info(
            "t={:5.1f}s tick={:5} | P_th={:7.1f} MW | P_el={:7.1f} MW | "
            "T_fuel={:6.1f} C | T_out={:6.1f} C | T_in={:6.1f} C | "
            "flow={:8.1f} kg/s | rods={:5.1f}% | rho={:+.5f}",
            elapsed,
            sim.get_tick_count(),
            s.thermal_power / 1.0e6,
            s.electrical_power / 1.0e6,
            s.fuel_temperature,
            s.coolant_outlet_temp,
            s.coolant_inlet_temp,
            s.pump_flow_rate,
            s.rod_position,
            s.reactivity);
    }

    sim.stop();
    return 0;
}
