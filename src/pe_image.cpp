/**
 * Cmulator - PE Image wrapper implementation
 */

#include "pe_image.h"

#include <plog/Log.h>
#include <pe-parse/parse.h>

#include <algorithm>

namespace {

struct ImportCtx {
    PEImage* img;
};

int importCallback(void* user, const peparse::VA& impAddr,
                   const std::string& modName, const std::string& symName) {
    auto* ctx = static_cast<ImportCtx*>(user);
    if (!ctx || !ctx->img) return 0;

    PEImportLib* lib = nullptr;
    std::string lowerMod = modName;
    std::transform(lowerMod.begin(), lowerMod.end(), lowerMod.begin(), ::tolower);

    for (auto& l : ctx->img->Imports) {
        std::string existing = l.dllName;
        std::transform(existing.begin(), existing.end(), existing.begin(), ::tolower);
        if (existing == lowerMod) { lib = &l; break; }
    }
    if (!lib) {
        ctx->img->Imports.push_back({modName, {}, 0});
        lib = &ctx->img->Imports.back();
    }

    PEImportEntry entry;
    entry.iatRva = static_cast<uint64_t>(impAddr);
    entry.funcName = symName;
    if (!symName.empty() && symName[0] == '#') entry.isOrdinal = true;
    lib->functions.push_back(entry);
    if (lib->iatRva == 0) lib->iatRva = entry.iatRva;
    return 0;
}

int sectionCallback(void* user, const peparse::VA& secBase,
                    const std::string& secName,
                    const peparse::image_section_header& s,
                    const peparse::bounded_buffer*) {
    auto* img = static_cast<PEImage*>(user);
    if (!img) return 0;
    img->Sections.push_back({secName, static_cast<uint64_t>(secBase), s.Misc.VirtualSize});
    return 0;
}

} // anon

bool PEImage::LoadFromFile(const std::string& path) {
    Close();
    FileName = path;
    _pe = peparse::ParsePEFromFile(path.c_str());
    if (!_pe) return false;

    auto& nt = _pe->peHeader.nt;
    Machine  = nt.FileHeader.Machine;

    if (nt.OptionalMagic == peparse::NT_OPTIONAL_32_MAGIC) {
        ImageBase     = nt.OptionalHeader.ImageBase;
        SizeOfImage   = nt.OptionalHeader.SizeOfImage;
        EntryPointRVA = nt.OptionalHeader.AddressOfEntryPoint;
        ImageWordSize = 4;
        Is64bit       = false;
    } else {
        ImageBase     = nt.OptionalHeader64.ImageBase;
        SizeOfImage   = nt.OptionalHeader64.SizeOfImage;
        EntryPointRVA = nt.OptionalHeader64.AddressOfEntryPoint;
        ImageWordSize = 8;
        Is64bit       = true;
    }

    ImportCtx impCtx{this};
    peparse::IterImpVAString(_pe, importCallback, &impCtx);
    peparse::IterSec(_pe, sectionCallback, this);

    return true;
}

void PEImage::Close() {
    if (_pe) { peparse::DestructParsedPE(_pe); _pe = nullptr; }
    Imports.clear();
    Sections.clear();
    TlsCallbacks.clear();
    ImageBase     = 0;
    SizeOfImage   = 0;
    EntryPointRVA = 0;
}
