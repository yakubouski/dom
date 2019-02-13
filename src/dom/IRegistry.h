#pragma once
#include "IUnknown.h"

namespace Dom {
	struct IRegistry : public virtual IUnknown {
		virtual bool RegisterClass(const clsuid& /* class uid */,std::string&& /* Namespace */) = 0;
		virtual bool UnRegisterClass(const clsuid& /* class uid */, std::string&& /* Namespace */) = 0;
		virtual bool ClassExist(const clsuid& /* class uid */, std::string&& /* Namespace */) = 0;

		IID(Registry)
	};
}