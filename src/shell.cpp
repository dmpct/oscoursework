#include "../include/shell.h"

namespace SH {
	bool determine_completeness(string command) {
		bool complete;
		if (command.size() < 3) return true;
		if (command.substr(command.size() - 2, 1) == "\\") {
			complete = false;
		}
		else {
			complete = true;
		}
		return complete;
	}
}

void Shell_CLI::show() {
	vector<string> history;
	function<bool(string)> cfn = [](string) { return true; };
	while (!exit) {
		if (mode == 2 || mode == 3) {
			if (term->read_key() == Term::Key::CTRL + 'x') {
				mode = 0;
				kernel->set_mode(0);
			}
			else continue;
		}
		string answer = Term::prompt(*term, 
			user + "@" + pwd_str + "> ",
			history, cfn);
		trim(answer);
		vector<string> cmds;
		string cmd = answer;
		auto pos = answer.find("&&");
		while (pos != string::npos) {
			cmd = answer.substr(0, pos);
			trim(cmd);
			answer = answer.substr(pos + 2);
			trim(answer);
			cmds.push_back(cmd);
			pos = answer.find("&&");
		}
		trim(answer);
		cmds.push_back(answer);
		for (auto line : cmds) {
			pos = line.find(" ");
			cmd = line.substr(0, pos);
			if (!cmd.size()) continue;
			if (cmd == "pwd") {
				this->pwd();
			}
			else if (cmd == "ls") {
				if (pos == string::npos) {
					this->ls();
				}
				else {
					bool valid = true;
					string args = line.substr(pos + 1);
					int r = 0;
					int l = 0;
					int a = 0;
					string path = pwd_str;
					while (pos != string::npos) {
						pos = args.find(" ");
						string par = args.substr(0, pos);
						if (pos != string::npos) args = args.substr(pos + 1);
						if (par[0] == '-') {
							if (par[1] == 'r') {
								r = 1;
							}
							else if (par[1] == 'l') {
								l = 1;
							}
							else if (par[1] == 'a') {
								a = 1;
							}
							else {
								cout << "Unknown argument: " << par << endl;
								valid = false;
								break;
							}
						}
						else {
							path = par;
							if (kernel->fs->exist(path) != 2) {
								cout << path << ": invalid dir." << endl;
								valid = false;
							}
						}
					}
					if (valid) this->ls(path, r, a, l);
				}
			}
			else if (cmd == "touch") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
				}
				else {
					string path = line.substr(pos + 1);
					this->touch(path);
				}
			}
			else if (cmd == "mkswap") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
				}
				else {
					string path = line.substr(pos + 1);
					this->mkswap(path);
				}
			}
			else if (cmd == "mkdir") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
				}
				else {
					string path = line.substr(pos + 1);
					this->mkdir(path);
				}
			}
			else if (cmd == "rm") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
				}
				else {
					string path = line.substr(pos + 1);
					if (kernel->fs->exist(path) != 1)
						cout << path << ": no such file." << endl;
					else this->rm(path);
				}
			}
			else if (cmd == "cd") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
				}
				else {
					string path = line.substr(pos + 1);
					if (path.size() > 1 && path[path.size() - 1] == '/')
						path = path.substr(0, path.size() - 1);
					if (path != ".." && path != "." &&
						kernel->fs->exist(path) != 2)
						cout << path << ": invalid dir." << endl;
					else this->cd(path);
				}
			}
			else if (cmd == "cat") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
				}
				else {
					string path = line.substr(pos + 1);
					if (kernel->fs->exist(path) != 1)
						cout << path << ": no such file." << endl;
					else this->cat(path);
				}
			}
			else if (cmd == "edit") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
				}
				else {
					string path = line.substr(pos + 1);
					if (kernel->fs->exist(path) != 1)
						cout << path << ": no such file." << endl;
					else this->edit(*term, path);
				}
			}
			else if (cmd == "mem") {
				this->mem();
			}
			else if (cmd == "set") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
				}
				else {
					string mode = line.substr(pos + 1);
					if (mode == "ps") {
						this->ps_snapshot();
					}
					else if (mode == "mem") {
						this->mem_snapshot();
					}
					else if (mode == "g") {
						this->show_window();
					}
					else cout << mode << ": unknown mode." << endl;
				}
			}
			else if (cmd == "exit") {
				kernel->exit();
				exit = true;
			}
			else if (cmd == "ps") {
				this->now();
				this->ps();
			}
			else if (cmd == "pause") {
				cout << "Kernel paused. Type \"pause\" again to resume." << endl;
				kernel->pause();
			}
			else if (cmd == "exec") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
				}
				else {
					string path;
					vector<string> fs;
					while (pos != string::npos) {
						line = line.substr(pos + 1);
						pos = line.find(" ");
						path = line.substr(0, pos);
						fs.push_back(path);
					}
					for (auto v : fs) {
						if (kernel->fs->exist(v) != 1)
							cout << v << ": no such file." << endl;
						else this->exec(v);
					}
				}
			}
			else if (cmd == "now") {
				this->now();
			}
			else if (cmd == "kill") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
					break;
				}
				string args = line.substr(pos + 1);
				trim(args);
				try {
					int pid = stoi(args);
					if (pid > 1) {
						kernel->sch->safe_kill(pid);
					}
					else {
						throw ;
					}
				}
				catch (...) {
					cout << "Invalid PID " << args << endl;
				}
			}
			else if (cmd == "alg") {
				pair<string, string> as = kernel->alg();
				cout << "Process Schedule Mode: " << as.first << endl;
				cout << "Memory Replacement Mode: " << as.second << endl;
			}
			else if (cmd == "stat") {
				cout << "System Average Turnaround Time=" 
					<< setprecision(2) << fixed << kernel->statistic() 
					<< endl;
				cout << "System CPU Usage="
					<< setprecision(2) << fixed << kernel->sch->cpu_rate()
					<< "%" << endl;
				cout << "System Throughtput="
					<< setprecision(2) << fixed << kernel->sch->cpu_rate()
					<< "/60Ticks" << endl;
			}
			else if (cmd == "chmod") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
					break;
				}
				string args = line.substr(pos + 1);
				pos = args.find(" ");
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
					break;
				}
				string path = args.substr(0, pos);
				string mode = args.substr(pos + 1);
				trim(mode);
				trim(args);
				kernel->fs->chmod(path, stoi(mode));
			}
			else if (cmd == "mount") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
					break;
				}
				string args = line.substr(pos + 1);
				trim(args);
				kernel->new_device(args);
			}
			else if (cmd == "unmount") {
				if (pos == string::npos) {
					cout << cmd << ": not enough argument." << endl;
					break;
				}
				string args = line.substr(pos + 1);
				trim(args);
				int ret = kernel->del_device(args);
				if (ret == -1) {
					cout << "Unknown device name: " << args << endl;
				}
				else if (ret == -2) {
					cout << "Device occupied. Release first." << endl;
				}
				else {
					// pass
				}
			}
			else if (cmd == "chalg") {
				bool ok = true;
				string pa{ "" }, ma{ "" };
				string args = line.substr(pos + 1);
				string arg;
				trim(args);
				auto pos = args.find(" ");
				bool set_p = false;
				bool set_m = false;
				bool p_done = false;
				bool m_done = false;
				while (pos != string::npos) {
					arg = args.substr(0, pos);
					trim(arg);
					if (arg[0] == '-') {
						if (arg[1] == 'p') {
							if (!set_p) {
								set_p = true;
								args = args.substr(pos + 1);
							}
							else {
								cout << "Redundant argument: " << arg << endl;
								ok = false;
								break;
							}
						}
						else if (arg[1] == 'm') {
							if (!set_m) {
								set_m = true;
								args = args.substr(pos + 1);
							}
							else {
								cout << "Redundant argument: " << arg << endl;
								ok = false;
								break;
							}
						}
						else {
							cout << "Invalid argument: " << arg << endl;
							ok = false;
							break;
						}
					}
					else {
						if (set_p && !p_done) {
							args = args.substr(pos + 1);
							pa = arg;
							p_done = true;
						}
						else if (set_m && !m_done) {
							args = args.substr(pos + 1);
							ma = arg;
							m_done = true;
						}
						else {
							cout << "Invalid argument: " << arg << endl;
							ok = false;
							break;
						}
					}
				}
				if (ok) this->chalg(pa, ma);
			}
			else {
				cout << cmd << ": unknown command." << endl;
			}
		}
		
	}
}

