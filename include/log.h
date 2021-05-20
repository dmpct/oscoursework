#pragma once
#include "precompile.h"
#define PROTO(x) cout << x << endl

namespace INTN {
	enum INT {
		REQ_LOAD,
		REQ_MEM_ACC,
		REQ_MEM_WRITE,
		PAGE_FAULT,
		PAGE_SWAP_IN,
		PAGE_SWAP_OUT,
		REQ_MEM_SWAP_IN_W,
		REQ_MEM_SWAP_IN_R,
		REQ_CLK,
		REQ_MEM_SWAP_OUT,
		DEVICE_REQ,
		DEVICE_DONE,
		RELEASE_SWAP,
		RELEASE_PAGE,
		REQ_FILE_OPEN,
		REQ_FILE_CLOSE,
		REQ_DEV_POP,
		REQ_UID
	};
}

class Log {
private:
	static mutex mtx_log;
public:
	static bool debug;
	static string logfname;
	static FILE* logfile;

	Log();
	~Log();

	static bool d(const char* format, ...);
	static bool i(const char* format, ...);
	static bool w(const char* format, ...);
};