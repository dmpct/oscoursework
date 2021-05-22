#include "../include/filesystem.h"

namespace FS {
	bool format_disk() {
		auto disk = fstream(DEVICE, ios::out | ios::trunc | ios::binary);
		auto total_size = (3
			+ N_INODEBLKS
			+ N_DATABLKS)
			* BLK_SIZE;
		char* buf = new char[total_size];
		if (disk.is_open() && buf) {
			memset(buf, 0, total_size);
			disk.write(buf, total_size);
			delete[] buf;
			disk.close();
			return true;
		}
		if (!disk) Log::w("(filesystem.cpp) format_disk: failed to open device.\n");
		if (!buf) Log::w("(filesystem.cpp) format_disk: failed to allocate buffer.\n");
		return false;
	}

	bool write_block(char* buf, uint32_t blk, uint32_t offset, uint32_t size) {
		if ((blk >= 3 + N_DATABLKS + N_INODEBLKS) || ((offset + size) > BLK_SIZE)) {
			Log::w("(filesystem.cpp) write_block: out of bound.\n");
			return false;
		}
		auto disk = fstream(DEVICE, ios::in | ios::out | ios::binary);
		if (!disk.is_open()) {
			Log::w("(filesystem.cpp) write_block: failed to open device.\n");
			return false;
		}
		disk.seekp(blk * BLK_SIZE + offset, ios::beg);
		disk.write(buf, size);
		disk.close();
		return true;
	}

	bool read_block(char* buf, uint32_t blk, uint32_t offset, uint32_t size) {
		if ((blk >= 3 + N_DATABLKS + N_INODEBLKS) || ((offset + size) > BLK_SIZE)) {
			Log::w("(filesystem.cpp) read_block: out of bound.\n");
			return false;
		}
		auto disk = fstream(DEVICE, ios::in | ios::binary);
		if (!disk.is_open()) {
			Log::w("(filesystem.cpp) read_block: failed to open device.\n");
			return false;
		}
		disk.seekg(blk * BLK_SIZE + offset, ios::beg);
		disk.read(buf, size);
		disk.close();
		return true;
	}

	bool write_inode(struct Inode* inode, int index) {
		if (index < 0 || index >= N_INODES) {
			Log::w("(filesystem.cpp) write_inode: out of bound.\n");
			return false;
		}
		uint32_t blk = (uint32_t)floor(index / (BLK_SIZE / 128)) + 1;
		uint32_t offset = index % (BLK_SIZE / 128);
		return write_block(reinterpret_cast<char*>(inode), 3 + blk, offset * 128, 128);
	}

	bool read_inode(struct Inode* inode, int index) {
		if (index < 0 || index >= N_INODES) {
			Log::w("(filesystem.cpp) read_inode: out of bound.\n");
			return false;
		}
		uint32_t blk = (uint32_t)floor(index / (BLK_SIZE / 128)) + 1;
		uint32_t offset = index % (BLK_SIZE / 128);
		return read_block(reinterpret_cast<char*>(inode), 3 + blk, offset * 128, 128);
	}

	bool set_bitmap(char* bitmap, int index, int val) {
		if (index < 0 || index >= 8 * BLK_SIZE) {
			Log::w("(filesystem.cpp) set_bitmap: out of bound.\n");
			return false;
		}
		int p = (int)floor(index / 8);
		int shift = (7 - index % 8);
		if (val) bitmap[p] |= static_cast<char>(1) << shift;
		else bitmap[p] &= ~(static_cast<char>(1) << shift);
		return true;
	}

