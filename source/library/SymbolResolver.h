#pragma once

#include "Loader.h"

class SymbolResolver {
    public:
    SymbolResolver(dl_handle *handle) : handle(handle) {}
    ~SymbolResolver() = default;

    uint32_t resolve(const char *name);
    const char *error_message();

    private:
    dl_handle *handle;
    std::string error;
};