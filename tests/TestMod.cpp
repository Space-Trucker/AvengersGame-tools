#include <stdint.h>

#if defined(MODE_OK)
static const void* g_api;
extern "C" __declspec(dllexport) int AvengersModInit(const void* api) { g_api = api; return 7; }
extern "C" __declspec(dllexport) const void* TestModApi() { return g_api; }
#elif defined(MODE_CRASH)
extern "C" __declspec(dllexport) int AvengersModInit(const void*) {
    *(volatile int*)nullptr = 1;
    return 0;
}
#elif defined(MODE_NOINIT)
extern "C" __declspec(dllexport) int TestModLoaded() { return 1; }
#else
#error "define MODE_OK, MODE_CRASH or MODE_NOINIT"
#endif
