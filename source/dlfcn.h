#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void *dlopen(const char *library);
void *dlsym(void *handle, const char *symbol);
char *dlerror();
int dlclose(void *handle);

#ifdef __cplusplus
}
#endif