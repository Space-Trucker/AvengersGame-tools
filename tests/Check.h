#pragma once
#include <stdio.h>

static int g_checks, g_failed;

#define CHECK(c)                                                                                     \
    do {                                                                                             \
        ++g_checks;                                                                                  \
        if (!(c)) { ++g_failed; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); }             \
    } while (0)

#define CHECK_EQ(a, b)                                                                               \
    do {                                                                                             \
        ++g_checks;                                                                                  \
        long long check_a_ = (long long)(a), check_b_ = (long long)(b);                              \
        if (check_a_ != check_b_) {                                                                  \
            ++g_failed;                                                                              \
            printf("  FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, check_a_, check_b_); \
        }                                                                                            \
    } while (0)

static int TestSummary(const char* name) {
    printf("%s: %d checks, %d failed\n", name, g_checks, g_failed);
    return g_failed ? 1 : 0;
}
