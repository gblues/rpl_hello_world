#include "elfio/elfio.hpp"
#include "wiiu_zlib.hpp"
#include "library/library.h"
#include "dlfcn.h"

#define LAST_ERROR_LEN 256

static char last_error[LAST_ERROR_LEN] = {};
static bool has_error = false;

static const char *ERR_BAD_RPL = "Does not seem to be a library";
static const char *ERR_BAD_HANDLE = "Expected a library handle but got a nullptr";
static const char *ERR_BAD_SYM = "Symbol name is null or empty";

static void set_error(const char *error_message);

void *dlopen(const char *library) {
    dl_handle *handle = new dl_handle();
    ELFIO::elfio reader(new wiiu_zlib());
    DEBUG_FUNCTION_LINE("Attempting to load library: %s", library);
    if(!reader.load(library)) {
        set_error(ERR_BAD_RPL);
        delete handle;
        return nullptr;
    }

    DEBUG_FUNCTION_LINE("Loaded library successfully");
    LibraryLoader loader(handle, reader);
    DEBUG_FUNCTION_LINE("Invoking LibraryLoader.load()");
    if(!loader.load()) {
        set_error(loader.error_message());
        delete handle;
        return nullptr;
    }
    DEBUG_FUNCTION_LINE("LibraryLoader.load() completed successfully");
    return (void *)handle;
}

void *dlsym(void *handle, const char *symbol) {
    if(handle == nullptr) {
        set_error(ERR_BAD_HANDLE);
        return nullptr;
    }

    if(symbol == nullptr || *symbol == '\0') {
        set_error(ERR_BAD_SYM);
        return nullptr;
    }

    DEBUG_FUNCTION_LINE("Attempting to find symbol: %s", symbol);

    SymbolResolver resolver((dl_handle *)handle);

    uint32_t symbol_address = resolver.resolve(symbol);
    if(symbol_address == 0) {
        set_error(resolver.error_message());
        return nullptr;
    }
    DEBUG_FUNCTION_LINE("Found symbol %s at address 0x%08x", symbol, symbol_address);
    return (void *)symbol_address;
}

char *dlerror() {
    char *error_message = has_error ? &last_error[0] : nullptr;
    has_error = false;

    return error_message;
}

int dlclose(void *handle) {
    dl_handle *h = (dl_handle *)handle;
    if(h)
        delete h;
    return 0;
}

static void set_error(const char *error_message) {
    snprintf(last_error, LAST_ERROR_LEN, "%s", error_message);
    has_error = true;
}