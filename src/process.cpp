#include "../include/process.h"

Scheduler::Scheduler(function<void(int, void*)> idt,
	PR::Algorithm algo1, MM::Algorithm algo2) :
	idt(idt),
	algo(algo1),
	ralgo(algo2) {
	clock = 0;
	prlist.resize(PR::MAX_PROC);
	Process* idle = new Process("idle", PR::READY, 0, 0, UINT16_MAX, clock, idt, ralgo);
	prlist[0] = idle;
	idle->est = INT_MAX;
	ready.push_back(0);
	running = -1;
	cur_tp = 0;
	doneprs = 0;
	turnaround = 0;
}
Scheduler::~Scheduler() {
	for (auto v : prlist) {
		if (v) delete v;
	}
}

uint16_t Scheduler::fork(uint16_t ppid) {
	uint16_t pid = new_pid();
	if (pid < PR::MAX_PROC) {
		Process* child = new Process(prlist.at(ppid), pid, clock);
		prlist[pid] = child;
	}
	return pid;
}

void Scheduler::fork() {
	//lock_guard<mutex> guard(lock);
	uint16_t pid = 1;
	Process* child = new Process("init", PR::DEAD, 1, 0, 3, clock, idt, ralgo);
	prlist[pid] = child;
}

bool Scheduler::exec(string path, uint16_t pid) {
	Process* pr = prlist[pid];
	struct args {
		string path;
		VirtMemoryModel* mm;
		int et;
		int pri;
		int state;
	} args;
	args.path = path;
	args.mm = pr->mem;
	args.state = 0;
	idt(INTN::INT::REQ_LOAD, &args);
	if (args.state) {
		auto pos = path.rfind("/");
		if (pos != string::npos) pr->name = path.substr(pos + 1);
		else pr->name = path;
		pr->state = PR::READY;
		pr->priority = args.pri;
		pr->est = args.et;
		if(!(algo == PR::Algorithm::MIXED_QUEUE)) ready.push_back(pid);
		else {
			if (prlist[pid]->priority <= 3) high_pr.push_back(pid);
			else if (prlist[pid]->priority <= 6) mid_pr.push_back(pid);
			else low_pr.push_back(pid);
		}
		return true;
	}
	else {
		return false;
	}
}

bool Scheduler::kill(uint16_t pid) {
	if (prlist[pid]) {
		doneprs++;
		turnaround += prlist[pid]->servtime;
		if(prlist[pid]->children)
			prlist[prlist[pid]->children]->parent = 1;
		prlist[pid]->release();
		prlist[pid]->state = PR::DEAD;
		ready.remove_if([pid](int p) { return p == pid; });
		waiting.remove_if([pid](int p) { return p == pid; });
		high_pr.remove_if([pid](int p) { return p == pid; });
		low_pr.remove_if([pid](int p) { return p == pid; });
		mid_pr.remove_if([pid](int p) { return p == pid; });
		delete prlist[pid];
		prlist[pid] = nullptr;
		return true;
	}
	return false;
}

bool Scheduler::safe_kill(uint16_t pid) {
	if (running == pid) {
		running = -1;
	}
	return kill(pid);
}

void Scheduler::wake(int pid) {
	prlist[pid]->state = PR::READY;
	waiting.remove(pid);
	if (!(algo == PR::Algorithm::MIXED_QUEUE)) ready.push_back(pid);
	else {
		if (prlist[pid]->priority <= 3) high_pr.push_back(pid);
		else if (prlist[pid]->priority <= 6) mid_pr.push_back(pid);
		else low_pr.push_back(pid);
	}
}

