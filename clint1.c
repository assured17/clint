#include "clint.h"

#ifndef HART_ID
#define HART_ID 0u
#endif

static inline void fence_io(void)
{
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

void clint_init(void)
{
    CLINT_MSIP(HART_ID) = 0u;
    fence_io();

    clint_set_mtimecmp(0x1000ULL);
}

void clint_set_mtimecmp(uint64_t value)
{
    uint32_t hi = (uint32_t)(value >> 32);
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFF);

    CLINT_MTIMECMP_HI(HART_ID) = 0xFFFFFFFFu;
    fence_io();
    CLINT_MTIMECMP_LO(HART_ID) = lo;
    fence_io();
    CLINT_MTIMECMP_HI(HART_ID) = hi;
    fence_io();
}

uint64_t clint_get_mtimecmp(void)
{
    uint32_t hi, lo;

    hi = CLINT_MTIMECMP_HI(HART_ID);
    fence_io();
    lo = CLINT_MTIMECMP_LO(HART_ID);
    fence_io();

    return ((uint64_t)hi << 32) | lo;
}

void clint_set_mtime(uint64_t value)
{
    uint32_t hi = (uint32_t)(value >> 32);
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFF);

    CLINT_MTIME_HI = 0xFFFFFFFFu;
    fence_io();
    CLINT_MTIME_LO = lo;
    fence_io();
    CLINT_MTIME_HI = hi;
    fence_io();
}

uint64_t clint_get_mtime(void)
{
    uint32_t hi1, lo, hi2;

    do
    {
        hi1 = CLINT_MTIME_HI;
        fence_io();
        lo = CLINT_MTIME_LO;
        fence_io();
        hi2 = CLINT_MTIME_HI;
        fence_io();
    } while (hi1 != hi2);

    return ((uint64_t)hi1 << 32) | lo;
}