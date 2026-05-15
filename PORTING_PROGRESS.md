# Cmulator C++ Porting Progress

> 从 Pascal (Free Pascal / Delphi) 移植到 C++17  ✅ 核心 100% 完成  
> 最后构建: 2026-05-14

---

## Pascal → C++ 文件级对照

### 核心文件 (12/12 ✅)

| Pascal | C++ | 函数数 |
|---|---|---|
| `Core/emu.pas` (1465行) | `Emu.h` + `Emu.cpp` + `emu_init.cpp` + `emu_hooks.cpp` | 10方法+5静态 |
| `Core/fnhook.pas` | `FnHook.h` | TLibFunction/THookFunction/TNewDll |
| `Core/globals.pas` | `Globals.h` + `Globals.cpp` | 全部全局变量 |
| `Core/utils.pas` | `Utils.h` + `Utils.cpp` | 22函数 |
| `Core/segments.pas` | `Segments.h` + `Segments.cpp` | GDT/MSR |
| `Core/pe_loader.pas` | `PE_Loader.h` + `PE_Loader.cpp` | 6函数 |
| `Core/nativehooks.pas` | `NativeHooks.h` + `NativeHooks.cpp` | NtContinue |
| `Core/jsplugins_engine.pas` | `js_engine.h` + `js_engine.cpp` | Init_QJS + ApiHook类 |
| `Core/jsemuobj.pas` (26函数) | `js_engine.cpp` (内联 Emu.* API) | 26 JS函数 |
| `Core/struct.pas` | `Struct.h` | PE结构体 |
| `Core/tep_peb.pas` | `TEP_PEB.h` (+ Context32/Peb32/Peb64) | TEB/PEB/CONTEXT |
| `Core/process/ethreads.pas` | `process/ethreads.h/cpp` | InitTEB_PEB/BuildPEB/buildPEB_Ldr |

### 外部库替代 (不需手写移植)

| Pascal | C++ 替代 | 文件数 |
|---|---|---|
| `Core/PE/*.pas` | pe-parse | 30+ → 4 |
| `Core/pesp/*.pas` | pe-parse | 17 → 0 |
| `Core/unicorn/*.pas` | `<unicorn/unicorn.h>` | 8 → 0 |
| `Core/Zydis/*.pas` | `<Zydis/Zydis.h>` | 4 → 0 |
| `Core/QJS/quickjs.pas` | `<quickjs.h>` | 1 → 0 |
| `Core/Crypto/xxhash.pas` | `<xxhash.h>` | 1 → 0 |
| `Core/JSON/*.pas` | `nlohmann-json` | 3 → 0 |
| `Core/generics_collections/*` | C++ STL | 6 → 0 |

### 未移植

| Pascal | 说明 |
|---|---|
| `Core/interactive.pas` | 交互式调试 stub |
| `Core/GUI/gui.pas` | GUI模块, CLI不需要 |

---

## 函数级对照

### TEmu 方法 (10/10 ✅)

| Pascal | C++ |
|---|---|
| `Create(FilePath, sc, x64)` | `TEmu(...)` in Emu.cpp |
| `Start()` | `TEmu::Start()` in emu_init.cpp |
| `MapPEtoUC()` | `TEmu::MapPEtoUC()` in emu_init.cpp |
| `SetHooks()` | `TEmu::SetHooks()` in emu_init.cpp |
| `init_segments()` | `TEmu::init_segments()` in emu_init.cpp |
| `SaveCPUState()` | `TEmu::SaveCPUState()` in emu_init.cpp |
| `RestoreCPUState()` | `TEmu::RestoreCPUState()` in emu_init.cpp |
| `ResetEFLAGS()` | `TEmu::ResetEFLAGS()` in emu_init.cpp |
| `GetGDT(index)` | `TEmu::GetGDT(index)` in emu_init.cpp |
| `Destroy()` | `~TEmu()` inline in Emu.h |

### 回调/静态函数 (8/8 ✅)