	bool makefs() {
		// init superblock
		time_t now = time(nullptr);
		struct Superblock* sb = new struct Superblock;
		if (sb) {
			sb->nblocks = N_DATABLKS;
			sb->ninodes = N_INODES;
			sb->nfreeblks = N_DATABLKS;
			sb->nfreeinodes = N_INODES;
			sb->block_size = BLK_SIZE;
			sb->max_blocks = 3
				+ N_INODES / (BLK_SIZE / sizeof(struct Inode))
				+ N_DATABLKS;
			sb->m_time = (uint32_t)now;
			sb->w_time = (uint32_t)now;
		}
		else {
			Log::w("(filesystem.cpp) makefs: failed to allocate superblock.\n");
			return false;
		}
		// init bitmaps
		char* d_bitmap = new char[BLK_SIZE];
		char* i_bitmap = new char[BLK_SIZE];
		if (!(d_bitmap && i_bitmap)) {
			Log::w("(filesystem.cpp) makefs: failed to allocate bitmap.\n");
			return false;
		}
		memset(d_bitmap, 0, BLK_SIZE);
		memset(i_bitmap, 0, BLK_SIZE);
		// init inodes
		struct Inode* inodes = new struct Inode[N_INODES];
		if (!inodes) {
			Log::w("(filesystem.cpp) makefs: failed to allocate inodes.\n");
			return false;
		}
		memset(inodes, 0, N_INODES * sizeof(struct Inode));
		// create /root
		struct Inode* root = &inodes[0];
		root->i_mode = File_t::Dir;
		root->i_size = 2 * sizeof(struct Dir);
		root->i_nlinks = 2;
		root->i_nblocks = 1;
		root->i_acl = ALL_OWNER | ALL_OTHER;
		root->i_blockaddr[0] = 0;

		struct Dir* root_dir = new struct Dir[2];
		if (!root_dir) {
			Log::w("(filesystem.cpp) makefs: failed to allocate root.\n");
			return false;
		}
		struct Dir* d = &root_dir[0];
		struct Dir* dd = &root_dir[1];
		strcpy(d->entry_name, ".");
		d->inode = 0;
		d->type = File_t::Dir;
		d->next_entry = sizeof(struct Dir);
		strcpy(dd->entry_name, "..");
		dd->inode = 0;
		dd->type = File_t::Dir;
		dd->next_entry = 0;
		set_bitmap(i_bitmap, 0, 1);
		set_bitmap(d_bitmap, 0, 1);
		sb->nfreeblks--;
		sb->nfreeinodes--;
		// write back
		write_block(reinterpret_cast<char*>(sb), 0, 0, sizeof(struct Superblock));
		write_block(d_bitmap, 1, 0, BLK_SIZE);
		write_block(i_bitmap, 2, 0, BLK_SIZE);
		
		//for (int i = 0; i < N_INODES; i++) { // too slow!!!
		//	write_inode(&inodes[i], i);
		//}
		auto disk = fstream(DEVICE, ios::in | ios::out | ios::binary);
		disk.seekp(3 * FS::BLK_SIZE, ios::beg);
		disk.write(reinterpret_cast<char*>(inodes), sizeof(struct FS::Inode) * FS::N_INODES);
		disk.close();

		write_block(reinterpret_cast<char*>(root_dir), 3 + N_INODEBLKS, 0,
			sizeof(struct Dir) * 2);

		delete sb;
		delete[] d_bitmap; 
		delete[] i_bitmap;
		delete[] inodes;
		delete[] root_dir;

		return true;
	}

	bool init() {
		ifstream f(DEVICE);
		if (!f.good()) {
			format_disk();
			makefs();
		}
		return true;
	}

	void _perform_test() {
		struct Superblock* sb = new struct Superblock;
		read_block(reinterpret_cast<char*>(sb), 0, 0, sizeof(struct Superblock));
		cout << "Superblock info:" << endl;
		cout << "    nblocks=" << sb->nblocks << endl;
		cout << "    ninodes=" << sb->ninodes << endl;
		cout << "    nfreeblks=" << sb->nfreeblks << endl;
		cout << "    nfreeinodes=" << sb->nfreeinodes << endl;
		cout << "    block_size=" << sb->block_size << endl;
		cout << "    max_blocks=" << sb->max_blocks << endl;
		cout << "    m_time=" << sb->m_time << endl;
		cout << "    w_time=" << sb->w_time << endl;
		delete sb;

		char* bm = new char[BLK_SIZE];
		
		read_block(bm, 1, 0, BLK_SIZE);
		cout << "Data bitmap(first4):" << endl;
		for (int j = 0; j < 4; j++) {
			bitset<8> bits(bm[j]);
			cout << bits << endl;
		}
		cout << endl;
		read_block(bm, 2, 0, BLK_SIZE);
		cout << "Inode bitmap(first4):" << endl;
		for (int j = 0; j < 4; j++) {
			bitset<8> bits(bm[j]);
			cout << bits << endl;
		}
		cout << endl;
		delete[] bm;

		struct Inode* root = new struct Inode;
		read_inode(root, 0);
		cout << "Root inode info:" << endl;
		cout << "    i_mode=" << root->i_mode << endl;
		cout << "    i_uid=" << root->i_uid << endl;
		cout << "    i_size=" << root->i_size << endl;
		cout << "    i_nlinks=" << root->i_nlinks << endl;
		cout << "    i_nblocks=" << root->i_nblocks << endl;
		cout << "    i_blockaddr[0]=" << root->i_blockaddr[0] << endl;
		cout << "    i_acl=" << root->i_acl << endl;
		struct Dir* dir = new struct Dir[8];
		read_block(reinterpret_cast<char*>(dir), 3 + N_INODEBLKS + root->i_blockaddr[0], 0, BLK_SIZE);
		cout << "Root dir info:" << endl;
		struct Dir* p = &dir[0];
		while (1) {
			cout << "  /" << p->entry_name << " " << p->inode
				<< " " << p->type << " " << p->next_entry << endl;
			if (!p->next_entry) break;
			p += (p->next_entry / sizeof(struct Dir));
		}
		delete root;
		delete[] dir;
	}
}

