#pragma once
#include <quickjs.h>
void Init_QJS();
void Uninit_JSEngine();
void LoadScript(const char* filename);
void InitJSEmu();
