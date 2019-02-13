#pragma once
#include "../IUnknown.h"

namespace Dom {
	template <class T> class Interface
	{
	private:
		T* _i;
	public:
		Interface() : _i(nullptr) { ; }
		Interface(IUnknown* unkw) : _i(nullptr) { unkw != nullptr && unkw->QueryInterface(T::guid(), (void**)&_i) && _i->AddRef(); }
		Interface(const Interface<T>&) = delete;
		Interface(const Interface<T>&& lp) : _i(lp._i) { _i != nullptr && _i->AddRef(); }
		~Interface() { Release(); }
		Interface<T>& operator = (const Interface<T>&) = delete;
		inline Interface<T>& operator = (const Interface<T>&& lp)
		{
			Release();
			_i = lp._i;
			_i != nullptr && _i->AddRef();
			return *this;
		}
		inline operator bool() const { return (_i != nullptr); }
		inline operator IUnknown* () const { return (IUnknown*)_i; }
		inline operator T* () const { return _i; }
		inline operator void** () const { return (void**)&_i; }
		inline T* operator -> () const { return _i; }
		inline bool operator ! () const { return (_i == nullptr); }

		inline void Release() { T* pTemp = _i; _i = nullptr; if (pTemp != nullptr) { pTemp->Release(); } }
		template <typename I> inline bool QueryInterface(I *pI) const { pI = nullptr; return _i != nullptr && _i->QueryInterface(I::guid(), (void **)&pI); }
		static inline const uiid& guid() { return T::guid(); }
	};
}