Filesystem::Filesystem(function<void(int, void*)> idt, uint64_t uid) : idt(idt), uid(uid) {
	//lock_guard<mutex> guard(file_lock);
	FS::init();
	sb = new struct FS::Superblock;
	FS::read_block(reinterpret_cast<char*>(sb), 0, 0, sizeof(struct FS::Superblock));
	imap = new char[FS::BLK_SIZE];
	dmap = new char[FS::BLK_SIZE];
	FS::read_block(dmap, 1, 0, FS::BLK_SIZE);
	FS::read_block(imap, 2, 0, FS::BLK_SIZE);
	pwd = "/";
	pwd_inode = new struct FS::Inode;
	c_dir = new struct FS::Dir[8];
	FS::read_inode(pwd_inode, 0);
	int blk = pwd_inode->i_blockaddr[0];
	FS::read_block(reinterpret_cast<char*>(c_dir), 3 + FS::N_INODEBLKS + blk, 0, FS::BLK_SIZE);
	file_table.resize(0);
	filequeue.resize(0);
}

Filesystem::~Filesystem() {
	//lock_guard<mutex> guard(file_lock);
	for (auto v : file_table) {
		if (v) delete v;
	}
	FS::write_block(reinterpret_cast<char*>(sb), 0, 0, sizeof(struct FS::Superblock));
	FS::write_block(dmap, 1, 0, FS::BLK_SIZE);
	FS::write_block(imap, 2, 0, FS::BLK_SIZE);
	delete sb; delete pwd_inode;
	delete[] c_dir;
	delete[] imap; delete[] dmap;
}

int Filesystem::walk(string path, struct FS::Inode* inode, struct FS::Dir* dir) {
	//lock_guard<mutex> guard(file_lock);
	//cout << "walk get: " << path << endl;
	if (!path.size()) return -1; // should not happen
	if (path.size() > 1 && path[path.size() - 1] == '/')
		path = path.substr(0, path.size() - 1); // normalize
	int index = 0;
	size_t pos = 0;
	string name;
	FS::read_inode(inode, index);
	int blk = inode->i_blockaddr[0];
	FS::read_block(reinterpret_cast<char*>(dir), 3 + FS::N_INODEBLKS + blk, 0, FS::BLK_SIZE);
	if(path[0] == '.') {
		if(path.size() == 1) { // .
			path = pwd;
		}
		else if(path == "..") { // ..
			path = pwd.substr(0, pwd.rfind("/"));
			if (!path.size()) path = "/";
		}
		else if(path[1] == '.' && path[2] == '/') { // ../xx
			if(pwd == "/") path = path.substr(2);
			else path = pwd.substr(0, pwd.rfind("/")) + path.substr(2);
		}
		else if(path[1] == '/') { // ./xx
			if(pwd == "/") path = path.substr(1);
			else path = pwd + path.substr(1);
		}
		else { // .a/xx, .../xx, etc.
			// pass
		}
	}
	if(path == "/") {
		return index;
	}
	else if (path[0] != '/') {
		if(pwd[pwd.size() - 1] == '/')
			path = pwd + path;
		else
			path = pwd + "/" + path;
	}
	path = path.substr(1);
	while (1) {
		pos = path.find('/');
		name = path.substr(0, pos);
		path = pos == string::npos ? "" : path.substr(pos + 1);
		int i = 0;
		for ( ; i < 8; i++) {
			if(dir[i].type) {
				if (name == dir[i].entry_name) {
					index = dir[i].inode;
					break;
				}
			}
		}
		//cout << "here" << endl;
		if(i == 8) {
			Log::w("(filesystem.cpp) walk: file not found 1.\n");
			return -1;
		}
		//cout << "where" << endl;
		FS::read_inode(inode, index);
		if (inode->i_mode == FS::File_t::Dir) {
			int blk = inode->i_blockaddr[0];
			FS::read_block(reinterpret_cast<char*>(dir), 3 + FS::N_INODEBLKS + blk, 0, FS::BLK_SIZE);
		}
		else if (path.size() && (inode->i_mode == FS::File_t::File)) {
			Log::w("(filesystem.cpp) walk: file not found 2.\n");
			delete inode;
			delete[] dir;
			return -1;
		}
		if (!path.size()) {
			break;
		}
	}
	
	return index;
}

int Filesystem::fread(int fid, int pid, int time) {
	int state = 1;
	if (file_table[fid]->rw != 2) { // free
		file_table[fid]->rw = 1;
		state = 0;
	}
	filequeue[fid].first.push_back(pid);
	return state;
}

