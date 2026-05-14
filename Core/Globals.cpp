#include "Globals.h"

const char* CM_VERSION = "v0.3.0";
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
JSValue JSEmu = 0;

std::u16string win32_dir = u"";
std::u16string win64_dir = u"";

std::string JSAPI = "";
std::u16string ApiSetSchemaPath = u"";