void Scheduler::schedule(PR::Timepiece time) { // todo: remove reschedule after a waiting?
	clock = time;
	if (algo == PR::Algorithm::FCFS) {
		if (!ready.size()) {
			if (running == 0) {
				prlist[0]->cputime++;
				return;
			}
			else return; // should not happen
		}
		if (running > 0) {
			// do nothing
		}
		else {
			uint16_t pid = ready.front();
			ready.pop_front();
			if (pid == 0) {
				if (ready.size()) {
					ready.push_back(pid);
					pid = ready.front();
					ready.pop_front();
				}
				else {
					prlist[pid]->state = PR::RUNNING;
					prlist[pid]->cputime++;
					running = 0;
					return;
				}
			}
			if (running == 0) {
				prlist[0]->state = PR::READY;
				ready.push_back(0);
			}
			prlist[pid]->state = PR::RUNNING;
			running = pid;
		}
		
		int res = -1;
		char* info = new char[3];
		res = prlist[running]->run(0, info);
		switch (res) {
		case 0: {
			break;
		}
		case 1: { // ?suspended waiting
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			int device = -1;
			if (info[0] == 'i') device = 1;
			else if (info[0] == 'p') device = 0;
			else {
				Log::w("(process.cpp) scheduler: unknown device.\n");
				return;
			}
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[1]);
			args.did = device;
			idt(INTN::INT::DEVICE_REQ, &args);
			running = -1;
			schedule(clock);
			break;
		}
		case 2: { // read
			[[fallthrough]];
		}
		case 3: { // write
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[2]);
			args.did = 2;
			idt(INTN::INT::DEVICE_REQ, &args);
			/*struct args {
				int pid;
				string fname;
				int rw;
				int fid;
			} args2;
			args2.pid = running;
			args2.fname = string(1, info[1]);
			args2.rw = info[0] == 'r'? 0 : 1;
			idt(INTN::INT::REQ_FILE_OPEN, &args2);
			if (find(prlist[running]->open_files.begin(),
				prlist[running]->open_files.end(),
				args2.fid) == prlist[running]->open_files.end()) {
				prlist[running]->open_files.push_back(args2.fid);
			}*/
			running = -1;
			schedule(clock);
			break;
		}
		case 5: { // fault
			Log::i("Process %d: %s segmentation fault, killed.\n",
				running, prlist[running]->name.c_str());
			[[fallthrough]];
		}
		case 4: { // return
			kill(running);
			running = -1;
			schedule(clock);
			break;
		}
		default: {
			Log::w("(process.cpp) schedule: Unknown state.\n");
			break;
		}  
		}
		delete[] info;
	}
	else if (algo == PR::Algorithm::SJF) {
		auto v = min_element(ready.begin(), ready.end(),
			[this](int a, int b) 
			{ return this->prlist[a]->est < this->prlist[b]->est; });
		int candidate = *v;
		
		if (running == -1) {
			running = 0;
			prlist[running]->state = PR::RUNNING;
		}
		if (prlist[candidate]->est < prlist[running]->est) { // preempt
			prlist[running]->state = PR::READY;
			ready.erase(v);
			ready.push_back(running);
			running = candidate;
			prlist[running]->state = PR::RUNNING;
		}
		if (running == 0) {
			prlist[running]->cputime++;
			return;
		}
		int res = -1;
		char* info = new char[3];
		res = prlist[running]->run(0, info);
		//prlist[running]->est--;
		switch (res) {
		case 0: { // cpu occupied
			//set_pending();
			break;
		}
		case 1: { // ?suspended waiting
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			int device = -1;
			if (info[0] == 'i') device = 1;
			else if (info[0] == 'p') device = 0;
			else {
				Log::w("(process.cpp) scheduler: unknown device.\n");
				return;
			}
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[1]);
			args.did = device;
			idt(INTN::INT::DEVICE_REQ, &args);
			running = -1;
			schedule(clock);
			break;
		}
		case 2: { // read
			[[fallthrough]];
		}
		case 3: { // write
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[2]);
			args.did = 2;
			idt(INTN::INT::DEVICE_REQ, &args);
			running = -1;
			schedule(clock);
			break;
		}
		case 5: { // fault
			Log::i("Process %d: %s segmentation fault, killed.\n",
				running, prlist[running]->name.c_str());
			[[fallthrough]];
		}
		case 4: { // return
			kill(running);
			running = -1;
			schedule(clock);
			break;
		}
		default: {
			Log::w("(process.cpp) schedule: Unknown state.\n");
			break;
		}
		}
		delete[] info;
	}
	else if (algo == PR::Algorithm::RR) {
		if (cur_tp == 0) { // time up
			if (running >= 0) {
				prlist[running]->state = PR::READY;
				ready.push_back(running);
				running = -1;
			}
			cur_tp = PR::RR_TP;
		}

		if (running == -1) {
			cur_tp = PR::RR_TP;
			running = ready.front();
			ready.pop_front();
			prlist[running]->state = PR::RUNNING;
		}

		if (running == 0) {
			if (ready.size()) {
				prlist[running]->state = PR::READY;
				ready.push_back(running);
				cur_tp = PR::RR_TP;
				running = ready.front();
				ready.pop_front();
				prlist[running]->state = PR::RUNNING;
			}
			else {
				prlist[running]->cputime++;
				cur_tp--;
				return;
			}
		}

		int res = -1;
		char* info = new char[3];
		res = prlist[running]->run(PR::RR_TP, info);
		//prlist[running]->est--;
		switch (res) {
		case 0: { // cpu occupied
			//set_pending();
			cur_tp--;
			break;
		}
		case 1: { // ?suspended waiting
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			int device = -1;
			if (info[0] == 'i') device = 1;
			else if (info[0] == 'p') device = 0;
			else {
				Log::w("(process.cpp) scheduler: unknown device.\n");
				return;
			}
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[1]);
			args.did = device;
			idt(INTN::INT::DEVICE_REQ, &args);
			running = -1;
			schedule(clock);
			break;
		}
		case 2: { // read
			[[fallthrough]];
		}
		case 3: { // write
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[2]);
			args.did = 2;
			idt(INTN::INT::DEVICE_REQ, &args);
			running = -1;
			schedule(clock);
			break;
		}
		case 5: { // fault
			Log::i("Process %d: %s segmentation fault, killed.\n",
				running, prlist[running]->name.c_str());
			[[fallthrough]];
		}
		case 4: { // return
			kill(running);
			running = -1;
			schedule(clock);
			break;
		}
		default: {
			Log::w("(process.cpp) schedule: Unknown state.\n");
			break;
		}
		}
		delete[] info;
	}
	else if (algo == PR::Algorithm::PRIORITY) {
		if (running == -1 || running == 0) {
			if (running == 0) {
				prlist[running]->state = PR::READY;
				ready.push_back(running);
			}
			auto v = min_element(ready.begin(), ready.end(),
				[this](int a, int b)
				{ return this->prlist[a]->est < this->prlist[b]->est; });
			running = *v;
			prlist[running]->state = PR::RUNNING;
			ready.erase(v);
		}
		if (running == 0) {
			prlist[running]->cputime++;
			return;
		}

		int res = -1;
		char* info = new char[3];
		res = prlist[running]->run(0, info);
		//prlist[running]->est--;
		switch (res) {
		case 0: { // cpu occupied
			//set_pending();
			break;
		}
		case 1: { // ?suspended waiting
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			int device = -1;
			if (info[0] == 'i') device = 1;
			else if (info[0] == 'p') device = 0;
			else {
				Log::w("(process.cpp) scheduler: unknown device.\n");
				return;
			}
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[1]);
			args.did = device;
			idt(INTN::INT::DEVICE_REQ, &args);
			running = -1;
			schedule(clock);
			break;
		}
		case 2: { // read
			[[fallthrough]];
		}
		case 3: { // write
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[2]);
			args.did = 2;
			idt(INTN::INT::DEVICE_REQ, &args);
			running = -1;
			schedule(clock);
			break;
		}
		case 5: { // fault
			Log::i("Process %d: %s segmentation fault, killed.\n",
				running, prlist[running]->name.c_str());
			[[fallthrough]];
		}
		case 4: { // return
			kill(running);
			running = -1;
			schedule(clock);
			break;
		}
		default: {
			Log::w("(process.cpp) schedule: Unknown state.\n");
			break;
		}
		}
		delete[] info;
	}
	else if (algo == PR::Algorithm::MIXED_QUEUE) {
		if (cur_tp == 0) { // time up
			if (running >= 0) {
				prlist[running]->state = PR::READY;
				if (running > 0) {
					if (prlist[running]->priority <= 3) high_pr.push_back(running);
					else if (prlist[running]->priority <= 6) mid_pr.push_back(running);
					else low_pr.push_back(running);
				}
				running = -1;
			}
			cur_tp = PR::RR_TP;
		}

		if (running == -1) {
			cur_tp = PR::RR_TP;
			if (high_pr.size()) {
				running = high_pr.front();
				high_pr.pop_front();
			}
			else if (mid_pr.size()) {
				running = mid_pr.front();
				mid_pr.pop_front();
			}
			else if (low_pr.size()) {
				running = low_pr.front();
				low_pr.pop_front();
			}
			else {
				running = 0;
			}
			prlist[running]->state = PR::RUNNING;
		}

		if (running == 0) {
			prlist[running]->cputime++;
			cur_tp--;
			return;
		}

		int res = -1;
		char* info = new char[3];
		res = prlist[running]->run(PR::RR_TP, info);
		//prlist[running]->est--;
		switch (res) {
		case 0: { // cpu occupied
			//set_pending();
			cur_tp--;
			break;
		}
		case 1: { // ?suspended waiting
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			int device = -1;
			if (info[0] == 'i') device = 1;
			else if (info[0] == 'p') device = 0;
			else {
				Log::w("(process.cpp) scheduler: unknown device.\n");
				return;
			}
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[1]);
			args.did = device;
			idt(INTN::INT::DEVICE_REQ, &args);
			running = -1;
			schedule(clock);
			break;
		}
		case 2: { // read
			[[fallthrough]];
		}
		case 3: { // write
			prlist[running]->state = PR::WAITING;
			waiting.push_back(running);
			struct args {
				int pid;
				int time;
				int did;
			} args;
			args.pid = running;
			args.time = static_cast<int>(info[2]);
			args.did = 2;
			idt(INTN::INT::DEVICE_REQ, &args);
			running = -1;
			schedule(clock);
			break;
		}
		case 5: { // fault
			Log::i("Process %d: %s segmentation fault, killed.\n",
				running, prlist[running]->name.c_str());
			[[fallthrough]];
		}
		case 4: { // return
			kill(running);
			running = -1;
			schedule(clock);
			break;
		}
		default: {
			Log::w("(process.cpp) schedule: Unknown state.\n");
			break;
		}
		}
		delete[] info;
	}
	else { // should not happen
		Log::w("(process.cpp) schedule: unknown schedule mode.\n");
	}
}

