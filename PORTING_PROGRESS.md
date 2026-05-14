# Cmulator C++ Porting Progress

> 从 Pascal (Free Pascal / Delphi) 移植到 C++17
> 最后源码移植更新: 2026-05-14 14:10
> 构建状态: 未重新构建（按要求继续源码移植，不运行 build）

---

## 架构映射

| Pascal | C++ | 状态 |
|---|---|---|---|
| Cmulator.pas | main.cpp | ✅ |
| Core/globals.pas | Core/globals.h | ✅ |
| Core/struct.pas | Core/struct.h | ✅ |
| Core/emu.pas | Core/emu.h + Core/emu_init.cpp | ✅ |
| Core/fnhook.pas | Core/fnhook.h | ✅ |
| Core/utils.pas | Core/utils.h / .cpp | ✅ |
| Core/segments.pas | Core/segments.h / .cpp | ✅ |
| Core/tep_peb.pas | Core/tep_peb.h (含 Peb32/Peb64) | ✅ |
| Core/pe_loader.pas | Core/pe_loader.h / .cpp | ✅ |
| Core/PE/*.pas (30+ 文件) | Core/pe_image.h / .cpp (pe-parse 封装) | ✅ |
| Core/nativehooks.pas | Core/nativehooks.h / .cpp | ✅ |
| Core/memmanager.pas | Core/memmanager.h | ✅ |
| Core/jsplugins_engine.pas | Core/js_engine.h / .cpp | ✅ |
| Core/jsemuobj.pas | Core/js_engine.cpp (内联, Emu.* API) | ✅ |
| **Core/process/ethreads.pas** | **Core/ethreads.h / .cpp** | **🆕** |
| Core/interactive.pas | (stub, TODO) | ⏳ |
| Core/GUI/gui.pas | (CLI only, 不需要) | ❌ |
| Core/unicorn/*.pas | <unicorn/unicorn.h> | ✅ |
| Core/QJS/quickjs.pas | <quickjs.h> | ✅ |
| Core/Zydis/*.pas | <Zydis/Zydis.h> | ✅ |
| Core/Crypto/xxhash.pas | <xxhash.h> | ✅ |
| Core/JSON/superobject.pas | nlohmann-json | ✅ |
| Core/pesp/*.pas (17 文件) | pe-parse (替代) | ✅ |

---

## 依赖 (7 库, 全 FetchContent)

| 库 | 版本 | 用途 | 类型 |
|---|---|---|---|
| Unicorn Engine | 2.1.4 | CPU 模拟 (x86/x64) | 静态库 |
| Zydis | 4.1.1 | x86-64 反汇编 | 静态库 |
| QuickJS (qjs) | 0.14.0 | JavaScript 引擎 | 静态库 |
| xxHash | 0.8.3 | 高速哈希 | 静态库 |
| pe-parse | master | 跨平台 PE 解析 | 静态库 (4源文件) |
| nlohmann-json | 3.11.3 | JSON 配置解析 | header-only |
| plog | 1.1.11 | 日志输出 | header-only |

---

## 构建

```batch
# 生成 + 编译 (VS 2026)
cmake -B Build -G "NMake Makefiles" -DUSE_VCPKG=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build Build

# 产物
Build\cmulator.exe   (6.5 MB)  主程序
Build\test_deps.exe  (5.1 MB)  依赖测试
Build\cmulator.lib              静态库
```

### 构建脚本

| 脚本 | 用途 |
|---|---|
| `cmake_config.bat` | 完整 configure + build + 运行 test_deps |
| `q.bat` / `mb.bat` | 增量 build |
| `b.bat` / `b2.bat` | build + run test |
| `test_build.bat` | clean 缓存 + 全量重建 |

---

## 移植阶段

### Phase 1: 基础结构 & 类型定义 ✅
- `types.h` — 公共类型, Windows SDK 兼容 (含 `<windows.h>`)
- `globals.h` — 全局状态 (C++17 inline 变量)
- `struct.h` — PE/DOS/NT/Optional/Section/Import/Export/TLS 结构体（含 TLS32/TLS64）
- `tep_peb.h` — TEB/PEB/异常常量/CONTEXT/LDR 结构体 + `Peb32`/`Peb64` (x86/x64 PEB)
- `segments.h/.cpp` — GDT 段描述符, FS/GS MSR 辅助
- `fnhook.h` — TLibFunction, THookFunction, TNewDll, TApiRed

### Phase 2: 工具函数 ✅
- `emu.h` — TEmu 类 (uc_engine, PEImage, Hooks, 方法: Start/init_segments/SetHooks/MapPEtoUC)
- `utils.h/.cpp` — reg_read/write, push/pop, ReadStringA/W, WriteStringA/W, ReadByte/Word/Dword/Qword, WriteByte/Word/Dword/Qword, ReadMem/WriteMem, HexDump, DumpStack, isprint, TextColor/NormVideo, CmuGetModulehandle, GetProcAddr, GetFullPath, GetDllFromApiSet, SplitReg, ExtractFileNameWithoutExt, UTF32CharToUTF8

### Phase 3: PE 解析库 ✅
- pe-parse (Trail of Bits) — 跨平台 PE 解析 (仅 ICU 依赖 on Linux)
- nlohmann-json — header-only JSON
- plog — header-only 日志
- `pe_image.h/.cpp` — PEImage 适配层 (LoadFromFile, Imports, Sections, TLS callbacks)

### Phase 4: 核心模拟器 ✅
- `pe_loader.h/.cpp` — load_sys_dll (DLL 加载+导出解析), HookImports (IAT 修复), InitTLS 回调执行, InitDll, Init_dlls
- `nativehooks.h/.cpp` — NtContinue 内置 Hook
- `memmanager.h` — TMemoryManager stub
- **`ethreads.h/.cpp`** — InitTEB_PEB, BuildPEB, BuildPEB_Ldr (TEB/PEB 初始化)
- **`emu_init.cpp`** — TEmu::Start, init_segments, SetHooks, MapPEtoUC (移植自 emu.pas)

### Phase 5: JS 集成 ✅
- `js_engine.h/.cpp` — QuickJS 初始化, ApiHook JS 类注册, importScripts, console.log, InitJSEmu
- `Core/jsemuobj.pas` — 已内联移植到 `js_engine.cpp`: `Emu.ReadReg/SetReg`, 字符串读写, 模块加载/查询, 内存读写, 栈操作, Stop/LastError, HexDump/StackDump

### Phase 6: 主入口 ✅
- `main.cpp` — CLI 参数解析, config.json 加载, PE→Unicorn 映射, 模拟循环

---

## 源文件统计 (24 文件)

```
Core/
├── types.h          ~85  类型系统
├── globals.h        ~80  全局状态
├── struct.h        ~250  PE 结构体
├── tep_peb.h       ~420  TEB/PEB 结构体 + Peb32/Peb64
├── fnhook.h        ~140  Hook 记录
├── segments.h      ~130  GDT/段声明
├── segments.cpp    ~145  GDT/MSR 实现
├── emu.h           ~100  TEmu 类 (含方法声明)
├── emu_init.cpp    ~275  TEmu::Start, init_segments, SetHooks, MapPEtoUC
├── ethreads.h       ~25  TEB/PEB 初始化声明
├── ethreads.cpp    ~300  TEB/PEB 初始化实现 (InitTEB_PEB, BuildPEB, BuildPEB_Ldr)
├── utils.h          ~95  工具声明
├── utils.cpp       ~490  工具实现 (22 函数)
├── pe_image.h       ~65  PEImage 适配层
├── pe_image.cpp     ~95  pe-parse 封装
├── pe_loader.h      ~22  PE 加载声明
├── pe_loader.cpp   ~220  PE 加载实现 (load_sys_dll, HookImports, InitDll, InitTLS)
├── nativehooks.h     ~9  Hook 声明
├── nativehooks.cpp  ~50  NtContinue 实现
├── memmanager.h     ~10  Stub
├── js_engine.h       ~6  JS 引擎声明
├── js_engine.cpp   ~570  JS 引擎 + Emu JS API 实现
├── main.cpp        ~180  入口 (config/CLI → TEmu::Start)
└── test_deps.cpp   ~185  测试

总计: 14 headers + 10 sources ≈ 3200+ lines
```

---

## 未移植 Pascal 文件

| 文件 | 说明 |
|---|---|
| `Core/interactive.pas` | 交互式调试 stub, 仅 TODO 注释 |
| `Core/GUI/gui.pas` | GUI 模块, CLI 模式不需要 |
| `Core/pesp/*.pas` (17 文件) | 旧 PE 解析器, 已由 pe-parse 替代 |
| `Core/PE/*.pas` (30+ 文件) | 旧 PE 解析库, 已由 pe-parse 替代 |
| `Core/Zydis/*.pas` | 由 <Zydis/Zydis.h> 替代 |
| `Core/QJS/quickjs.pas` | 由 <quickjs.h> 替代 |
| `Core/unicorn/*.pas` | 由 <unicorn/unicorn.h> 替代 |
| `Core/Crypto/xxhash.pas` | 由 <xxhash.h> 替代 |
| `Core/JSON/*.pas` | 由 nlohmann-json 替代 |

这些文件均为外部库包装/stub, 不需要手写移植.

---

## 关键设计决策

- **类型系统**: Windows 上复用 SDK 类型 (DWORD/BYTE/WORD/ULONG)，非 Windows 自建
- **PE 解析**: pe-parse 替代 30+ 文件 Pascal PE 库，4 源文件覆盖全部需求
- **全局状态**: C++17 `inline` 变量替代 Pascal `var` 全局
- **_PEPARSE_WINDOWS_CONFLICTS**: PUBLIC 定义，传递给所有消费者
- **/FS**: MSVC PDB 并行编译保护
- **GIT_SHALLOW**: pe-parse FetchContent 避免 git stash 冲突（仍需后续构建验证）
- **TEB/PEB 初始化**: `ethreads.h/.cpp` 替代 `Core/process/ethreads.pas`，直接字节偏移写入
- **Peb32/Peb64**: 最小化结构体 + padding 数组匹配 Pascal 布局
- **启动路径整合**: `main.cpp` 现在委托 `TEmu::Start()`，避免绕过 segments/TEB/PEB/DLL/TLS 初始化
- **PE 映射**: `MapPEtoUC()` 改为写入 PE headers + section virtual layout，不再把 raw file blob 直接写到 ImageBase
- **TLS 回调**: `PEImage` 解析 TLS callback table，`InitTLS()` 以 Pascal 相同参数顺序执行回调并恢复 CPU 上下文

## 当前未验证 / 风险

- 未运行构建或测试（用户要求继续源码移植且不尝试 build）。
- Shellcode 模式仍未完整移植；`main.cpp` 当前显式拒绝 `-sc`，避免误走 PE 路径。
- syscall/sysenter、交互式调试、`TMemoryManager` 仍是 stub/minimal 实现。
- JS API 与 TLS callback 执行已按 Pascal 迁移，但需要后续 QuickJS/Unicorn/编译验证确认函数签名与运行时行为。

## 上次更新

2026-05-14 14:10 — 继续源码移植: main.cpp 接入 TEmu::Start(), PE virtual layout 映射,
修复 hook/context 传参, port `Core/jsemuobj.pas` 的 Emu JS API 到 `Core/js_engine.cpp`,
补齐 TLS32/TLS64 callback 解析与 `InitTLS()` 回调执行路径。
