#pragma once
#include "filesystem.h"
#include "process.h"
#include "device.h"

namespace KN {
	constexpr int millis_per_clock = 2000;
}

class Kernel {
private:
	vector<Device*> devices;
	PR::Timepiece clock;
	bool exit_kernel;
	int mode;
	bool header = true;
	inline void trim(string& s) {
		s.erase(s.begin(), find_if(s.begin(), s.end(),
			[](unsigned char ch) {
				return !isspace(ch);
			}));
		s.erase(find_if(s.rbegin(), s.rend(),
			[](unsigned char ch) {
				return !isspace(ch);
			}).base(), s.end());
	}
	uint64_t uid;

public:
	PageMemoryModel* pg;
	Filesystem* fs;
	Scheduler* sch;
	Kernel(PR::Algorithm pa, MM::Algorithm ma, uint64_t uid);
	~Kernel();
	void int_handler(int int_type, void* args);
	int load_prog(string path, VirtMemoryModel* mm, int* et, int* pri);

	string get_pwd();
	int get_clock();
	void set_mode(int mode);
	void run();
	void pause();
	void exit();
	void chalg(PR::Algorithm, MM::Algorithm);
	pair<string, string> alg();
	vector<vector<string>> mem_pr();
	vector<vector<string>> expose_pr();
	vector<int> mem_map();
	vector<int> swap_map();
	double statistic();
	void new_device(string name);
	int del_device(string name);
	vector<pair<string, vector<pair<int, int>>>> expose_devices();
};