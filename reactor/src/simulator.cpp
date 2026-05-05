#include "reactor/simulator.h"

#include <utility>

namespace reactor {

Simulator::Simulator(SimulatorConfig config)
    : coordinator_(std::make_unique<Coordinator>(config)) {}

Simulator::~Simulator() {
    stop();
}

void Simulator::start() {
    coordinator_->start();
}

void Simulator::stop() {
    coordinator_->stop();
}

void Simulator::move_control_rods(double target_percent) {
    coordinator_->commands().rod_target.store(target_percent, std::memory_order_relaxed);
}

void Simulator::set_pump_flow_rate(double kg_per_sec) {
    coordinator_->commands().pump_flow_rate.store(kg_per_sec, std::memory_order_relaxed);
}

double Simulator::get_thermal_power() const {
    return coordinator_->current_state().thermal_power;
}

double Simulator::get_fuel_temperature() const {
    return coordinator_->current_state().fuel_temperature;
}

double Simulator::get_coolant_outlet_temperature() const {
    return coordinator_->current_state().coolant_outlet_temp;
}

double Simulator::get_coolant_inlet_temperature() const {
    return coordinator_->current_state().coolant_inlet_temp;
}

double Simulator::get_pump_flow_rate() const {
    return coordinator_->current_state().pump_flow_rate;
}

double Simulator::get_control_rod_position() const {
    return coordinator_->current_state().rod_position;
}

double Simulator::get_reactivity() const {
    return coordinator_->current_state().reactivity;
}

uint64_t Simulator::get_tick_count() const {
    return coordinator_->tick_count();
}

} // namespace reactor
