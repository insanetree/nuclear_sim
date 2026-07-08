#pragma once

#include "reactor/constants.h"
#include "reactor/module.h"

namespace reactor {

class ReactorCore : public Module
{
public:
	ReactorCore(const NeutronicsParams& neutronics, const FuelParams& fuel, const ReactivityParams& reactivity);

	void tick(std::chrono::duration<double> dt, const ReactorState& read, ReactorState& write) override;

private:
	NeutronicsParams neutronics_;
	FuelParams fuel_;
	ReactivityParams reactivity_;
};

} // namespace reactor
