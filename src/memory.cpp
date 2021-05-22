#include "../include/memory.h"

PhysMemoryModel::PhysMemoryModel(int phys_mem_size) :
	phys_mem_size(phys_mem_size) {
	memory = new char[phys_mem_size];
}

PhysMemoryModel::~PhysMemoryModel() {
	delete[] memory;
}

bool PhysMemoryModel::load(char* buf, MM::phys_addr from, uint32_t size) {
	lock_guard<mutex> guard(mem_lock);
	if (size == 0) return true;
	if (from + size > phys_mem_size) {
		Log::w("(memory.cpp) load: out of bound.\n");
		return false;
	}
	memcpy(memory + from, buf, size);
	return true;
}

bool PhysMemoryModel::dump(char* buf, MM::phys_addr from, uint32_t size) {
	lock_guard<mutex> guard(mem_lock);
	if (size == 0) return true;
	if (from + size > phys_mem_size) {
		Log::w("(memory.cpp) dump: out of bound.\n");
		return false;
	}
	memcpy(buf, memory + from, size);
	return true;
}

int PhysMemoryModel::swap_out(MM::phys_addr from, function<void(int, void*)> idt, int blk) {
	struct {
		string file;
		char* buf;
		int blk;
		int state;
	} args;
	args.file = swapspace[(int)blk / 15];
	args.buf = memory + from;
	args.blk = blk % 15;
	args.state = -1;
	idt(INTN::INT::PAGE_SWAP_OUT, &args);
	return args.state;
}

int PhysMemoryModel::swap_in(MM::phys_addr from, function<void(int, void*)> idt, int blk) {
	struct {
		string file;
		char* buf;
		int blk;
		int state;
	} args;
	args.file = swapspace[(int)blk / 15];
	args.buf = memory + from;
	args.blk = blk % 15;
	args.state = -1;
	idt(INTN::INT::PAGE_SWAP_IN, &args);
	return args.state;
}

bool PageMemoryModel::pg_swap_out(int pg) {
	//cout << "swap out: " << pg << endl;
	//if (pgtable[pg]->locked) return false;
	/*if (freepgs.size() >= (pgtable.size() / 4)) {
		pgtable[pg]->refed = 1;
		pgtable[pg]->counter = 0;
		return true;
	}*/
	int fs = -1;
	for (int i = 0; i < swapbitmap.size(); i++) {
		//cout << "in s " << i << endl;
		if (swapbitmap[i] == 0) {
			fs = i;
			swapbitmap[i] = 1; 
			break;
		}
	}
	//cout << "swapped to block: " << fs << endl;
	if (fs == -1) {
		Log::w("(memory.cpp) pg_swap_out: out of swapspace.\n");
		return false;
	}
	struct Swap_info* ss = new struct Swap_info;
	ss->page = pg;
	ss->blk = fs;
	ss->desc = pgtable[pg];
	pgtable[pg] = new struct Page;
	freepgs.push_back(pg);
	swaptable.push_back(ss);
	int res = swap_out(pg * MM::PAGE_SIZE, idt, fs);
	return res == 0;
}
int PageMemoryModel::pg_swap_in(int pg) {
	//cout << "swap in: " << pg << endl;
	if (pgtable[pg]->refed && pgtable[pg]->counter == 0) {
		pgtable[pg]->counter++;
		return pg;
	}
	auto v = swaptable.begin();
	int blk = -1;
	for (; v != swaptable.end(); v++) {
		if ((*v)->page == pg) {
			blk = (*v)->blk; break;
		}
	}
	struct Page* p = new struct Page;
	if (blk != -1) {
		p = (*v)->desc;
		swaptable.erase(v);
		swapbitmap[blk] = 0;
	}
	else {
		Log::w("(memory.cpp) pg_swap_in: page loss.\n");
		delete p;
		return false;
	}
	int new_page = alloc_page();
	delete pgtable[new_page];
	pgtable[new_page] = p;
	swap_in(new_page * MM::PAGE_SIZE, idt, blk);
	return new_page;
}

PageMemoryModel::PageMemoryModel(function<void(int, void*)> idt, int phys_mem_size, int pg_size)
	: PhysMemoryModel(phys_mem_size), pg_size(pg_size), idt(idt) {
	npgs = static_cast<int>(floor(phys_mem_size / pg_size));
	pgtable.resize(npgs);
	for (auto i = pgtable.begin(); i != pgtable.end(); i++) {
		struct Page* p = new struct Page;
		*i = p;
	}
	for (int i = 0; i < pgtable.size(); i++) {
		freepgs.push_back(i);
	}
	swapbitmap.resize(15);
	for (auto v = swapbitmap.begin(); v != swapbitmap.end(); v++) {
		*v = 0;
	}
}
PageMemoryModel::~PageMemoryModel() {
	for (auto v : pgtable) {
		if(v) delete v;
	}
}

