#include "../include/kernel.h"

Kernel::Kernel(PR::Algorithm pa, MM::Algorithm ma, uint64_t uid) : uid(uid) {
	function<void(int, void*)> idt = bind(&Kernel::int_handler, this, placeholders::_1, placeholders::_2);
	pg = new PageMemoryModel(idt,
		MM::PHYS_MEM_SIZE, MM::PAGE_SIZE);
	fs = new Filesystem(idt, uid);
	sch = new Scheduler(idt,
		pa, ma);
	if(!fs->exist("/.swap")) fs->create_swapspace("", ".swap");
	vector<string> swaps = pg->get_swaps();
	for (auto ss : swaps) {
		fs->reset_swapspace(ss);
	}
	Printer* printer = new Printer("Printer", idt);
	Keyboard* keyboard = new Keyboard("Keyboard", idt);
	Device* disk_dummy = new Device("Disk", idt);
	devices.push_back(printer);
	devices.push_back(keyboard);
	devices.push_back(disk_dummy);
	clock = 0;
	exit_kernel = false;
	mode = 1;
}

void Kernel::new_device(string name) {
	function<void(int, void*)> idt = bind(&Kernel::int_handler, this, placeholders::_1, placeholders::_2);
	Device* dev = new Device(name, idt);
	devices.push_back(dev);
}

int Kernel::del_device(string name) {
	auto v = find_if(devices.begin(), devices.end(),
		[name](Device* d) { return d->name == name; });
	if (v == devices.end()) {
		return -1;
	}
	else {
		if ((*v)->query()) {
			return -2;
		}
		else {
			devices.erase(v);
			return 0;
		}
	}
	return -3;
}

vector<pair<string, vector<pair<int, int>>>> Kernel::expose_devices() {
	vector<pair<string, vector<pair<int, int>>>> res;
	for (auto v : devices) {
		res.push_back(v->stat());
	}
	return res;
}

vector<pair<string, pair<list<int>, list<int>>>> Kernel::expose_sft() {
	vector<pair<string, pair<list<int>, list<int>>>> res;
	pair<string, pair<list<int>, list<int>>> pp;
	vector<pair<int, string>> ff = fs->expose_sft();
	vector<pair<list<int>, list<int>>> q = fs->expose_sft_queue();
	for (auto v : ff) {
		res.push_back({ v.second, q[v.first] });
	}
	return res;
}

void Kernel::run() {
	sch->fork();
	while (!exit_kernel) {
		if (mode < 0) {
			continue;
		}
		Log::d("Kernel running... time=%d\n", clock);
		for (auto v : devices) {
			sch->set_pending(v->poll());
		}
		sch->schedule(clock);
		sch->set_serv();
		if (mode == 2) {
			if (header) {
				cout << setw(12) << left << "pid";
				cout << setw(12) << left << "name";
				cout << setw(12) << left << "state";
				cout << setw(12) << left << "parent";
				cout << setw(12) << left << "priority";
				cout << setw(12) << left << "cputime";
				cout << setw(12) << left << "born";
				cout << setw(12) << left << "iotime";
				cout << setw(12) << left << "eta";
				cout << setw(12) << left << "nmapped";
				cout << setw(12) << left << "nblocks";
				cout << setw(12) << left << "pf rate(%)" << endl;
			}
			cout << setfill('_') << setw(12 * 12) << "_" 
				 << setfill(' ') << endl;
			sch->read_table();
			header = false;
		}
		if (mode == 3) {
			if (header) {
				cout << setw(6) << left << "pid";
				cout << setw(12) << left << "name";
				cout << setw(12) << left << "refed";
				cout << setw(12) << left << "present";
				cout << setw(12) << left << "time_in";
				cout << setw(12) << left << "time_ref";
				cout << setw(12) << left << "vpage";
				cout << setw(12) << left << "page" << endl;
				cout << setfill('_') << setw(12 * 8 - 6) << "_" << endl;
				cout << setfill(' ');
			}
			sch->print_mem();
			pg->stat();
			header = false;
		}
		clock++;
		this_thread::sleep_for(chrono::milliseconds(KN::millis_per_clock));
	}
	Log::i("Kernel exit.\n");
}

