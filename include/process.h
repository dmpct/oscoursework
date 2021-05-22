#pragma once
#include "memory.h"

namespace PR {
	using Timepiece = uint32_t;

	constexpr uint16_t RR_TP = 2;
	constexpr uint16_t MAX_PROC = 32768;

	constexpr uint16_t NAME_LEN = 16;
	constexpr uint16_t NEWBORN = 0;
	constexpr uint16_t READY = 1;
	constexpr uint16_t RUNNING = 2;
	constexpr uint16_t WAITING =3;
	constexpr uint16_t DEAD = 4;
	// constexpr int SWAPPED = 4;

	//struct Proc_desc { // 32B
	//	uint16_t state; // 2B
	//	uint16_t pid; // 2B
	//	uint16_t parent; // 2B
	//	uint16_t sibling; // 2B
	//	uint16_t children; // 2B
	//	uint16_t priority; // 2B
	//	char files[4]; // 4B
	//	Timepiece cputime;
	//	Timepiece servtime;
	//	struct Mem_desc* mem; // 8B in x64
	//	//struct P_info* pcb; // 8B in x64
	//};

	//struct P_info {
	//	struct Proc_desc* desc; // 8B in x64
	//	char name[NAME_LEN]; // 16B
	//};

	enum class Algorithm {
		FCFS, // non-preemptive
		SJF, // preemptive
		RR, // preemptive
		PRIORITY, // non-preemptive
		MIXED_QUEUE,
		/* Processes are arranged into several
		*  priority queues. Processes are 
		*  scheduled using non-preemptive priority
		*  mode between queues and are scheduled
		*  with RR mode within one queue.
		*/
		NONE
	};
}

class Process {
private:
	mt19937 rd_s;
	char names[240];
	string seek_names(int id);
	char last_ins;
	string last_file;
	int last_size;
public:
	VirtMemoryModel* mem;
	string pwd;
	string name;
	uint16_t state;
	uint16_t pid;
	uint16_t parent;
	uint16_t sibling;
	uint16_t children;
	uint16_t priority;
	vector<pair<string, int>> open_files;
	PR::Timepiece cputime;
	PR::Timepiece servtime;
	PR::Timepiece iotime;
	PR::Timepiece workload;
	PR::Timepiece timeleft;
	PR::Timepiece born;
	MM::virt_addr ip;
	function<void(int, void*)> idt;
	int est;
	int last_page;

	Process(string name, uint16_t state, uint16_t pid, 
		uint16_t parent, uint16_t priority, PR::Timepiece time,
		function<void(int, void*)> idt, MM::Algorithm algo, string pwd);
	Process(Process* father, uint16_t pid, PR::Timepiece time);
	~Process();

	void release();
	int run(PR::Timepiece tp, void* info);
	int generate_random_pg();
};

class Scheduler {
private:
	vector<Process*> prlist;
	PR::Algorithm algo;
	MM::Algorithm ralgo;
	function<void(int, void*)> idt;
	PR::Timepiece clock;
	list<int> ready;
	list<int> waiting;
	int running;
	mutex lock;
	uint16_t new_pid();
	PR::Timepiece cur_tp;

	list<int> high_pr;
	list<int> mid_pr;
	list<int> low_pr;

	int turnaround;
	int doneprs;
	int cpu_piece;
	int idle_piece;
	
public:
	Scheduler(function<void(int, void*)> idt,
		PR::Algorithm algo1, MM::Algorithm algo2);
	~Scheduler();

	uint16_t fork(uint16_t ppid);
	void fork();
	bool exec(string path, uint16_t pid);
	bool exec_wp(string path, uint16_t pid, string pwd);
	void set_pending(int pid);
	void set_serv();
	void wake(int pid);
	bool kill(uint16_t pid);
	bool safe_kill(uint16_t pid);
	void schedule(PR::Timepiece time);
	void read_table();
	void print_mem();
	double cpu_rate();
	double throughput();
	vector<vector<string>> expose();
	vector<vector<string>> expose_mem();
	void chalg(PR::Algorithm, MM::Algorithm);
	pair<string, string> alg();
	double statistic();
};
