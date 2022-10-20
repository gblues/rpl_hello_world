#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/systeminfo.h>
#include <nn/ac.h>

#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/log_udp.h>

#include <memory>
#include "dlfcn.h"

void test();

int (*hello_world_fun)();

int main(int argc, char **argv)
{
   nn::ac::ConfigIdNum configId;

   nn::ac::Initialize();
   nn::ac::GetStartupId(&configId);
   nn::ac::Connect(configId);

   WHBProcInit();
   WHBLogUdpInit();
   WHBLogConsoleInit();

   test();
   

   while(WHBProcIsRunning()) {
      WHBLogConsoleDraw();
      OSSleepTicks(OSMillisecondsToTicks(100));
   }

   WHBLogConsoleFree();
   WHBProcShutdown();

   nn::ac::Finalize();
   return 0;
}

void test() {
   auto handle = dlopen("/vol/content/libhello.rpl");
   if(handle == nullptr) {
      WHBLogPrintf("dlopen failed: %s\n", dlerror());
      return;
   }
   auto sym = (int (*)())dlsym(handle, "hello_world");
   if(sym == nullptr) {
      WHBLogPrintf("dlsym failed: %s\n", dlerror());
      return;
   }
   WHBLogPrintf("dlsym succeeded\n");

   #if 1
   hello_world_fun = sym;
   int hello_world = hello_world_fun();

   WHBLogPrintf("invoking sym returned %d\n", hello_world);

   #endif
}
