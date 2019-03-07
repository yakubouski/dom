#pragma once
#include "../IManager.h"
#include "../IRegistry.h"
#include <sys/stat.h>
#include <dlfcn.h>
#include <climits>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <atomic>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <system_error>
#include <forward_list>

namespace Dom {
	namespace Client {
		extern "C"
		{
			typedef bool(*__DllCreateInstance)(const clsuid&, void**);
			typedef bool(*__DllCanUnloadNow)();
			typedef bool(*__DllRegisterServer)(IUnknown*, std::string&&);
			typedef bool(*__DllUnRegisterServer)(IUnknown*, std::string&&);
			typedef bool(*__DllInstallServer)(IUnknown*);
			typedef bool(*__DllUnInstallServer)(IUnknown*);
			typedef bool(*__DllInitialize)(IUnknown*);
			typedef bool(*__DllFinalize)(IUnknown*);
		}

		static inline std::string PathName(const std::string&& path, const std::string&& dir = std::string()) {
			auto result = path + (path.back() == '/' ? std::string() : "/") + (!dir.empty() && dir.front() == '/' ? dir.substr(1) : dir);
			return result + (result.back() == '/' ? "" : "/");
		}

		static inline void EnumFiles(const std::string &path, std::function<void(const std::string&&, const struct ::dirent &)>&& cb, bool recursive = true) {
			std::forward_list<std::string> dirs({ PathName(std::move(path)) });
			for (auto&& item : dirs) {
				if (auto dir = opendir(path.c_str())) {
					while (auto f = readdir(dir)) {
						if (!f->d_name || f->d_name[0] == '.') continue;

						if (f->d_type & DT_DIR) {
							cb(item + f->d_name + '/', *f);
							if (recursive) {
								dirs.emplace_after(dirs.end(), item + f->d_name + "/");
							}
						}
						else if (f->d_type & DT_REG) {
							cb(item + f->d_name, *f);
						}
					}
					closedir(dir);
				}
#ifdef DEBUG
				else {
					fprintf(stderr, "Dom::FileSystem::Enum(%s) `%s (%ld)`\n", path.c_str(), strerror(errno), errno);
				}
#endif // DEBUG
			}
		}

		static inline size_t Exist(const std::string& path)
		{
			struct stat info;
			return stat(path.c_str(), &info) == 0 ? info.st_mode : 0;
		}

		static inline int MakeDir(const std::string& dirname, int mode = 0777)
		{
			if (mkdir(dirname.c_str(), mode) == 0)
				return 0;
			if (errno == EEXIST) {
				return Exist(dirname) & S_IFDIR ? 0 : ENOTDIR;
			}
			if (errno == ENOENT) {
				size_t pos = dirname.find_last_of('/');
				if (pos == std::string::npos)
					return 0;
				auto res = MakeDir(dirname.substr(0, pos), mode);
				if (!res) {
					return mkdir(dirname.c_str(), mode);
				}
				return res;
			}
			return errno;
		}

		/* Object server interfaces implement */
		template <typename T, typename ... IFACES>
		struct Embedded : virtual public IUnknown, virtual public IFACES... {
		private:
			std::atomic_long refs;
			template<typename TP>
			struct cast_interface {
				gid ciid;
				cast_interface(const clsuid& iid) : ciid(iid) { ; }
				inline bool cast(const clsuid& iid, TP& t, void **ppv) const { if (ciid == iid) { *ppv = static_cast<void*>(&t);  static_cast<IUnknown*>(&t)->AddRef(); return true; }  return false; }
			};
		public:
			Embedded() : refs(0) { DOM_CALL_TRACE(""); }
			virtual ~Embedded() { DOM_CALL_TRACE(""); }
			
			inline operator IUnknown*() { return static_cast<IUnknown*>(this); }

