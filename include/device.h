#include "log.h"

class Device {
protected:
	bool occupied;
	list<pair<int, int>> waiting;
	function<void(int, void*)> idt;
public:
	string name;
	Device(string name, function<void(int, void*)> idt);
	~Device();
	virtual void require(int time, int pid);
	virtual int poll();
	virtual void pop(int pid);
	virtual pair<string ,vector<pair<int, int>>> stat();
	bool query() {
		return occupied;
	}
};

class Printer : public Device {
public:
	Printer(string name, function<void(int, void*)> idt);
	~Printer();
};

class Keyboard : public Device {
public:
	Keyboard(string name, function<void(int, void*)> idt);
	~Keyboard();
};