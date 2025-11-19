#ifndef CLINT_H
#define CLINT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef CLINT_BASE
#define CLINT_BASE 0x02000000UL
#endif

/* 32-bit bus addresses for hart 0 */
#define CLINT_MTIMECMP_LO_ADDR (CLINT_BASE + 0x4000UL)
#define CLINT_MTIMECMP_HI_ADDR (CLINT_BASE + 0x4004UL)
#define CLINT_MTIME_LO_ADDR (CLINT_BASE + 0xBFF8UL)
#define CLINT_MTIME_HI_ADDR (CLINT_BASE + 0xBFFCUL)

    /* Program 64-bit MTIMECMP via two 32-bit stores:
     *   HI = 0xFFFF_FFFF -> LO = value[31:0] -> HI = value[63:32]
     */
    void clint_write_mtimecmp(uint32_t hi, uint32_t lo);

    /* Read back MTIMECMP as {HI, LO} using a stable HI/LO/HI sequence. */
    void clint_read_mtimecmp(uint32_t *hi, uint32_t *lo);

    /* Write MTIME via two 32-bit stores (LO then HI). */
    void clint_write_mtime(uint32_t hi, uint32_t lo);

    /* Read MTIME as {HI, LO} using a stable HI/LO/HI sequence. */
    void clint_read_mtime(uint32_t *hi, uint32_t *lo);

#ifdef __cplusplus
}
#endif

#endif /* CLINT_H */