			inline virtual long AddRef() {
#ifdef DEBUG
				if (refs < 0) {
					long _refs = refs;
					fprintf(stderr, "`Object::uiid(%s)` was to be destroyed. Incorrect reference counter (%ld < 0). `%s:%ls`\n", T::guid().c_str(), _refs, __PRETTY_FUNCTION__, __LINE__);
				}
#endif // DEBUG
				DOM_CALL_TRACE("Refs<%ld>", (long)refs + 1);
				return ++refs;
			}
			inline virtual long Release() {
				if ((--refs) == 0) { delete static_cast<T*>(this); return 0; }
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
				const static cast_interface<decltype(*this)> guids[] = { IFACES::guid()...,IUnknown::guid() };
				*ppv = nullptr;
				for (auto&& it : guids) {
					if (it.cast(iid, *this, ppv)) { DOM_CALL_TRACE("`%s`", iid.c_str()); return true; }
				}
				DOM_ERR("Interface `uiid(%s)` for `uiid(%s)` not implemented", iid.c_str(), std::remove_reference<decltype(*this)>::type::guid().c_str());
				return false;
			}

		};

	class Dll {
		private:
			void*					_handle;
			std::string				_soname;
			__DllCreateInstance		_createinstance;
			__DllCanUnloadNow		_canunloadnow;
			__DllRegisterServer		_registerserver;
			__DllUnRegisterServer	_unregisterserver;
			__DllInstallServer		_install;
			__DllUnInstallServer	_uninstall;
			__DllInitialize			_initialize;
			__DllFinalize			_finalize;

			inline void __unload() {
				if (_handle != nullptr) {
					(*_canunloadnow)() && dlclose(_handle);
					_handle = nullptr;
					_createinstance = nullptr;	_canunloadnow = nullptr;	_registerserver = nullptr;	_unregisterserver = nullptr;
					_install = nullptr;			_uninstall = nullptr;		_initialize = nullptr;		_finalize = nullptr;
				}
			}

			inline void __load() {
				if (_handle == nullptr && !_soname.empty()) {
					_handle = dlopen(_soname.c_str(), RTLD_NOW);
					if (_handle == nullptr) {
						throw std::system_error(EINVAL, std::system_category(), dlerror());
					}
					_createinstance = (__DllCreateInstance)dlsym(_handle, "DllCreateInstance");
					_canunloadnow = (__DllCanUnloadNow)dlsym(_handle, "DllCanUnloadNow");
					_registerserver = (__DllRegisterServer)dlsym(_handle, "DllRegisterServer");
					_unregisterserver = (__DllUnRegisterServer)dlsym(_handle, "DllUnRegisterServer");
					_install = (__DllInstallServer)dlsym(_handle, "DllInstallServer");
					_uninstall = (__DllUnInstallServer)dlsym(_handle, "DllUnInstallServer");
					_initialize = (__DllInitialize)dlsym(_handle, "DllInitialize");
					_finalize = (__DllFinalize)dlsym(_handle, "DllFinalize");
					if (_createinstance == nullptr || _canunloadnow == nullptr || _registerserver == nullptr || _unregisterserver == nullptr) {
						__unload();
						throw std::system_error(EFAULT, std::system_category(), "One or many function not exported from server (DllCreateInstance, DllCanUnloadNow, DllRegisterServer, DllUnInstallServer)");
					}
				}
			}

		public:
			Dll() : _handle(nullptr), _soname(), _createinstance(nullptr), _canunloadnow(nullptr),
				_registerserver(nullptr), _unregisterserver(nullptr), _install(nullptr), _uninstall(nullptr), _initialize(nullptr), _finalize(nullptr) {
				;
			}
			Dll(std::string so) :
				_handle(nullptr), _soname(so), _createinstance(nullptr), _canunloadnow(nullptr),
				_registerserver(nullptr), _unregisterserver(nullptr), _install(nullptr), _uninstall(nullptr), _initialize(nullptr), _finalize(nullptr) {
				__load();
			}
			
			~Dll() { __unload(); }

			Dll(const Dll& so) = delete;

			Dll(const Dll&& so) {
				_handle = so._handle;
				_soname = move(so._soname);
				_createinstance = so._createinstance;
				_canunloadnow = so._canunloadnow;
				_registerserver = so._registerserver;
				_unregisterserver = so._unregisterserver;
				_install = so._install;
				_uninstall = so._uninstall;
				_initialize = so._initialize;
				_finalize = so._finalize;
			}

			inline operator bool() { return _handle != nullptr; }