int Filesystem::fwrite(int fid, int size, int pid, int time) {
	int state = 1;
	if (file_table[fid]->rw == 0) { // free
		file_table[fid]->rw = 2;
		//char* dummy = new char[size];
		//write(file_table[fid]->fname, dummy, file_table[fid]->offset, size);
		state = 0;
	}
	filequeue[fid].second.push_back(pid);
	return state;
}

void Filesystem::fpop(int pid, int fid, int rw, int size) {
	if (rw == 1) {
		filequeue[fid].first.remove(pid);
		if (!filequeue[fid].first.size()) {
			file_table[fid]->rw = 0;
			if (filequeue[fid].second.size()) {
				idt(INTN::INT::FILE_WAKE, &filequeue[fid].second.front());
			}
		}
	}
	else if (rw == 2) {
		filequeue[fid].second.remove(pid);
		/*char* dummy = new char[size];
		write(file_table[fid]->fname, dummy, file_table[fid]->offset, size);
		file_table[fid]->offset += size;*/
		if (!filequeue[fid].second.size()) {
			file_table[fid]->rw = 0;
			for (auto v : filequeue[fid].first) {
				idt(INTN::INT::FILE_WAKE, &v);
			}
		}
	}
}

int Filesystem::open(string path, int rw, int truncate) {
	
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(path, inode, d);
	if (index == -1 || inode->i_mode != FS::File_t::File) {
		Log::w("(filesystem.cpp) open: file not found.\n");
		delete inode;
		delete[] d;
		return -1;
	}
	if (uid == inode->i_uid) {
		if (!(inode->i_acl & FS::RD_OWNER)) {
			Log::w("(filesystem.cpp) open: permission denied.\n");
			delete inode; delete[] d;
			return -1;
		}
	}
	else if(uid) {
		if (!(inode->i_acl & FS::RD_OTHER)) {
			Log::w("(filesystem.cpp) open: permission denied.\n");
			delete inode; delete[] d;
			return -1;
		}
	}
	else {
		//admin
	}
	for (int i = 0; i < file_table.size(); i++) {
		if (file_table[i]->inode == index) {
			file_table[i]->counter++;
			return i;
		}
	}
	if (truncate) {
		inode->i_size = 0;
		FS::write_inode(inode, index);
	}
	delete inode;
	delete[] d;
	struct FS::File* f = new struct FS::File;
	f->fname = path;
	f->counter = 1;
	f->offset = 0;
	f->rw = 0;
	f->inode = index;
	file_table.push_back(f);
	filequeue.resize(max(filequeue.size(), file_table.size()));
	return file_table.size() - 1;
}

void Filesystem::close(int fd) {
	file_table[fd]->counter--;
	/*if (!file_table[fd]->counter < 0) {
		auto f = file_table.begin();
		for (int i = 0; i < fd; i++, f++);
		file_table.erase(f);
	}*/
}

void Filesystem::reset_swapspace(string path) {
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(path, inode, d);
	if (index == -1) {
		Log::w("(filesystem.cpp) reset_swapspace: swap file does not exist.\n");
		delete inode;
		delete[] d;
		return;
	}
	if (inode->i_mode == FS::File_t::Dir) {
		Log::w("(filesystem.cpp) reset_swapspace: invalid file.\n");
		delete inode;
		delete[] d;
		return;
	}
	inode->i_size = 0;
	FS::write_inode(inode, index);
}

