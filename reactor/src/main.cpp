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

        spdlog::info(
            "t={:5.1f}s tick={:5} | P_th={:7.1f} MW | P_el={:7.1f} MW | "
            "T_fuel={:6.1f} C | T_out={:6.1f} C | T_in={:6.1f} C | "
            "flow={:8.1f} kg/s | rods={:5.1f}% | rho={:+.5f}",
            elapsed,
            sim.get_tick_count(),
            sim.get_thermal_power() / 1.0e6,
            sim.get_electrical_power() / 1.0e6,
            sim.get_fuel_temperature(),
            sim.get_coolant_outlet_temperature(),
            sim.get_coolant_inlet_temperature(),
            sim.get_pump_flow_rate(),
            sim.get_control_rod_position(),
            sim.get_reactivity());
    }

    sim.stop();
    return 0;
}