			const Dll& operator = (const Dll& so) = delete;
			inline const Dll& operator = (const Dll&& so) {
				_handle = so._handle;
				_soname = move(so._soname);
				_createinstance = so._createinstance;
				_canunloadnow = so._canunloadnow;
				_registerserver = so._registerserver;
				_unregisterserver = so._unregisterserver;
				_install = so._install;
				_uninstall = so._uninstall;
				_initialize = so._initialize;
				_finalize = so._finalize;
				return *this;
			}
			inline bool CreateInstance(const clsuid& id, void** ppv) { return (*_createinstance)(id, ppv); }
			inline bool CanUnloadNow() { return (*_canunloadnow)(); }
			inline bool RegisterServer(IUnknown* unkn, std::string&& scope) { return (*_registerserver)(unkn, std::move(scope)); }
			inline bool UnRegisterServer(IUnknown* unkn, std::string&& scope) { return (*_unregisterserver)(unkn, std::move(scope)); }
			inline bool InstallServer(IUnknown* unkn) { return _install != nullptr ? (*_install)(unkn) : true; }
			inline bool UnInstallServer(IUnknown* unkn) { return _uninstall != nullptr ? (*_uninstall)(unkn) : true; }
			inline bool Initialize(IUnknown* unkn) { return _initialize != nullptr ? (*_initialize)(unkn) : true; }
			inline bool Finalize(IUnknown* unkn) { return _finalize != nullptr ? (*_finalize)(unkn) : true; }

		};

		template<typename ... IFACES>
		class Manager : virtual public IUnknown, virtual public IFACES... {
			template<typename TP>
			struct cast_interface {
				gid ciid;
				cast_interface(const clsuid& iid) : ciid(iid) { ; }
				inline bool cast(const clsuid& iid, TP& t, void **ppv) const { if (ciid == iid) { *ppv = static_cast<void*>(&t);  static_cast<IUnknown*>(&t)->AddRef(); return true; }  return false; }
			};
		private:
			std::mutex																listLock;
			std::unordered_multimap<clsuid, std::pair<std::string, std::string>, Dom::GUID::Hash, Dom::GUID::Equal>	listClasses;
			std::unordered_map<std::string, Dll>									listServers;
			class CSharedServer : virtual public IUnknown, virtual public IRegistry {
				std::string SoPathName, RegistryPath;
			public:
				CSharedServer(std::string& So, std::string& Path) : SoPathName(So), RegistryPath(PathName(std::move(Path))) { DOM_CALL_TRACE(""); }

				virtual ~CSharedServer() { DOM_CALL_TRACE(""); }

				inline operator IUnknown*() { return static_cast<IUnknown*>(this); }

				inline virtual long AddRef() { return 1; }
				inline virtual long Release() { return 1; }

				inline virtual bool QueryInterface(const uiid& iid, void **ppv) {
					if (IUnknown::guid() == iid) {
						*ppv = static_cast<IUnknown*>(this); 
						static_cast<IUnknown*>(this)->AddRef(); 
						return true;
					}
					else if (IRegistry::guid() == iid) {
						*ppv = static_cast<IRegistry*>(this);
						static_cast<IUnknown*>(this)->AddRef();
						return true;
					}
#ifdef DEBUG
					fprintf(stderr, "Interface `uiid(%s)` for `uiid(%s)` not implemented. `%s:%ls`\n", iid.c_str(), "CSharedServer", __PRETTY_FUNCTION__, __LINE__);
#endif // DEBUG
					return false;
				}

				inline bool Register(std::string& Scope) {
					Dll so(SoPathName);
					IUnknown* registry;
					this->QueryInterface(IUnknown::guid(), (void**)&registry);
					if (so.RegisterServer(registry, std::move(Scope))) {
						so.InstallServer(registry);
						return true;
					}
					return false;
				}
				inline bool UnRegister(std::string& Scope) {
					Dll so(SoPathName);
					IUnknown* registry;
					this->QueryInterface(IUnknown::guid(), (void**)&registry);
					so.UnInstallServer(registry);
					return so.UnRegisterServer(registry, std::move(Scope));
				}
				inline virtual bool RegisterClass(const clsuid& uid, std::string&& Scope) {
					auto ScopePath = PathName(std::move(RegistryPath), std::move(Scope));
					if (MakeDir(ScopePath) == 0 && symlink(SoPathName.c_str(), std::string(ScopePath + uid.c_str()).c_str()) == 0) {
						return true;
					}
#ifdef DEBUG
					else {
						fprintf(stderr, "MakeDir(%s) `%s (%ld)`\n", ScopePath.c_str(), strerror(errno), errno);
					}
#endif // DEBUG
					return false;
				}
				inline virtual bool UnRegisterClass(const clsuid& uid, std::string&& Scope) {
					auto ScopePath = PathName(std::move(RegistryPath), std::move(Scope));
					return remove(std::string(ScopePath + uid.c_str()).c_str()) == 0;
				}
				inline virtual bool ClassExist(const clsuid& uid, std::string&& Scope) {
					auto ScopePath = PathName(std::move(RegistryPath), std::move(Scope));
					return Exist(std::string(ScopePath + uid.c_str())) == 0;
				}
			};

