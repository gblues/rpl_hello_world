#include <coreinit/cache.h>
#include <memory/mappedmemory.h>

#include "library.h"

bool LibraryLoader::load() {
    if(has_executed)
        return result;
    
    has_executed = true; 
    result = false;

    parse_library_metadata();

    if(!allocate_memory())
        return false;

    init_sections();

    if(!link_sections())
        return false;

    add_relocation_data();
    if(!process_relocations())
        return false;

    resolve_exports();
    
    DCFlushRange( (void *)handle->library, handle->library_size);
    ICInvalidateRange((void *)handle->library, handle->library_size);

    if (handle->library_data.getBSSAddr() != 0) {
        memset((void *) handle->library_data.getBSSAddr(), 0, handle->library_data.getBSSSize());
    }
    if (handle->library_data.getSBSSAddr() != 0) {
        memset((void *) handle->library_data.getSBSSAddr(), 0, handle->library_data.getSBSSSize());
    }
    DCFlushRange((void *) 0x00800000, 0x00800000);
    ICInvalidateRange((void *) 0x00800000, 0x00800000);


    result = true;
    return true;
}

const char *LibraryLoader::error_message() {
    return error.c_str();
}

bool LibraryLoader::allocate_memory() {
    if(handle->library != nullptr) {
        error = "Library already appears to be allocated. handle may be in a bad state.";
        return false;
    }

    if(code_size == 0) {
        error = "No code sections found";
        return false;
    }

    handle->library = MEMAllocFromMappedMemoryEx(code_size, 0x100);
    if(handle->library == nullptr) {
        error = "Failed to allocate " + std::to_string(code_size) + " bytes of memory";
        return false;
    }
    handle->library_size = code_size;
    return true;
}

void LibraryLoader::parse_library_metadata() {
    size_t size = 0;

    for(int i = 0; i < reader.sections.size(); i++) {
        ELFIO::section *section = reader.sections[i];

        if((section->get_type() == ELFIO::SHT_PROGBITS || section->get_type() == ELFIO::SHT_NOBITS) &&
           (section->get_flags() & ELFIO::SHF_ALLOC) ) {
            size += section->get_size() + 1;
            code_sections.push_back(section);
        }

        if(section->get_type() == ELFIO::SHT_RPL_IMPORTS) {
            import_names[i] = section->get_name();
        }

        if(section->get_type() == ELFIO::SHT_REL || section->get_type() == ELFIO::SHT_RELA) {
            relocation_sections.push_back(section);
        }

        if(section->get_type() == ELFIO::SHT_RPL_EXPORTS) {
            parse_exports(section->get_data());
        }
    }

    code_size = (size + 0x100) & 0xffffff00;
}

void LibraryLoader::parse_exports(const char *export_section_data) {
    f_export_header header;
    ELFIO::endianess_convertor convertor;
    convertor.setup(reader.get_encoding());
    const char *section_base_addr = export_section_data;

    memcpy((void *)&header, export_section_data, sizeof(header));
    header.num_entries = convertor(header.num_entries);
    header.id = convertor(header.id);

    export_section_data += sizeof(header);
    for(size_t i = 0; i < header.num_entries; i++) {
        ExportData exportData(export_section_data, convertor, section_base_addr);
        export_entries.push_back(exportData);
    }
    DEBUG_FUNCTION_LINE("Found %d export symbols", export_entries.size());
}

void LibraryLoader::init_sections() {
    base_offset = (uint32_t) handle->library;
    text_offset = base_offset;
    data_offset = base_offset;
    entrypoint = text_offset + ((uint32_t) reader.get_entry() - 0x02000000);

    for(size_t i = 0; i < code_sections.size(); i++) {
        ELFIO::section *section = code_sections[i];
        uint32_t section_size = section->get_size();
        uint32_t address = (uint32_t) section->get_address();

        destinations[section->get_index()] = (uint8_t *)base_offset;

        uint32_t destination = base_offset + address;
        if ((address >= 0x02000000) && address < 0x10000000) {
            destination -= 0x02000000;
            destinations[section->get_index()] -= 0x02000000;
            base_offset += section_size;
            data_offset += section_size;
        } else if ((address >= 0x10000000) && address < 0xC0000000) {
            destination -= 0x10000000;
            destinations[section->get_index()] -= 0x10000000;
        } else if (address >= 0xC0000000) {
            DEBUG_FUNCTION_LINE("%s: Loading section from 0xC0000000 is not supported", section->get_name().c_str());
            continue;
        } else {
            DEBUG_FUNCTION_LINE("Don't know what to do with address: 0x%08x", address);
        }

        if(section->get_type() == ELFIO::SHT_NOBITS) {
            DEBUG_FUNCTION_LINE("%s: Zeroing SHT_NOBITS section (0x%08x-%08x)", 
                section->get_name().c_str(),
                destination,
                destination+section_size);
            memset((void *)destination, 0, section_size);
        } else if(section->get_type() == ELFIO::SHT_PROGBITS) {
            DEBUG_FUNCTION_LINE("%s: Copying SHT_PROGBITS section (0x%08x-%08x)", 
                section->get_name().c_str(),
                destination,
                destination+section_size);
            memcpy((void *)destination, section->get_data(), section_size);
        }

        if(section->get_name() == ".bss") {
            handle->library_data.setBSSLocation(destination, section_size);
        } else if(section->get_name() == ".sbss") {
            handle->library_data.setSBSSLocation(destination, section_size);
        }

        DCFlushRange((void *) destination, section_size);
        ICInvalidateRange((void *) destination, section_size);
    }
}

