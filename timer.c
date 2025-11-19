#include "timer.h"

#define APB_BASE 0x10080000UL

#define TIMER1_LOAD (*(volatile uint32_t *)(APB_BASE + 0x00))
#define TIMER1_VALUE (*(volatile uint32_t *)(APB_BASE + 0x04))
#define TIMER1_CTRL (*(volatile uint32_t *)(APB_BASE + 0x08))
#define TIMER1_INTCLR (*(volatile uint32_t *)(APB_BASE + 0x0C))

#define TIMER2_LOAD (*(volatile uint32_t *)(APB_BASE + 0x10))
#define TIMER2_VALUE (*(volatile uint32_t *)(APB_BASE + 0x14))
#define TIMER2_CTRL (*(volatile uint32_t *)(APB_BASE + 0x18))
#define TIMER2_INTCLR (*(volatile uint32_t *)(APB_BASE + 0x1C))

void timer1_init(void)
{
    timer1_stop();
    TIMER1_LOAD = 0x1000;
    TIMER1_VALUE = 0x1000;
    TIMER1_INTCLR = 1;
}

void timer1_start_oneshot(void)
{
    TIMER1_CTRL = TIMER_CTRL_ENABLE;
}

void timer1_start_periodic(void)
{
    TIMER1_CTRL = TIMER_CTRL_ENABLE | TIMER_CTRL_MODE;
}

void timer1_stop(void)
{
    TIMER1_CTRL = 0;
}

void timer1_set_load(uint32_t value)
{
    TIMER1_LOAD = value;
    if (TIMER1_CTRL & TIMER_CTRL_ENABLE)
    {
        TIMER1_VALUE = value;
    }
}

uint32_t timer1_get_current(void)
{
    return TIMER1_VALUE;
}

uint8_t timer1_is_running(void)
{
    return (TIMER1_CTRL & TIMER_CTRL_ENABLE) ? 1 : 0;
}

void timer2_init(void)
{
    timer2_stop();
    TIMER2_LOAD = 0x2000;
    TIMER2_VALUE = 0x2000;
    TIMER2_INTCLR = 1;
}

void timer2_start_oneshot(void)
{
    TIMER2_CTRL = TIMER_CTRL_ENABLE;
}

void timer2_start_periodic(void)
{
    TIMER2_CTRL = TIMER_CTRL_ENABLE | TIMER_CTRL_MODE;
}

void timer2_stop(void)
{
    TIMER2_CTRL = 0;
}

void timer2_set_load(uint32_t value)
{
    TIMER2_LOAD = value;

    if (TIMER2_CTRL & TIMER_CTRL_ENABLE)
    {
        TIMER2_VALUE = value;
    }
}

uint32_t timer2_get_current(void)
{
    return TIMER2_VALUE;
}

uint8_t timer2_is_running(void)
{
    return (TIMER2_CTRL & TIMER_CTRL_ENABLE) ? 1 : 0;
}

void timer2_restart(void)
{

    TIMER2_VALUE = TIMER2_LOAD;
    timer2_start_oneshot();
}
