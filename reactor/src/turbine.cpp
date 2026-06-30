#include "reactor/turbine.h"

namespace reactor {

Turbine::Turbine(const TurbineParams& params)
    : params_(params) {}

void Turbine::tick(std::chrono::duration<double> /*dt*/, const ReactorState& read, ReactorState& write) {
    // Heat delivered by the primary coolant to the secondary side drives the
    // turbine. Convert that thermal power to gross electrical output using a
    // fixed thermal-to-electric efficiency.
    write.electrical_power = params_.thermal_efficiency * read.heat_removal_rate;
}

} // namespace reactor
