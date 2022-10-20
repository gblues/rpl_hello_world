#include <coreinit/cache.h>
#include <map>
#include <string>
#include <vector>

#include <whb/log_console.h>
#include <memory/mappedmemory.h>

#include "dlfcn.h"
#include "elfio/elfio.hpp"
#include "utils/FileUtils.h"
#include "module/ModuleData.h"
#include "module/SymbolData.h"
#include "ElfUtils.h"
#include <coreinit/dynload.h>


#define LAST_ERROR_LEN 256

static char last_error[LAST_ERROR_LEN] = {};
static bool has_error = false;

static const char *ERR_LOAD_FILE_TO_MEM = "Failed to read library '%s' into memory";
static const char *ERR_ELFIO_READ = "Failed to parse ELF data";
static const char *ERR_DESTINATIONS_ALLOC = "Failed to allocate memory for destinations array";
static const char *ERR_MODULE_ALLOC = "Failed to allocate memory for module";

static size_t _getModuleSize(ELFIO::elfio &reader);
static bool _load(ELFIO::elfio &reader, dl_handle *handle);
static bool _init(ELFIO::elfio &reader, dl_handle *handle);
static std::vector<RelocationData> _getImportRelocationData(const ELFIO::elfio &reader, uint8_t **destinations);
static bool _doRelocation(dl_handle *handle);
static bool _linkSection(ELFIO::elfio &reader, uint32_t idx, uint32_t destination, uint32_t base_text, uint32_t base_data);



std::unique_ptr<dl_handle> dlopen(const char *filename) {
    dl_handle *handle = new dl_handle();
    auto result = std::unique_ptr<dl_handle>(handle);
    ELFIO::elfio reader;
    uint8_t *buffer = nullptr;
    uint32_t fsize  = 0;

    if (LoadFileToMem(filename, &buffer, &fsize) < 0) {
        snprintf(last_error, LAST_ERROR_LEN, ERR_LOAD_FILE_TO_MEM, filename);
        goto error;
    }

    if(!reader.load(reinterpret_cast<char *>(buffer), fsize)) {
        snprintf(last_error, LAST_ERROR_LEN, "%s", ERR_ELFIO_READ);
        goto error;
    }

    handle->library_size = _getModuleSize(reader);
    handle->library = MEMAllocFromMappedMemoryEx(handle->library_size, 0x100);
    if(!handle->library) {
        snprintf(last_error, LAST_ERROR_LEN, "%s", ERR_MODULE_ALLOC);
        goto error;
    }

    if(!_load(reader, handle)) {
        // NB: _load() will set the error message so we won't set it here
        goto error;
    }

    if(!_init(reader, handle)) {
        goto error;
    }

    free(buffer);

    handle->entrypoint = (rpl_entrypoint_fn)dlsym(result, "rpl_entry");
    if(handle->entrypoint && !handle->entrypoint(nullptr, OS_DYNLOAD_LOADED)) {
        snprintf(last_error, LAST_ERROR_LEN, "library failed to initialize");
        goto error;
    }

    return result;

    error:

    has_error = true;

    if(buffer)
        free(buffer);

    return nullptr;
}

void *dlsym(std::unique_ptr<dl_handle> &handle, const char *symbol) {
    uint32_t base_address = (uint32_t) handle->library;

    if(handle == nullptr) {
        snprintf(last_error, LAST_ERROR_LEN, "invalid handle");
        goto error;
    }
    if(symbol == nullptr || *symbol == '\0') {
        snprintf(last_error, LAST_ERROR_LEN, "Null or empty symbol");
        goto error;
    }

    for(auto const &func : handle->module_data.getGlobalFunctionList()) {

        if(!strcmp(symbol, func.getName().c_str())) {
            uint32_t offset = func.getOffset();
            
            if(offset >= 0xC0000000) {
                snprintf(last_error, LAST_ERROR_LEN, "symbol %s: loading from section 0xC00000000 is NOT supported.", symbol);
                goto error;
            }
            return (void *)(base_address + offset);
        }
    }
    snprintf(last_error, LAST_ERROR_LEN, "No symbol matching '%s' found", symbol);

    error:
    has_error = true;
    return nullptr;
}