			class CEmbedServer : virtual public IUnknown, virtual public IRegistry {
				std::string SoPathName;
				std::unordered_multimap<clsuid, std::pair<std::string, std::string>,GUID::Hash, GUID::Equal>&	listClasses;
				std::unordered_map<std::string, Dll>&								listServers;
			public:
				CEmbedServer(std::string& So, std::string& Scope, std::unordered_multimap<clsuid, std::pair<std::string, std::string>,GUID::Hash, GUID::Equal>& classes, std::unordered_map<std::string, Dll>& servers)
					: SoPathName(So), listClasses(classes), listServers(servers) {
					Dll so(So);
					IUnknown* registry;
					this->QueryInterface(IUnknown::guid(), (void**)&registry);
					if (so.RegisterServer(registry, std::move(Scope))) {
						so.InstallServer(registry);
					}
				}
				virtual ~CEmbedServer() { DOM_CALL_TRACE(""); }

				inline operator IUnknown*() { return static_cast<IUnknown*>(this); }

				inline virtual long AddRef() { return 1; }
				inline virtual long Release() { return 1; }

				inline virtual bool QueryInterface(const uiid& iid, void **ppv) {
					if (IUnknown::guid() == iid) {
						*ppv = static_cast<IUnknown*>(this);
						static_cast<IUnknown*>(this)->AddRef();
						return true;
					}
					else if (IRegistry::guid() == iid) {
						*ppv = static_cast<IRegistry*>(this);
						static_cast<IUnknown*>(this)->AddRef();
						return true;
					}
#ifdef DEBUG
					fprintf(stderr, "Interface `uiid(%s)` for `uiid(%s)` not implemented. `%s:%ls`\n", iid.c_str(), "CEmbedServer", __PRETTY_FUNCTION__, __LINE__);
#endif // DEBUG
					return false;
				}

				inline virtual bool RegisterClass(const clsuid& uid, std::string&& Scope) {
					listClasses.emplace(uid, std::make_pair(Scope, SoPathName));
					listServers.emplace(SoPathName, Dll(SoPathName));
					return true;
				}
				inline virtual bool UnRegisterClass(const clsuid& uid, std::string&& Scope) {
					auto&& range = listClasses.equal_range(uid);
					for (auto&& it = range.first; it != range.second; it++) {
						if (it->second.first == Scope) {
							listClasses.erase(it);
							return true;
						}
					}
					return false;
				}
				inline virtual bool ClassExist(const clsuid& uid, std::string&& Scope) {
					auto&& range = listClasses.equal_range(uid);
					for (auto&& it = range.first; it != range.second; it++) {
						if (it->second.first == Scope) {
							return true;
						}
					}
					return false;
				}
			};

		public:
			Manager() { DOM_CALL_TRACE(""); }
			virtual ~Manager() { DOM_CALL_TRACE(""); }

			inline operator IUnknown*() { return static_cast<IUnknown*>(this); }

			inline virtual long AddRef() { DOM_CALL_TRACE(""); return 1; }
			inline virtual long Release() { DOM_CALL_TRACE(""); return 1; }
			inline virtual bool QueryInterface(const uiid& iid, void **ppv) {
				const static cast_interface<decltype(*this)> guids[] = { IFACES::guid()...,IUnknown::guid() };
				*ppv = nullptr;
				for (auto&& it : guids) {
					if (it.cast(iid, *this, ppv)) { DOM_CALL_TRACE("`%s`", iid.c_str()); return true; }
				}
				DOM_ERR("Interface `uiid(%s)` for `uiid(%s)` not implemented", iid.c_str(), std::remove_reference<decltype(*this)>::type::guid().c_str());
				return false;
			}