| Pascal | C++ |
|---|---|
| `Handle_SEH(uc, code)` | `TEmu::Handle_SEH(...)` in emu_hooks.cpp |
| `HookMemInvalid(...)` | `TEmu::HookMemInvalid(...)` in emu_hooks.cpp |
| `HookMemX86(...)` | `TEmu::HookMemX86(...)` in emu_hooks.cpp |
| `HookCode(...)` | `TEmu::HookCode(...)` in emu_hooks.cpp |
| `FlushMemMapping` | `TEmu::FlushMemMapping(...)` in emu_hooks.cpp |
| `CallJS` | `TEmu::CallJS(...)` in emu_hooks.cpp |
| `CheckHook` | `TEmu::CheckHook(...)` in emu_hooks.cpp |
| `CheckAddrHooks` | `TEmu::CheckAddrHooks(...)` in emu_hooks.cpp |

### Emu JS API (26/26 ✅)

| API | 状态 | API | 状态 |
|---|---|---|---|
| `ReadReg, SetReg` | ✅ | `WriteByte, WriteWord, WriteDword, WriteQword` | ✅ |
| `ReadStringA, ReadStringW` | ✅ | `WriteStringA, WriteStringW` | ✅ |
| `WriteMem` | ✅ | `ReadByte, ReadWord, ReadDword, ReadQword` | ✅ |
| `ReadMem` | ✅ | `push, pop` | ✅ |
| `LoadLibrary` | ✅ | `GetModuleName, GetModuleHandle, GetProcAddr` | ✅ |
| `Stop` | ✅ | `LastError` | ✅ |
| `HexDump` | ✅ | `StackDump` | ✅ |

---

## 依赖

| 库 | 版本 | 用途 | 集成 |
|---|---|---|---|
| Unicorn | 2.1.4 | CPU 模拟 (x86/x64) | FetchContent |
| Zydis | 4.1.1 | 反汇编 | FetchContent |
| QuickJS | 0.14.0 | JS 引擎 | FetchContent |
| xxHash | 0.8.3 | 哈希 | FetchContent |
| pe-parse | master | PE 解析 | FetchContent (shallow) |
| nlohmann-json | 3.11.3 | JSON | FetchContent (header-only) |
| plog | 1.1.11 | 日志 | FetchContent (header-only) |

## 构建

```batch
cmake -B Build -G "NMake Makefiles" -DUSE_VCPKG=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build Build
Build\cmulator.exe   (6.5 MB)
Build\test_deps.exe  (5.1 MB)  # 4/4
```

## 源文件 (25文件, ~3700行)

```
Core/
├── types.h          ~85
├── Globals.h/.cpp   ~80 + 20
├── Struct.h        ~250
├── TEP_PEB.h       ~420 (含 Peb32/Peb64)
├── FnHook.h         ~80 (含 IsWapi)
├── Segments.h/.cpp ~130 + 145
├── Emu.h           ~105 (含 THooks)
├── Emu.cpp          ~20 (构造函数)
├── emu_init.cpp    ~280 (Start, SetHooks, MapPEtoUC, init_segments)
├── emu_hooks.cpp   ~490 (HookCode, SEH, HookMemX86, CallJS)
├── Utils.h/.cpp     ~95 + 490
├── PE_Loader.h/.cpp ~22 + 220
├── NativeHooks.h/.cpp ~9 + 60 (含 EFLAGS 恢复)
├── MemManager.h     ~10
├── js_engine.h/.cpp  ~6 + 570
├── PE/pe_image.h/.cpp ~65 + 95
├── process/ethreads.h/.cpp ~25 + 300
Cmulator.cpp         ~180 (CLI入口)
test_deps.cpp         ~185
```

## 上次修复

2026-05-14:
- `FnHook.h`: 添加 `IsWapi` 字段 (JS 回调区分 Wide-char API)
- `NativeHooks.cpp`: NtContinue 补全 ExceptionRecord 读取 + EFLAGS 恢复
- 确认 `Ident` + `VerboseExcp` 已在 Globals 中
- 清理残留文件: 根目录 `memmanager.pas` / `nativehooks.cpp`
- 构建验证通过 (cmulator.exe + test_deps.exe)