bool Filesystem::create_swapspace(string path, string fname) {
	string full_path = path + "/" + fname;
	if (!path.size()) path = "/";
	if (fname.find('/') != string::npos) {
		Log::w("(filesystem.cpp) create_swapspace: '/' not allowed in file name.\n");
		return false;
	}
	if (fname.size() > FS::MAX_NAME_LEN) {
		Log::w("(filesystem.cpp) create_swapspace: file name length exceeded.\n");
		return false;
	}
	if (sb->nfreeblks < FS::MAX_N_BLKS) {
		Log::w("(filesystem.cpp) create_swapspace: not enough disk space.\n");
		return false;
	}
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(full_path, inode, d);
	if (index != -1) {
		Log::w("(filesystem.cpp) create_swapspace: swap file exists.\n");
		/*inode->i_size = 0;
		FS::write_inode(inode, index);*/
		delete inode;
		delete[] d;
		return false;
	}
	index = walk(path, inode, d);

	if (inode->i_mode == FS::File_t::File) {
		Log::w("(filesystem.cpp) create_swapspace: cannot create under file.\n");
		delete inode;
		delete[] d;
		return false;
	}
	
	struct FS::Inode* new_inode = new struct FS::Inode;
	int in = 0;
	int db = 0;
	for (; in < FS::BLK_SIZE; in++) if (!imap[in]) {
		imap[in] = 1; break;
	}
	new_inode->i_mode = FS::File_t::File;
	new_inode->i_nblocks = FS::MAX_N_BLKS;
	new_inode->i_nlinks = 1;
	new_inode->i_size = 0;
	int nblks = 0;
	for (; db < FS::BLK_SIZE && nblks < FS::MAX_N_BLKS; db++) {
		if (!dmap[db]) {
			dmap[db] = 1;
			new_inode->i_blockaddr[nblks++] = db;
			sb->nfreeblks--;
		}
	}
	sb->nfreeinodes--;

	int i = 0;
	for (i = 0; i < 8; i++) {
		if (!d[i].type) {
			strcpy(d[i].entry_name, fname.c_str());
			if (i > 0) {
				if (i != 7) {
					d[i].next_entry = d[i - 1].next_entry;
				}
				else {
					d[i].next_entry = 0;
				}
				d[i - 1].next_entry = sizeof(struct FS::Dir);
			}
			else {
				d[i].next_entry = sizeof(struct FS::Dir);
			}
			d[i].type = FS::File_t::File;
			d[i].inode = in;
			break;
		}
	}
	if (i == 8) {
		Log::w("(filesystem.cpp) create: dir full.\n");
		delete new_inode;
		return false;
	}
	inode->i_size += sizeof(struct FS::Dir);
	FS::write_inode(inode, index);
	FS::write_inode(new_inode, in);
	FS::write_block(reinterpret_cast<char*>(d), 3 + FS::N_INODEBLKS + inode->i_blockaddr[0], 0, FS::BLK_SIZE);
	delete new_inode;
	delete inode;
	delete[] d;
	set_pwd(pwd);
	return true;
}

