#pragma once

#include "library.h"
#include "../elfio/elfio.hpp"

class ExportData {
    public:
    ExportData(const char *&export_section_data, ELFIO::endianess_convertor &convertor, const char *base_addr) {
        function_offset = read_uint32_t(export_section_data, convertor);
        uint32_t name_offset = read_uint32_t(export_section_data, convertor);

        if(function_offset > 0x02000000 && function_offset < 0x10000000)
            function_offset -= 0x02000000;
        else if(function_offset >= 0x10000000 && function_offset < 0xC0000000)
            function_offset -= 0x10000000;

        name = std::string(base_addr + name_offset);
    }

    [[nodiscard]] uint32_t getFunctionOffset() const {
        return function_offset;
    }

    [[nodiscard]] std::string getName() const {
        return name;
    }

    private:
    uint32_t read_uint32_t(const char *&export_section_data, ELFIO::endianess_convertor &convertor) {
        union _int32buffer { uint32_t word; char buf[4]; } int32buffer = {0};
        memcpy(int32buffer.buf, export_section_data, 4);
        export_section_data += 4;
        int32buffer.word = convertor(int32buffer.word);

        return int32buffer.word;
    }
    uint32_t function_offset;
    std::string name;
};