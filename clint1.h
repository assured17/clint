#ifndef CLINT_H
#define CLINT_H

#include <stdint.h>

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

    void clint_init(void);
    void clint_set_mtimecmp(uint64_t value);
    uint64_t clint_get_mtimecmp(void);
    void clint_set_mtime(uint64_t value);
    uint64_t clint_get_mtime(void);

#ifdef __cplusplus
}
#endif

#endif // CLINT_H