Shell_CLI::Shell_CLI(Kernel* kernel, Term::Terminal* term, string username) 
	: kernel(kernel), term(term), user(username) {
	pwd_str = kernel->get_pwd();
	exit = false;
	mode = 1;
}

Shell_CLI::~Shell_CLI() {
	delete kernel;
}

void Shell_CLI::pwd() {
	cout << pwd_str << endl;
}
void Shell_CLI::ls() {
	this->ls(pwd_str, 0, 0, 0);
}
void Shell_CLI::ls(string path, int r, int a, int l) {
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* dir = new struct FS::Dir[8];
	int idx = kernel->fs->walk(path, inode, dir);
	int cnt = 1;
	for (int i = 0; i < 8; i++) {
		if (dir[i].type == FS::File_t::None) continue;
		string name = dir[i].entry_name;
		if (r) name = path[path.size() - 1] == '/' ?
			path + name : path + "/" + name;
		string desc;
		if (l) {
			if (a && ((name.rfind("/") == string::npos && name[0] == '.')
				|| (name.rfind("/") != string::npos && name.substr(name.rfind("/") + 1)[0] == '.'))) {
				continue;
			}
			FS::read_inode(inode, dir[i].inode);
			if (dir[i].type == FS::File_t::Dir) {
				desc = Term::color(Term::fg::white) + "<DIR>";
				name = Term::color(Term::fg::bright_green)
					+ Term::color(Term::style::bold) + name;
			}
			if (dir[i].type == FS::File_t::File) {
				desc = Term::color(Term::fg::white) 
					+ to_string(inode->i_size) + " Bytes"
					+ Term::color(Term::fg::white)
					+ acl_str(inode->i_acl);
				name = Term::color(Term::fg::bright_white)
					+ Term::color(Term::style::reset) + name;
				
			}
			cout << setw(18) << left << desc << name << endl;
			cout << Term::color(Term::style::reset);
		}
		else {
			if (a && ((name.rfind("/") == string::npos && name[0] == '.')
				|| (name.rfind("/") != string::npos && name.substr(name.rfind("/") + 1)[0] == '.'))) {
				continue;
			}
			if (dir[i].type == FS::File_t::Dir) {
				name = Term::color(Term::fg::bright_green)
					+ Term::color(Term::style::bold) +name;
			}
			if (dir[i].type == FS::File_t::File) {
				name = Term::color(Term::fg::bright_white)
					+ Term::color(Term::style::reset) + name;
			}
			cout << name << endl;
		}
	}
	cout << Term::color(Term::bg::reset)
		  + Term::color(Term::fg::reset)
		  + Term::color(Term::style::reset);
	if (r) {
		for (int i = 0; i < 8; i++) {
			if (dir[i].type == FS::File_t::None
				|| dir[i].type == FS::File_t::File
				|| strcmp(dir[i].entry_name, ".") == 0
				|| strcmp(dir[i].entry_name, "..") == 0) continue;
			string name = dir[i].entry_name;
			string npath = path[path.size() - 1] == '/' ?
				path + name : path + "/" + name;
			ls(npath, r, a, l);
		}
	}
	delete inode;
	delete[] dir;
}

