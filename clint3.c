#include "clint.h"

/* 32-bit MMIO helpers kept private to this translation unit */
static inline void mmio_write32(uintptr_t addr, uint32_t data)
{
    *(volatile uint32_t *)addr = data;
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void mmio_fence(void)
{
#if defined(__riscv)
    __asm__ volatile("fence iorw, iorw" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

/* ----------------------------------------------------------
 * MTIMECMP (64-bit compare value for machine timer interrupt)
 * ----------------------------------------------------------*/
void clint_write_mtimecmp(uint32_t hi, uint32_t lo)
{
    /* Arm HI to max so LO write cannot cause a transient match. */
    mmio_fence();
    mmio_write32(CLINT_MTIMECMP_HI_ADDR, 0xFFFFFFFFu);
    mmio_fence();

    /* Program LO then final HI. */
    mmio_write32(CLINT_MTIMECMP_LO_ADDR, lo);
    mmio_fence();
    mmio_write32(CLINT_MTIMECMP_HI_ADDR, hi);
    mmio_fence();
}

void clint_read_mtimecmp(uint32_t *hi, uint32_t *lo)
{
    uint32_t h1, h2, l;

    /* HI -> LO -> HI, repeat until HI is stable. */
    do
    {
        h1 = mmio_read32(CLINT_MTIMECMP_HI_ADDR);
        l = mmio_read32(CLINT_MTIMECMP_LO_ADDR);
        h2 = mmio_read32(CLINT_MTIMECMP_HI_ADDR);
    } while (h1 != h2);

    if (hi != 0)
    {
        *hi = h1;
    }
    if (lo != 0)
    {
        *lo = l;
    }
}

/* ----------------------------------------------------------
 * MTIME (64-bit free-running counter)
 * ----------------------------------------------------------*/
void clint_write_mtime(uint32_t hi, uint32_t lo)
{
    /* Program LO first, then HI. Counter will continue running. */
    mmio_fence();
    mmio_write32(CLINT_MTIME_LO_ADDR, lo);
    mmio_fence();
    mmio_write32(CLINT_MTIME_HI_ADDR, hi);
    mmio_fence();
}

void clint_read_mtime(uint32_t *hi, uint32_t *lo)
{
    uint32_t h1, h2, l;

    /* HI -> LO -> HI, repeat until HI is stable. */
    do
    {
        h1 = mmio_read32(CLINT_MTIME_HI_ADDR);
        l = mmio_read32(CLINT_MTIME_LO_ADDR);
        h2 = mmio_read32(CLINT_MTIME_HI_ADDR);
    } while (h1 != h2);

    if (hi != 0)
    {
        *hi = h1;
    }
    if (lo != 0)
    {
        *lo = l;
    }
}
