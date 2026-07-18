#pragma once

#include "reactor/module.h"

namespace reactor {

class Turbine : public Module
{
public:
	void tick(std::chrono::duration<double> dt, const ReactorState& read, ReactorState& write) override;
};

} // namespace reactor
