#pragma once

#include <cstdint>
#include <unicorn/unicorn.h>

struct PSegmentDescriptor {
    // Stub
};

void init_segments(uc_engine* uc, uint64_t* gdt_address, PSegmentDescriptor** gdt);
