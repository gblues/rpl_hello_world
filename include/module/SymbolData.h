#pragma once

#include "elfio/elfio.hpp"
#include <string>

class SymbolData {
    public:
    SymbolData(const ELFIO::symbol_section_accessor &accessor, int symbol) {
        accessor.get_symbol( symbol, name, offset, size, bind, type, section_index, other );
        _fixup_address(offset);
    }

    [[nodiscard]] std::string getName() const {
        return name;
    }

    [[nodiscard]] char getType() const {
        return type;
    }

    [[nodiscard]] char getBind() const {
        return bind;
    }

    [[nodiscard]] char getOther() const {
        return other;
    }

    [[nodiscard]] uint32_t getOffset() const {
        return (uint32_t) offset;
    }

    [[nodiscard]] size_t getSize() const {
        return (size_t) size;
    }

    [[nodiscard]] int getSectionIndex() const {
        return (int) section_index;
    }

    private:
    void _fixup_address(ELFIO::Elf64_Addr &address) {
        uint32_t base_address = (uint32_t)section_base_address;

        if ((address >= 0x02000000) && address < 0x10000000) {
            address -= 0x02000000;
        }
        else if ((address >= 0x10000000) && address < 0xC0000000) {
            address -= 0x10000000;
    }
}
    std::string name;
    ELFIO::Elf64_Addr offset;
    ELFIO::Elf_Xword size;
    unsigned char bind;
    unsigned char type;
    ELFIO::Elf_Half section_index;
    unsigned char other;
};