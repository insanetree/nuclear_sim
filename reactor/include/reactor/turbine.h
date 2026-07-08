#pragma once

#include "reactor/constants.h"
#include "reactor/module.h"

namespace reactor {

class Turbine : public Module
{
public:
	explicit Turbine(const TurbineParams& params);
	void tick(std::chrono::duration<double> dt, const ReactorState& read, ReactorState& write) override;

private:
	TurbineParams params_;
};

} // namespace reactor
