#pragma once

#ifdef _WIN32
#include <windows.h>
#else
// On non-Windows platforms, we would need to define the PE structures manually or use a library like LIEF/pelite.
// For now, assuming Windows build or providing placeholders.
#include <cstdint>

// Minimal placeholders for non-Windows compilation to succeed if needed
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t BYTE;
typedef uint64_t ULONGLONG;
typedef void* PVOID;

// ... other PE structures would go here for cross-platform ...
#endif
