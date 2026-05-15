#pragma once

#include <cstdint>
#include <string>
#include "types.h"
#include <quickjs.h>

// Forward declarations
class TEmu;

extern uint64_t microseconds;

extern bool VerboseExcp;
extern bool Verbose;
extern bool VerboseEx;
extern bool VerboseExx;
extern bool Speed;
extern bool ShowASM;
extern bool InterActive;

extern uint64_t Steps_limit;
extern uint64_t Steps;

extern TEmu* Emulator;

extern JSRuntime* rt;
extern JSContext* ctx;
extern JSValue JSEmu;

// Pascal's UnicodeString maps roughly to std::u16string or std::wstring
extern UnicodeString win32_dir; // 'win32' is a macro on windows, renamed to win32_dir
extern UnicodeString win64_dir; // 'win64' renamed to win64_dir

// Pascal's AnsiString maps to std::string
extern std::string JSAPI;
extern UnicodeString ApiSetSchemaPath;
extern uint64_t Ident;
