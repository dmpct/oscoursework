#include "../include/log.h"

bool Log::debug = true;
mutex Log::mtx_log;
string Log::logfname = "log.txt";
FILE* Log::logfile = fopen(Log::logfname.c_str(), "a");

Log::Log() {}

Log::~Log() {}

bool Log::d(const char* format, ...) {
	if (debug) {
		lock_guard<mutex> guard(mtx_log);
		auto t_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
		auto time = strtok(ctime(&t_time), "\n");
		//fprintf(logfile, "[%s][Debug]", time);
		//fprintf(stdout, "[%s][Debug]", time);
		va_list args;
		va_start(args, format);
		//vfprintf(logfile, format, args);
		//vfprintf(stdout, format, args);
		va_end(args);
	}
	return true;
}

bool Log::i(const char* format, ...) {
	lock_guard<mutex> guard(mtx_log);
	auto t_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
	auto time = strtok(ctime(&t_time), "\n");
	//fprintf(logfile, "[%s][Info]", time);
	//fprintf(stdout, "[%s][Info]", time);
	va_list args;
	va_start(args, format);
	//vfprintf(logfile, format, args);
	//vfprintf(stdout, format, args);
	va_end(args);
	return true;
}

bool Log::w(const char* format, ...) {
	lock_guard<mutex> guard(mtx_log);
	/*auto t_time = chrono::system_clock::to_time_t(chrono::system_clock::now());
	auto time = strtok(ctime(&t_time), "\n");
	fprintf(logfile, "[%s][Warning]", time);
	fprintf(stdout, "[%s][Warning]", time);
	va_list args;
	va_start(args, format);
	vfprintf(logfile, format, args);
	vfprintf(stdout, format, args);
	va_end(args);*/
	return true;
}
