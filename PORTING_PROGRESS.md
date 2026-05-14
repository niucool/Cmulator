# Cmulator C++ Porting Progress

> 从 Pascal (Free Pascal / Delphi) 移植到 C++17  ✅ 完成  
> 最后构建: 2026-05-13 22:01

---

## 架构映射

| Pascal | C++ | 状态 |
|---|---|---|
| Cmulator.pas | src/main.cpp | ✅ |
| Core/globals.pas | src/globals.h | ✅ |
| Core/struct.pas | src/struct.h | ✅ |
| Core/emu.pas | src/emu.h | ✅ |
| Core/fnhook.pas | src/fnhook.h | ✅ |
| Core/utils.pas | src/utils.h / .cpp | ✅ |
| Core/segments.pas | src/segments.h / .cpp | ✅ |
| Core/tep_peb.pas | src/tep_peb.h | ✅ |
| Core/pe_loader.pas | src/pe_loader.h / .cpp | ✅ |
| Core/PE/*.pas (30+ 文件) | src/pe_image.h / .cpp (pe-parse 封装) | ✅ |
| Core/nativehooks.pas | src/nativehooks.h / .cpp | ✅ |
| Core/memmanager.pas | src/memmanager.h | ✅ |
| Core/jsplugins_engine.pas | src/js_engine.h / .cpp | ✅ |
| Core/jsemuobj.pas | src/js_engine.cpp (内联) | ✅ |
| Core/unicorn/*.pas | <unicorn/unicorn.h> | ✅ |
| Core/QJS/quickjs.pas | <quickjs.h> | ✅ |
| Core/Zydis/*.pas | <Zydis/Zydis.h> | ✅ |
| Core/Crypto/xxhash.pas | <xxhash.h> | ✅ |

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
- `struct.h` — PE/DOS/NT/Optional/Section/Import/Export/TLS 结构体
- `tep_peb.h` — TEB/PEB/异常常量/CONTEXT/LDR 结构体
- `segments.h/.cpp` — GDT 段描述符, FS/GS MSR 辅助
- `fnhook.h` — TLibFunction, THookFunction, TNewDll, TApiRed

### Phase 2: 工具函数 ✅
- `emu.h` — TEmu 类 (uc_engine, PEImage, Hooks)
- `utils.h/.cpp` — reg_read/write, push/pop, ReadStringA/W, WriteStringA/W, ReadByte/Word/Dword/Qword, WriteByte/Word/Dword/Qword, ReadMem/WriteMem, HexDump, DumpStack, isprint, TextColor/NormVideo, CmuGetModulehandle, GetProcAddr, GetFullPath, GetDllFromApiSet, SplitReg, ExtractFileNameWithoutExt, UTF32CharToUTF8

### Phase 3: PE 解析库 ✅
- pe-parse (Trail of Bits) — 跨平台 PE 解析 (仅 ICU 依赖 on Linux)
- nlohmann-json — header-only JSON
- plog — header-only 日志
- `pe_image.h/.cpp` — PEImage 适配层 (LoadFromFile, Imports, Sections)

### Phase 4: 核心模拟器 ✅
- `pe_loader.h/.cpp` — load_sys_dll (DLL 加载+导出解析), HookImports (IAT 修复), InitTLS, InitDll, Init_dlls
- `nativehooks.h/.cpp` — NtContinue 内置 Hook
- `memmanager.h` — TMemoryManager stub

### Phase 5: JS 集成 ✅
- `js_engine.h/.cpp` — QuickJS 初始化, ApiHook JS 类注册, importScripts, console.log, InitJSEmu

### Phase 6: 主入口 ✅
- `main.cpp` — CLI 参数解析, config.json 加载, PE→Unicorn 映射, 模拟循环

---

## 源文件统计 (21 文件)

```
src/
├── types.h          ~85  类型系统
├── globals.h        ~80  全局状态
├── struct.h        ~250  PE 结构体
├── tep_peb.h       ~290  TEB/PEB 结构体
├── fnhook.h        ~140  Hook 记录
├── segments.h      ~130  GDT/段声明
├── segments.cpp    ~145  GDT/MSR 实现
├── emu.h            ~45  TEmu 类
├── utils.h          ~95  工具声明
├── utils.cpp       ~490  工具实现 (22 函数)
├── pe_image.h       ~65  PEImage 适配层
├── pe_image.cpp     ~95  pe-parse 封装
├── pe_loader.h      ~22  PE 加载声明
├── pe_loader.cpp   ~135  PE 加载实现
├── nativehooks.h     ~9  Hook 声明
├── nativehooks.cpp  ~50  NtContinue 实现
├── memmanager.h     ~10  Stub
├── js_engine.h       ~6  JS 引擎声明
├── js_engine.cpp   ~245  JS 引擎实现
├── main.cpp        ~165  入口
└── test_deps.cpp   ~185  测试

总计: 13 headers + 8 sources ≈ 2720 lines
```

---

## 关键设计决策

- **类型系统**: Windows 上复用 SDK 类型 (DWORD/BYTE/WORD/ULONG)，非 Windows 自建
- **PE 解析**: pe-parse 替代 30+ 文件 Pascal PE 库，4 源文件覆盖全部需求
- **全局状态**: C++17 `inline` 变量替代 Pascal `var` 全局
- **_PEPARSE_WINDOWS_CONFLICTS**: PUBLIC 定义，传递给所有消费者
- **/FS**: MSVC PDB 并行编译保护
- **GIT_SHALLOW**: pe-parse FetchContent 避免 git stash 冲突

## 上次更新

2026-05-13 22:01 — 全 6 阶段完成, cmulator.exe + test_deps.exe 构建通过
