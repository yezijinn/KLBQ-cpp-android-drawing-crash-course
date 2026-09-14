#ifndef NATIVESURFACE_MEMREAD_H
#define NATIVESURFACE_MEMREAD_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <malloc.h>
#include <math.h>
#include <thread>
#include <iostream>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <dlfcn.h>

typedef unsigned int ADDRESS;
typedef char PACKAGENAME;
typedef unsigned short UTF16;
typedef char UTF8;
typedef unsigned int UTF32;

#define UNI_SUR_HIGH_START (UTF32)0xD800
#define UNI_SUR_HIGH_END   (UTF32)0xDBFF
#define UNI_SUR_LOW_START  (UTF32)0xDC00
#define UNI_SUR_LOW_END    (UTF32)0xDFFF

#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD
#define UNI_MAX_BMP          (UTF32)0x0000FFFF
#define UNI_MAX_UTF16        (UTF32)0x0010FFFF
#define UNI_MAX_UTF32        (UTF32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32  (UTF32)0x0010FFFF

class SysHal {
private:
    pid_t pid;

#if defined(__arm__)
    int process_vm_readv_syscall  = 376;
    int process_vm_writev_syscall = 377;
#elif defined(__aarch64__)
    int process_vm_readv_syscall  = 270;
    int process_vm_writev_syscall = 271;
#elif defined(__i386__)
    int process_vm_readv_syscall  = 347;
    int process_vm_writev_syscall = 348;
#else
    int process_vm_readv_syscall  = 310;
    int process_vm_writev_syscall = 311;
#endif

    ssize_t process_v(pid_t __pid, const struct iovec *__local_iov, unsigned long __local_iov_count,
                      const struct iovec *__remote_iov, unsigned long __remote_iov_count,
                      unsigned long __flags, bool iswrite)
    {
        (void)__local_iov_count;
        (void)__remote_iov_count;
        return syscall((iswrite ? process_vm_writev_syscall : process_vm_readv_syscall),
                       __pid, __local_iov, __local_iov_count,
                       __remote_iov, __remote_iov_count, __flags);
    }

    bool pvm(void *address, void *buffer, size_t size, bool iswrite)
    {
        struct iovec local[1];
        struct iovec remote[1];
        local[0].iov_base  = buffer;
        local[0].iov_len   = size;
        remote[0].iov_base = address;
        remote[0].iov_len  = size;

        if (pid <= 0) return false;

        ssize_t bytes = process_v(pid, local, 1, remote, 1, 0, iswrite);
        return bytes == size;
    }

public:
   ~SysHal() {

   }

    SysHal() {
        pid = -1;
    }

	void initialize(pid_t pid) {
		this->pid = pid;
	}

    // 当前绑定的目标进程 (本项目"二选一"驱动需要读回 PID)
    pid_t get_pid() const {
        return pid;
    }

    bool read(uintptr_t addr, void *buffer, size_t size) {
        return pvm((void*)addr, buffer, size, false);
    }

    template <typename T>
    T read(uintptr_t addr) {
        T res{};
        if (read(addr, &res, sizeof(T)))
            return res;
        return res;
    }

    bool write(uintptr_t addr, void *buffer, size_t size) {
        return pvm((void*)addr, buffer, size, true);
    }

    template <typename T>
    bool write(uintptr_t addr, T value) {
        return this->write(addr, &value, sizeof(T));
    }


    int getPID(const char *packageName) {
        DIR *dir = opendir("/proc");
        if (!dir) return -1;

        struct dirent *entry;
        char filename[64], cmdline[256];
        int id = -1;

        while ((entry = readdir(dir))) {
            id = atoi(entry->d_name);
            if (id <= 0) continue;

            snprintf(filename, sizeof(filename), "/proc/%d/cmdline", id);
            FILE *fp = fopen(filename, "r");
            if (!fp) continue;

            fgets(cmdline, sizeof(cmdline), fp);
            fclose(fp);

            if (!strcmp(packageName, cmdline)) {
                closedir(dir);
                this->pid = id;
                return id;
            }
        }
        closedir(dir);
        return -1;
    }


    uintptr_t get_module_base(pid_t pid, const char *module_name) {
        char filename[64], line[1024];
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
        FILE *fp = fopen(filename, "r");
        if (!fp) return 0;

        uintptr_t base = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, module_name)) {
                char *dash = strchr(line, '-');
                if (dash) {
                    *dash = 0;
                    base = strtoul(line, NULL, 16);
                    break;
                }
            }
        }
        fclose(fp);
        return base;
    }
};

// inline: 本头文件会被多个 .cpp 包含, 保证全局只有一个实例 (C++17)
inline SysHal *driver = new SysHal();

typedef char PACKAGENAME; // 包名
inline pid_t pid = 0;     // 进程ID


inline int getPID(const char *PackageName)
{
	FILE *fp;
	char cmd[0x100] = "pidof ";
	strcat(cmd, PackageName);
	fp = popen(cmd, "r");
	if (fp == nullptr) return -1;
	if (fscanf(fp, "%d", &pid) != 1) pid = 0;
	pclose(fp);
	if (pid > 0)
	{
		driver->initialize(pid);
	}
	return pid;
}

inline long getModuleBase(const char *module_name)
{
	uintptr_t base = 0;
	base = driver->get_module_base(pid, module_name);
	return base;
}

inline long ReadValue(long addr)
{
	long he = 0;
	if (addr < 0xFFFFFFFF)
	{
		driver->read(addr, &he, 4);
	}
	else
	{
		driver->read(addr, &he, 8);
	}
	return he;
}

inline long ReadDword(long addr)
{
	long he = 0;
	driver->read(addr, &he, 4);
	return he;
}

inline float ReadFloat(long addr)
{
	float he = 0;
	driver->read(addr, &he, 4);
	return he;
}

inline int WriteDword(long int addr, int value)
{
	driver->write(addr, &value, 4);
	return 0;
}

inline int WriteFloat(long int addr, float value)
{
	driver->write(addr, &value, 4);
	return 0;
}

#endif
