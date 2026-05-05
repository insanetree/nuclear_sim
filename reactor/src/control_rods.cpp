#include "reactor/control_rods.h"

#include <algorithm>
#include <cmath>

namespace reactor {

ControlRods::ControlRods(const ReactivityParams& params)
    : params_(params) {}

void ControlRods::tick(double dt, const ReactorState& read, ReactorState& write) {
    double position = read.rod_position;
    double target = read.rod_target;

    double error = target - position;
    double max_step = params_.max_rod_speed * dt;
    double step = std::clamp(error, -max_step, max_step);

    position = std::clamp(position + step, 0.0, 100.0);

    write.rod_position = position;
    write.rod_target = target;
}

} // namespace reactor
