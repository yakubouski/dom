#pragma once
#include <string>
#include <cstring>


#define dom_guid_pre_name	"IID#"
#define dom_cls_pre_name	"CLSID#"

#define IID(name) \
	inline static const Dom::uiid& guid() { const static Dom::GUID idn(dom_guid_pre_name #name); return idn; }

#define CLSID(name) \
	inline static const Dom::clsuid& guid() { const static  Dom::GUID idn(dom_cls_pre_name #name); return idn; }

namespace Dom {

	class GUID {
		size_t		sLength;
		char*		sGUID;
	public:
		struct Hash {
			inline size_t operator()(const GUID& id) const { return id.hash(); }
		};
		struct Equal {
			inline bool operator()(const GUID& l, const GUID& r) const { return l == r; }
		};
	public:
		GUID(const char* vGUID = nullptr) : sLength(0), sGUID(nullptr) { if (vGUID != nullptr) { sLength = std::strlen(vGUID); sGUID = new char[sLength + 1]; sGUID[sLength] = '\0'; std::strncpy(sGUID, vGUID, sLength); } }
		GUID(const GUID& v) : sLength(v.sLength), sGUID(nullptr) { if (v.sGUID != nullptr) { sGUID = new char[sLength + 1]; sGUID[sLength] = '\0'; std::strncpy(sGUID, v.sGUID, sLength); } }
		GUID(const std::string vGUID) : sLength(vGUID.length()), sGUID(nullptr) { if (!vGUID.empty()) { sGUID = new char[sLength + 1]; sGUID[sLength] = '\0'; std::strncpy(sGUID, vGUID.data(), sLength); } }
		~GUID() { if (sGUID != nullptr) { char* str = sGUID; sGUID = nullptr; sLength = 0; delete[] str; } }
		inline bool operator == (const GUID& WithThis) const { return sGUID == WithThis.sGUID || (sLength == WithThis.sLength && std::strncmp(sGUID, WithThis.sGUID, sLength) == 0); }
		inline size_t hash() const { return std::_Hash_impl::hash(sGUID, sLength); }
		inline const char* c_str() const { return sGUID; }
		inline size_t length() const { return sLength; }
		inline bool empty() const { return !sLength; }
	};

	using gid = GUID;
	using uiid = gid;
	using clsuid = gid;

	static inline const gid ClsId(const std::string& class_name) { return GUID(std::string(dom_cls_pre_name) + class_name); }
	static inline const gid IId(const std::string& interface_name) { return GUID(std::string(dom_guid_pre_name) + interface_name); }
}
