#pragma once

#include "reactor/constants.h"
#include "reactor/module.h"

namespace reactor {

class CoolantLoop : public Module {
public:
    explicit CoolantLoop(const CoolantParams& params, const FuelParams& fuel);
    void tick(double dt, const ReactorState& read, ReactorState& write) override;

private:
    CoolantParams params_;
    FuelParams fuel_;
};

} // namespace reactor
