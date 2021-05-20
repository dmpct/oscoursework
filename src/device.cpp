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

void Device::pop(int pid) {
	auto v = find_if(waiting.begin(), waiting.end(),
		[pid](pair<int, int> p) { return p.second == pid; });
	if (v != waiting.end()) {
		waiting.erase(v);
	}
}

pair<string, vector<pair<int, int>>> Device::stat() {
	vector<pair<int, int>> state;
	for (auto v : waiting) {
		state.push_back({ v.first, v.second });
	}
	auto res = make_pair(name, state);
	return res;
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