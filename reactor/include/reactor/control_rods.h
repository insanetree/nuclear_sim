#pragma once

#include "reactor/constants.h"
#include "reactor/module.h"

namespace reactor {

class ControlRods : public Module
{
public:
	explicit ControlRods(const ReactivityParams& params);
	void tick(std::chrono::duration<double> dt, const ReactorState& read, ReactorState& write) override;

private:
	ReactivityParams params_;
};

} // namespace reactor
