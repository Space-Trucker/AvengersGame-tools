#include <stdint.h>

#ifndef CHAIN_VERSION
#define CHAIN_VERSION 1
#endif

extern "C" __declspec(dllexport) int __cdecl AvengersTileChain() { return CHAIN_VERSION; }
extern "C" __declspec(dllexport) void __fastcall ChainLayout(uint8_t*) {}
extern "C" __declspec(dllexport) void __fastcall ChainRefresh(uint8_t*) {}
extern "C" __declspec(dllexport) void __fastcall ChainAction(uint8_t*, int, uint8_t) {}
