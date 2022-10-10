#ifndef DLFCN_H
#define DLFCN_H
#include <memory>

#include "module/ModuleData.h"
#include "common/module_defines.h"

typedef struct dl_handle_t {
    void *library;
    size_t library_size;
    ModuleData module_data;
    relocation_trampolin_entry_t trampolines[DYN_LINK_TRAMPOLIN_LIST_LENGTH];

    dl_handle_t() : module_data(ModuleData()) {}
    ~dl_handle_t() {
        if(library) {
            free(library);
            library = nullptr;
        }
    }
} dl_handle;

std::unique_ptr<dl_handle> dlopen(const char *filename);
char *dlerror();

#endif // DLFCN_H