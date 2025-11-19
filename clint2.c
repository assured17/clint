
#include "clint.h"

#ifndef HART_ID
#define HART_ID 0u
#endif

static inline void fence_io(void) { __asm__ volatile("" ::: "memory"); }

void clint_init(void)
{
    // Mask and clear. Program mtime to 0 and mtimecmp to a large value first.
    // Then set mtimecmp to 0x00000000_00001000 as requested.

    // Prevent spurious compare match during programming:
    // 1) Set MTIMECMP_HI to all ones
    CLINT_MTIMECMP_HI(HART_ID) = 0xFFFFFFFFu;

    // 2) Write desired MTIMECMP_LO
    CLINT_MTIMECMP_LO(HART_ID) = 0x00001000u;

    // 3) Write desired MTIMECMP_HI
    CLINT_MTIMECMP_HI(HART_ID) = 0x00000000u;

    // Reset MTIME to 0 safely: write HI first to freeze, write LO, then HI=0
    CLINT_MTIME_HI = 0xFFFFFFFFu;

    CLINT_MTIME_LO = 0x00000000u;

    CLINT_MTIME_HI = 0x00000000u;

    // Clear msip for hart0 just in case
    CLINT_MSIP(HART_ID) = 0u;
}

void clint_write_mtime(uint32_t hi, uint32_t lo)
{
    // Safe 64-bit write: HI to max, LO, HI desired
    CLINT_MTIME_HI = 0xFFFFFFFFu;

    CLINT_MTIME_LO = lo;

    CLINT_MTIME_HI = hi;
}

// �?�?�'�?��?�??�?�?�止�??�?'�?�?�?'�?�任�?�??令
static inline void compiler_barrier(void) { __asm__ volatile("" ::: "memory"); }

// �?�?��??�?"�?�?��?? IPO/�??�?"�??读�??并�??�??�?'
__attribute__((noinline)) void clint_read_mtime(uint32_t *hi, uint32_t *lo)
{
    uint32_t hi1, lo1, hi2;
    do
    {
        hi1 = CLINT_MTIME_HI; // 1) BFFC
        compiler_barrier();

        lo1 = CLINT_MTIME_LO; // 2) BFF8
        compiler_barrier();

        // �?�"�??�?��?��??MMIO 读�?强�?�桥/�?�线�?�?�?�??�??�?��?便�?波形对�??
        (void)CLINT_MSIP(0);
        compiler_barrier();

        hi2 = CLINT_MTIME_HI; // 3) BFFC
        compiler_barrier();

    } while (hi1 != hi2);

    if (hi)
        *hi = hi1;
    if (lo)
        *lo = lo1;
}

void clint_write_mtimecmp(uint32_t hi, uint32_t lo)
{
    // Safe sequence per RISC-V privileged spec advice:
    // write HI to max, then LO, then HI desired
    CLINT_MTIMECMP_HI(HART_ID) = 0xFFFFFFFFu;

    CLINT_MTIMECMP_LO(HART_ID) = lo;

    CLINT_MTIMECMP_HI(HART_ID) = hi;
}

void clint_read_mtimecmp(uint32_t *hi, uint32_t *lo)
{
    // For compare register, read LO then HI is fine since writes follow safe rules
    uint32_t lo1 = CLINT_MTIMECMP_LO(HART_ID);

    uint32_t hi1 = CLINT_MTIMECMP_HI(HART_ID);

    if (hi)
        *hi = hi1;
    if (lo)
        *lo = lo1;
}

uint32_t clint_mtime_lo(void) { return CLINT_MTIME_LO; }
uint32_t clint_mtime_hi(void) { return CLINT_MTIME_HI; }
uint32_t clint_mtimecmp_lo(void) { return CLINT_MTIMECMP_LO(HART_ID); }
uint32_t clint_mtimecmp_hi(void) { return CLINT_MTIMECMP_HI(HART_ID); }