void Kernel::int_handler(int int_type, void* args) { // interrupt callback
	switch (int_type) {
	case INTN::INT::REQ_LOAD: {
		struct ss {
			string path;
			VirtMemoryModel* mm;
			int et;
			int pri;
			int state;
			string pwd;
		}* ss = static_cast<struct ss*>(args);
		ss->state = load_prog(ss->path, ss->mm, &ss->et, &ss->pri);
		ss->pwd = get_pwd();
		break;
	}
	case INTN::INT::REQ_MEM_ACC: {
		struct ss {
			int pg;
			char* buf;
		}*ss = static_cast<struct ss*>(args);
		pg->get(ss->pg, ss->buf);
		break;
	}
	case INTN::INT::REQ_MEM_WRITE: {
		struct ss {
			int pg;
			char* buf;
			int addr;
			int size;
		}*ss = static_cast<struct ss*>(args);
		pg->put(ss->pg, ss->buf, ss->addr, ss->size);
		break;
	}
	case INTN::INT::PAGE_FAULT: {
		struct ss {
			int pg;
			char* buf;
			int addr;
			int size;
		}*ss = static_cast<struct ss*>(args);
		//cout << "page fault" << endl;
		ss->pg = pg->alloc_page();
		pg->put(ss->pg, ss->buf, ss->addr, ss->size);
		break;
	}
	case INTN::INT::PAGE_SWAP_OUT: {
		struct ss {
			string file;
			char* buf;
			int blk;
			int state;
		}*ss = static_cast<struct ss*>(args);
		ss->state = fs->write_swapspace(ss->file, ss->buf, ss->blk);
		break;
	}
	case INTN::INT::PAGE_SWAP_IN: {
		struct ss {
			string file;
			char* buf;
			int blk;
			int state;
		}*ss = static_cast<struct ss*>(args);
		ss->state = fs->read_swapspace(ss->file, ss->buf, ss->blk);
		break;
	}
	case INTN::INT::REQ_MEM_SWAP_IN_W: {
		struct ss {
			int pg;
			char* buf;
			int addr;
			int size;
		}* ss = static_cast<struct ss*>(args);
		ss->pg = pg->pg_swap_in(ss->pg);
		pg->put(ss->pg, ss->buf, ss->addr, ss->size);
		break;
	}
	case INTN::INT::REQ_MEM_SWAP_IN_R: {
		struct ss {
			int pg;
			char* buf;
		}*ss = static_cast<struct ss*>(args);
		ss->pg = pg->pg_swap_in(ss->pg);
		pg->get(ss->pg, ss->buf);
		break;
	}
	case INTN::INT::REQ_MEM_SWAP_OUT: {
		struct ss {
			int pg;
		}*ss = static_cast<struct ss*>(args);
		pg->pg_swap_out(ss->pg);
		break;
	}
	case INTN::INT::DEVICE_REQ: {
		struct ss {
			int pid;
			int time;
			int did;
		}*ss = static_cast<struct ss*>(args);
		devices[ss->did]->require(ss->time, ss->pid);
		break;
	}
	case INTN::INT::DEVICE_DONE: {
		int pid = *static_cast<int*>(args);
		sch->wake(pid);
		break;
	}
	case INTN::INT::REQ_CLK: {
		int* arg = static_cast<int*>(args);
		*arg = clock;
		break;
	}
	case INTN::INT::RELEASE_SWAP: {
		int page = *static_cast<int*>(args);
		pg->release_swap(page);
		break;
	}
	case INTN::INT::RELEASE_PAGE: {
		int page = *static_cast<int*>(args);
		pg->free_page(page);
		break;
	}
	case INTN::INT::REQ_FILE_OPEN: {
		struct ss {
			int pid;
			string fname;
			int fid;
		} *ss = static_cast<struct ss*>(args);
		ss->fid = fs->open(ss->fname, 0, 1);
		break;
	}
	case INTN::INT::REQ_FILE_CLOSE: {
		int fid = *static_cast<int*>(args);
		fs->close(fid);
		break;
	}
	case INTN::INT::REQ_FILE_READ: {
		struct ss {
			int pid;
			int fid;
			int time;
			int state;
		} *ss = static_cast<struct ss*>(args);
		ss->state = fs->fread(ss->fid, ss->pid, ss->time);
		break;
	}
	case INTN::INT::REQ_FILE_WRITE: {
		struct ss {
			int pid;
			int fid;
			int time;
			int bytes;
			int state;
		} *ss = static_cast<struct ss*>(args);
		ss->state = fs->fwrite(ss->fid, ss->bytes, ss->pid, ss->time);
		break;
	}
	case INTN::INT::REQ_DEV_POP: {
		int pid = *static_cast<int*>(args);
		for (auto d : devices) {
			d->pop(pid);
		}
		break;
	}
	case INTN::INT::FILE_DONE: {
		struct ss {
			int pid;
			int fid;
			int rw;
			int size;
		} *ss = static_cast<struct ss*>(args);
		fs->fpop(ss->pid, ss->fid, ss->rw, ss->size);
		break;
	}
	case INTN::INT::FILE_WAKE: {
		int pid = *static_cast<int*>(args);
		sch->wake(pid);
	}
	}
}

