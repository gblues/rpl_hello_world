#include "lib_hello.h"

#include <coreinit/dynload.h>

int
answer_to_life()
{
   return 42;
}

const char *
hello_world()
{
   return "Hello, World!";
}

int
rpl_entry(OSDynLoad_Module module,
          OSDynLoad_EntryReason reason)
{
   if (reason == OS_DYNLOAD_LOADED) {
      // Do stuff on load
   } else if (reason == OS_DYNLOAD_UNLOADED) {
      // Do stuff on unload
   }

   return 0;
}