void PageMemoryModel::release_swap(int pg) {
	if (pgtable[pg]->refed && pgtable[pg]->counter == 0) {
		free_page(pg);
	}
	auto v = swaptable.begin();
	for (; v != swaptable.end(); v++) {
		if ((*v)->page == pg) {
			swapbitmap[(*v)->blk] = 0; break;
			delete (*v);
		}
	}
	swaptable.erase(v);
}

int PageMemoryModel::alloc_page() {
	lock_guard<mutex> guard(pg_lock);
	if (freepgs.empty()) {
		int spg = -1;
		for (int i = 0; i < pgtable.size(); i++) {
			auto pg = pgtable[i];
			if (pg->refed && !pg->counter) {
				spg = i;
				break;
			}
		}
		if (spg != -1) {
			pg_swap_out(spg);
		}
		else {
			//process swapout
			return -1;
		}
	}
	int pg = freepgs.front();
	freepgs.pop_front();
	pgtable[pg]->counter++;
	return pg;
}

int PageMemoryModel::free_page(int pg) {
	lock_guard<mutex> guard(pg_lock);
	if (pgtable[pg]->locked) {
		return 1;
	}
	pgtable[pg]->refed = 0;
	pgtable[pg]->counter = 0;
	if (pgtable[pg]->dirty) {
		// writeback????
	}
	freepgs.push_back(pg);
	return 0;
}

void PageMemoryModel::get(int pg, char* buf) {
	dump(buf, pg * MM::PAGE_SIZE, MM::PAGE_SIZE);
}

void PageMemoryModel::stat() {
	cout << setfill('_') << setw(12 * 8 - 6) << "_" << endl << setfill(' ');
	for (int i = 0; i < npgs; i++) {
		auto pg = pgtable[i];
		cout << setw(6) << left << i;
		cout << setw(6) << left << pg->counter;
		if ((i + 1) % 8 == 0) cout << endl;
	}
	cout << endl << setfill('_') << setw(12 * 8 - 6) << "_" << endl << setfill(' ');
	cout << "Swap:" << endl;
	for (auto a : swaptable) {
		cout << a->page << "->" << a->blk << endl;
	}
	cout << setfill('_') << setw(12 * 8 - 6) << "_" << endl << setfill(' ');
}

bool PageMemoryModel::put(int pg, char* buf, int offset, int size) {
	lock_guard<mutex> guard(pg_lock);
	//cout << pg << endl;
	//Log::i("*** visited pg = %d\n", pg);
	if (pg == -1) return false;
	if (pgtable[pg]->locked) {
		return false;
	}
	load(buf, pg * MM::PAGE_SIZE + offset, size);
	pgtable[pg]->dirty = 1;
	return true;
}


vector<int> PageMemoryModel::expose_mem_map() {
	vector<int> res;
	for (auto v : pgtable) {
		if (v) {
			res.push_back(v->counter);
		}
		else {
			res.push_back(0);
		}
	}
	return res;
}

vector<int> PageMemoryModel::expose_swap_map() {
	vector<int> res(swapbitmap.size(), -1);
	for (auto v: swaptable) {
		res[v->blk] = v->page;
	}
	return res;
}

VirtMemoryModel::VirtMemoryModel(function<void(int, void*)> idt, MM::Algorithm algo) 
	: idt(idt), algo(algo) {
	pgtable.resize(MM::VIRT_MEM_SIZE / MM::PAGE_SIZE);
	for (auto i = pgtable.begin(); i != pgtable.end(); i++) {
		struct VPage* p = new struct VPage;
		*i = p;
	}
	nmapped = 0;
	nblocks = 0;
	acc_cnt = 0;
	repl_cnt = 0;
}

VirtMemoryModel::VirtMemoryModel(VirtMemoryModel* v)
{
	pgtable.resize(MM::VIRT_MEM_SIZE / MM::PAGE_SIZE);
	for (int i = 0; i < pgtable.size(); i++) {
		struct VPage* p = new struct VPage(v->pgtable[i]);
		pgtable[i] = p;
	}
	nmapped = v->nmapped;
	set_blocks(v->nblocks);
	idt = v->idt;
	algo = v->algo;
	acc_cnt = 0;
	repl_cnt = 0;
}

