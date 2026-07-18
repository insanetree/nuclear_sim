#pragma once

#include "reactor/module.h"

namespace reactor {

class CoolantLoop : public Module
{
public:
	void tick(std::chrono::duration<double> dt, const ReactorState& read, ReactorState& write) override;
};

} // namespace reactor