void Scheduler::set_pending(int pid) {
	if (pid >= 0 && prlist[pid]) {
		prlist[pid]->iotime++;
		prlist[pid]->est--;
	}
}

void Scheduler::set_serv() {
	for (auto v : prlist) {
		if (v) {
			v->servtime++;
		}
	}
}

double Scheduler::statistic() {
	return doneprs ? turnaround / doneprs : 0;
}

void Scheduler::read_table() {
	vector<const char*> states = {
		"Newborn",
		"Ready",
		"Running",
		"Waiting",
		"Dead"
	};
	for (auto st : prlist) {
		if (st) {
			cout << setw(12) << left << st->pid;
			cout << setw(12) << left << st->name;
			cout << setw(12) << left << states[st->state];
			cout << setw(12) << left << st->parent;
			cout << setw(12) << left << st->priority;
			cout << setw(12) << left << st->cputime;
			cout << setw(12) << left << st->servtime;
			cout << setw(12) << left << st->iotime;
			cout << setw(12) << left << st->est;
			cout << setw(12) << left << st->mem->get_nmapped();
			cout << setw(12) << left << st->mem->get_nblocks();
			cout << setw(12) << left << setprecision(2) << fixed << st->mem->pf_rate();
			cout << endl;
		}
	}
}

vector<vector<string>> Scheduler::expose() {
	vector<const char*> statestring = {
		"Newborn",
		"Ready",
		"Running",
		"Waiting",
		"Dead"
	};
	vector<vector<string>> res;
	for (auto v : prlist) {
		if (v) {
			vector<string> state;
			state.push_back(to_string(v->pid));
			state.push_back(v->name);
			state.push_back(string(statestring[v->state]));
			state.push_back(to_string(v->parent));
			state.push_back(to_string(v->priority));
			state.push_back(to_string(v->cputime));
			state.push_back(to_string(v->servtime));
			state.push_back(to_string(v->iotime));
			state.push_back(to_string(v->est));
			string pfr = to_string(v->mem->pf_rate());
			state.push_back(pfr.substr(0, pfr.find(".") + 3));
			res.push_back(state);
		}
	}
	return res;
}