bool Filesystem::create(string path, string fname, FS::File_t type) {
	//lock_guard<mutex> guard(file_lock);
	if (!path.size()) path = pwd;
	if (fname.size() == 0 || fname.size() > FS::MAX_NAME_LEN) {
		Log::w("(filesystem.cpp) create: invalid file name length.\n");
		return false;
	}
	if(fname.find('/') != string::npos) {
		Log::w("(filesystem.cpp) create: '/' not allowed in file name.\n");
		return false;
	}
	if (path.size() > 1 && path[path.size() - 1] == '/') 
		path = path.substr(0, path.size() - 1);
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(path + fname, inode, d);
	if (index != -1) {
		Log::w("(filesystem.cpp) create: file exist.\n");
		delete inode;
		delete[] d;
		return false;
	}
	index = walk(path, inode, d);

	if (inode->i_mode == FS::File_t::File) {
		Log::w("(filesystem.cpp) create: cannot create under file.\n");
		delete inode;
		delete[] d;
		return false;
	}

	struct FS::Inode* new_inode = new struct FS::Inode;
	int in = 0;
	int db = 0;
	for (; in < FS::BLK_SIZE; in++) {
		if (!imap[in]) {
			imap[in] = 1; break;
		}
	}
	for (; db < FS::BLK_SIZE; db++) {
		if (!dmap[db]) {
			dmap[db] = 1; break;
		}
	}
	//cout << in << " " << db << endl;
	new_inode->i_mode = type;
	new_inode->i_nblocks = 1;
	new_inode->i_nlinks = 1;
	new_inode->i_size = 0;
	new_inode->i_blockaddr[0] = db;
	new_inode->i_uid = uid;
	if (fname.size() > 2 && fname.substr(fname.size() - 2, fname.size()) == ".p") {
		new_inode->i_acl = FS::ALL_OWNER | FS::ALL_OTHER;
	}
	else {
		new_inode->i_acl = FS::RW_OWNER | FS::RW_OTHER;
	}
	sb->nfreeblks--;
	sb->nfreeinodes--;

	int i = 0;
	for (i = 0; i < 8; i++) {
		if (!d[i].type) {
			strcpy(d[i].entry_name, fname.c_str());
			if (i > 0) {
				if (i != 7) {
					d[i].next_entry = d[i - 1].next_entry;
				}
				else {
					d[i].next_entry = 0;
				}
				d[i - 1].next_entry = sizeof(struct FS::Dir);
			}
			else {
				d[i].next_entry = sizeof(struct FS::Dir);
			}
			d[i].type = type;
			d[i].inode = in;
			break;
		}
	}
	if (i == 8) {
		Log::w("(filesystem.cpp) create: dir full.\n");
		delete new_inode;
		delete inode;
		delete[] d;
		return false;
	}
	if (type == FS::File_t::Dir) {
		new_inode->i_nlinks += 2;
		new_inode->i_size = sizeof(struct FS::Dir) * 2;
		struct FS::Dir* root_dir = new struct FS::Dir[2];
		struct FS::Dir* d = &root_dir[0];
		struct FS::Dir* dd = &root_dir[1];
		strcpy(d->entry_name, ".");
		d->inode = in;
		d->type = FS::File_t::Dir;
		d->next_entry = sizeof(struct FS::Dir);
		strcpy(dd->entry_name, "..");
		dd->inode = index;
		dd->type = FS::File_t::Dir;
		dd->next_entry = 0;
		FS::write_block(reinterpret_cast<char*>(root_dir), 3 + FS::N_INODEBLKS + db, 0, 
			sizeof(struct FS::Dir) * 2);
		delete[] root_dir;	
	}
	inode->i_size += sizeof(struct FS::Dir);
	FS::write_inode(inode, index);
	FS::write_inode(new_inode, in);
	FS::write_block(reinterpret_cast<char*>(d), 3 + FS::N_INODEBLKS + inode->i_blockaddr[0], 0, FS::BLK_SIZE);
	delete new_inode;
	delete inode;
	delete[] d;
	set_pwd(pwd);
	return true;
}
bool Filesystem::fdelete(string path) {
	//lock_guard<mutex> guard(file_lock);
	if (path == pwd || path == ".") {
		Log::w("(filesystem.cpp) fdelete: cannot delete pwd.\n");
		return false;
	}
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(path, inode, d);
	if (index == -1) {
		Log::w("(filesystem.cpp) fdelete: file does not exist.\n");
		delete inode; delete[] d;
		return false;
	}

	if (uid == inode->i_uid) {
		if (!(inode->i_acl & FS::WR_OWNER)) {
			Log::w("(filesystem.cpp) fdelete: permission denied.\n");
			delete inode; delete[] d;
			return false;
		}
	}
	else if (uid) {
		if (!(inode->i_acl & FS::WR_OTHER)) {
			Log::w("(filesystem.cpp) fdelete: permission denied.\n");
			delete inode; delete[] d;
			return false;
		}
	}
	else {
		//admin
	}

	if (inode->i_mode == FS::File_t::Dir) {
		
		if (inode->i_size > sizeof(struct FS::Dir) * 2) {
			Log::w("(filesystem.cpp) fdelete: dir not empty.\n");
			delete inode;
			delete[] d;
			return false;
		}
		for (int i = 0; i < inode->i_nblocks; i++) {
				dmap[inode->i_blockaddr[i]] = 0;
			}
			imap[index] = 0;
		// if (--inode->i_nlinks == 1) {
		// 	for (int i = 0; i < inode->i_nblocks; i++) {
		// 		dmap[inode->i_blockaddr[i]] = 0;
		// 	}
		// 	imap[index] = 0;
		// }
		int idx = 0;
		int last = 0;
		for(int i = 0; i < 8; i++) {
			d[i].type = FS::File_t::None;
		}
		sb->nfreeblks += inode->i_nblocks;
		sb->nfreeinodes++;
		FS::write_inode(inode, index);
		int id = 0;
		for( ; strcmp(d[id].entry_name, ".."); id++) ;
		FS::read_inode(inode, d[id].inode);
		inode->i_size -= sizeof(struct FS::Dir);
		FS::read_block(reinterpret_cast<char*>(d), 3 + FS::N_INODEBLKS + inode->i_blockaddr[0], 0, FS::BLK_SIZE);
		for(int i = 0; i < 8; i++) {
			if(d[i].inode == index) {
				d[i].type = FS::File_t::None;
			}
		}
		FS::write_block(reinterpret_cast<char*>(d), 3 + FS::N_INODEBLKS + inode->i_blockaddr[0], 0, FS::BLK_SIZE);
		FS::write_inode(inode, d[id].inode);
	}
	else if (inode->i_mode == FS::File_t::File) {
		if (--inode->i_nlinks == 0) {
			for (int i = 0; i < inode->i_nblocks; i++) {
				dmap[inode->i_blockaddr[i]] = 0;
			}
			imap[index] = 0;
		}
		int idx = 0;
		int last = 0;
		for(int i = 0; i < 8; i++) {
			if (d[i].type && (d[i].inode == index)) {
				d[i].type = FS::File_t::None;
				d[last].next_entry = d[idx].next_entry;
				break;
			}
			last = i;
		}
		sb->nfreeblks += inode->i_nblocks;
		sb->nfreeinodes++;
		FS::write_inode(inode, index);
		int id = 0;
		for( ; strcmp(d[id].entry_name, "."); id++) ;
		FS::read_inode(inode, d[id].inode);
		inode->i_size -= sizeof(struct FS::Dir);
		FS::read_block(reinterpret_cast<char*>(d), 3 + FS::N_INODEBLKS + inode->i_blockaddr[0], 0, FS::BLK_SIZE);
		for(int i = 0; i < 8; i++) {
			if(d[i].inode == index) {
				d[i].type = FS::File_t::None;
			}
		}
		FS::write_block(reinterpret_cast<char*>(d), 3 + FS::N_INODEBLKS + inode->i_blockaddr[0], 0, FS::BLK_SIZE);
		FS::write_inode(inode, d[id].inode);
	}
	delete inode;
	delete[] d;
	set_pwd(pwd);
	return true;
}

