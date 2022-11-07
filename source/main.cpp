#include <memory>
#include <string>

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/systeminfo.h>
#include <nn/ac.h>

#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>

#include "dlfcn.h"
#include "logger.h"

void test_dlopen();
typedef const char *(*my_first_export_fn)();

int main(int argc, char **argv)
{
   nn::ac::ConfigIdNum configId;

   nn::ac::Initialize();
   nn::ac::GetStartupId(&configId);
   nn::ac::Connect(configId);

   WHBProcInit();
   initLogging();

   test_dlopen();

   while(WHBProcIsRunning()) {
      OSCalendarTime tm;
      OSTicksToCalendarTime(OSGetTime(), &tm);
      WHBLogConsoleDraw();
      OSSleepTicks(OSMillisecondsToTicks(100));
   }

   deinitLogging();
   WHBProcShutdown();

   nn::ac::Finalize();
   return 0;
}

void test_dlopen() {
    auto handle = dlopen("/vol/content/my_first_rpl.rpl");
    if(handle == nullptr) {
        WHBLogPrintf("Failed to open library: %s\n", dlerror());
        return;
    }

    WHBLogPrintf("Opened library successfully.\n");
    my_first_export_fn my_first_export = (my_first_export_fn)dlsym(handle, "my_first_export");
    if(my_first_export == nullptr) {
        WHBLogPrintf("Failed too lookup symbol: %s\n", dlerror());
        dlclose(handle);
        return;
    }
    DEBUG_FUNCTION_LINE("my_first_export resolved to %08x", my_first_export);
    DEBUG_FUNCTION_LINE("first 8 bytes at that address:");

    auto charbuf = std::unique_ptr<char[]>(new char[17]);
    const char *fptr = (const char *)my_first_export;
    snprintf(charbuf.get(), 17, "%02x%02x%02x%02x%02x%02x%02x%02x",
        fptr[0], fptr[1], fptr[2], fptr[3], fptr[4], fptr[5], fptr[6], fptr[7]);
    DEBUG_FUNCTION_LINE("%s", charbuf.get());

    WHBLogPrintf("Calling my_first_export: %s\n", (*my_first_export)());
    dlclose(handle);
}