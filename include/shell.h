#pragma once
#include "gui.h"
#include "editor.h"

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

//inline string acl_str(int acl) {
//	string res(6, '-');
//	char perms[6] = { 'r', 'w', 'x', 'r', 'w', 'x' };
//	for (int i = 0; i < 6; i++) {
//		int mask = 1 << (5 - i);
//		int bit = acl & mask;
//		if (bit) {
//			res[i] = perms[i];
//		}
//	}
//	return res;
//}

namespace SH {
	bool determine_completeness(string command);
}

class Shell_CLI {
private:
	Kernel* kernel;
	Term::Terminal* term;
	string pwd_str;
	string user;
	bool exit;
	int mode;
public:
	Shell_CLI(Kernel* kernel, Term::Terminal* term, string uname);
	~Shell_CLI();
	void show();
	void pwd();
	void ls();
	void ls(string path, int r, int a, int l);
	void touch(string name);
	void mkswap(string name);
	void mkdir(string name);
	void rm(string name);
	void cd(string path);
	void cat(string name);
	void ps();
	void mem();
	void edit(Term::Terminal term, string name);
	void exec(string path);
	void now();
	void ps_snapshot();
	void mem_snapshot();
	void show_window();
	void chalg(string p1, string p2);
};