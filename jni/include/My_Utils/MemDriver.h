#pragma once
// ==================== 双驱动二选一 (兼容层) ====================
// 业务代码统一通过 IDriver* dr 调用 (driver.h 里声明的接口), 本类负责把调用
// 转发给选定的后端:
//
//   MEM_DRIVER_KERNEL : Driver  (driver.h)  -> 共享内存 + 内核通信, 需要装内核驱动
//   MEM_DRIVER_SYSCALL: SysHal  (SysHal.h)  -> process_vm_readv/writev 系统调用直读
//
// 只有 IDriver 里列出的接口会被转发。硬件断点 / 触摸 / 陀螺仪 / 虚拟定位 /
// 内存扫描 / DumpMemory 这些是内核驱动独有的能力, 系统调用后端无法提供
// (会返回安全默认值, 不会崩溃)。

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "driver.h"
#include "SysHal.h"

enum MemDriverMode {
    MEM_DRIVER_KERNEL  = 0, // 共享内存内核驱动 (Driver)
    MEM_DRIVER_SYSCALL = 1, // process_vm_readv 系统调用 (SysHal)
};

class MemDriver : public IDriver {
public:
    static constexpr int MODE_KERNEL  = MEM_DRIVER_KERNEL;
    static constexpr int MODE_SYSCALL = MEM_DRIVER_SYSCALL;

    MemDriver() = default;
    MemDriver(const MemDriver &) = delete;
    MemDriver &operator=(const MemDriver &) = delete;

    // ---------- 打开 / 切换 ----------
    // 后端一旦打开就不再切换: 内核驱动握手成功后不能安全释放
    // (用户态退出但内核侧仍在轮询, 会空转), 所以必须先选好模式再点初始化。
    bool Open(int mode) {
        if (kernel_ != nullptr || syscall_ != nullptr) return ok_;

        mode_ = mode;
        ok_ = (mode == MEM_DRIVER_SYSCALL) ? OpenSyscall() : OpenKernel();
        return ok_;
    }

    bool IsOpen() const { return ok_; }
    bool CanSwitch() const { return kernel_ == nullptr && syscall_ == nullptr; }
    // 后端对象已建立 (可能还没 SetGlobalPid)
    bool IsStarted() const { return kernel_ != nullptr || syscall_ != nullptr; }

    int CurrentMode() const {
        return syscall_ != nullptr ? MEM_DRIVER_SYSCALL : MEM_DRIVER_KERNEL;
    }

    const char *CurrentModeName() const {
        return CurrentMode() == MEM_DRIVER_SYSCALL ? "系统调用" : "内核驱动";
    }

    // 从外部 Driver 指针接入本兼容层 (需要直接持有内核驱动实例时用)
    MemDriver &AttachKernel(Driver *d) {
        if (d == nullptr) return *this;
        kernel_ = d;
        syscall_ = nullptr;
        mode_ = MEM_DRIVER_KERNEL;
        ok_ = true;
        return *this;
    }

    // ---------- IDriver 实现 ----------
    int Read(uint64_t address, void *buffer, size_t size) override {
        if (buffer == nullptr || size == 0) return -EINVAL;
        if (syscall_ != nullptr) {
            return syscall_->read(static_cast<uintptr_t>(address), buffer, size) ? static_cast<int>(size) : -EIO;
        }
        if (kernel_ != nullptr) return kernel_->Read(address, buffer, size);
        return -EIO;
    }

    int Write(uint64_t address, void *buffer, size_t size) override {
        if (buffer == nullptr || size == 0) return -EINVAL;
        if (syscall_ != nullptr) {
            return syscall_->write(static_cast<uintptr_t>(address), buffer, size) ? static_cast<int>(size) : -EIO;
        }
        if (kernel_ != nullptr) return kernel_->Write(address, buffer, size);
        return -EIO;
    }

    // 只使用选定的后端, 不做静默 fallback: 选内核就只走内核, 选系统调用就只走系统调用
    int GetPid(std::string_view packageName) override {
        if (packageName.empty()) return -1;
        const std::string package(packageName);

        if (syscall_ != nullptr) {
            const int found = syscall_->getPID(package.c_str());
            if (found > 0) global_pid_ = found;
            return found;
        }
        if (kernel_ != nullptr) {
            const int found = kernel_->GetPid(package);
            if (found > 0) global_pid_ = found;
            return found;
        }
        return -1;
    }

    int GetGlobalPid() override {
        if (syscall_ != nullptr && global_pid_ <= 0) return static_cast<int>(syscall_->get_pid());
        return global_pid_;
    }

    void SetGlobalPid(int pid) override {
        global_pid_ = pid;
        if (syscall_ != nullptr) syscall_->initialize(pid);
        if (kernel_ != nullptr) kernel_->SetGlobalPid(pid);
    }

    bool GetModuleAddress(std::string_view moduleName, short segmentIndex, uint64_t *outAddress, bool isStart) override {
        if (outAddress == nullptr) return false;
        *outAddress = 0;
        if (syscall_ != nullptr) {
            if (segmentIndex != 0) return false; // 系统调用后端只提供模块起始地址
            const std::string name(moduleName);
            const uintptr_t base = syscall_->get_module_base(GetGlobalPid(), name.c_str());
            if (base == 0) return false;
            *outAddress = static_cast<uint64_t>(base);
            return true;
        }
        if (kernel_ != nullptr) return kernel_->GetModuleAddress(moduleName, segmentIndex, outAddress, isStart);
        return false;
    }

    bool DumpMemory(std::string_view target, std::string *dumpPath = nullptr) override {
        if (kernel_ != nullptr) return kernel_->DumpMemory(target, dumpPath);
        return false; // 系统调用后端不支持整段 dump
    }

    std::vector<std::pair<uintptr_t, uintptr_t>> GetScanRegions() override {
        std::vector<std::pair<uintptr_t, uintptr_t>> regions;
        if (kernel_ != nullptr) return kernel_->GetScanRegions();
        if (syscall_ != nullptr) {
            char filename[64];
            snprintf(filename, sizeof(filename), "/proc/%d/maps", GetGlobalPid());
            FILE *fp = fopen(filename, "r");
            if (fp == nullptr) return regions;
            char line[1024];
            while (fgets(line, sizeof(line), fp)) {
                unsigned long long start = 0, end = 0;
                if (sscanf(line, "%llx-%llx", &start, &end) == 2 && end > start) {
                    regions.emplace_back(static_cast<uintptr_t>(start), static_cast<uintptr_t>(end));
                }
            }
            fclose(fp);
        }
        return regions;
    }

    // ---------- 调试用 ----------
    Driver *kernel() { return kernel_; }
    SysHal *syscall() { return syscall_; }
    const char *LastError() const { return error_; }

private:
    bool OpenKernel() {
        if (kernel_ == nullptr) kernel_ = new Driver(10, true, true);
        if (kernel_ == nullptr) {
            error_ = "内核驱动对象创建失败";
            return false;
        }
        // 构造函数里的 InitCommunication() 已经在等内核握手, 能构造出来即说明通道可用
        return true;
    }

    bool OpenSyscall() {
        if (syscall_ == nullptr) syscall_ = new SysHal();
        if (syscall_ == nullptr) {
            error_ = "系统调用后端创建失败";
            return false;
        }
        return true;
    }

    Driver *kernel_ = nullptr;
    SysHal *syscall_ = nullptr;
    int mode_ = MEM_DRIVER_KERNEL;
    bool ok_ = false;
    int global_pid_ = 0;
    const char *error_ = "";
};
