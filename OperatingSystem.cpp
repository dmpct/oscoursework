#include "include/shell.h"

struct user_entry {
	string uname;
	size_t pass;
	PR::Algorithm pa;
	MM::Algorithm ma;
};

void ask_for_options(Term::Terminal* term, 
	vector<string> &dummy, function<bool(string)> dummy2,
	PR::Algorithm &pralg, MM::Algorithm &mmalg) {
	bool set1 = false;
	bool set2 = false;
	while (!set1) {
		string answer = Term::prompt(*term,
			"Process schedule mode(FCFS, SJF, RR, PR, MQ): ",
			dummy, dummy2);
		trim(answer);
		if (answer == "FCFS") {
			pralg = PR::Algorithm::FCFS;
			set1 = true;
		}
		else if (answer == "SJF") {
			pralg = PR::Algorithm::SJF;
			set1 = true;
		}
		else if (answer == "RR") {
			pralg = PR::Algorithm::RR;
			set1 = true;
		}
		else if (answer == "PR") {
			pralg = PR::Algorithm::PRIORITY;
			set1 = true;
		}
		else if (answer == "MQ") {
			pralg = PR::Algorithm::MIXED_QUEUE;
			set1 = true;
		}
		else {
			cout << answer << ": Unknown algorithm." << endl;
		}
	}

	while (!set2) {
		string answer = Term::prompt(*term,
			"Page replacement mode(FIFO, LRU): ",
			dummy, dummy2);
		trim(answer);
		if (answer == "FIFO") {
			mmalg = MM::Algorithm::FIFO;
			set2 = true;
		}
		else if (answer == "LRU") {
			mmalg = MM::Algorithm::LRU;
			set2 = true;
		}
		else {
			cout << answer << ": Unknown algorithm." << endl;
		}
	}
}

int main() {
	Term::Terminal* term = new Term::Terminal(true, true);
	vector<string> dummy;
	function<bool(string)> dummy2 = [](string) { return true; };
	PR::Algorithm pralg = PR::Algorithm::FCFS;
	MM::Algorithm mmalg = MM::Algorithm::FIFO;
	bool logged = false;
	bool new_user = false;
	bool load = false;
	string uname, pass;

	ifstream info("info.bin", ios::binary);

	if (!info.good()) {
		info.close();
		uname = "admin";
		cout << "No user exist, login as admin..." << endl;
		string apass = Term::prompt(*term,
			"Set admin password: ",
			dummy, dummy2);
		struct user_entry admin;
		admin.uname = "admin";
		admin.pass = hash<string>{}(apass);
		cout << "Created admin user." << endl;
		ask_for_options(term, dummy, dummy2, pralg, mmalg);
		admin.pa = pralg;
		admin.ma = mmalg;
		ofstream info("info.bin", ios::binary);
		info.write(reinterpret_cast<char*>(&admin), sizeof(struct user_entry));
		info.close();
	}
	else {
		vector<struct user_entry> uv;
		auto vp = uv.end();

		while (!info.eof()) {
			struct user_entry us;
			info.read(reinterpret_cast<char*>(&us), sizeof(struct user_entry));
			uv.push_back(us);
		}
		
		while (!logged) {
			uname = Term::prompt(*term,
				"Register/Login as: ",
				dummy, dummy2);
			trim(uname);

			pass = Term::prompt(*term,
				"Password: ",
				dummy, dummy2);

			vp = find_if(uv.begin(), uv.end(),
				[uname](struct user_entry a) { return a.uname == uname; });

			if (vp == uv.end()) {
				cout << "Created new user " << uname << "." << endl;
				logged = true;
				new_user = true;
			}
			else {
				auto ph = hash<string>{}(pass);
				if (ph == (*vp).pass) {
					cout << "Logged in as " << uname << "." << endl;
					logged = true;
					new_user = false;
				}
				else {
					cout << "Wrong password for " << uname << "." << endl;
				}
			}
		}
		if (new_user) {
			struct user_entry nu;
			nu.uname = uname;
			nu.pass = hash<string>{}(pass);
			ask_for_options(term, dummy, dummy2, pralg, mmalg);
			nu.pa = pralg;
			nu.ma = mmalg;
			uv.push_back(nu);
		}
		else {
			while (!load) {
				string answer = Term::prompt(*term,
					"Load last configuration?[y/n] ",
					dummy, dummy2);
				trim(answer);
				if (answer == "y" || answer == "Y") {
					pralg = (*vp).pa;
					mmalg = (*vp).ma;
					load = true;
				}
				else if (answer == "n" || answer == "N") {
					ask_for_options(term, dummy, dummy2, pralg, mmalg);
					(*vp).pa = pralg;
					(*vp).ma = mmalg;
					load = true;
				}
				else {
					cout << "(Specify with \"y/Y\" or \"n/N\".)" << endl;
				}
			}
		}
		
		info.close();
		ofstream info("info.bin", ios::binary);
		for (auto v : uv) {
			info.write(reinterpret_cast<char*>(&v), sizeof(struct user_entry));
		}
		info.close();
	}
	cout << "Initializing...";
	Kernel* kernel = new Kernel(pralg, mmalg);
	Shell_CLI shell(kernel, term, uname);
	cout << "Done." << endl;
	cout << Term::clear_screen() << Term::move_cursor(0, 0);
	thread sh(&Shell_CLI::show, &shell);
	thread kn(&Kernel::run, kernel);

	sh.join();
	kn.join();

	return 0;
}