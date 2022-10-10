#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/systeminfo.h>
#include <nn/ac.h>

#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>

#include <thread>

#include "dlfcn.h"

int
main(int argc, char **argv)
{
   nn::ac::ConfigIdNum configId;

   nn::ac::Initialize();
   nn::ac::GetStartupId(&configId);
   nn::ac::Connect(configId);

   WHBProcInit();
   WHBLogConsoleInit();

   std::unique_ptr<dl_handle> handle = dlopen("/vol/content/libhello.rpl");
   if(handle == nullptr) {
    WHBLogPrintf("%s\n", dlerror());
   }

   WHBLogConsoleFree();
   WHBProcShutdown();

   nn::ac::Finalize();
   return 0;
}