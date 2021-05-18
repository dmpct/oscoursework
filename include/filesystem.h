#pragma once
#include "log.h"

/*
0____1____2____3_________1027__________________
|SBLK|D_BM|I_BM|Inodes...| data | .... | data |
．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．．
Superblock occupies a single block,
bitmaps each occupy a single block,
each block contains 8 inodes, all 8192 inodes 
occupy 1024 blocks. Each block can contain at most
8 dir entries.
*/

namespace FS {
	constexpr uint32_t BLK_SIZE = 1024; // Bytes -> 1KB
	constexpr uint32_t N_DATABLKS = BLK_SIZE * 8; // 8192 -> 8192*1KB=8MB
	constexpr uint32_t N_INODES = BLK_SIZE * 8; // 8192 -> 8192*128B=1MB
	// disk amount = (3 + N_DATABLKS + N_INODES/(BLK_SIZE/INODE_SIZE)) * BLK_SIZE = 9MB + 1KB
	constexpr uint32_t MAX_N_BLKS = 15; // max blocks for each file
	constexpr uint32_t MAX_NAME_LEN = 120; // max length for entry names
	constexpr uint32_t N_INODEBLKS = N_INODES / (BLK_SIZE / 128);
	constexpr auto DEVICE = "disk.bin"; // the disk file
	constexpr uint32_t RD_OWNER = 1 << 5;
	constexpr uint32_t WR_OWNER = 1 << 4;
	constexpr uint32_t EX_OWNER = 1 << 3;
	constexpr uint32_t RD_OTHER = 1 << 2;
	constexpr uint32_t WR_OTHER = 1 << 1;
	constexpr uint32_t EX_OTHER = 1 << 0;
	constexpr uint32_t ALL_OWNER = RD_OWNER | WR_OWNER | EX_OWNER;
	constexpr uint32_t ALL_OTHER = RD_OTHER | WR_OTHER | EX_OTHER;
	constexpr uint32_t RW_OWNER = RD_OWNER | WR_OWNER;
	constexpr uint32_t RW_OTHER = RD_OTHER | WR_OTHER;

	enum File_t {
		None = 0,
		File,
		Dir
	};

	struct Superblock { // 32B
		uint32_t nblocks; // data blocks
		uint32_t ninodes; // 
		uint32_t nfreeblks; // 
		uint32_t nfreeinodes; // 
		uint32_t block_size; // 
		uint32_t max_blocks; // size in blocks
		uint32_t m_time; // last mount time
		uint32_t w_time; // last write time
	};

	struct Inode { // 128B
		uint16_t i_mode; // File_t
		uint16_t i_uid; // owner
		uint64_t i_size; // size in B
		uint32_t i_nlinks; // n hard links
		uint32_t i_nblocks; // n data blocks
		uint32_t i_atime; // last access time
		uint32_t i_ctime; // last change time (modify inode)
		uint32_t i_mtime; // last modify time (modify data)
		uint32_t i_blockaddr[MAX_N_BLKS]; // pointer to data blocks
		uint32_t i_acl; // permissions, rwxrwx
		uint32_t fill1; // dummy
		uint32_t fill2; // dummy
		uint32_t fill3; // dummy
		uint32_t fill4; // dummy
		uint32_t fill5; // dummy
		uint32_t fill6; // dummy
	};

	struct Dir { // 128B
		uint32_t inode; // 
		uint16_t next_entry; // link to next entry
		uint16_t type; // File_t
		char entry_name[MAX_NAME_LEN];
		Dir() {
			next_entry = 0;
			memset(entry_name, 0, MAX_NAME_LEN);
		}
	};

	bool format_disk();
	bool write_block(char* buf, uint32_t blk, uint32_t offset, uint32_t size);
	bool read_block(char* buf, uint32_t blk, uint32_t offset, uint32_t size);
	bool write_inode(struct Inode* inode, int index);
	bool read_inode(struct Inode* inode, int index);
	bool set_bitmap(char* bitmap, int index, int val);
	bool makefs();
	bool init();

	void _perform_test();

	struct File {
		uint32_t offset;
		uint32_t counter;
		uint32_t inode;
		uint16_t r;
		uint16_t w;
		uint32_t t_open;
	};
}

class Filesystem {
private:
	vector<struct FS::File*> file_table;
	struct FS::Superblock* sb;
	char* imap;
	char* dmap;
	string pwd;
	struct FS::Inode* pwd_inode;
	struct FS::Dir* c_dir;
	void set_pwd_str(string path);
	mutex file_lock;
public:
	Filesystem(function<void(int, void*)> idt);
	~Filesystem();
	int walk(string path, struct FS::Inode* inode, struct FS::Dir* dir);
	int open(string path, int rw, int truncate);
	void close(int fd);
	bool create(string path, string name, FS::File_t type);
	bool fdelete(string path);
	bool write(string path, char* buf, uint32_t offset, size_t size);
	int read(string path, char* buf, uint32_t offset, int size);
	int exist(string path);
	string get_pwd();
	void set_pwd(string path);
	bool create_swapspace(string path, string name);
	int write_swapspace(string path, char* buf, int blk);
	int read_swapspace(string path, char* buf, int blk);
	void reset_swapspace(string path);
};