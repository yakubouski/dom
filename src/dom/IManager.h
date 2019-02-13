#pragma once
#include "IUnknown.h"
#include <forward_list>
#include <string>

namespace Dom {

	using ClassList = std::forward_list<std::pair<std::string, std::string>>;

	struct IManager : public virtual IUnknown {
		virtual bool CreateInstance(const clsuid& /* Class Unique ID */, void **, std::string /* Additional namespace */) = 0;
	};
}