VirtMemoryModel::~VirtMemoryModel() {
	for (auto v : pgtable) {
		if (v && v->refed) {
			if (!v->present)
				idt(INTN::INT::RELEASE_SWAP, &v->addr);
			else
				idt(INTN::INT::RELEASE_PAGE, &v->addr);
		}
		if (v) delete v;
	}
}

bool VirtMemoryModel::access_page(int pg, char* buf) {
	if (!pgtable[pg]->refed) { // seg fault;
		return false;
	}
	int clk = -1;
	idt(INTN::INT::REQ_CLK, &clk);
	if (pgtable[pg]->present) { // in frame, in memory
		struct args {
			int pg;
			char* buf;
		} args;
		args.pg = pgtable[pg]->addr;
		args.buf = buf;
		idt(INTN::INT::REQ_MEM_ACC, &args);
		pgtable[pg]->t_ref = clk;
		return true;
	}
	else { // not in frame
		struct args {
			int pg;
			char* buf;
		} args;
		args.pg = pgtable[pg]->addr;
		args.buf = buf;
		idt(INTN::INT::REQ_MEM_SWAP_IN_R, &args);
		pgtable[pg]->addr = args.pg;
		pgtable[pg]->t_in = clk;
		pgtable[pg]->t_ref = clk;
		int f = alloc_frame();
		frame[f] = pg;
		return true;
	}
	return false;
}

bool VirtMemoryModel::access(MM::virt_addr addr, char* buf) {
	if (addr > MM::VIRT_MEM_SIZE) {
		// segmentation fault
		return false;
	}
	auto offset = addr % MM::PAGE_SIZE;
	auto pg = addr / MM::PAGE_SIZE;
	//cout << "mem read: " << pg << endl;
	char* vpbuf = new char[MM::PAGE_SIZE];
	bool nfault = access_page(pg, vpbuf);
	if (nfault) {
		memcpy(buf, vpbuf + offset, 1);
	}
	delete[] vpbuf;
	if(pg > 0) acc_cnt++;
	return nfault;
}

bool VirtMemoryModel::view(MM::virt_addr from, MM::virt_addr to, char* buf) {
	if (max(from, to) > MM::VIRT_MEM_SIZE) {
		// segmentation fault
		return false;
	}
	auto pgfrom = from / MM::PAGE_SIZE;
	auto pgto = to / MM::PAGE_SIZE;
	if (pgfrom != pgto) {
		return false;
	}
	auto offset = from % MM::PAGE_SIZE;
	char* vpbuf = new char[MM::PAGE_SIZE];
	bool nfault = access_page(pgfrom, vpbuf);
	if (nfault) {
		memcpy(buf, vpbuf + offset, to - from);
	}
	delete[] vpbuf;
	return nfault;
}

void VirtMemoryModel::set_blocks(int blks) {
	nblocks = blks;
	frame.resize(blks);
	for (int i = 0; i < blks; i++) {
		frame[i] = -1;
	}
}

void VirtMemoryModel::replace() {
	if (algo == MM::Algorithm::FIFO) {
		int fout = -1;
		int mtime = INT_MAX;
		for (int i = 0; i < nblocks; i++) {
			if (frame[i] && pgtable[frame[i]]->t_in < mtime) {
				fout = i;
				mtime = pgtable[frame[i]]->t_in;
			}
		}
		pgtable[frame[fout]]->present = 0;
		struct args {
			int pg;
		} args;
		args.pg = pgtable[frame[fout]]->addr;
		idt(INTN::INT::REQ_MEM_SWAP_OUT, &args);
		frame[fout] = -1;
		nmapped--;
	}
	else if (algo == MM::Algorithm::LRU) {
		int fout = -1;
		int mtime = INT_MAX;
		for (int i = 0; i < nblocks; i++) {
			if (frame[i] && pgtable[frame[i]]->t_ref < mtime) {
				fout = i;
				mtime = pgtable[frame[i]]->t_ref;
			}
		}
		pgtable[frame[fout]]->present = 0;
		struct args {
			int pg;
		} args;
		args.pg = pgtable[frame[fout]]->addr;
		idt(INTN::INT::REQ_MEM_SWAP_OUT, &args);
		frame[fout] = -1;
		nmapped--;
	}
	else {
		// should not happen
	}
}

