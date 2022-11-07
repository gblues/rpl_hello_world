#pragma once

#include <memory>

#include <zlib.h>
#include "elfio/elfio_utils.hpp"
#include "logger.h"

class wiiu_zlib : public ELFIO::wiiu_zlib_interface {
    public:
    std::unique_ptr<char[]> inflate(const char *data, const ELFIO::endianess_convertor *convertor, ELFIO::Elf_Xword compressed_size, ELFIO::Elf_Xword &uncompressed_size) const {
        z_stream s = { 0 };
        int z_result = 0;

        s.zalloc = Z_NULL;
        s.zfree = Z_NULL;
        s.opaque = Z_NULL;
        ELFIO::Elf_Xword actual_size = uncompressed_size;

        if(!parse_actual_size(data, convertor, compressed_size, actual_size)) {
            DEBUG_FUNCTION_LINE("Failed to parse actual size");
            return nullptr;
        }

        const char *compressed_data = data + 4;
        auto uncompressed_data = std::unique_ptr<char[]>(new char[actual_size+1]());
        if(uncompressed_data == nullptr) {
            DEBUG_FUNCTION_LINE("error allocating %d bytes of memory for uncompressed section\n", actual_size+1);
            return nullptr;
        }

        if(Z_OK != (z_result = inflateInit_(&s, ZLIB_VERSION, sizeof(s)))) {
            DEBUG_FUNCTION_LINE("error initializing zlib: %d\n", z_result);
            return nullptr;
        }

        s.avail_in = compressed_size;
        s.next_in = (Bytef *)compressed_data;
        s.avail_out = actual_size;
        s.next_out = (Bytef *)uncompressed_data.get();

        z_result = ::inflate(&s, Z_FINISH);
        inflateEnd(&s);
        if (z_result != Z_OK && z_result != Z_STREAM_END) {
            DEBUG_FUNCTION_LINE("error decompressing section: %d\n", z_result);
            return nullptr;
        }

        uncompressed_data[actual_size] = '\0';
        uncompressed_size = actual_size;

        return uncompressed_data;
    }

    std::unique_ptr<char[]> deflate(const char *data, const ELFIO::endianess_convertor *convertor, ELFIO::Elf_Xword decompressed_size, ELFIO::Elf_Xword &compressed_size) const {
        int z_result = 0;
        int bytes_consumed = 0;
        z_stream s = { 0 };
        s.zalloc = Z_NULL;
        s.zfree = Z_NULL;
        s.opaque = Z_NULL;

        auto compressed = std::unique_ptr<char[]>(new char[decompressed_size + 4]);
        if(compressed == nullptr) {
            DEBUG_FUNCTION_LINE("error allocating %d bytes of memory for compressed section\n", decompressed_size+4);
            return nullptr;
        }

        write_actual_size(compressed.get(), convertor, decompressed_size);
        if(Z_OK != (z_result = deflateInit(&s, Z_DEFAULT_COMPRESSION))) {
            DEBUG_FUNCTION_LINE("error initializing zlib: %d\n", z_result);
            return nullptr;
        }

        s.avail_in = decompressed_size;
        s.next_in = (Bytef *)data + 4;
        s.avail_out = decompressed_size;
        s.next_out = (Bytef *)compressed.get();

        z_result = ::deflate(&s, Z_FINISH);
        bytes_consumed = s.avail_out;
        deflateEnd(&s);

        if(z_result != Z_OK && z_result != Z_STREAM_END) {
            DEBUG_FUNCTION_LINE("error compressing section: %d\n", z_result);
            return nullptr;
        }

        compressed_size = decompressed_size - bytes_consumed;
        return compressed;
    }

    private:
    bool parse_actual_size(const char *buffer, const ELFIO::endianess_convertor *convertor, ELFIO::Elf_Xword &buffer_size, ELFIO::Elf_Xword &actual_size) const {
        union _int32buffer { uint32_t word; char buf[4]; } int32buffer;

        if(buffer_size < 4)
            return false;

        buffer_size -= 4;
        memcpy(int32buffer.buf, buffer, 4);
        int32buffer.word = (*convertor)(int32buffer.word);

        actual_size = int32buffer.word;
        return true;
    }

    void write_actual_size(const char *buffer, const ELFIO::endianess_convertor *convertor, ELFIO::Elf_Xword actual_size) const {
        union _int32buffer { uint32_t word; char buf[4]; } int32buffer;

        int32buffer.word = (*convertor)(actual_size);
        memcpy((void *)buffer, int32buffer.buf, 4);
    }
};