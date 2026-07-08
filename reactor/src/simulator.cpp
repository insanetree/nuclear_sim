#include "reactor/simulator.h"

#include <utility>

namespace reactor {

Simulator::Simulator(SimulatorConfig config) : coordinator_(std::make_unique<Coordinator>(config)) {}

Simulator::~Simulator()
{
	stop();
}

void
Simulator::start()
{
	coordinator_->start();
}

void
Simulator::stop()
{
	coordinator_->stop();
}

void
Simulator::move_control_rods(double target_percent)
{
	coordinator_->commands().rod_target.store(target_percent, std::memory_order_relaxed);
}

void
Simulator::set_pump_flow_rate(double kg_per_sec)
{
	coordinator_->commands().pump_flow_rate.store(kg_per_sec, std::memory_order_relaxed);
}

ReactorState
Simulator::get_reactor_state() const
{
	return coordinator_->get_reactor_state();
}

uint64_t
Simulator::get_tick_count() const
{
	return coordinator_->tick_count();
}

} // namespace reactor
