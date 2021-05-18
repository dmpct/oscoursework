#include "log.h"

class Device {
protected:
	bool occupied;
	list<pair<int, int>> waiting;
	function<void(int, void*)> idt;
	string name;
public:
	Device(string name, function<void(int, void*)> idt);
	~Device();
	virtual void require(int time, int pid);
	virtual int poll();
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