void Shell_CLI::touch(string name) {
	auto pos = name.rfind("/");
	string path, fname;
	if(pos == string::npos) {
		path = pwd_str;
		fname = name;
	}
	else {
		path = name.substr(0, name.rfind("/"));
		fname = name.substr(name.rfind("/") + 1);
	}
	kernel->fs->create(path, fname, FS::File_t::File);
}
void Shell_CLI::mkswap(string name) {
	auto pos = name.rfind("/");
	string path, fname;
	if (pos == string::npos) {
		path = pwd_str;
		fname = name;
	}
	else {
		path = name.substr(0, name.rfind("/") - 1);
		fname = name.substr(name.rfind("/") + 1);
	}
	trim(path);
	trim(fname);
	kernel->pg->new_swap(path + "/" + name);
	kernel->fs->create_swapspace(path + "/", name);
}
void Shell_CLI::mkdir(string name) {
	kernel->fs->create(pwd_str, name, FS::File_t::Dir);
}
void Shell_CLI::rm(string name) {
	kernel->fs->fdelete(name);
}
void Shell_CLI::cd(string path) {
	kernel->fs->set_pwd(path);
	pwd_str = kernel->fs->get_pwd();
}
void Shell_CLI::cat(string name) {
	char* buf = new char[FS::MAX_N_BLKS * FS::BLK_SIZE];
	memset(buf, 0, FS::MAX_N_BLKS * FS::BLK_SIZE);
	kernel->fs->read(name, buf, 0, -1);
	string content(buf);
	cout << content << endl;
	delete[] buf;
}
void Shell_CLI::edit(Term::Terminal term, string name) {
	std::function<void(std::string, char*)> ofn =
		bind(&Filesystem::read, kernel->fs, placeholders::_1, placeholders::_2,
			0, -1);
	std::function<void(std::string, char*, size_t)> sfn =
		bind(&Filesystem::write, kernel->fs, placeholders::_1, placeholders::_2,
			0, placeholders::_3);
	Editor::editor(term, name, ofn, sfn);
}
void Shell_CLI::ps() {
	cout << Term::color(Term::fg::bright_white) + Term::color(Term::style::bold);
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
	cout << setfill('_') << setw(12 * 12) << "_" << endl;
	cout << setfill(' ') << Term::color(Term::fg::reset) + Term::color(Term::style::reset);
	kernel->sch->read_table();
}
void Shell_CLI::mem() {
	cout << Term::color(Term::fg::bright_white) + Term::color(Term::style::bold);
	cout << setw(6) << left << "pid";
	cout << setw(12) << left << "name";
	cout << setw(12) << left << "refed";
	cout << setw(12) << left << "present";
	cout << setw(12) << left << "time_in";
	cout << setw(12) << left << "time_ref";
	cout << setw(12) << left << "vpage";
	cout << setw(12) << left << "page" << endl;
	cout << setfill('_') << setw(12 * 8 - 6) << "_" << endl;
	cout << setfill(' ') << Term::color(Term::fg::reset) + Term::color(Term::style::reset);
	kernel->sch->print_mem();
	kernel->pg->stat();
}
void Shell_CLI::exec(string path) {
	if(path.size() > 2 && path.substr(path.size() - 2) == ".p")
		kernel->sch->exec(path, kernel->sch->fork(1));
	else {
		cout << "Not a executable file." << endl;
	}
}
void Shell_CLI::now() {
	cout << kernel->get_clock() << endl;
}
void Shell_CLI::ps_snapshot() {
	mode = 2;
	cout << "Will continuously print snapshots. Press Ctrl+X to exit." << endl;
	kernel->set_mode(2);
}