vector<vector<string>> Scheduler::expose_mem() {
	vector<vector<string>> res;
	for (auto v : prlist) {
		if (v) {
			vector<vector<string>> state = v->mem->mm_expose(v->pid, v->name);
			res.insert(res.end(), state.begin(), state.end());
		}
	}
	return res;
}

void Scheduler::print_mem() {
	for (auto st : prlist) {
		if (st) {
			st->mem->stat(st->pid, st->name);
		}
	}
}

uint16_t Scheduler::new_pid() {
	int i = UINT16_MAX;
	for (i = 0; i < prlist.size(); i++) {
		if (!prlist[i]) break;
	}
	return i;
}

void Scheduler::chalg(PR::Algorithm pa, MM::Algorithm ma) {
	if (pa != PR::Algorithm::NONE) this->algo = pa;
	if (ma != MM::Algorithm::NONE) {
		this->ralgo = ma;
		for (auto v : prlist) {
			if (v && v->pid > 1) {
				v->mem->chalg(ma);
			}
		}
	}
}

pair<string, string> Scheduler::alg() {
	string pa, ma;
	if (algo == PR::Algorithm::FCFS) pa = "FCFS";
	else if (algo == PR::Algorithm::SJF) pa = "SJF";
	else if (algo == PR::Algorithm::RR) pa = "RR";
	else if (algo == PR::Algorithm::PRIORITY) pa = "PR";
	else if (algo == PR::Algorithm::MIXED_QUEUE) pa = "MQ";
	else pa = "Unknown";
	if (ralgo == MM::Algorithm::FIFO) ma = "FIFO";
	else if (ralgo == MM::Algorithm::LRU) ma = "LRU";
	else ma = "Unknown";
	return { pa, ma };
}

