/**
 * test_deps.cpp - Cmulator dependency verification test
 *
 * Tests:
 *   1. Unicorn Engine  - CPU emulation
 *   2. Zydis           - x86/x86-64 disassembler
 *   3. QuickJS (qjs)   - JavaScript engine
 *   4. xxHash          - fast hash algorithm
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Unicorn
#include <unicorn/unicorn.h>

static bool test_unicorn() {
    printf("[TEST] Unicorn Engine ... ");

    uc_engine *uc = nullptr;
    uc_err err;

    err = uc_open(UC_ARCH_X86, UC_MODE_32, &uc);
    if (err != UC_ERR_OK) {
        printf("FAILED (uc_open: %s)\n", uc_strerror(err));
        return false;
    }

    err = uc_mem_map(uc, 0x400000, 2 * 1024 * 1024, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        printf("FAILED (uc_mem_map: %s)\n", uc_strerror(err));
        uc_close(uc);
        return false;
    }

    uint8_t code[] = { 0xC3 };  // ret
    err = uc_mem_write(uc, 0x400000, code, sizeof(code));
    if (err != UC_ERR_OK) {
        printf("FAILED (uc_mem_write: %s)\n", uc_strerror(err));
        uc_close(uc);
        return false;
    }

    // Map stack memory at 0x700000
    err = uc_mem_map(uc, 0x700000, 0x10000, UC_PROT_ALL);
    if (err != UC_ERR_OK) {
        printf("FAILED (uc_mem_map stack: %s)\n", uc_strerror(err));
        uc_close(uc);
        return false;
    }

    // Set ESP and write a return address so ret jumps to 0x400001 (until addr)
    uint32_t esp_val = 0x700FFC;
    uint32_t ret_addr = 0x400001;
    err = uc_mem_write(uc, esp_val, &ret_addr, sizeof(ret_addr));
    if (err != UC_ERR_OK) {
        printf("FAILED (uc_mem_write ret_addr: %s)\n", uc_strerror(err));
        uc_close(uc);
        return false;
    }
    err = uc_reg_write(uc, UC_X86_REG_ESP, &esp_val);
    if (err != UC_ERR_OK) {
        printf("FAILED (uc_reg_write ESP: %s)\n", uc_strerror(err));
        uc_close(uc);
        return false;
    }

    uint32_t eip_val = 0x400000;
    err = uc_reg_write(uc, UC_X86_REG_EIP, &eip_val);
    if (err != UC_ERR_OK) {
        printf("FAILED (uc_reg_write: %s)\n", uc_strerror(err));
        uc_close(uc);
        return false;
    }

    err = uc_emu_start(uc, 0x400000, 0x400001, 0, 1);
    if (err != UC_ERR_OK) {
        printf("FAILED (uc_emu_start: %s)\n", uc_strerror(err));
        uc_close(uc);
        return false;
    }

    uc_close(uc);
    printf("OK\n");
    return true;
}

// Zydis
#include <Zydis/Zydis.h>

static bool test_zydis() {
    printf("[TEST] Zydis Decoder ... ");

    ZydisDecoder decoder;
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

    uint8_t nop[] = { 0x90 };  // nop
    ZydisDecodedInstruction insn;
    ZyanStatus status = ZydisDecoderDecodeInstruction(
        &decoder, nullptr, nop, sizeof(nop), &insn);

    if (!ZYAN_SUCCESS(status)) {
        printf("FAILED (decode: 0x%08X)\n", status);
        return false;
    }

    if (insn.mnemonic != ZYDIS_MNEMONIC_NOP) {
        printf("FAILED (expected NOP, got 0x%04X)\n", insn.mnemonic);
        return false;
    }

    printf("OK  (decoded: NOP, length=%d)\n", (int)insn.length);
    return true;
}

// QuickJS
#include <quickjs.h>

static bool test_quickjs() {
    printf("[TEST] QuickJS Engine ... ");

    JSRuntime *rt = JS_NewRuntime();
    if (!rt) {
        printf("FAILED (JS_NewRuntime)\n");
        return false;
    }

    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) {
        printf("FAILED (JS_NewContext)\n");
        JS_FreeRuntime(rt);
        return false;
    }

    const char *js_code = "1 + 2";
    JSValue result = JS_Eval(ctx, js_code, (int)strlen(js_code), "<test>",
                             JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        printf("FAILED (JS_Eval threw exception)\n");
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return false;
    }

    int value = 0;
    if (JS_ToInt32(ctx, &value, result) != 0 || value != 3) {
        printf("FAILED (expected 3, got %d)\n", value);
        JS_FreeValue(ctx, result);
        JS_FreeContext(ctx);
        JS_FreeRuntime(rt);
        return false;
    }

    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    printf("OK  (1+2 = %d)\n", value);
    return true;
}

// xxHash
#define XXH_INLINE_ALL
#include <xxhash.h>

static bool test_xxhash() {
    printf("[TEST] xxHash ... ");

    const char *input = "Cmulator";
    XXH64_hash_t hash = XXH64(input, strlen(input), 0);

    if (hash == 0) {
        printf("FAILED (hash is zero)\n");
        return false;
    }

    printf("OK  (XXH64(\"Cmulator\") = 0x%016llX)\n",
           (unsigned long long)hash);
    return true;
}

// Main
int main() {
    printf("\n=== Cmulator Dependency Test ===\n\n");

    int passed = 0;
    int total  = 4;

    if (test_unicorn())  passed++;
    if (test_zydis())    passed++;
    if (test_quickjs())  passed++;
    if (test_xxhash())   passed++;

    printf("\n=== Result: %d/%d tests passed ===\n", passed, total);

    return (passed == total) ? 0 : 1;
}
