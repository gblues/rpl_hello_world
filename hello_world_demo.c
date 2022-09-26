#include <coreinit/dynload.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>

const char *hello_world_library = "libhello";
OSDynLoad_Module handle;

void call_hello_world();

int
main(int argc, char **argv)
{
   WHBProcInit();
   WHBLogConsoleInit();

   call_hello_world();

   while(WHBProcIsRunning()) {
      WHBLogConsoleDraw();
      OSSleepTicks(OSMillisecondsToTicks(30));
   }

   WHBLogConsoleFree();
   WHBProcShutdown();
   return 0;
}

const char *os_dynload_error_to_string(OSDynLoad_Error error) {
   switch(error) {
      case OS_DYNLOAD_OK:
         return "OS_DYNLOAD_OK";
      case OS_DYNLOAD_OUT_OF_MEMORY:
         return "OS_DYNLOAD_OUT_OF_MEMORY";
      case OS_DYNLOAD_INVALID_NOTIFY_PTR:
         return "OS_DYNLOAD_INVALID_NOTIFY_PTR";
      case OS_DYNLOAD_INVALID_MODULE_NAME_PTR:
         return "OS_DYNLOAD_INVALID_MODULE_NAME_PTR";
      case OS_DYNLOAD_INVALID_MODULE_NAME:
         return "OS_DYNLOAD_INVALID_MODULE_NAME";
      case OS_DYNLOAD_INVALID_ACQUIRE_PTR:
         return "OS_DYNLOAD_INVALID_ACQUIRE_PTR";
      case OS_DYNLOAD_EMPTY_MODULE_NAME:
         return "OS_DYNLOAD_EMPTY_MODULE_NAME";
      case OS_DYNLOAD_INVALID_ALLOCATOR_PTR:
         return "OS_DYNLOAD_INVALID_ALLOCATOR_PTR";
      case OS_DYNLOAD_OUT_OF_SYSTEM_MEMORY:
         return "OS_DYNLOAD_OUT_OF_SYSTEM_MEMORY";
      case OS_DYNLOAD_TLS_ALLOCATOR_LOCKED:
         return "OS_DYNLOAD_TLS_ALLOCATOR_LOCKED";
      default:
         return "Undocumented OSDynLoad_Error";
   }
}

void call_hello_world() {
   const char *(*hello_world_fn)();
   OSDynLoad_Error ret;
   ret = OSDynLoad_Acquire(hello_world_library, &handle);
   if(ret != OS_DYNLOAD_OK) {
      WHBLogPrintf("Failed to acquire library %s:\n - %s (0x%08x)\n", hello_world_library, os_dynload_error_to_string(ret), ret);
      return;
   }
   ret = OSDynLoad_FindExport(handle, false, "hello_world", (void **)&hello_world_fn);
   if(ret != OS_DYNLOAD_OK) {
      WHBLogPrintf("Failed to locate export \"hello_world\":\n - %s (0x%08x)\n", os_dynload_error_to_string(ret), ret);
      return;
   }
   WHBLogPrintf("hello_world() returned %s\n", hello_world_fn());
   OSDynLoad_Release(handle);
   handle = NULL;
}