Process::Process(string name, uint16_t state, uint16_t pid, 
	uint16_t parent, uint16_t priority, PR::Timepiece time,
	function<void(int, void*)> idt, MM::Algorithm algo)
	: idt(idt), name(name), state(state), pid(pid), parent(parent), priority(priority) {
	random_device rd;
	mt19937::result_type seed = rd() ^ (
		(mt19937::result_type)
		chrono::duration_cast<chrono::seconds>(
			chrono::system_clock::now().time_since_epoch()
			).count() +
		(mt19937::result_type)
		chrono::duration_cast<chrono::microseconds>(
			chrono::high_resolution_clock::now().time_since_epoch()
			).count());

	rd_s = mt19937(seed);
	mem = new VirtMemoryModel(idt, algo);
	sibling = 0;
	children = 0;
	cputime = 0;
	iotime = 0;
	servtime = 0;
	born = time;
	ip = 0;
	last_page = 0;
	est = -1;
	timeleft = -1;
	workload = -1;
}

Process::Process(Process* father, uint16_t _pid, PR::Timepiece time) {
	mem = new VirtMemoryModel(father->mem);
	idt = father->idt;
	random_device rd;
	mt19937::result_type seed = rd() ^ (
		(mt19937::result_type)
		chrono::duration_cast<chrono::seconds>(
			chrono::system_clock::now().time_since_epoch()
			).count() +
		(mt19937::result_type)
		chrono::duration_cast<chrono::microseconds>(
			chrono::high_resolution_clock::now().time_since_epoch()
			).count());

	rd_s = mt19937(seed);
	parent = father->pid;
	state = PR::READY;
	pid = _pid;
	sibling = 0;
	children = 0;
	priority = father->priority;
	open_files = vector<int>(father->open_files);
	cputime = 0;
	iotime = 0;
	servtime = 0;
	ip = father->ip;
	est = 0;
	workload = 0;
	timeleft = 0;
	last_page = father->last_page;
	born = time;
}

