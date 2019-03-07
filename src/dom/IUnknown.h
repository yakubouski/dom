#pragma once
#include "guid.h"

namespace Dom {
	struct IUnknown
	{
		virtual long AddRef() = 0;
		virtual long Release() = 0;
		virtual bool QueryInterface(const uiid&, void **ppv) = 0;

		IID(Unknown)
	};
}