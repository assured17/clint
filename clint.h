
#ifndef CLINT_H
#define CLINT_H

#include <stdint.h>

/*
 * Assumptions:
 * - 32-bit system. All CLINT 64-bit registers are accessed via two 32-bit words.
 * - hart 0 only.
 * - CLINT base range: 0x0200_0000 - 0x020B_FFFF
 * - Standard CLINT layout (SiFive-style):
 *     msip      : 0x0000 + 4*hart
 *     mtimecmp  : 0x4000 + 8*hart (lo @ +0x0, hi @ +0x4)
 *     mtime     : 0xBFF8 (lo @ +0x0, hi @ +0x4)
 */

#define CLINT_BASE 0x02000000UL

#define CLINT_MSIP(hart) (*(volatile uint32_t *)(CLINT_BASE + 0x0000 + 4u * (hart)))

#define CLINT_MTIMECMP_LO(hart) (*(volatile uint32_t *)(CLINT_BASE + 0x4000 + 8u * (hart)))
#define CLINT_MTIMECMP_HI(hart) (*(volatile uint32_t *)(CLINT_BASE + 0x4004 + 8u * (hart)))

#define CLINT_MTIME_LO (*(volatile uint32_t *)(CLINT_BASE + 0xBFF8))
#define CLINT_MTIME_HI (*(volatile uint32_t *)(CLINT_BASE + 0xBFFC))

#ifdef __cplusplus
extern "C"
{
#endif

    void clint_init(void);                            // safe init. clears pending state and programs mtimecmp = 0x1000
    void clint_write_mtime(uint32_t hi, uint32_t lo); // safe 64-bit write sequence
    void clint_read_mtime(uint32_t *hi, uint32_t *lo);

    void clint_write_mtimecmp(uint32_t hi, uint32_t lo); // safe 64-bit write sequence
    void clint_read_mtimecmp(uint32_t *hi, uint32_t *lo);

    uint32_t clint_mtime_lo(void);
    uint32_t clint_mtime_hi(void);
    uint32_t clint_mtimecmp_lo(void);
    uint32_t clint_mtimecmp_hi(void);

#ifdef __cplusplus
}
#endif

#endif // CLINT_H
