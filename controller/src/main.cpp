#include "controller/decision_making.hpp"
#include "controller/diagnostics.hpp"
#include "controller/ipc.hpp"

#include <spdlog/spdlog.h>

#include <charconv>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

template<typename T>
T*
create_and_map(const char* name, int& fd)
{
	fd = shm_open(name, O_RDWR, 0666);
	if (fd == -1) {
		spdlog::error("shm_open({}) failed: {}", name, std::strerror(errno));
		return nullptr;
	}
	void* addr = mmap(nullptr, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		spdlog::error("mmap({}) failed: {}", name, std::strerror(errno));
		close(fd);
		fd = -1;
		return nullptr;
	}
	return static_cast<T*>(addr);
}

struct status_snapshot
{
	double control_rod_position;
	double pump_flow_rate;
	double thermal_power;
	double electrical_power;
	double coolant_inlet_temperature;
	double coolant_outlet_temperature;
	double steam_generator_effectiveness;
};

// Reads a torn-read-free snapshot of the status panel via the seqlock
// bracketed by tick_1/tick_2 (mirrors controller::diagnostics's read loop).
status_snapshot
read_status(const controller::status_panel* panel)
{
	status_snapshot snapshot{};
	uint64_t tick1;
	uint64_t tick2;
	do {
		tick1 = panel->tick_1.load(std::memory_order_acquire);
		snapshot.control_rod_position = panel->control_rod_position.load(std::memory_order_relaxed);
		snapshot.pump_flow_rate = panel->pump_flow_rate.load(std::memory_order_relaxed);
		snapshot.thermal_power = panel->thermal_power.load(std::memory_order_relaxed);
		snapshot.electrical_power = panel->electrical_power.load(std::memory_order_relaxed);
		snapshot.coolant_inlet_temperature = panel->coolant_inlet_temperature.load(std::memory_order_relaxed);
		snapshot.coolant_outlet_temperature = panel->coolant_outlet_temperature.load(std::memory_order_relaxed);
		snapshot.steam_generator_effectiveness = panel->steam_generator_effectiveness.load(std::memory_order_relaxed);
		tick2 = panel->tick_2.load(std::memory_order_acquire);
	} while (tick1 != tick2);
	return snapshot;
}

void
print_status(const controller::status_panel* panel)
{
	const status_snapshot snapshot = read_status(panel);
	std::cout << "Control rod position:          " << snapshot.control_rod_position << " %\n";
	std::cout << "Pump flow rate:                " << snapshot.pump_flow_rate << " kg/s\n";
	std::cout << "Thermal power:                 " << snapshot.thermal_power << " W\n";
	std::cout << "Electrical power:              " << snapshot.electrical_power << " W\n";
	std::cout << "Coolant inlet temperature:     " << snapshot.coolant_inlet_temperature << " degC\n";
	std::cout << "Coolant outlet temperature:    " << snapshot.coolant_outlet_temperature << " degC\n";
	std::cout << "Steam generator effectiveness: " << snapshot.steam_generator_effectiveness << " %\n";
}

void
handle_rod_command(const std::string& arg, controller::decision_maker& decision)
{
	double value;
	auto result = std::from_chars(arg.data(), arg.data() + arg.size(), value);
	if (result.ec != std::errc{} || result.ptr != arg.data() + arg.size()) {
		std::cout << "Error: invalid rod target '" << arg << "', expected a number\n";
		return;
	}
	if (value < 0.0 || value > 100.0) {
		std::cout << "Error: rod target " << value << " out of range [0, 100]\n";
		return;
	}
	decision.set_control_rods(value);
}

void
run_console(controller::decision_maker& decision, const controller::status_panel* status_panel)
{
	std::cout << "Reactor controller console. Commands: rod <0-100>, status, quit\n";
	std::string line;
	while (std::cin.good()) {
		std::cout << "> ";
		if (!std::getline(std::cin, line)) {
			break;
		}

		std::istringstream iss(line);
		std::string command;
		iss >> command;
		if (command.empty()) {
			continue;
		}

		if (command == "rod") {
			std::string arg;
			if (!(iss >> arg)) {
				std::cout << "Error: 'rod' requires a numeric argument\n";
				continue;
			}
			handle_rod_command(arg, decision);
		} else if (command == "status") {
			print_status(status_panel);
		} else if (command == "quit" || command == "exit") {
			break;
		} else {
			std::cout << "Error: unknown command '" << command << "'\n";
		}
	}
}

}

int
main()
{
	int control_panel_fd = -1;
	int status_panel_fd = -1;

	controller::status_panel* status_panel =
		create_and_map<controller::status_panel>(controller::shm_status_panel_name, status_panel_fd);
	controller::control_panel* control_panel =
		create_and_map<controller::control_panel>(controller::shm_control_panel_name, control_panel_fd);

	if (!status_panel || !control_panel) {
		spdlog::error("Failed to map shared memory segments");
		return EXIT_FAILURE;
	}

	if (std::strncmp(status_panel->magic_value, "STATPNL", 8) != 0) {
		spdlog::error("Status panel magic value mismatch");
		return EXIT_FAILURE;
	}

	if (std::strncmp(control_panel->magic_value, "CTRLPNL", 8) != 0) {
		spdlog::error("Control panel magic value mismatch");
		return EXIT_FAILURE;
	}

	{
		// diagnostics and decision_maker must be stopped and joined (on scope
		// exit) before the shared-memory segments are unmapped below, otherwise
		// their poller threads can dereference the panels after they've been
		// unmapped.
		auto diag = std::make_shared<controller::diagnostics>(status_panel);
		diag->start();

		controller::decision_maker decision(control_panel, diag);
		decision.start();

		run_console(decision, status_panel);
	}

	munmap(status_panel, sizeof(controller::status_panel));
	munmap(control_panel, sizeof(controller::control_panel));
	close(status_panel_fd);
	close(control_panel_fd);
	return 0;
}
