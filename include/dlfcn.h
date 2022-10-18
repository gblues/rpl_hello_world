#ifndef DLFCN_H
#define DLFCN_H
#include <memory>
#include <whb/log_console.h>

#include "module/ModuleData.h"
#include "common/module_defines.h"

typedef struct dl_handle_t {
    void *library;
    size_t library_size;
    ModuleData module_data;

    dl_handle_t() : module_data(ModuleData()) {}
    ~dl_handle_t() {
        if(library) {
            free(library);
            library = nullptr;
        }
    }
} dl_handle;

std::unique_ptr<dl_handle> dlopen(const char *filename);
void *dlsym(std::unique_ptr<dl_handle> &handle, const char *symbol);
char *dlerror();

#endif // DLFCN_H