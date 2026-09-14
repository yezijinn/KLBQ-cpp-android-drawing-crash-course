---
tags: [教程, 附录, 排错]
aliases: [附录C, troubleshooting]
---

# 附录 C · 排错手册

> [!abstract] 怎么用
> 30 个常见错误的**症状 → 原因 → 解法**。按主题分类，用 Ctrl+F 搜症状关键词。
>
> **按报错原文查**请用 [[附录J-错误信息反查表]]（150+ 条，含编译/链接/加载/崩溃/Vulkan 各阶段）——
> 两者互补：本附录按"现象"组织，J 按"报错文本"组织。

## 编译类

### 1. `missing separator`
**症状**：Makefile 报错 `*** missing separator. Stop.`
**原因**：命令行用了空格缩进
**解法**：改成 Tab。VS Code 右下角把缩进设为 Tab，或加 `.editorconfig`

### 2. `undefined reference to xxx`
**症状**：链接阶段报未定义引用
**原因**（按概率）：
1. 源文件没加到 `LOCAL_SRC_FILES`
2. 库没加到 `LOCAL_STATIC_LIBRARIES` / `LOCAL_LDLIBS`
3. **链接顺序错**（依赖方要在左）
**解法**：第 27、28 章；用 `nm -u` 查缺什么

### 3. `multiple definition of xxx`
**症状**：同一个符号定义了多次
**原因**：在头文件里定义了全局变量（没用 `extern` 或 `inline`）
**解法**：头文件只放 `extern` 声明，定义放 `.cpp`

### 4. `fatal error: xxx.h: No such file`
**原因**：`LOCAL_C_INCLUDES` 没配，或路径写错
**解法**：用 `$(LOCAL_PATH)/...` 而非相对路径；`ndk-build V=1` 确认 `-I` 参数

### 5. `stray '\357' in program`
**症状**：中文乱码报错
**原因**：文件是 UTF-8 **带 BOM**
**解法**：转成无 BOM；`sed -i '1s/^\xEF\xBB\xBF//' file.cpp`

### 6. `error: only position independent executables (PIE) are supported`
**解法**：加 `-fPIE -pie`（现代 NDK 默认已开，除非手动关了）

### 7. 编译极慢
**原因**：`-flto` + 大字体 `.cpp`（如 `heiti_ttf.cpp` 十几 MB）
**解法**：调试期去掉 `-flto`；裁剪字体

## 部署运行类

### 8. `Permission denied`（执行时）
**原因**：没 chmod，或推到了 `/sdcard`（noexec）
**解法**：`chmod 755`；放 `/data/local/tmp`

### 9. `No such file or directory`（文件明明在）
**原因**：缺动态库
**解法**：`readelf -d app | grep NEEDED` 查依赖；补 `libc++_shared.so` 等

### 10. `not executable: 64-bit ELF`
**原因**：ABI 不匹配（32 位程序跑到只支持 64 位的环境，或反之）
**解法**：确认 `APP_ABI`；`file app` 检查

### 11. `CANNOT LINK EXECUTABLE: library "xxx.so" not found`
**原因**：动态链接的库不在设备上，或 App 命名空间限制
**解法**：
- 系统私有库：改用 `dlopen`（第 60 章）
- `libc++_shared.so`：一起 push 并用 `LD_LIBRARY_PATH`

### 12. 程序一闪而退，无输出
**解法**：
```bash
adb logcat -c
adb shell /data/local/tmp/app
adb logcat -d | tail -40
adb logcat -b crash
```

### 13. 崩溃但 logcat 没堆栈
**解法**：保留符号（去掉 `-s`），用 `addr2line`（附录 A）

## 权限类

### 14. root 了还是 `Permission denied`
**原因**：SELinux
**解法**：
```bash
adb shell su -c dmesg | grep avc      # 看有没有 avc: denied
adb shell su -c setenforce 0          # 仅排错时临时关
```
确认后恢复 `setenforce 1`，改用合规路径