char *dlerror() {
    char *error_message = has_error ? &last_error[0] : nullptr;
    has_error = false;

    return error_message;
}

static size_t _getModuleSize(ELFIO::elfio &reader) {
    uint32_t count = reader.sections.size();
    size_t size = 0;

    for(uint32_t i = 0; i < count; i++) {
        ELFIO::section *psec = reader.sections[i];
        if(psec->get_type() == 0x80000002) {
            continue;
        }
        if ((psec->get_type() == SHT_PROGBITS || psec->get_type() == SHT_NOBITS) && (psec->get_flags() & SHF_ALLOC)) {
            size += psec->get_size() + 1;
        }
    }

    return (size + 0x100) & 0xffffff00;
}

static bool _load(ELFIO::elfio &reader, dl_handle *handle) {
    uint32_t baseOffset = (uint32_t)handle->library;
    uint32_t offset_text = baseOffset;
    uint32_t offset_data = offset_text;
    uint32_t entrypoint = offset_text + (uint32_t) reader.get_entry() - 0x02000000;
    uint32_t totalSize = 0;
    std::vector<RelocationData> relocationData;

    uint8_t **destinations;
    size_t sec_num = reader.sections.size();

    destinations = (uint8_t **) malloc(sizeof(uint8_t *) * sec_num);
    if(!destinations) {
        snprintf(last_error, LAST_ERROR_LEN, "%s", ERR_DESTINATIONS_ALLOC);
        goto error;
    }
    
    for(uint32_t i = 0; i < sec_num; i++) {
        ELFIO::section *psec = reader.sections[i];

        if (psec->get_type() == 0x80000002 || psec->get_name() == ".wut_load_bounds") {
            continue;
        }

        if ((psec->get_type() == SHT_PROGBITS || psec->get_type() == SHT_NOBITS) && (psec->get_flags() & SHF_ALLOC)) {
            uint32_t sectionSize = psec->get_size();
            auto address         = (uint32_t) psec->get_address();

            destinations[psec->get_index()] = (uint8_t *) baseOffset;

            uint32_t destination = baseOffset + address;
            if ((address >= 0x02000000) && address < 0x10000000) {
                destination -= 0x02000000;
                destinations[psec->get_index()] -= 0x02000000;
                baseOffset += sectionSize;
                offset_data += sectionSize;
            } else if ((address >= 0x10000000) && address < 0xC0000000) {
                destination -= 0x10000000;
                destinations[psec->get_index()] -= 0x10000000;
            } else if (address >= 0xC0000000) {
                snprintf(last_error, LAST_ERROR_LEN, "%s", "Loading section from 0xC0000000 is NOT supported");
                goto error;
            } else {
                snprintf(last_error, LAST_ERROR_LEN, "Don't know what to do with address 0x%08x", address);
                goto error;
            }

            const char *p = reader.sections[i]->get_data();

            if (psec->get_type() == SHT_NOBITS) {
                // DEBUG_FUNCTION_LINE("memset section %s %08X [%08X] to 0 (%d bytes)", psec->get_name().c_str(), destination, destination + sectionSize, sectionSize);
                memset((void *) destination, 0, sectionSize);
            } else if (psec->get_type() == SHT_PROGBITS) {
                // DEBUG_FUNCTION_LINE("Copy section %s %08X -> %08X [%08X] (%d bytes)", psec->get_name().c_str(), p, destination, destination + sectionSize, sectionSize);
                memcpy((void *) destination, p, sectionSize);
            }

            //nextAddress = ROUNDUP(destination + sectionSize,0x100);
            if (psec->get_name() == ".bss") {
                handle->module_data.setBSSLocation(destination, sectionSize);
                // DEBUG_FUNCTION_LINE("Saved %s section info. Location: %08X size: %08X", psec->get_name().c_str(), destination, sectionSize);
            } else if (psec->get_name() == ".sbss") {
                handle->module_data.setSBSSLocation(destination, sectionSize);
                // DEBUG_FUNCTION_LINE("Saved %s section info. Location: %08X size: %08X", psec->get_name().c_str(), destination, sectionSize);
            }
            totalSize += sectionSize;

            // DEBUG_FUNCTION_LINE("DCFlushRange %08X - %08X", destination, destination + sectionSize);
            DCFlushRange((void *) destination, sectionSize);
            // DEBUG_FUNCTION_LINE("ICInvalidateRange %08X - %08X", destination, destination + sectionSize);
            ICInvalidateRange((void *) destination, sectionSize);
        }
    }

    for (uint32_t i = 0; i < sec_num; ++i) {
        ELFIO::section *psec = reader.sections[i];
        if(psec->get_type() == SHT_SYMTAB) {
            const ELFIO::symbol_section_accessor symbols( reader, psec );
            
            for ( unsigned int j = 0; j < symbols.get_symbols_num(); ++j ) {
                SymbolData sym = SymbolData(symbols, j);
                if(sym.getBind() == STB_GLOBAL && sym.getType() == STT_FUNC) {
                    handle->module_data.addGlobalFunction(sym);
                }
            }
        }
    }

    for (uint32_t i = 0; i < sec_num; ++i) {
        ELFIO::section *psec = reader.sections[i];
        if ((psec->get_type() == SHT_PROGBITS || psec->get_type() == SHT_NOBITS) && (psec->get_flags() & SHF_ALLOC)) {
            DEBUG_FUNCTION_LINE("Linking (%d)... %s", i, psec->get_name().c_str());
            if (!_linkSection(reader, psec->get_index(), (uint32_t) destinations[psec->get_index()], offset_text, offset_data)) {
                snprintf(last_error, LAST_ERROR_LEN, "Failed to link %d: %s", i, psec->get_name().c_str());
                goto error;
            }
        }
    }

    relocationData = _getImportRelocationData(reader, destinations);

    for (auto const &reloc : relocationData) {
        handle->module_data.addRelocationData(reloc);
    }

    DCFlushRange((void *) baseOffset, totalSize);
    ICInvalidateRange((void *) baseOffset, totalSize);

    handle->module_data.setEntrypoint(entrypoint);

    free(destinations);
    return true;

    error:
    has_error = true;
    
    if(destinations)
        free(destinations);

    return false;
}