bool LibraryLoader::link_sections() {
    for(size_t i = 0; i < code_sections.size(); i++) {
        ELFIO::section *section = code_sections[i];
        if(!link_section(section->get_index())) {
            error = "Failed to link section at index " + std::to_string(section->get_index());
            return false;
        }
    }

    return true;
}

bool LibraryLoader::link_section(uint32_t section_index) {
    uint32_t destination = (uint32_t)destinations[section_index];
    int failure_count = 0;

    for(uint32_t i = 0; i < reader.sections.size(); i++) {
        ELFIO::section *section = reader.sections[i];

        if(section->get_info() == section_index) {
            ELFIO::relocation_section_accessor rel(reader, section);
            for(uint32_t entry = 0; entry < rel.get_entries_num(); entry++) {
                ELFIO::Elf64_Addr offset;
                ELFIO::Elf_Word type;
                ELFIO::Elf_Sxword addend;
                std::string sym_name;
                ELFIO::Elf64_Addr sym_value;
                ELFIO::Elf_Sxword sym_section_index;

                if (!rel.get_entry(entry, offset, sym_value, sym_name, type, addend, sym_section_index)) {
                    break;
                }

                auto adjusted_sym_value = (uint32_t) sym_value;
                auto adjusted_sym_index = (uint32_t) sym_section_index;
                if ((adjusted_sym_value >= 0x02000000) && adjusted_sym_value < 0x10000000) {
                    adjusted_sym_value -= 0x02000000;
                    adjusted_sym_value += text_offset;
                } else if ((adjusted_sym_value >= 0x10000000) && adjusted_sym_value < 0xC0000000) {
                    adjusted_sym_value -= 0x10000000;
                    adjusted_sym_value += data_offset;
                } else if (adjusted_sym_value >= 0xC0000000) {
                    // Skip imports
                    continue;
                } else if (adjusted_sym_value != 0x0) {
                    DEBUG_FUNCTION_LINE("Bad adjusted_sym_value: 0x%08x", adjusted_sym_value);
                    return false;
                }

                if( ((adjusted_sym_index & ELFIO::SHN_LORESERVE) == ELFIO::SHN_LORESERVE) && adjusted_sym_index != ELFIO::SHN_ABS) {
                    DEBUG_FUNCTION_LINE("sym_section_index (%d) > ELFIO::SHN_LORESERVE (%08x) && sym_section_index != ELFIO::SHN_ABS (%08x)", sym_section_index, ELFIO::SHN_LORESERVE, ELFIO::SHN_ABS);
                    return false;
                }

                if(!ElfUtils::elfLinkOne(type, offset, addend, destination, adjusted_sym_value, nullptr, 0, RELOC_TYPE_FIXED)) {
                    failure_count++;
                }
            }
        }
    }

    if(failure_count > 0) {
        DEBUG_FUNCTION_LINE("encountered %d link failures", failure_count);
    }

    return true;
}

void LibraryLoader::add_relocation_data() {
    for(uint32_t i = 0; i < relocation_sections.size(); i++) {
        ELFIO::section *section = relocation_sections[i];
        ELFIO::relocation_section_accessor rel(reader, section);

        for(uint32_t entry = 0; entry < rel.get_entries_num(); entry++) {
                ELFIO::Elf64_Addr offset;
                ELFIO::Elf_Word type;
                ELFIO::Elf_Sxword addend;
                std::string sym_name;
                ELFIO::Elf64_Addr sym_value;
                ELFIO::Elf_Sxword sym_section_index;

                if (!rel.get_entry(entry, offset, sym_value, sym_name, type, addend, sym_section_index)) {
                    break;
                }

                auto adjusted_sym_value = (uint32_t) sym_value;
                if (adjusted_sym_value < 0xC0000000) {
                    continue;
                }
                std::optional<ImportRPLInformation> rplInfo = ImportRPLInformation::createImportRPLInformation(import_names[sym_section_index]);
                if (!rplInfo) {
                    break;
                }

                uint32_t section_index = section->get_info();

                RelocationData relocationData(type, offset - 0x02000000, addend, (void *) (destinations[section_index] + 0x02000000), sym_name, rplInfo.value());
                handle->library_data.addRelocationData(relocationData);
        }
    }
}

bool LibraryLoader::process_relocations() {
    std::vector<RelocationData> relocData = handle->library_data.getRelocationDataList();
    DEBUG_FUNCTION_LINE("Processing %d relocations", relocData.size());
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
            error = "Failed to find export " + functionName + " in library " + rplName;
            return false;
        }
        if (!ElfUtils::elfLinkOne(cur.getType(), cur.getOffset(), cur.getAddend(), (uint32_t) cur.getDestination(), functionAddress, nullptr, 0, RELOC_TYPE_IMPORT)) {
            error = "Failed to link export " + functionName + " in library " + rplName;
            return false;
        }
    }
    return true;
}

void LibraryLoader::resolve_exports() {
    for(auto const &entry : export_entries) {
        DEBUG_FUNCTION_LINE("export: %s => 0x%08x", entry.getName().c_str(), entry.getFunctionOffset());
        handle->exports[entry.getName()] = text_offset + entry.getFunctionOffset();
    }
}
