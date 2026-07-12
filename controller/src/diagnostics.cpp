#include "controller/diagnostics.hpp"

namespace controller {

namespace {

// The reactor process's true fuel/coolant gap conductance is internal to
// that module and is not exposed over IPC. This constant is an independent
// diagnostic estimate used to back out an approximate fuel temperature from
// the coolant and thermal-power values that are available, it is not a
// physical replica of the reactor's internal model.
constexpr double k_gap_conductance = 1.0e7; // W / degC

} // namespace

diagnostics::diagnostics(const status_panel* statpnl) : m_statpnl(statpnl) {}

void
diagnostics::start()
{
	if (m_run.exchange(true, std::memory_order_relaxed)) {
		return; // already running
	}
	thread = std::jthread([this](std::stop_token stoken) { run(stoken); });
}

const std::pair<diagnostics::diagnostic_panel, diagnostics::state_e>
diagnostics::get_diagnostics() const
{
	const size_t idx = m_read_index.load(std::memory_order_acquire);
	return {m_diagnostics_buffer[idx], m_state.load(std::memory_order_acquire)};
}

void
diagnostics::run(std::stop_token stoken)
{
	diagnostic_panel prev{};
	bool have_prev = false;
	int consecutive_failures = 0;
	uint64_t last_tick = 0;
	bool have_tick = false;

	m_last_update = std::chrono::steady_clock::now();

	while (!stoken.stop_requested() && m_run.load(std::memory_order_relaxed)) {
		m_last_update += std::chrono::duration_cast<std::chrono::steady_clock::duration>(m_update_interval);
		std::this_thread::sleep_until(m_last_update);

		double rod_position = 0.0;
		double flow_rate = 0.0;
		double thermal_power = 0.0;
		double electrical_power = 0.0;
		double inlet_temp = 0.0;
		double outlet_temp = 0.0;
		bool ok = false;
		uint64_t tick = 0;

		// status_panel is written using a seqlock (tick_1/tick_2 bracket the
		// field updates); retry once if we observe a torn read.
		for (int attempt = 0; attempt < 2 && !ok; ++attempt) {
			const uint64_t tick1 = m_statpnl->tick_1.load(std::memory_order_acquire);
			rod_position = m_statpnl->control_rod_position.load(std::memory_order_relaxed);
			flow_rate = m_statpnl->pump_flow_rate.load(std::memory_order_relaxed);
			thermal_power = m_statpnl->thermal_power.load(std::memory_order_relaxed);
			electrical_power = m_statpnl->electrical_power.load(std::memory_order_relaxed);
			inlet_temp = m_statpnl->coolant_inlet_temperature.load(std::memory_order_relaxed);
			outlet_temp = m_statpnl->coolant_outlet_temperature.load(std::memory_order_relaxed);
			const uint64_t tick2 = m_statpnl->tick_2.load(std::memory_order_acquire);
			ok = (tick1 == tick2);
			tick = tick1;
		}

		if (ok && have_tick && tick == last_tick) {
			// Reactor hasn't produced a new sample since our last poll; nothing
			// new to compute, leave the published buffer/state untouched.
			continue;
		}

		const auto now = std::chrono::steady_clock::now();
		diagnostic_panel next{};
		next.timestamp = now;

		if (!ok) {
			++consecutive_failures;
			m_state.store(consecutive_failures > 1 ? state_e::ERROR : state_e::TRY_AGAIN, std::memory_order_release);
			// Publish the last known-good values (if any) rather than garbage.
			next = have_prev ? prev : diagnostic_panel{};
			next.timestamp = now;
		} else {
			consecutive_failures = 0;
			have_tick = true;
			last_tick = tick;
			m_state.store(state_e::READY, std::memory_order_release);

			const double dt = have_prev ? std::chrono::duration<double>(now - prev.timestamp).count() : 0.0;
			const double coolant_avg = (inlet_temp + outlet_temp) / 2.0;
			const double fuel_temp = coolant_avg + thermal_power / k_gap_conductance;

			next.control_rod_position = rod_position;
			next.pump_flow_rate = flow_rate;
			next.thermal_power = thermal_power;
			next.electrical_power = electrical_power;
			next.coolant_inlet_temperature = inlet_temp;
			next.coolant_outlet_temperature = outlet_temp;
			next.fuel_temperature = fuel_temp;

			if (dt > 0.0) {
				next.d_thermal_power = (thermal_power - prev.thermal_power) / dt;
				next.d_electrical_power = (electrical_power - prev.electrical_power) / dt;
				next.d_fuel_temperature = (fuel_temp - prev.fuel_temperature) / dt;
			} else {
				next.d_thermal_power = 0.0;
				next.d_electrical_power = 0.0;
				next.d_fuel_temperature = 0.0;
			}

			prev = next;
			have_prev = true;
		}

		const size_t write_index = 1 - m_read_index.load(std::memory_order_relaxed);
		m_diagnostics_buffer[write_index] = next;
		m_read_index.store(write_index, std::memory_order_release);
	}
}

} // namespace controller
