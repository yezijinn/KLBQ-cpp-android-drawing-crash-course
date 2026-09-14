// SysHal.h 的落地编译单元。
//
// 作用: 保证 SysHal.h 里的 inline 全局实例 (driver / pid) 只有一份, 且 SysHal 的
// 成员函数与 process_vm_readv/writev 实现一定被编译进可执行文件, 不会因为头文件
// 里"看起来没人直接调用"而被 -ffunction-sections 丢掉。
//
// 项目里的实际调用路径是 MemDriver (My_Utils/MemDriver.h) 选择系统调用后端后,
// 直接调用 SysHal::read / SysHal::write / SysHal::getPID / SysHal::get_module_base。
#include "SysHal.h"