void Shell_CLI::mem_snapshot() {
	mode = 3;
	cout << "Will continuously print snapshots. Press Ctrl+X to exit." << endl;
	kernel->set_mode(3);
}
void Shell_CLI::show_window() {
	thread gui(main_window, kernel);
	gui.detach();
}

void Shell_CLI::chalg(string p1, string p2) {
	PR::Algorithm pa = PR::Algorithm::NONE;
	MM::Algorithm ma = MM::Algorithm::NONE;
	bool ok = true;
	if (p1 == "FCFS") {
		pa = PR::Algorithm::FCFS;
	}
	else if (p1 == "SJF") {
		pa = PR::Algorithm::SJF;
	}
	else if (p1 == "PR") {
		pa = PR::Algorithm::PRIORITY;
	}
	else if (p1 == "RR") {
		pa = PR::Algorithm::RR;
	}
	else if (p1 == "MQ") {
		pa == PR::Algorithm::MIXED_QUEUE;
	}
	else if (p1 == "") {
		pa == PR::Algorithm::NONE;
	}
	else {
		cout << "Unknown Process Schedule Mode: " << p1 << endl;
		ok = false;
	}
	if (p2 == "FIFO") {
		ma = MM::Algorithm::FIFO;
	}
	else if (p2 == "LRU") {
		ma = MM::Algorithm::LRU;
	}
	else if (p2 == "") {
		ma = MM::Algorithm::NONE;
	}
	else {
		cout << "Unknown Memory Schedule Mode: " << p2 << endl;
		ok = false;
	}
	if (ok) kernel->chalg(pa, ma);
}