#include "../include/device.h"

Device::Device(string name, function<void(int, void*)> idt)
	: name(name), idt(idt) {
	occupied = false;
}

Device::~Device() {

}

void Device::require(int time, int pid) {
	occupied = true;
	waiting.push_back(make_pair(time, pid));
}

int Device::poll() {
	if (!occupied) return -1;
	int pid = waiting.front().second;
	waiting.front().first--;
	if (!waiting.front().first) {
		idt(INTN::INT::DEVICE_DONE, &pid);
		waiting.pop_front();
	}
	if (!waiting.size()) occupied = false;
	return pid;
}


Printer::Printer(string name, function<void(int, void*)> idt)
	: Device(name, idt) {

}

Printer::~Printer() {

}

Keyboard::Keyboard(string name, function<void(int, void*)> idt)
	: Device(name, idt) {

}

Keyboard::~Keyboard() {

}