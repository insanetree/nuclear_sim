#include "reactor/ipc.h"
#include "reactor/simulator.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <new>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace {

// Create, size, and map a POSIX shared-memory segment, then construct a fresh T
// in it (sets the magic value, zero-inits the atomics). Returns a pointer into
// the mapped region, or nullptr on failure. On success the fd can be closed;
// the mapping stays valid.
template<typename T>
T*
create_and_map(const char* name, int& fd)
{
	fd = shm_open(name, O_CREAT | O_RDWR, 0666);
	if (fd == -1) {
		spdlog::error("shm_open({}) failed: {}", name, std::strerror(errno));
		return nullptr;
	}
	if (ftruncate(fd, sizeof(T)) == -1) {
		spdlog::error("ftruncate({}) failed: {}", name, std::strerror(errno));
		close(fd);
		shm_unlink(name);
		fd = -1;
		return nullptr;
	}
	void* addr = mmap(nullptr, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED) {
		spdlog::error("mmap({}) failed: {}", name, std::strerror(errno));
		close(fd);
		shm_unlink(name);
		fd = -1;
		return nullptr;
	}
	return new (addr) T{};
}

} // namespace

extern "C" const char*
__tsan_default_suppressions()
{
	return "race:reactor::Coordinator\n"
		   "race:reactor::ControlRods::tick\n"
		   "race:reactor::ReactorCore::tick\n"
		   "race:reactor::CoolantLoop::tick\n"
		   "race:reactor::Turbine::tick\n";
}

int
main()
{
	using namespace std::chrono_literals;

	// Owner side: create and map the two shared-memory segments the external
	// controller attaches to. `control` is written by the controller and read
	// here; `status` is written here and read by the controller.
	int control_panel_fd = -1;
	int status_panel_fd = -1;
	reactor::control_panel* control =
		create_and_map<reactor::control_panel>(reactor::shm_control_panel_name, control_panel_fd);
	reactor::status_panel* status =
		create_and_map<reactor::status_panel>(reactor::shm_status_panel_name, status_panel_fd);
	if (control == nullptr || status == nullptr) {
		return 1;
	}
	close(control_panel_fd);
	close(status_panel_fd);

	reactor::Simulator sim;

	reactor::ReactorState reactor_state = sim.get_reactor_state();
	control->control_rod_target.store(reactor_state.rod_target);
	control->pump_flow_target.store(reactor_state.pump_flow_rate);
	sim.start();

	const auto run_duration = 60s;
	const auto update_interval = 10ms;
	const auto report_interval = 500ms;

	const auto start = std::chrono::steady_clock::now();
	auto curr_time = start;
	auto next_update = start + update_interval;
	auto next_report = start + report_interval;

	while (curr_time - start < run_duration) {
		std::this_thread::sleep_until(next_update);
		next_update += update_interval;
		curr_time = std::chrono::steady_clock::now();

		const reactor::ReactorState s = sim.get_reactor_state();
		const uint64_t tick = sim.get_tick_count();

		status->tick_1.store(tick);
		status->control_rod_position.store(s.rod_position, std::memory_order_relaxed);
		status->pump_flow_rate.store(s.pump_flow_rate, std::memory_order_relaxed);
		status->thermal_power.store(s.thermal_power, std::memory_order_relaxed);
		status->electrical_power.store(s.electrical_power, std::memory_order_relaxed);
		status->coolant_inlet_temperature.store(s.coolant_inlet_temp, std::memory_order_relaxed);
		status->coolant_outlet_temperature.store(s.coolant_outlet_temp, std::memory_order_relaxed);
		status->tick_2.store(tick);

		sim.set_pump_flow_rate(control->pump_flow_target.load(std::memory_order_relaxed));
		sim.move_control_rods(control->control_rod_target.load(std::memory_order_relaxed));

		if (curr_time >= next_report) {
			next_report += report_interval;
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
			spdlog::info("t={:5.1f}s tick={:5} | P_th={:7.1f} MW | P_el={:7.1f} MW | "
			             "T_fuel={:6.1f} C | T_out={:6.1f} C | T_in={:6.1f} C | "
			             "flow={:8.1f} kg/s | rods={:5.1f}% | rho={:+.5f}",
			             elapsed,
			             tick,
			             s.thermal_power / 1.0e6,
			             s.electrical_power / 1.0e6,
			             s.fuel_temperature,
			             s.coolant_outlet_temp,
			             s.coolant_inlet_temp,
			             s.pump_flow_rate,
			             s.rod_position,
			             s.reactivity);
		}
	}

	sim.stop();

	// Unmap and remove the segments (owner cleans up so they don't persist).
	munmap(control, sizeof(reactor::control_panel));
	munmap(status, sizeof(reactor::status_panel));
	shm_unlink(reactor::shm_control_panel_name);
	shm_unlink(reactor::shm_status_panel_name);
	return 0;
}
