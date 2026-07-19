#include "controller/decision_making.hpp"

#include "controller/diagnostics.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

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
	m_last_update = std::chrono::steady_clock::now();

	while (!stoken.stop_requested() && m_run.load(std::memory_order_relaxed)) {
		m_last_update += std::chrono::duration_cast<std::chrono::steady_clock::duration>(m_update_interval);
		std::this_thread::sleep_until(m_last_update);

		// Simple passthrough: forward the last commanded rod position to the
		// shared-memory control panel every cycle.
		m_control_panel->control_rod_target.store(m_target_power.load(std::memory_order_relaxed),
		                                           std::memory_order_relaxed);
	}
}

} // namespace controller
