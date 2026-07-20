#include "controller/decision_making.hpp"

#include "controller/diagnostics.hpp"
#include "controller/thresholds.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <thread>

namespace controller {

decision_maker::decision_maker(control_panel* control_panel, std::shared_ptr<const diagnostics> diagnostics) :
	m_control_panel(control_panel),
	m_diagnostics(std::move(diagnostics)),
	m_target_power(control_panel->control_rod_target.load(std::memory_order_relaxed))
{
}

void
decision_maker::set_control_rods(double rod_position)
{
	m_target_power.store(std::clamp(rod_position, 0.0, 100.0), std::memory_order_release);
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
	auto last_update = std::chrono::steady_clock::now();

	while (!stoken.stop_requested() && m_run.load(std::memory_order_relaxed)) {
		last_update += std::chrono::duration_cast<std::chrono::steady_clock::duration>(m_update_interval);
		std::this_thread::sleep_until(last_update);

		auto status = m_diagnostics->get_diagnostics();
		if (status.second == diagnostics::state_e::ERROR) {
			spdlog::critical("Diagnostics malfunctioned. SCRAM");
			m_run.store(false);
			return;
		}

		if (status.second == diagnostics::state_e::TRY_AGAIN) {
			std::this_thread::sleep_for(m_update_interval / 10);
			status = m_diagnostics->get_diagnostics();
		}

		if (status.second != diagnostics::state_e::READY) {
			spdlog::error("Diagnostics retry failed.");
			continue;
		}

		if (status.first.timestamp <= m_last_status.timestamp) {
			continue;
		}

		m_last_status = status.first;

		// Simple passthrough: forward the last commanded rod position to the
		// shared-memory control panel every cycle.
		m_control_panel->control_rod_target.store(m_target_power.load(std::memory_order_relaxed),
		                                          std::memory_order_relaxed);

		m_control_panel->steam_generator_effectiveness_target.store(get_steam_generation());
	}
}

double
decision_maker::get_steam_generation()
{
	double T_avg = (m_last_status.coolant_inlet_temperature + m_last_status.coolant_outlet_temperature) / 2.0;
	double T_diff;
	double T_new;

	if (T_avg == thresholds::T_AVG) {
		return m_last_status.steam_generator_effectiveness;
	}

	T_diff = T_avg - thresholds::T_AVG;

	if (T_diff < -1.0) {
		T_new = m_last_status.steam_generator_effectiveness - 1.0;
	} else if (T_diff > 1.0) {
		T_new = m_last_status.steam_generator_effectiveness + 1.0;
	} else {
		T_new = m_last_status.steam_generator_effectiveness + T_diff;
	}

	return std::clamp(T_new, 0.0, 100.0);
}

} // namespace controller
