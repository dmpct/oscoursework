#pragma once
#include "log.h"

// 2^24 = 16M

namespace MM {
	constexpr uint32_t PHYS_MEM_SIZE = 1 << 14; // 16K
	constexpr uint32_t PAGE_SIZE = 1 << 10; // 1K 
	constexpr uint32_t VIRT_MEM_SIZE = 1 << 15; // 32K
	// npgs = 16
	//constexpr uint32_t PHYS_MEM_KERNEL = 1 << 20; // 1MB
	using phys_addr = uint32_t;
	using virt_addr = uint32_t;
	using log_addr = uint32_t;

	struct VMA {
		uint32_t start; // 4B
		uint32_t end; // 4B
		uint32_t offset; // 4B
		uint32_t perm; // 4B
		struct File* file; // 8B
		struct VMA* next; // 8B
	};

	struct Mem_desc {
		uint32_t size;
		struct VMA* map;
		struct VPage_desc* pgt_beg;
	};

	enum class Algorithm {
		LRU = 0,
		FIFO,
		NONE
	};
	enum class FreeStatus {
		Success = 0,
		Failed,
		Writeback
	};
}

struct Page {
	int locked;
	int refed;
	int dirty;
	int counter;
	//atomic_int32_t mapcounter;
	//struct list_head lru;
	Page() {
		locked = 0;
		refed = 0;
		dirty = 0;
		counter = 0;
	}
	Page(struct Page* p) {
		locked = p->locked;
		refed = p->refed;
		dirty = p->dirty;
		counter = p->counter;
	}
};

struct VPage {
	int present;
	int rw;
	//int us;
	int dirty;
	int refed;
	int t_in;
	int t_ref;
	uint32_t addr;
	VPage() {
		present = 0;
		rw = 0;
		//us = 0;
		dirty = 0;
		refed = 0;
		addr = -1;
		t_in = -1;
		t_ref = -1;
	}
	VPage(struct VPage* v) {
		present = v->present;
		rw = v->rw;
		//us = v->us;
		dirty = v->dirty;
		refed = v->refed;
		addr = v->addr;
		t_in = v->t_in;
		t_ref = v->t_ref;
	}
};

struct Swap_info {
	int page;
	int blk;
	struct Page* desc;
};

class PhysMemoryModel {
private:
	char* memory;
	mutex mem_lock;
	vector<string> swapspace = {"/.swap"};

protected:
	int phys_mem_size;
	virtual bool load(char* buf, MM::phys_addr from, uint32_t size) final;
	virtual bool dump(char* buf, MM::phys_addr from, uint32_t size) final;
	virtual int swap_out(MM::phys_addr from, function<void(int, void*)> idt, int blk) final;
	virtual int swap_in(MM::phys_addr from, function<void(int, void*)> idt, int blk) final;
	
public:
	vector<string> get_swaps() {
		return swapspace;
	}
	void reg_swap(string path) {
		swapspace.push_back(path);
	}
	PhysMemoryModel(int phys_mem_size);
	~PhysMemoryModel();
};

class PageMemoryModel : public PhysMemoryModel {
private:
	int pg_size;
	int npgs;
	function<void(int, void*)> idt;
	list<struct Swap_info*> swaptable;
	vector<int> swapbitmap;
	mutex pg_lock;

	vector<struct Page*> pgtable;
	list<int> freepgs;

public:
	PageMemoryModel(function<void(int, void*)> idt, int phys_mem_size, int pg_size);
	~PageMemoryModel();
	int alloc_page();
	int free_page(int pg);
	void get(int pg, char* buf);
	bool put(int pg, char* buf, int offset, int size);
	bool pg_swap_out(int pg);
	int pg_swap_in(int pg);
	void release_swap(int pg);
	void stat();
	void new_swap(string path) {
		reg_swap(path);
		swapbitmap.resize(swapbitmap.size() + 15);
	}
	vector<int> expose_mem_map();
	vector<int> expose_swap_map();
};

class VirtMemoryModel {
private:
	vector<struct VPage*> pgtable;
	vector<int> frame;
	int nmapped;
	int nblocks;
	function<void(int, void*)> idt;
	MM::Algorithm algo;
	int acc_cnt;
	int repl_cnt;

public:
	VirtMemoryModel(function<void(int, void*)> idt, MM::Algorithm algo);
	VirtMemoryModel(VirtMemoryModel* v);
	~VirtMemoryModel();
	void replace();
	int alloc_frame();
	bool access_page(int pg, char* buf);
	bool access(MM::virt_addr addr, char* buf);
	int get_nmapped() { return nmapped; }
	int get_nblocks() { return nblocks; }
	void set_blocks(int blks);
	bool write_page(int pg, char* buf, MM::log_addr addr, int size);
	bool write(MM::virt_addr addr, char* buf, int size);
	void load(char* buf, int size);
	void stat(int pid, string name);
	vector<vector<string>> mm_expose(int pid, string name);
	void chalg(MM::Algorithm newalg);
	double pf_rate();
};