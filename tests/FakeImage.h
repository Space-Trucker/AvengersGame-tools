#pragma once
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct FakeImage {
    static const size_t PIECE = 0x10000;
    uint8_t* base = nullptr;
    size_t size = 0;
    struct Piece { uint8_t* at; HANDLE file, map; };
    Piece pieces[32] = {};
    int count = 0;

    explicit FakeImage(size_t bytes) : size(bytes) {
        base = (uint8_t*)VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS);
        if (base) VirtualFree(base, 0, MEM_RELEASE);
    }
    ~FakeImage() {
        for (int i = 0; i < count; i++) {
            if (pieces[i].map) {
                UnmapViewOfFile(pieces[i].at);
                CloseHandle(pieces[i].map);
                CloseHandle(pieces[i].file);
            } else {
                VirtualFree(pieces[i].at, 0, MEM_RELEASE);
            }
        }
    }
    FakeImage(const FakeImage&) = delete;
    FakeImage& operator=(const FakeImage&) = delete;

    uint8_t* At(uintptr_t rva) const { return base + rva; }
    static uintptr_t PieceOf(uintptr_t rva) { return rva & ~(uintptr_t)(PIECE - 1); }
    bool Has(uintptr_t rva) const {
        for (int i = 0; i < count; i++) if (pieces[i].at == base + PieceOf(rva)) return true;
        return false;
    }

    bool Commit(uintptr_t rva) {
        if (Has(rva)) return true;
        if (!base || count == _countof(pieces)) return false;
        uint8_t* at = base + PieceOf(rva);
        if (VirtualAlloc(at, PIECE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) != at) return false;
        memset(at, 0xCC, PIECE);
        pieces[count++] = { at, nullptr, nullptr };
        return true;
    }

    bool ReadOnlyView(uintptr_t rva, const void* bytes, size_t n) {
        if (Has(rva) || !base || count == _countof(pieces)) return false;
        static uint8_t content[PIECE];
        memset(content, 0xCC, PIECE);
        memcpy(content + (rva - PieceOf(rva)), bytes, n);
        wchar_t dir[MAX_PATH], path[MAX_PATH];
        GetTempPathW(MAX_PATH, dir);
        if (!GetTempFileNameW(dir, L"fim", 0, path)) return false;
        HANDLE f = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (f == INVALID_HANDLE_VALUE) return false;
        DWORD wrote = 0;
        HANDLE map = WriteFile(f, content, PIECE, &wrote, nullptr) && wrote == PIECE
                         ? CreateFileMappingW(f, nullptr, PAGE_READONLY, 0, 0, nullptr) : nullptr;
        uint8_t* at = base + PieceOf(rva);
        void* view = map ? MapViewOfFileEx(map, FILE_MAP_READ, 0, 0, PIECE, at) : nullptr;
        if (view != at) {
            if (view) UnmapViewOfFile(view);
            if (map) CloseHandle(map);
            CloseHandle(f);
            return false;
        }
        pieces[count++] = { at, f, map };
        return true;
    }

    bool Put(uintptr_t rva, const void* bytes, size_t n, DWORD rights) {
        if (!Commit(rva) || !Commit(rva + n - 1)) return false;
        DWORD old;
        if (!VirtualProtect(At(rva), n, PAGE_READWRITE, &old)) return false;
        memcpy(At(rva), bytes, n);
        if (!VirtualProtect(At(rva), n, rights, &old)) return false;
        FlushInstructionCache(GetCurrentProcess(), At(rva), n);
        return true;
    }

    bool Jump(uintptr_t rva, const void* to) {
        uint8_t code[12] = { 0x48, 0xB8 };
        memcpy(code + 2, &to, 8);
        code[10] = 0xFF; code[11] = 0xE0;
        return Put(rva, code, sizeof code, PAGE_EXECUTE_READ);
    }

    DWORD Rights(uintptr_t rva) const {
        MEMORY_BASIC_INFORMATION m = {};
        VirtualQuery(At(rva), &m, sizeof m);
        return m.Protect;
    }
};