static bool _init(ELFIO::elfio &reader, dl_handle *handle) {
        if (!_doRelocation(handle)) {
            DEBUG_FUNCTION_LINE("relocations failed");
        }

        if (handle->module_data.getBSSAddr() != 0) {
            memset((void *) handle->module_data.getBSSAddr(), 0, handle->module_data.getBSSSize());
        }
        if (handle->module_data.getSBSSAddr() != 0) {
            memset((void *) handle->module_data.getSBSSAddr(), 0, handle->module_data.getSBSSSize());
        }
        DCFlushRange((void *) 0x00800000, 0x00800000);
        ICInvalidateRange((void *) 0x00800000, 0x00800000);
        // return handle->library.getEntrypoint();
        return true;
}

static std::vector<RelocationData> _getImportRelocationData(const ELFIO::elfio &reader, uint8_t **destinations) {
    std::vector<RelocationData> result;
    std::map<uint32_t, std::string> infoMap;

    uint32_t sec_num = reader.sections.size();

    for (uint32_t i = 0; i < sec_num; ++i) {
        ELFIO::section *psec = reader.sections[i];
        if (psec->get_type() == 0x80000002) {
            infoMap[i] = psec->get_name();
        }
    }

    for (uint32_t i = 0; i < sec_num; ++i) {
        ELFIO::section *psec = reader.sections[i];
        if (psec->get_type() == SHT_RELA || psec->get_type() == SHT_REL) {
            ELFIO::relocation_section_accessor rel(reader, psec);
            for (uint32_t j = 0; j < (uint32_t) rel.get_entries_num(); ++j) {
                ELFIO::Elf64_Addr offset;
                ELFIO::Elf_Word type;
                ELFIO::Elf_Sxword addend;
                std::string sym_name;
                ELFIO::Elf64_Addr sym_value;
                ELFIO::Elf_Half sym_section_index;

                if (!rel.get_entry(j, offset, sym_value, sym_name, type, addend, sym_section_index)) {
                    break;
                }

                auto adjusted_sym_value = (uint32_t) sym_value;
                if (adjusted_sym_value < 0xC0000000) {
                    continue;
                }
                std::optional<ImportRPLInformation> rplInfo = ImportRPLInformation::createImportRPLInformation(infoMap[sym_section_index]);
                if (!rplInfo) {
                    break;
                }

                uint32_t section_index = psec->get_info();

                // When these relocations are performed, we don't need the 0xC0000000 offset anymore.
                RelocationData relocationData(type, offset - 0x02000000, addend, (void *) (destinations[section_index] + 0x02000000), sym_name, rplInfo.value());
                //relocationData->printInformation();
                result.push_back(relocationData);
            }
        }
    }
    return result;
}

