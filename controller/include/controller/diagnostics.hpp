#pragma once

#include "controller/ipc.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

namespace controller {

class diagnostics
{
public:
	enum class state_e
	{
		READY,     // Gathered data is up to date and ready to consume
		TRY_AGAIN, // Failed reading the status once, try again in next tick
		ERROR,     // Failed reading data more than once in a row.
	};
	struct diagnostic_panel
	{
		std::chrono::steady_clock::time_point timestamp;
		double control_rod_position;
		double pump_flow_rate;
		double thermal_power;
		double d_thermal_power; // derivative of thermal power
		double electrical_power;
		double d_electrical_power; // derivative of electrical power
		double coolant_inlet_temperature;
		double coolant_outlet_temperature;
		double steam_generator_effectiveness;
		double fuel_temperature; // calculated fuel temperature from coolant outlet temperature and thermal power
		double d_fuel_temperature;
	};

	diagnostics(const status_panel* statpnl);

	void start();

	const std::pair<diagnostic_panel, state_e> get_diagnostics() const;

private:
	void run(std::stop_token stoken);

	std::jthread thread;

	const status_panel* m_statpnl;

	// Configuration for poller thread
	static constexpr std::chrono::duration<double> m_update_interval = std::chrono::duration<double>(0.1);

	// State
	std::chrono::steady_clock::time_point m_last_update;
	std::array<diagnostic_panel, 2> m_diagnostics_buffer;
	std::atomic<size_t> m_read_index{0};
	std::atomic<bool> m_run = false;
	std::atomic<state_e> m_state{state_e::ERROR};
};

}
