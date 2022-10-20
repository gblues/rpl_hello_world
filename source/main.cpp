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

const char *(*hello_world_fun)();
int (*answer_to_life_fun)();

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

   auto hello_world_sym = (const char *(*)())dlsym(handle, "hello_world");
   if(hello_world_sym == nullptr) {
      WHBLogPrintf("failed to find hello_world symbol: %s\n", dlerror());
      return;
   }

   auto answer_to_life_sym = (int (*)())dlsym(handle, "answer_to_life");
   if(answer_to_life_sym == nullptr) {
      WHBLogPrintf("failed to find answer_to_life symbol: %s\n", dlerror());
      return;
   }
   WHBLogPrintf("All symbols found\n");

   #if 1
   hello_world_fun = hello_world_sym;
   answer_to_life_fun = answer_to_life_sym;
   const char *hello_world = hello_world_fun();
   int answer_to_life = answer_to_life_fun();

   WHBLogPrintf("hello_world: %s\n", hello_world);
   WHBLogPrintf("answer_to_life: %d\n", answer_to_life);

   #endif
}