bool Filesystem::write(string path, char* buf, uint32_t offset, size_t size) {
	//lock_guard<mutex> guard(file_lock);
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(path, inode, d);
	if (index == -1) {
		Log::w("(filesystem.cpp) write: file does not exist.\n");
		delete inode; delete[] d;
		return false;
	}
	if (inode->i_mode != FS::File_t::File) {
		Log::w("(filesystem.cpp) write: cannot write into dir.\n");
		delete inode; delete[] d;
		return false;
	}
	if (uid == inode->i_uid) {
		if (!(inode->i_acl & FS::WR_OWNER)) {
			Log::w("(filesystem.cpp) write: permission denied.\n");
			delete inode; delete[] d;
			return false;
		}
	}
	else if (uid) {
		if (!(inode->i_acl & FS::WR_OTHER)) {
			Log::w("(filesystem.cpp) write: permission denied.\n");
			delete inode; delete[] d;
			return false;
		}
	}
	else {
		//admin
	}
	if (offset > inode->i_size) {
		Log::w("(filesystem.cpp) write: invalid offset.\n");
		delete inode; delete[] d;
		return false;
	}
	if (offset + size > inode->i_size) {
		int nblks = static_cast<int>(
			ceil((offset + size - inode->i_size) / FS::BLK_SIZE) );
		if (sb->nfreeblks < nblks) {
			Log::w("(filesystem.cpp) write: not enough free data blocks.\n");
			delete inode; delete[] d;
			return false;
		}
		if (inode->i_nblocks + nblks > FS::MAX_N_BLKS) {
			Log::w("(filesystem.cpp) write: max file size exceeded.\n");
			delete inode; delete[] d;
			return false;
		}
		for (int i = inode->i_nblocks; i < inode->i_nblocks + nblks; i++) {
			int db = -1;
			for (; dmap[db]; db++);
			inode->i_blockaddr[i] = db;
		}
		inode->i_nblocks += nblks;
	}
	inode->i_size = offset + size;//inode->i_size < (offset + size) ? 
		//offset + size : inode->i_size;
	auto blk_off = static_cast<int>(floor(offset / FS::BLK_SIZE));
	offset %= FS::BLK_SIZE;
	for (int i = blk_off; i < inode->i_nblocks; i++) {
		if (size + offset > FS::BLK_SIZE) {
			FS::write_block(buf, 3 + FS::N_INODEBLKS + inode->i_blockaddr[i], offset, FS::BLK_SIZE - offset);
			offset = 0;
			size -= (FS::BLK_SIZE - offset);
		}
		else {
			FS::write_block(buf, 3 + FS::N_INODEBLKS + inode->i_blockaddr[i], offset, size);
			break;
		}
	}
	FS::write_inode(inode, index);
	delete inode;
	delete[] d;
	return true;
}

void Filesystem::chmod(string path, int mode) {
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(path, inode, d);
	if (index == -1) {
		Log::w("(filesystem.cpp) chmod: file does not exist.\n");
		delete inode; delete[] d;
		return;
	}
	if (inode->i_mode != FS::File_t::File) {
		Log::w("(filesystem.cpp) chmod: chmod on directory.\n");
		delete inode; delete[] d;
		return;
	}
	if (uid == inode->i_uid) {
		if (!(inode->i_acl & FS::WR_OWNER)) {
			Log::w("(filesystem.cpp) chmod: permission denied.\n");
			delete inode; delete[] d;
			return;
		}
	}
	else if (uid) {
		if (!(inode->i_acl & FS::WR_OTHER)) {
			Log::w("(filesystem.cpp) chmod: permission denied.\n");
			delete inode; delete[] d;
			return;
		}
	}
	else {
		//admin
	}
	inode->i_acl = mode;
	FS::write_inode(inode, index);
	delete inode; delete[] d;
}

int Filesystem::write_swapspace(string path, char* buf, int blk) {
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(path, inode, d);
	if (inode->i_mode != FS::File_t::File) {
		Log::w("(filesystem.cpp) write_swapspace: invalid swapspace.\n");
		delete inode; delete[] d;
		return -1;
	}
	if (inode->i_size == 15 * FS::BLK_SIZE) {
		Log::w("(filesystem.cpp) write_swapspace: swapspace full.\n");
		delete inode; delete[] d;
		return -2;
	}
	FS::write_block(buf, 3 + FS::N_INODEBLKS + inode->i_blockaddr[blk], 0, FS::BLK_SIZE);
	inode->i_size += FS::BLK_SIZE;
	FS::write_inode(inode, index);
	delete inode;
	delete[] d;
	return blk;
}

