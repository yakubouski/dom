#ifdef DOM_SAMPLE

#include "../src/dom/dom.h"
#include "IHello.h"

class SimpleHello : public Dom::Server::Object<SimpleHello, IHello> {
public:
	SimpleHello() { printf("%s\n", __PRETTY_FUNCTION__); }
	~SimpleHello() { printf("%s\n", __PRETTY_FUNCTION__); }
	virtual void Say() {
		printf("Hello from %s\n", __PRETTY_FUNCTION__);
	}
	CLSID(SimpleHello)
};

DOM_SERVER_EXPORT(Dom::Server::ClassRegistry, SimpleHello);

#endif