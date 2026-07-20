#pragma once

#include "controller/diagnostics.hpp"
#include "controller/ipc.hpp"
#include "controller/thresholds.hpp"

#include <memory>
#include <thread>

namespace controller {

class decision_maker
{
public:
	decision_maker(control_panel* control_panel, std::shared_ptr<const diagnostics> diagnostics);

	// Sets the desired control rod position, clamped to [0, 100]. The
	// background loop forwards this value to the shared-memory control
	// panel every update cycle.
	void set_control_rods(double rod_position);

	void start();

private:
	void run(std::stop_token);

	double get_steam_generation();

	static constexpr std::chrono::duration<double> m_update_interval = std::chrono::duration<double>(0.1);

	std::jthread thread;
	std::atomic<bool> m_run;

	control_panel* m_control_panel;
	std::shared_ptr<const diagnostics> m_diagnostics;
	diagnostics::diagnostic_panel m_last_status;
	std::atomic<double> m_target_power;
};

} // namespace controller