int Filesystem::read_swapspace(string path, char* buf, int blk) {
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(path, inode, d);
	if (inode->i_mode != FS::File_t::File) {
		Log::w("(filesystem.cpp) read_swapspace: invalid swapspace.\n");
		delete inode; delete[] d;
		return -1;
	}
	FS::read_block(buf, 3 + FS::N_INODEBLKS + inode->i_blockaddr[blk], 0, FS::BLK_SIZE);
	inode->i_size -= FS::BLK_SIZE;
	FS::write_inode(inode, index);
	delete inode;
	delete[] d;
	return blk;
}

int Filesystem::read(string path, char* buf, uint32_t offset, int size) {
	//lock_guard<mutex> guard(file_lock);
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* d = new struct FS::Dir[8];
	int index = walk(path, inode, d);
	if (index == -1) {
		Log::w("(filesystem.cpp) read: file does not exist.\n");
		delete inode; delete[] d;
		return -1;
	}
	if (inode->i_mode != FS::File_t::File) {
		Log::w("(filesystem.cpp) read: cannot read from dir.\n");
		delete inode; delete[] d;
		return -1;
	}
	if (uid == inode->i_uid) {
		if (!(inode->i_acl & FS::RD_OWNER)) {
			Log::w("(filesystem.cpp) read: permission denied.\n");
			delete inode; delete[] d;
			return -1;
		}
	}
	else if (uid) {
		if (!(inode->i_acl & FS::RD_OTHER)) {
			Log::w("(filesystem.cpp) read: permission denied.\n");
			delete inode; delete[] d;
			return -1;
		}
	}
	else {
		//admin
	}
	
	if (size == -1) {
		size = inode->i_size;
	}
	if (offset + size > inode->i_size) {
		Log::w("(filesystem.cpp) read: file size exceed.\n");
		delete inode; delete[] d;
		return -1;
	}
	int rsize = size;
	auto blk_off = static_cast<int>(floor(offset / FS::BLK_SIZE));
	offset %= FS::BLK_SIZE;
	for (int i = blk_off; i < inode->i_nblocks; i++) {
		if (size + offset > FS::BLK_SIZE) {
			FS::read_block(buf, 3 + FS::N_INODEBLKS + inode->i_blockaddr[i], offset, FS::BLK_SIZE - offset);
			offset = 0;
			size -= (FS::BLK_SIZE - offset);
			buf += (FS::BLK_SIZE - offset);
		}
		else {
			FS::read_block(buf, 3 + FS::N_INODEBLKS + inode->i_blockaddr[i], offset, size);
			buf += size;
			break;
		}
	}
	*buf = 0;
	delete inode;
	delete[] d;
	return rsize;
}

string Filesystem::get_pwd() {
	return pwd;
}

void Filesystem::set_pwd_str(string path) {
	if (!path.size()) return; // should not happen
	if (path.size() > 1 && path[path.size() - 1] == '/') 
		path = path.substr(0, path.size() - 1); // normalize
	if (path[0] == '/') // /xx/yy
		pwd = path;
	else if (path[0] == '.') {
		if (path.size() == 1) { // .
			path = "";
		}
		else if (path == "..") { // ..
			pwd = pwd.substr(0, pwd.rfind("/"));
			if (!pwd.size()) pwd = "/";
		}
		else if (path[1] == '.' && path[2] == '/') { // ../xx
			path = path.substr(3);
			if (pwd == "/") pwd = pwd;
			else pwd = pwd.substr(0, pwd.rfind("/"));
			set_pwd_str(path);
		}
		else if (path[1] == '/') { // ./xx
			set_pwd_str(path.substr(2));
		}
		else { // .a/xx, .../xx, etc.
			if (pwd[pwd.size() - 1] != '/') pwd += '/';
			pwd += path;
		}
	}
	else {
		if (pwd[pwd.size() - 1] != '/') pwd += '/';
		pwd += path;
	}
}

void Filesystem::set_pwd(string path) {
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* dir = new struct FS::Dir[8];
	int index = walk(path, inode, dir);

	delete pwd_inode;
	pwd_inode = inode;
	delete[] c_dir;
	c_dir = dir;

	if(path != pwd) set_pwd_str(path);
}

int Filesystem::exist(string path) {
	struct FS::Inode* inode = new struct FS::Inode;
	struct FS::Dir* dir = new struct FS::Dir[8];
	if (walk(path, inode, dir) == -1) {
		return 0;
	}
	else {
		return inode->i_mode;
	}
	delete inode;
	delete[] dir;
}