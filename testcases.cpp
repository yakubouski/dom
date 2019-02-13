#ifndef DOM_SAMPLE

#include <cstdio>
#include "src/dom/dom.h"

using namespace Dom;

struct IRegistry2 : public virtual IUnknown {
	virtual bool RegisterClass2(const clsuid& /* class uid */, std::string&& /* Namespace */) = 0;
	virtual bool UnRegisterClass2(const clsuid& /* class uid */, std::string&& /* Namespace */) = 0;
	virtual bool ClassExist2(const clsuid& /* class uid */, std::string&& /* Namespace */) = 0;

	IID(Registry2)
};

class CManager : virtual public Dom::Client::Manager<IRegistry2> {
public:
	virtual bool RegisterClass2(const clsuid& /* class uid */, std::string&& /* Namespace */) {
		return true;
	}
	virtual bool UnRegisterClass2(const clsuid& /* class uid */, std::string&& /* Namespace */) {
		return true;
	}
	virtual bool ClassExist2(const clsuid& /* class uid */, std::string&& /* Namespace */) {
		return true;
	}
};

#include "skeleton/IHello.h"

int main()
{
	/* Case #1 */
	{
		{
			Dom::Client::Manager<> regitry;
			regitry.RegisterServer("/home/irokez/projects/Dynamic-Object-Model/bin/x64/Debug-Skel/lib-sample.so");
		}
		
		{
			Dom::Client::Manager<> manager;
			manager.LoadRegistry();

			Interface<IHello> hello;
			manager.CreateInstance("SimpleHello", hello, "");

			/* Say Hello */
			hello->Say();
		}
	}

	/* Case #2 */
	{
		/* Overload Dom::Client::Manager for extend with embed interfce <IRegistry2> */
		CManager manager;

		/* Emplace Dom Server with class SimpleHello implementation */
		manager.EmplaceServer("/home/irokez/projects/Dynamic-Object-Model/bin/x64/Debug-Skel/lib-sample.so", "");

		/* Create new instance  of SimpleHello class */
		Interface<IHello> hello;
		manager.CreateInstance("SimpleHello", hello, "");

		/* Say Hello */
		hello->Say();
	}
	

	
	return 0;
}

/*
#include <string>
#include <tuple>
#include <memory>
#include <functional>
#include <unordered_map>


class Base {
public:
	virtual ~Base() { ; }
};

class A : public Base {
public:
	A() { printf("%s\n", __PRETTY_FUNCTION__); }
	~A() { printf("%s\n", __PRETTY_FUNCTION__); }
	static Base* self() { return new A(); }
	static std::string name() { return "A"; }
};

class B : public Base {
public:
	B() { printf("%s\n", __PRETTY_FUNCTION__); }
	~B() { printf("%s\n", __PRETTY_FUNCTION__); }
	static Base* self() { return new B(); }
	static std::string name() { return "B"; }
};

template<typename ... CLASSES>
class ClassRegistry {
	template<typename T>
	static inline T* cast() { return new T(); }
public:
	ClassRegistry() { ; }
	Base* create(std::string name) {
		static const std::unordered_map<std::string, std::function<void*()>> list = { std::make_pair(CLASSES::name(),&cast<CLASSES>)... };
		auto&& it = list.find(name);
		if (it != list.end()) {
			return (Base*)it->second();
		}
		return nullptr;
	}
};

int main()
{
    printf("hello from DynamicObjectModel!\n");
	ClassRegistry<A, B> in;
	std::unique_ptr<Base> none(in.create("ddd"));
	std::unique_ptr<Base> a(in.create("A"));
	std::unique_ptr<Base> b(in.create("B"));
    return 0;
}

*/

#endif // !DOM_SAMPLE
