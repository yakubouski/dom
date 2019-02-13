#pragma once
#include "../IRegistry.h"
#include <atomic>
#include <functional>
#include <unordered_map>

namespace Dom {
	namespace Server {

		/* Object server interfaces implement */
		template <typename T, typename ... IFACES>
		struct Object : virtual public IUnknown, virtual public IFACES... {
		private:
			template<typename TP>
			struct cast_interface {
				gid ciid;
				cast_interface(const clsuid& iid) : ciid(iid) { ; }
				inline bool cast(const clsuid& iid, TP* t, void **ppv) const { if (ciid == iid) { *ppv = static_cast<void*>(t); static_cast<IUnknown*>(t)->AddRef(); return true; } return false; }
			};
		private:
			std::atomic_long refs;
		public:
			Object() : refs(0) { ; }
			virtual ~Object() { ; }

			inline virtual long AddRef() {
#ifdef DEBUG
				if (refs < 0) {
					long _refs = refs;
					fprintf(stderr, "`Object::uiid(%s)` was to be destroyed. Incorrect reference counter (%ld < 0). `%s:%ls`\n", T::guid().c_str(), _refs, __PRETTY_FUNCTION__, __LINE__);
				}
#endif // DEBUG
				if (refs == 0) { extern long DllRefIncrement(); DllRefIncrement(); }
				DOM_CALL_TRACE("Refs<%ld>", (long)refs + 1);
				return ++refs;
			}
			inline virtual long Release() {
				if ((--refs) == 0) { extern long DllRefDecrement(); DllRefDecrement();  delete static_cast<T*>(this); }
#ifdef DEBUG
				if (refs < 0) {
					long _refs = refs;
					fprintf(stderr, "Incorrect release of `Object::uiid(%s)`. Reference counter less zero (%ld). `%s:%ls`\n", T::guid().c_str(), _refs, __PRETTY_FUNCTION__, __LINE__);
				}
#endif // DEBUG
				DOM_CALL_TRACE("Refs<%ld>", (long)refs);
				return refs;
			}
			inline virtual bool QueryInterface(const uiid& iid, void **ppv) {
				static const cast_interface<T> guids[] = { IFACES::guid()...,IUnknown::guid() };
				*ppv = nullptr;
				for (auto&& it : guids) {
					if (it.cast(iid, (T*)this, ppv)) { DOM_CALL_TRACE("`%s`", iid.c_str()); return true; }
				}
#ifdef DEBUG
				fprintf(stderr, "Interface `uiid(%s)` for `uiid(%s)` not implemented. `%s:%ls`\n", iid.c_str(), T::guid().c_str(), __PRETTY_FUNCTION__, __LINE__);
#endif // DEBUG
				return false;
			}
		};
		template<typename ... CLASSLIST>
		class ClassRegistry {
		private:
			template<typename T>
			static inline bool CreateObject(const clsuid& iid, void **ppv) {
				T *cls = new T;
				if (cls != nullptr) {
					if (cls->QueryInterface(iid, ppv)) {
						return true;
					}
					delete cls;
				}
				return false;
			}
			std::atomic_long															RegistryRefsCounter;
			std::unordered_map<clsuid, std::function<bool(const clsuid& iid, void **ppv)>>	RegistryExports;
		public:
			ClassRegistry() : RegistryRefsCounter(0), RegistryExports({ std::make_pair(CLASSLIST::guid(),&CreateObject<CLASSLIST>)... }) { DOM_CALL_TRACE(""); }
			virtual ~ClassRegistry() { DOM_CALL_TRACE(""); }
			inline bool CreateInstance(const clsuid& iid, void **ppv) const { DOM_CALL_TRACE(""); *ppv = nullptr; auto&& it = RegistryExports.find(iid); return it != RegistryExports.end() && it->second(IUnknown::guid(), ppv); }

			inline bool RegisterServer(IUnknown* unknown, std::string&& ns) const { DOM_CALL_TRACE(""); Interface<IRegistry> registry(unknown); if (registry) { for (auto&& it : RegistryExports) { if (!registry->RegisterClass(it.first, std::move(ns))) return false; } return true; } return false; }
			inline bool UnRegisterServer(IUnknown* unknown, std::string&& ns) const { DOM_CALL_TRACE(""); Interface<IRegistry> registry(unknown); if (registry) { for (auto&& it : RegistryExports) { if (!registry->UnRegisterClass(it.first, std::move(ns))) return false; } return true; } return false; }

			inline long IncrementRef() { DOM_CALL_TRACE("%ld", (long)RegistryRefsCounter + 1); return ++RegistryRefsCounter; }
			inline long DecrementRef() { DOM_CALL_TRACE("%ld", (long)RegistryRefsCounter - 1); return --RegistryRefsCounter; }

			inline virtual bool CanUnloadNow() { DOM_CALL_TRACE("%s (%ld)", RegistryRefsCounter == 0 ? "yes" : "no", (long)RegistryRefsCounter); return RegistryRefsCounter == 0; }

			inline virtual bool InstallServer(IUnknown* unknown) const { DOM_CALL_TRACE(""); return true; }
			inline virtual bool UnInstallServer(IUnknown* unknown) const { DOM_CALL_TRACE(""); return true; }

			inline virtual bool Initialize(IUnknown* unknown) const { DOM_CALL_TRACE(""); return true; }
			inline virtual bool Finalize(IUnknown* unknown) const { DOM_CALL_TRACE(""); return true; }
		};
	}
}

#define DOM_SERVER_EXPORT(CLASS_REGISTRY,...)\
	static CLASS_REGISTRY<__VA_ARGS__>	DllClassServerManager;\
	extern "C" {\
		bool DllCreateInstance(Dom::clsuid& iid, void** ppv) {	return DllClassServerManager.CreateInstance(iid,ppv); }\
		bool DllCanUnloadNow() { return DllClassServerManager.CanUnloadNow(); } \
		bool DllRegisterServer(Dom::IUnknown* unknown, std::string&& ns) { return DllClassServerManager.RegisterServer(unknown,std::move(ns)); } \
		bool DllUnRegisterServer(Dom::IUnknown* unknown, std::string&& ns) { return DllClassServerManager.UnRegisterServer(unknown,std::move(ns)); }\
		bool DllInstallServer(Dom::IUnknown* unknown) { return DllClassServerManager.InstallServer(unknown); } \
		bool DllUnInstallServer(Dom::IUnknown* unknown) { return DllClassServerManager.UnInstallServer(unknown); }\
		bool DllInitialize(Dom::IUnknown* unknown) { return DllClassServerManager.Initialize(unknown); } \
		bool DllFinalize(Dom::IUnknown* unknown) { return DllClassServerManager.Finalize(unknown); }\
	};\
	namespace Dom {\
		namespace Server{\
			long DllRefIncrement(){ return DllClassServerManager.IncrementRef();}\
			long DllRefDecrement(){ return DllClassServerManager.DecrementRef();}\
		}}