int VirtMemoryModel::alloc_frame() {
	if (nmapped == nblocks) replace();
	for (int i = 0; i < frame.size(); i++) {
		if (frame[i] == -1) {
			repl_cnt++;
			nmapped++;
			return i;
		}
	}
	return -1;
}

bool VirtMemoryModel::write_page(int pg, char* buf, MM::log_addr addr, int size) {
	int clk = -1;
	idt(INTN::INT::REQ_CLK, &clk);
	if (!pgtable[pg]->refed) { // not in frame, not in memory, not in swap
		struct args {
			int pg;
			char* buf;
			int addr;
			int size;
		} args;
		args.pg = pgtable[pg]->addr;
		args.buf = buf;
		args.addr = addr;
		args.size = size;
		idt(INTN::INT::PAGE_FAULT, &args);
		pgtable[pg]->refed = 1;
		pgtable[pg]->addr = args.pg;
		pgtable[pg]->dirty = 1;
		pgtable[pg]->present = 1;
		pgtable[pg]->t_in = clk;
		pgtable[pg]->t_ref = clk;
		int f = alloc_frame();
		frame[f] = pg;
		return true;
	}
	if (pgtable[pg]->present) { // in frame and in memory
		struct args {
			int pg;
			char* buf;
			int addr;
			int size;
		} args;
		args.pg = pgtable[pg]->addr;
		args.buf = buf;
		args.addr = addr;
		args.size = size;
		pgtable[pg]->dirty = 1;
		pgtable[pg]->t_ref = clk;
		idt(INTN::INT::REQ_MEM_WRITE, &args);
		return true;
	}
	else { // not in frame, could be in memory or in swap
		struct args {
			int pg;
			char* buf;
			int addr;
			int size;
		} args;
		args.pg = pgtable[pg]->addr;
		args.buf = buf;
		args.addr = addr;
		args.size = size;
		idt(INTN::INT::REQ_MEM_SWAP_IN_W, &args);
		pgtable[pg]->addr = args.pg;
		pgtable[pg]->present = 1;
		pgtable[pg]->t_in = clk;
		pgtable[pg]->t_ref = clk;
		int f = alloc_frame();
		frame[f] = pg;
		return true;
	}
	return false;
}

bool VirtMemoryModel::write(MM::virt_addr addr, char* buf, int size) {
	if (addr > MM::VIRT_MEM_SIZE) {
		// segmentation fault
		return false;
	}
	bool nfault = true;
	auto offset = addr % MM::PAGE_SIZE;
	auto pg = addr / MM::PAGE_SIZE;
	while (offset + size > MM::PAGE_SIZE) {
		nfault &= write_page(pg, buf, offset, MM::PAGE_SIZE - offset);
		offset = 0;
		size -= (MM::PAGE_SIZE - offset);
		pg++;
	}
	nfault &= write_page(pg, buf, offset, size);
	acc_cnt++;
	return nfault;
}

void VirtMemoryModel::load(char* buf, int size) {
	write(0, buf, size);
}

void VirtMemoryModel::stat(int pid, string name) {
	for (int i = 0; i < nblocks; i++) {
		cout << setw(6) << left << pid;
		cout << setw(12) << left << name;
		if (frame[i] != -1) {
			auto entry = pgtable[frame[i]];
			cout << setw(12) << left << entry->refed;
			cout << setw(12) << left << entry->present;
			cout << setw(12) << left << entry->t_in;
			cout << setw(12) << left << entry->t_ref;
			cout << setw(12) << left << frame[i];
			cout << setw(12) << left << entry->addr << endl;
		}
		else {
			cout << setw(12 * 6) << left << "empty frame" << endl;
		}
	}
}

vector<vector<string>> VirtMemoryModel::mm_expose(int pid, string name) {
	vector<vector<string>> res;
	for (int i = 0; i < nblocks; i++) {
		vector<string> state;
		state.push_back(to_string(pid));
		state.push_back(name);
		if (frame[i] != -1) {
			auto entry = pgtable[frame[i]];
			state.push_back(to_string(entry->refed));
			state.push_back(to_string(entry->present));
			state.push_back(to_string(entry->t_in));
			state.push_back(to_string(entry->t_ref));
			state.push_back(to_string(frame[i]));
			state.push_back(to_string(entry->addr));
		}
		else {
			state.push_back(string("empty frame"));
		}
		res.push_back(state);
	}
	return res;
}

void VirtMemoryModel::chalg(MM::Algorithm newalg) {
	this->algo = newalg;
}

double VirtMemoryModel::pf_rate() {
	return acc_cnt ? (repl_cnt * 100) / acc_cnt : 0;
}