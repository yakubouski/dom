#pragma once

#include "../src/dom/IUnknown.h"

struct IHello : virtual public Dom::IUnknown {
	virtual void Say() = 0;

	IID(Hello)
};