#pragma once
#include <string>

#define dom_guid_pre_name	"IID#"
#define dom_cls_pre_name	"CLSID#"

#define IID(name) \
	inline static const Dom::uiid& guid() { const static std::string idn(dom_guid_pre_name #name); return idn; }

#define CLSID(name) \
	inline static const Dom::clsuid& guid() { const static std::string idn(dom_cls_pre_name #name); return idn; }


namespace Dom {

	using gid = std::string;
	using uiid = gid;
	using clsuid = gid;

	struct IUnknown
	{
		virtual long AddRef() = 0;
		virtual long Release() = 0;
		virtual bool QueryInterface(const uiid&, void **ppv) = 0;

		IID(Unknown)
	};
}