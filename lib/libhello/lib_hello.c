#include "lib_hello.h"

#include <coreinit/dynload.h>

static int answer_to_life_the_universe_and_everything = 42;
const char *hello_world_str = "Hello, World!";

static int increment(int value);

int
answer_to_life()
{
   return increment(answer_to_life_the_universe_and_everything);
}

const char *
hello_world()
{
   return hello_world_str;
}

int
rpl_entry(OSDynLoad_Module module,
          OSDynLoad_EntryReason reason)
{
   if (reason == OS_DYNLOAD_LOADED) {
      // TODO: library initialization, if any
   } else if (reason == OS_DYNLOAD_UNLOADED) {
      // TODO: library de-initialization, if any
   }

   return 0;
}

static int increment(int value) {
   return ++value;
}