void Process::release() {
	delete mem;
	/*for (auto v : open_files) {
		idt(INTN::INT::REQ_FILE_CLOSE, &v);
	}*/
}

int Process::run(PR::Timepiece tp, char* info) {
	bool seg;
	char ins;
	if (workload > 0) {
		cputime++;
		workload--;
		est--;
		return 0;
	}
	seg = mem->access(ip++, &ins);
	int state = -1;
	switch (ins) {
	case 'c': {
		seg &= mem->access(ip++, &ins);
		workload = static_cast<uint32_t>(ins);
		workload--;
		est--;
		cputime++;
		state = 0;
		break;
	}
	case 'i': {
		seg &= mem->access(ip++, &ins);
		info[0] = 'i';
		info[1] = ins;
		mem->write(generate_random_pg() * MM::PAGE_SIZE + 1, nullptr, 0);
		state = 1;
		break;
	}
	case 'p': {
		seg &= mem->access(ip++, &ins);
		info[0] = 'p';
		info[1] = ins;
		mem->write(generate_random_pg() * MM::PAGE_SIZE + 1, nullptr, 0);
		state = 1;
		break;
	}
	case 'r': {
		info[0] = 'r';
		seg &= mem->access(ip++, &ins);
		info[1] = ins;
		seg &= mem->access(ip++, &ins);
		info[2] = ins;
		
		mem->write(generate_random_pg() * MM::PAGE_SIZE + 1, nullptr, 0);
		state = 2;
		break;
	}
	case 'w': {
		info[0] = 'w';
		seg &= mem->access(ip++, &ins);
		info[1] = ins;
		seg &= mem->access(ip++, &ins);
		info[2] = ins;
		mem->write(generate_random_pg() * MM::PAGE_SIZE + 1, nullptr, 0);
		state = 3;
		break;
	}
	case 't': {
		state = 4;
		break;
	}
	default:
		Log::w("(process.cpp) run: unknown command\n");
	}
	if (!seg) return 5;
	return state;
}

int Process::generate_random_pg() {
	uniform_int_distribution<int> meta(0, 9);
	uniform_int_distribution<int> dist_local(last_page - 1, 
		min(last_page + 1, static_cast<int>(MM::VIRT_MEM_SIZE / MM::PAGE_SIZE - 1)));
	uniform_int_distribution<int> dist_far(1, MM::VIRT_MEM_SIZE / MM::PAGE_SIZE - 1);
	int page = -1;
	int roll = meta(rd_s);
	if (roll < 5) { // 50%
		page = last_page;
	}
	else if (roll < 8) { // 30%
		page = dist_local(rd_s);
	}
	else { // 20%
		page = dist_far(rd_s);
	}
	last_page = page;
	return page;
}

Process::~Process() {
	Log::i("Process %d: %s released.\n", this->pid, this->name.c_str());
}