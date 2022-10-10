#include <coreinit/cache.h>
#include <map>
#include <string>
#include <vector>

#include "dlfcn.h"
#include "elfio/elfio.hpp"
#include "utils/FileUtils.h"
#include "module/ModuleData.h"
#include "ElfUtils.h"

#define LAST_ERROR_LEN 256

static char last_error[LAST_ERROR_LEN] = {};
static bool has_error = false;

static const char *ERR_LOAD_FILE_TO_MEM = "Failed to read library '%s' into memory";
static const char *ERR_ELFIO_READ = "Failed to parse ELF data";
static const char *ERR_DESTINATIONS_ALLOC = "Failed to allocate memory for destinations array";
static const char *ERR_MODULE_ALLOC = "Failed to allocate memory for module";

static size_t _getModuleSize(ELFIO::elfio &reader);
static bool _load(ELFIO::elfio &reader, dl_handle &handle, ModuleData &moduleData);
static bool _linkSection(ELFIO::elfio &reader, uint32_t idx, uint32_t destination, uint32_t base_text, uint32_t base_data, relocation_trampolin_entry_t *trampoline_data, uint32_t trampolin_data_length);
static std::vector<RelocationData> _getImportRelocationData(const ELFIO::elfio &reader, uint8_t **destinations);

std::unique_ptr<dl_handle> dlopen(const char *filename) {
    ELFIO::elfio reader;
    ModuleData moduleData;
    uint8_t *buffer = nullptr;
    uint32_t fsize  = 0;
    dl_handle handle;

    if (LoadFileToMem(filename, &buffer, &fsize) < 0) {
        snprintf(last_error, LAST_ERROR_LEN, ERR_LOAD_FILE_TO_MEM, filename);
        goto error;
    }

    if(!reader.load(reinterpret_cast<char *>(buffer), fsize)) {
        snprintf(last_error, LAST_ERROR_LEN, "%s", ERR_ELFIO_READ);
        goto error;
    }

    handle.library_size = _getModuleSize(reader);
    handle.library = aligned_alloc(0x100, handle.library_size);
    if(!handle.library) {
        snprintf(last_error, LAST_ERROR_LEN, "%s", ERR_MODULE_ALLOC);
        goto error;
    }

    if(!_load(reader, handle, moduleData)) {
        // NB: _load() will set the error message so we won't set it here
        goto error;
    }

    free(buffer);
    return std::unique_ptr<dl_handle>(&handle);

    error:

    has_error = true;
    if(buffer)
        free(buffer);

    return nullptr;
}

void *dlsym(std::unique_ptr<ModuleData> &handle, const char *symbol) {
    return NULL;
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

static bool _load(ELFIO::elfio &reader, dl_handle &handle, ModuleData &moduleData) {
    uint32_t baseOffset = (uint32_t)handle.library;
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
                moduleData.setBSSLocation(destination, sectionSize);
                // DEBUG_FUNCTION_LINE("Saved %s section info. Location: %08X size: %08X", psec->get_name().c_str(), destination, sectionSize);
            } else if (psec->get_name() == ".sbss") {
                moduleData.setSBSSLocation(destination, sectionSize);
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
        if ((psec->get_type() == SHT_PROGBITS || psec->get_type() == SHT_NOBITS) && (psec->get_flags() & SHF_ALLOC)) {
            DEBUG_FUNCTION_LINE("Linking (%d)... %s", i, psec->get_name().c_str());
            if (!_linkSection(reader, psec->get_index(), (uint32_t) destinations[psec->get_index()], offset_text, offset_data, handle.trampolines, DYN_LINK_TRAMPOLIN_LIST_LENGTH)) {
                snprintf(last_error, LAST_ERROR_LEN, "Failed to link %d: %s", i, psec->get_name().c_str());
                goto error;
            }
        }
    }

    relocationData = _getImportRelocationData(reader, destinations);

    for (auto const &reloc : relocationData) {
        moduleData.addRelocationData(reloc);
    }

    DCFlushRange((void *) baseOffset, totalSize);
    ICInvalidateRange((void *) baseOffset, totalSize);

    moduleData.setEntrypoint(entrypoint);

    free(destinations);
    return true;

    error:
    has_error = true;
    
    if(destinations)
        free(destinations);

    return false;
}

static bool _linkSection(ELFIO::elfio &reader, uint32_t section_index, uint32_t destination, uint32_t base_text, uint32_t base_data, relocation_trampolin_entry_t *trampoline_data, uint32_t trampolin_data_length) {
    uint32_t sec_num = reader.sections.size();

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

                if (!ElfUtils::elfLinkOne(type, offset, addend, destination, adjusted_sym_value, trampoline_data, trampolin_data_length, RELOC_TYPE_FIXED)) {
                    return false;
                }
            }
        }
    }
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