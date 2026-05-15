#include "Globals.h"

uint64_t microseconds = 1000000;

bool VerboseExcp = false;
bool Verbose = false;
bool VerboseEx = false;
bool VerboseExx = false;
bool Speed = false;
bool ShowASM = false;
bool InterActive = false;

uint64_t Steps_limit = 4000000;
uint64_t Steps = 0;

TEmu* Emulator = nullptr;

JSRuntime* rt = nullptr;
JSContext* ctx = nullptr;
JSValue JSEmu = JS_UNDEFINED;

UnicodeString win32_dir = L"";
UnicodeString win64_dir = L"";

std::string JSAPI = "";
UnicodeString ApiSetSchemaPath = L"";