			inline bool LoadRegistry(std::string RegistryPath = std::string(DOM_REGPATH)) { 
				std::unique_lock<std::mutex> lock(listLock);
				EnumFiles(RegistryPath, [&](const std::string& fullpath, const dirent& e) {
					if (e.d_type & DT_LNK) {
						auto ScopeName = fullpath.substr(RegistryPath.length());
						std::string Scope;
						if (size_t pos = ScopeName.rfind('/') != std::string::npos) {
							Scope = ScopeName.substr(pos);
						}
						listClasses.emplace(e.d_name, std::make_pair(Scope, fullpath));
						listServers.emplace(fullpath,Dll(fullpath));
					}
				});
				return true;
			}
			
			inline virtual bool CreateInstance(const clsuid& cid, void ** ppv, std::string Scope = std::string()) {
				*ppv = nullptr;
				std::unique_lock<std::mutex> lock(listLock);
				try {
					auto clsId = Dom::ClsId(cid.c_str());
					auto&& clsEntries = listClasses.equal_range(clsId);
					for (auto&& clsEntry = clsEntries.first; clsEntry != clsEntries.second;clsEntry++) {
						if (clsEntry->second.first == Scope) {
							auto&& clsDll = listServers.find(clsEntry->second.second);
							if (clsDll == listServers.end()) {
								DOM_ERR("Shared object `%s` for Class `%s/%s` not found in registry", clsEntry->second.second.c_str(), Scope.c_str(), cid.c_str());
								return false;
							}
							DOM_CALL_TRACE("%s/%s", Scope.c_str(), cid.c_str());
							return clsDll->second.CreateInstance(clsId, ppv);
						}
					}
					DOM_ERR("Class `%s/%s` not found in registry", Scope.c_str(), cid.c_str());
				}
				catch (std::exception ex) {
					DOM_ERR("Exception `%s`", ex.what());
					throw;
				}
				return false;
			}
			
			inline virtual bool EmplaceServer(std::string SoServer, std::string Scope = std::string()) { 
				try {
					std::unique_lock<std::mutex> lock(listLock);
					CEmbedServer server(SoServer, Scope, listClasses,listServers);
					return true;
				}
				catch (std::exception ex) {
					DOM_ERR("Exception `%s`", ex.what());
					throw;
				}
				return false;
			}
			
			inline virtual ClassList EnumClasses(std::string Scope = std::string()) { 
				ClassList list;
				std::unique_lock<std::mutex> lock(listLock);
				for (auto&& co : listClasses) {
					if (Scope.empty() || Scope == co.second.first) {
						list.emplace_after(list.end(), std::make_pair(co.first, co.second.first));
					}
				}
				return list;
			}

			inline virtual bool RegisterServer(std::string SoServer, std::string RegistryPath = std::string(DOM_REGPATH), std::string Scope = std::string()) {
				try {
					std::unique_lock<std::mutex> lock(listLock);
					CSharedServer server(SoServer, RegistryPath);
					return server.Register(Scope);
				}
				catch (std::exception ex) {
					DOM_ERR("Exception `%s`", ex.what());
					throw;
				}
				return false;
			}
			inline virtual bool UnRegisterServer(std::string SoServer, std::string RegistryPath = std::string(DOM_REGPATH), std::string Scope = std::string()) {
				try {
					std::unique_lock<std::mutex> lock(listLock);
					CSharedServer server(SoServer, RegistryPath);
					return server.UnRegister(Scope);
				}
				catch (std::exception ex) {
					DOM_ERR("Exception `%s`", ex.what());
					throw;
				}
				return false;
			}
			inline virtual ClassList EnumServers(std::string RegistryPath = std::string(DOM_REGPATH), std::string Scope = std::string()) {
				std::unique_lock<std::mutex> lock(listLock);
				auto ScopePath = PathName(std::move(RegistryPath),std::move(Scope));
				ClassList list;
				EnumFiles(ScopePath, [&](const std::string& fullpath, const dirent& e) {
					if (e.d_type & DT_LNK) {
						auto ScopeName = fullpath.substr(RegistryPath.length());
						list.emplace_after(list.end(), std::make_pair(e.d_name, ScopeName.substr(ScopeName.rfind('/'))));
					}
				});
				return list;
			}
		};
	}
}