static bool _doRelocation(dl_handle *handle) {
    std::vector<RelocationData> relocData = handle->module_data.getRelocationDataList();

    for (auto const &curReloc : relocData) {
        const RelocationData &cur  = curReloc;
        std::string functionName   = cur.getName();
        std::string rplName        = cur.getImportRPLInformation().getName();
        int32_t isData             = cur.getImportRPLInformation().isData();


        OSDynLoad_Module rplHandle = nullptr;
        OSDynLoad_Acquire(rplName.c_str(), &rplHandle);

        uint32_t functionAddress = 0;
        OSDynLoad_FindExport(rplHandle, isData, functionName.c_str(), (void **) &functionAddress);
        if (functionAddress == 0) {
            return false;
        }
        if (!ElfUtils::elfLinkOne(cur.getType(), cur.getOffset(), cur.getAddend(), (uint32_t) cur.getDestination(), functionAddress, nullptr, 0, RELOC_TYPE_IMPORT)) {
            DEBUG_FUNCTION_LINE("Relocation failed");
            return false;
        }
    }
    return true;
}

static bool _linkSection(ELFIO::elfio &reader, uint32_t section_index, uint32_t destination, uint32_t base_text, uint32_t base_data) {
    uint32_t sec_num = reader.sections.size();
    uint32_t failure_count = 0;

    for (uint32_t i = 0; i < sec_num; ++i) {
        ELFIO::section *psec = reader.sections[i];
        if (psec->get_info() == section_index) {
            ELFIO::relocation_section_accessor rel(reader, psec);
            for (uint32_t j = 0; j < (uint32_t) rel.get_entries_num(); ++j) {
                ELFIO::Elf64_Addr offset;
                ELFIO::Elf_Word type;
                ELFIO::Elf_Sxword addend;
                std::string sym_name;
                ELFIO::Elf64_Addr sym_value;
                ELFIO::Elf_Half sym_section_index;

                if (!rel.get_entry(j, offset, sym_value, sym_name, type, addend, sym_section_index)) {
                    break;
                }

                auto adjusted_sym_value = (uint32_t) sym_value;
                if ((adjusted_sym_value >= 0x02000000) && adjusted_sym_value < 0x10000000) {
                    adjusted_sym_value -= 0x02000000;
                    adjusted_sym_value += base_text;
                } else if ((adjusted_sym_value >= 0x10000000) && adjusted_sym_value < 0xC0000000) {
                    adjusted_sym_value -= 0x10000000;
                    adjusted_sym_value += base_data;
                } else if (adjusted_sym_value >= 0xC0000000) {
                    // Skip imports
                    continue;
                } else if (adjusted_sym_value == 0x0) {
                    //
                } else {
                    return false;
                }

                if (sym_section_index == SHN_ABS) {
                    //
                } else if (sym_section_index > SHN_LORESERVE) {
                    return false;
                }

                if (!ElfUtils::elfLinkOne(type, offset, addend, destination, adjusted_sym_value, nullptr, 0, RELOC_TYPE_FIXED)) {
                    failure_count++;
                }
            }
        }
    }
    if(failure_count > 0) {
        WHBLogPrintf("WARN: %d link failures occurred\n", failure_count);
    }
    return true;
}
