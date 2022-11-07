#include "library.h"

uint32_t SymbolResolver::resolve(const char *name) {
    if(name == nullptr || *name == '\0') {
        error = "Symbol name is null or empty";
        return 0;
    }
    std::string key = name;
    auto search = handle->exports.find(key);
    if(search != handle->exports.end()) {
        return search->second;
    }

    error = "Symbol " + key + " not found";
    return 0;
}

const char *SymbolResolver::error_message() {
    return error.c_str();
}