### 15. 触摸注入无效
**检查清单**：
```bash
ls -l /dev/uinput              # 存在吗？
adb shell getevent -l          # 能收到事件吗？
adb shell settings put system pointer_location 1   # 开指针位置看
```
**常见原因**：忘了 `SYN_REPORT`；坐标超出声明的范围；权限不足

### 16. `dlopen failed: ... is not accessible for the namespace`
**原因**：Android 7+ 对 App 的私有库限制
**解法**：用独立可执行文件而非 App 的 .so（第 32 章）

## 图形渲染类

### 17. 悬浮窗有黑底
**原因**：`compositeAlpha` 用了 `OPAQUE`
**解法**：改用 `VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR`（第 59 章）

### 18. 菜单能画但触摸没反应
**原因**：图层不接收输入；或触摸没喂给 ImGui
**解法**：确认 `Touch::UpdateImGuiInput()` 被调用；用 `getevent` 确认有事件

### 19. 图层看不见
**检查清单**：
```bash
adb shell dumpsys SurfaceFlinger | grep -i overlay    # 图层在吗？
```
- 不在 → 创建失败，看日志
- 在但 z 太低 → 提高 `setLayer`
- 在但没 `show()` → 调用 `t.show(sc)`

### 20. 画面撕裂/闪烁
**原因**：没用交换链或呈现模式不对
**解法**：用 `VK_PRESENT_MODE_FIFO_KHR`（一定支持）

### 21. 中文显示为方块
**原因**：字体没有中文字形
**解法**：用 `GetGlyphRangesChineseFull()` 或自定义范围；确认字体文件支持中文

### 22. 中文字体导致崩溃
**原因**：`FontDataOwnedByAtlas` 默认为 true，ImGui 会 free 静态数组
**解法**：设 `config.FontDataOwnedByAtlas = false`

### 23. 旋转屏幕后错位
**解法**：检测 `displayInfo.orientation` 变化，重建窗口并更新触摸变换（第 69 章）

## 内存读取类

### 24. `process_vm_readv` 返回 `EPERM`
**原因**：权限不足（UID 不匹配且无 `CAP_SYS_PTRACE`）
**解法**：用 root 运行；检查 SELinux

### 25. 返回 `ESRCH`
**原因**：目标进程不存在（没启动/已退出）
**解法**：重新查找 PID；每次操作前检查进程存活

### 26. 返回 `EFAULT`
**原因**：地址不可访问
**解法**：
- 偏移失效 → 重新定位（第 87 章）
- 对象已释放 → 加强合法性过滤（第 89 章）

### 27. 读出来的数据全是 0
**检查清单**：
1. PID 对不对
2. 模块基址对不对（`memview` 验证）
3. 偏移对不对
4. 读取的字节数对不对（float 4 字节、指针 8 字节）

### 28. 读出来的数据是天文数字
**原因**：类型搞错（把 float 当 int 读，或 4 字节当 8 字节）
**解法**：hexdump 原始字节，对照 IEEE754（第 26 章）

### 29. 数据第一次对，后面错
**原因**：缓存没更新，或对象被释放
**解法**：每帧重读；加强有效性检查

### 30. 遮挡判定全错
**检查清单**：
- `tMax` 是不是设成了"到目标的距离"？（不能是无穷）
- `hit.geomID` 每次查询前重置了吗？
- 场景 commit 了吗？
- 坐标系对不对（世界坐标还是局部坐标）？

## 通用排查流程

```
1. 能复现吗？         → 加日志，记录触发条件
2. 报错是什么？       → 分类（编译/链接/运行/逻辑）
3. 在哪一行？         → 二分法注释代码，或关键路径打日志
4. 最小复现？         → 抽出 20 行小程序
5. 验证修复？         → 跑失败用例 + 相关成功用例
```

## 应急：触摸失灵

如果开发触摸拦截时程序崩溃导致设备触摸失效：

```bash
adb shell input tap 500 1000              # adb 还能操作
adb shell input keyevent KEYCODE_BACK
adb shell stop && adb shell start         # 重启框架（不丢数据）
adb reboot                                # 最后手段
```

**预防**：加信号处理，退出前 `EVIOCGRAB UNGRAB`。

→ 返回 [[00-开始之前]]