int Kernel::load_prog(string path, VirtMemoryModel* mm, int* et, int* pri) {
	*et = 0;
	*pri = -1;
	char* buf = new char[15 * FS::BLK_SIZE];
	char* buf_r = new char[MM::PAGE_SIZE];
	char* buf_code = buf_r + 240;
	char* p = buf_code;
	char* sp = buf_r;
	int si = 0;
	*sp++ = static_cast<char>(240);
	fs->read(path, buf, 0, -1);
	string code(buf);
	trim(code);
	vector<string> lines;
	auto pos = code.find('\n');
	while (pos != string::npos) {
		lines.push_back(code.substr(0, pos));
		code = code.substr(pos + 1);
		pos = code.find('\n');
	}
	lines.push_back(code);
	for (auto l : lines) {
		trim(l);
		if (!l.size() || l[0] == ';') continue;
		pos = l.find(" ");
		if (pos == string::npos) {
			if (l == "ret") {
				*p++ = 't';
				continue;
			}
			else {
				delete[] buf;
				return 0;
			}
		}
		string cmd = l.substr(0, pos);
		string args = l.substr(pos + 1);
		if (cmd == "cal") {
			int arg = stoi(args);
			*et += arg;
			*p++ = 'c';
			*p++ = static_cast<char>(arg);
		}
		else if (cmd == "input") {
			int arg = stoi(args);
			*et += arg;
			*p++ = 'i';
			*p++ = static_cast<char>(arg);
		}
		else if (cmd == "print") {
			int arg = stoi(args);
			*et += arg;
			*p++ = 'p';
			*p++ = static_cast<char>(arg);
		}
		else if (cmd == "mem") {
			int arg = stoi(args);
			mm->set_blocks(arg);
		}
		else if (cmd == "pri") {
			int arg = stoi(args);
			*pri = arg;
		}
		else if (cmd == "read") {
			pos = args.find(" ");
			if (pos == string::npos) {
				delete[] buf;
				return 0;
			}
			string f = args.substr(0, pos);
			strcpy(sp, f.c_str());
			sp += (f.size() + 1);
			int arg = stoi(args.substr(pos + 1));
			*et += arg;
			*p++ = 'r';
			*p++ = static_cast<char>(si++);
			*p++ = static_cast<char>(arg);
		}
		else if (cmd == "write") {
			pos = args.find(" ");
			if (pos == string::npos) {
				delete[] buf;
				return 0;
			}
			string f = args.substr(0, pos);
			strcpy(sp, f.c_str());
			sp += (f.size() + 1);
			string as = args.substr(pos + 1);
			pos = args.find(" ");
			if (pos == string::npos) {
				delete[] buf;
				return 0;
			}
			int arg1 = stoi(as.substr(0, pos));
			int arg2 = stoi(as.substr(pos + 1));
			*et += arg1;
			*p++ = 'w';
			*p++ = static_cast<char>(si++);
			*p++ = static_cast<char>(arg1);
			*p++ = static_cast<char>(arg2);
		}
		else {
			delete[] buf;
			return 0;
		}
	}
	mm->load(buf_r, static_cast<int>(p - buf_r));
	/*auto pp = buf_r;
	cout << "Compiled:" << endl;
	while (pp != p) {
		if (32 <= *pp && *pp <= 126)
			cout << *pp;
		else cout << '_';
		pp++;
	}
	cout << endl;*/
	delete[] buf_r;
	delete[] buf;
	return 1;
}

string Kernel::get_pwd() {
	return fs->get_pwd();
}

int Kernel::get_clock() {
	return clock;
}

void Kernel::set_mode(int mode) {
	this->mode = mode;
	header = true;
}

void Kernel::pause() {
	mode = -mode;
}

void Kernel::exit() {
	exit_kernel = true;
}

vector<vector<string>> Kernel::expose_pr() {
	return sch->expose();
}

vector<vector<string>> Kernel::mem_pr() {
	return sch->expose_mem();
}

vector<int> Kernel::mem_map() {
	return pg->expose_mem_map();
}

vector<int> Kernel::swap_map() {
	return pg->expose_swap_map();
}

void Kernel::chalg(PR::Algorithm pa, MM::Algorithm ma) {
	sch->chalg(pa, ma);
}

pair<string, string> Kernel::alg() {
	return sch->alg();
}

double Kernel::statistic() {
	return sch->statistic();
}

Kernel::~Kernel() {
	delete pg;
	delete fs;
	delete sch;
}