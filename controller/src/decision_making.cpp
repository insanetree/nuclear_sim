#include "controller/decision_making.hpp"

#include "controller/diagnostics.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace controller {

namespace {
// Proportional gain translating a fuel-temperature error (°C, vs.
// OPERATING_CORE_TEMPERATURE) into a pump-flow-target adjustment (kg/s).
// Hotter core -> more flow (more cooling); cooler core -> less flow.
constexpr double kFlowGain = 50.0; // kg/s per °C of temperature error

// Pump flow bounds mirroring reactor::CoolantParams (not directly
// accessible from the controller process, so duplicated here).
constexpr double kMinFlowRate = 0.0;     // kg/s
constexpr double kMaxFlowRate = 15000.0; // kg/s
} // namespace

decision_maker::decision_maker(control_panel* control_panel, std::shared_ptr<const diagnostics> diagnostics) :
	m_control_panel(control_panel),
	m_diagnostics(std::move(diagnostics)),
	m_target_power(0.0)
{
}

void
decision_maker::set_target_power(double power)
{
	m_target_power.store(std::clamp(power, 0.0, MAX_ELECTRICAL_POWER), std::memory_order_release);
}

void
decision_maker::start()
{
	if (m_run.exchange(true, std::memory_order_relaxed)) {
		return; // already running
	}
	thread = std::jthread([this](std::stop_token stoken) { run(stoken); });
}

void
decision_maker::run(std::stop_token stoken)
{
	m_last_update = std::chrono::steady_clock::now();

	while (!stoken.stop_requested() && m_run.load(std::memory_order_relaxed)) {
		m_last_update += std::chrono::duration_cast<std::chrono::steady_clock::duration>(m_update_interval);
		std::this_thread::sleep_until(m_last_update);

		const auto [panel, state] = m_diagnostics->get_diagnostics();
		if (state != diagnostics::state_e::READY) {
			// Nothing fresh to act on this cycle; hold the last commanded flow.
			continue;
		}

		// Proportional feedback: adjust pump flow so the fuel (core)
		// temperature settles at OPERATING_CORE_TEMPERATURE.
		const double error = panel.fuel_temperature - OPERATING_CORE_TEMPERATURE;
		const double new_flow = std::clamp(panel.pump_flow_rate + kFlowGain * error, kMinFlowRate, kMaxFlowRate);

		m_control_panel->pump_flow_target.store(new_flow, std::memory_order_relaxed);
	}
}

} // namespace controller
