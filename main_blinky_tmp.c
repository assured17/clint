/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/******************************************************************************
 * This project provides two demo applications.  A simple blinky style project,
 * and a more comprehensive test and demo application.  The
 * mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting in main.c is used to select
 * between the two.  See the notes on using mainCREATE_SIMPLE_BLINKY_DEMO_ONLY
 * in main.c.  This file implements the simply blinky version.
 *
 * This file only contains the source code that is specific to the basic demo.
 * Generic functions, such FreeRTOS hook functions, are defined in main.c.
 ******************************************************************************
 *
 * main_blinky() creates one queue, one software timer, and two tasks.  It then
 * starts the scheduler.
 *
 * The Queue Send Task:
 * The queue send task is implemented by the prvQueueSendTask() function in
 * this file.  It uses vTaskDelayUntil() to create a periodic task that sends
 * the value 100 to the queue every 200 (simulated) milliseconds.
 *
 * The Queue Send Software Timer:
 * The timer is an auto-reload timer with a period of two (simulated) seconds.
 * Its callback function writes the value 200 to the queue.  The callback
 * function is implemented by prvQueueSendTimerCallback() within this file.
 *
 * The Queue Receive Task:
 * The queue receive task is implemented by the prvQueueReceiveTask() function
 * in this file.  prvQueueReceiveTask() waits for data to arrive on the queue.
 * When data is received, the task checks the value of the data, then outputs a
 * message to indicate if the data came from the queue send task or the queue
 * send software timer.
 */

/* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "uart.h"
#include "sdram.h"
#include "gpio.h"
#include "plic.h"
#include "clint.h"

#define UART_TASK_STACK_SIZE 256
#define UART_LOOPBACK_PRIORITY (tskIDLE_PRIORITY + 1)
#define SDRAM_CTRL_BASE 0x100B0000
#define SDRAM_BASE 0x80000000

/* Priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define mainQUEUE_SEND_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

#define CLINT_TEST_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define TEST_PERIOD_MS pdMS_TO_TICKS(500)

/* The rate at which data is sent to the queue.  The times are converted from
 * milliseconds to ticks using the pdMS_TO_TICKS() macro. */
#define mainTASK_SEND_FREQUENCY_MS pdMS_TO_TICKS(200UL)
#define mainTIMER_SEND_FREQUENCY_MS pdMS_TO_TICKS(2000UL)

/* The number of items the queue can hold at once. */
#define mainQUEUE_LENGTH (2)

/* The values sent to the queue receive task from the queue send task and the
 * queue send software timer respectively. */
#define mainVALUE_SENT_FROM_TASK (100UL)
#define mainVALUE_SENT_FROM_TIMER (200UL)

/*-----------------------------------------------------------*/

/*
 * The tasks as described in the comments at the top of this file.
 */
static void prvQueueReceiveTask(void *pvParameters);
static void prvQueueSendTask(void *pvParameters);

/*
 * The tasks is loopback of uart
 */
static void prvUARTLoopbackTask(void *pvParameters);
static void prvSDRAMTestTask(void *pvParameters);

/*
 * Task for GPIO
 */
static void prvGPIOOutputTask(void *prvParameters);
static void prvGPIOInputTask(void *prvParameters);
static void prvGPIOIntTask(void *prvParameters);

/*
 * The callback function executed when the software timer expires.
 */
static void prvQueueSendTimerCallback(TimerHandle_t xTimerHandle);

/*-----------------------------------------------------------*/
/* CSR helpers and CLINT low-level access                    */
/*-----------------------------------------------------------*/

static inline uint32_t read_csr(const char *csr)
{
    uint32_t x;
    asm volatile("csrr %0, %1" : "=r"(x) : "i"(0));
    return x;
} /* unused */

static inline uint32_t csr_read_mstatus(void)
{
    uint32_t x;
    asm volatile("csrr %0, mstatus" : "=r"(x));
    return x;
}

static inline uint32_t csr_read_mie(void)
{
    uint32_t x;
    asm volatile("csrr %0, mie" : "=r"(x));
    return x;
}

static inline void csr_write_mstatus(uint32_t x)
{
    asm volatile("csrw mstatus, %0" ::"r"(x));
}

static inline void csr_write_mie(uint32_t x)
{
    asm volatile("csrw mie, %0" ::"r"(x));
}

static inline uint32_t irq_global_off(void)
{
    uint32_t old = csr_read_mstatus();
    csr_write_mstatus(old & ~0x8u); /* clear MIE */
    return old;
}

static inline uint32_t irq_mask_msip_mtip(void)
{
    uint32_t old = csr_read_mie();
    csr_write_mie(old & ~((1u << 3) | (1u << 7))); /* clear MSIE/MTIE */
    return old;
}

static inline void irq_global_restore(uint32_t mstatus_old)
{
    csr_write_mstatus(mstatus_old);
}

static inline void irq_mie_restore(uint32_t mie_old)
{
    csr_write_mie(mie_old);
}

/* CLINT mtime (64-bit) addresses on 32-bit bus */
#define CLINT_MTIME_LO_ADDR ((uintptr_t)(CLINT_BASE + 0xBFF8u))
#define CLINT_MTIME_HI_ADDR ((uintptr_t)(CLINT_BASE + 0xBFFCu))

/* 原来的汇编版 mtime 读取（暂时保留，可不用） */
__attribute__((noinline, optimize("O0"))) void clint_read_mtime_asm(uint32_t *hi, uint32_t *lo)
{
    register uint32_t h1 asm("t0");
    register uint32_t l1 asm("t1");
    register uint32_t h2 asm("t2");
    register uintptr_t ahi asm("t3") = CLINT_MTIME_HI_ADDR;
    register uintptr_t alo asm("t4") = CLINT_MTIME_LO_ADDR;

    asm volatile(
        "lw %0, 0(%3)\n\t" /* h1 = [HI] */
        "lw %1, 0(%4)\n\t" /* l1 = [LO] */
        "lw %2, 0(%3)\n\t" /* h2 = [HI] */
        : "=r"(h1), "=r"(l1), "=r"(h2)
        : "r"(ahi), "r"(alo)
        : "memory");

    if (hi)
        *hi = h1;
    if (lo)
        *lo = l1;
}

/* 本文件内的 32-bit MMIO 读写封装，用于 mtime / mtimecmp 测试 */
static inline void clint_mmio_write32(uintptr_t addr, uint32_t data)
{
    *(volatile uint32_t *)addr = data;
}

static inline uint32_t clint_mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

/* 稳定读取 64-bit MTIME：HI->LO->HI，直到 HI 一致 */
static void clint_read_mtime(uint32_t *hi, uint32_t *lo)
{
    uint32_t h1, h2, l;

    do
    {
        h1 = clint_mmio_read32(CLINT_MTIME_HI_ADDR);
        l = clint_mmio_read32(CLINT_MTIME_LO_ADDR);
        h2 = clint_mmio_read32(CLINT_MTIME_HI_ADDR);
    } while (h1 != h2);

    if (hi)
        *hi = h1;
    if (lo)
        *lo = l;
}

/* 通过 32-bit 总线写 MTIME：先 LO 再 HI */
static void clint_write_mtime(uint32_t hi, uint32_t lo)
{
    clint_mmio_write32(CLINT_MTIME_LO_ADDR, lo);
    clint_mmio_write32(CLINT_MTIME_HI_ADDR, hi);
}

/* 稳定读取 64-bit MTIMECMP：HI->LO->HI */
static void clint_read_mtimecmp(uint32_t *hi, uint32_t *lo)
{
    uint32_t h1, h2, l;

    do
    {
        h1 = clint_mmio_read32(CLINT_MTIMECMP_HI_ADDR);
        l = clint_mmio_read32(CLINT_MTIMECMP_LO_ADDR);
        h2 = clint_mmio_read32(CLINT_MTIMECMP_HI_ADDR);
    } while (h1 != h2);

    if (hi)
        *hi = h1;
    if (lo)
        *lo = l;
}

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

/*-----------------------------------------------------------*/

void my_handle(void)
{
    uint32_t irq_num = PLIC_GetClaim();
    switch (irq_num)
    {
    case 8:
        GPIO_SetEOI(GPIO_NUM_0);
        PLIC_SetComplete(GPIO_INT_NUM_0);
        break;
    default:
        break;
    }
}

static void assert_or_spin(int cond)
{
    if (!cond)
    {
        /* Trap here on failure */
        for (;;)
        {
        }
    }
}

/*==========================================================
=                 CLINT 功能综合测试任务                     =
==========================================================*/
static void prvClintTestTask(void *pvParameters)
{
    (void)pvParameters;

    printf("\r\n========== CLINT 32-bit functional test ==========\r\n");
    printf("[CLINT] BASE = 0x%08x, MTIMECMP@0x%08x/0x%08x, MTIME@0x%08x/0x%08x\r\n",
           (unsigned int)CLINT_BASE,
           (unsigned int)CLINT_MTIMECMP_LO_ADDR,
           (unsigned int)CLINT_MTIMECMP_HI_ADDR,
           (unsigned int)CLINT_MTIME_LO_ADDR,
           (unsigned int)CLINT_MTIME_HI_ADDR);

    /* 关闭全局 MIE 和 MSIP/MTIP，避免 FreeRTOS tick / CLINT 中断干扰测试 */
    uint32_t mstatus_old = irq_global_off();
    uint32_t mie_old = irq_mask_msip_mtip();

    /* -------- Test 1: mtime 自增性检查 -------- */
    uint32_t mt_hi0, mt_lo0, mt_hi1, mt_lo1;

    clint_read_mtime(&mt_hi0, &mt_lo0);

    for (volatile uint32_t i = 0; i < 100000; i++)
    {
        __asm__ volatile("nop");
    }

    clint_read_mtime(&mt_hi1, &mt_lo1);

    printf("[CLINT] mtime sample 0 = 0x%08x_%08x\r\n",
           (unsigned int)mt_hi0, (unsigned int)mt_lo0);
    printf("[CLINT] mtime sample 1 = 0x%08x_%08x\r\n",
           (unsigned int)mt_hi1, (unsigned int)mt_lo1);

    int mtime_increasing =
        (mt_hi1 > mt_hi0) || ((mt_hi1 == mt_hi0) && (mt_lo1 > mt_lo0));
    if (!mtime_increasing)
    {
        printf("[CLINT][FAIL] mtime does not appear to be incrementing\r\n");
        assert_or_spin(0);
    }
    printf("[CLINT][PASS] mtime is incrementing on its own\r\n");

    /* -------- Test 2: MTIMECMP 写入 + 回读（高/低 32-bit） -------- */
    const uint32_t cmp_hi = 0x00000000u;
    const uint32_t cmp_lo = 0x00001000u;
    uint32_t cmp_hi_rd, cmp_lo_rd;

    clint_write_mtimecmp(cmp_hi, cmp_lo); /* 通过 clint.c 封装的 HI=FFFF_FFFF -> LO -> HI */
    clint_read_mtimecmp(&cmp_hi_rd, &cmp_lo_rd);

    printf("[CLINT] MTIMECMP written = 0x%08x_%08x\r\n",
           (unsigned int)cmp_hi, (unsigned int)cmp_lo);
    printf("[CLINT] MTIMECMP read    = 0x%08x_%08x\r\n",
           (unsigned int)cmp_hi_rd, (unsigned int)cmp_lo_rd);

    if ((cmp_hi_rd != cmp_hi) || (cmp_lo_rd != cmp_lo))
    {
        printf("[CLINT][FAIL] MTIMECMP HI/LO readback mismatch\r\n");
        assert_or_spin(0);
    }
    printf("[CLINT][PASS] MTIMECMP 64-bit value can be programmed via 32-bit bus\r\n");

    /* -------- Test 3: MTIME 写入 + 回读（高/低 32-bit） -------- */
    uint32_t mt_hi_orig, mt_lo_orig;
    clint_read_mtime(&mt_hi_orig, &mt_lo_orig);

    /* 在原有基础上往前跳一点，避免太大突变 */
    uint32_t mt_hi_new = mt_hi_orig;
    uint32_t mt_lo_new = mt_lo_orig + 1000u;

    clint_write_mtime(mt_hi_new, mt_lo_new);

    uint32_t mt_hi_chk, mt_lo_chk;
    clint_read_mtime(&mt_hi_chk, &mt_lo_chk);

    printf("[CLINT] MTIME original         = 0x%08x_%08x\r\n",
           (unsigned int)mt_hi_orig, (unsigned int)mt_lo_orig);
    printf("[CLINT] MTIME after write/read = 0x%08x_%08x\r\n",
           (unsigned int)mt_hi_chk, (unsigned int)mt_lo_chk);

    int hi_ok = (mt_hi_chk == mt_hi_new) || (mt_hi_chk == (mt_hi_new + 1u));
    int lo_ok = 1;
    if (mt_hi_chk == mt_hi_new)
    {
        lo_ok = (mt_lo_chk >= mt_lo_new);
    }

    if (!(hi_ok && lo_ok))
    {
        printf("[CLINT][FAIL] MTIME write/read 32-bit path check failed\r\n");
        assert_or_spin(0);
    }

    printf("[CLINT][PASS] MTIME HI/LO can be written and read back via 32-bit accesses\r\n");

    /* 恢复中断，使 FreeRTOS tick 和其它任务继续正常运行 */
    irq_mie_restore(mie_old);
    irq_global_restore(mstatus_old);

    printf("========== CLINT test completed, task will now idle ==========\r\n");

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*==========================================================
=                        main_blinky                       =
==========================================================*/
void main_blinky(void)
{
    /* 创建队列 */
    xQueue = xQueueCreate(mainQUEUE_LENGTH, sizeof(uint32_t));

    if (xQueue != NULL)
    {
        /* 队列接收任务 */
        xTaskCreate(prvQueueReceiveTask,
                    "Rx",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    mainQUEUE_RECEIVE_TASK_PRIORITY,
                    NULL);

        /* 队列发送任务 */
        xTaskCreate(prvQueueSendTask,
                    "Tx",
                    configMINIMAL_STACK_SIZE,
                    NULL,
                    mainQUEUE_SEND_TASK_PRIORITY,
                    NULL);

        /* 软件定时器 */
        xTimer = xTimerCreate("Timer",
                              mainTIMER_SEND_FREQUENCY_MS,
                              pdTRUE,
                              (void *)0,
                              prvQueueSendTimerCallback);

        if (xTimer != NULL)
        {
            xTimerStart(xTimer, 0);
        }
    }

    /* UART loopback 任务 */
    xTaskCreate(prvUARTLoopbackTask,
                "UART",
                UART_TASK_STACK_SIZE,
                NULL,
                UART_LOOPBACK_PRIORITY,
                NULL);

    /* SDRAM 测试任务：如需启用，可取消下面两行注释 */
    /*
    sdram_init_c();
    xTaskCreate(prvSDRAMTestTask,
                "SDRAMTest",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
    */

    /* GPIO 测试任务 */
    xTaskCreate(prvGPIOOutputTask,
                "GPIOOutput",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    xTaskCreate(prvGPIOInputTask,
                "GPIOInput",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    xTaskCreate(prvGPIOIntTask,
                "GPIOInt",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);

    /* CLINT 测试任务 */
    xTaskCreate(prvClintTestTask,
                "ClintTest",
                configMINIMAL_STACK_SIZE * 2,
                NULL,
                CLINT_TEST_TASK_PRIORITY,
                NULL);

    vTaskStartScheduler();

    for (;;)
    {
    }
}

/*==========================================================
=                  原有 Queue / UART / GPIO 任务           =
==========================================================*/

static void prvQueueSendTask(void *pvParameters)
{
    TickType_t xNextWakeTime;
    const TickType_t xBlockTime = mainTASK_SEND_FREQUENCY_MS;
    const uint32_t ulValueToSend = mainVALUE_SENT_FROM_TASK;

    (void)pvParameters;

    xNextWakeTime = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&xNextWakeTime, xBlockTime);
        xQueueSend(xQueue, &ulValueToSend, 0U);
    }
}
/*-----------------------------------------------------------*/

static void prvQueueSendTimerCallback(TimerHandle_t xTimerHandle)
{
    const uint32_t ulValueToSend = mainVALUE_SENT_FROM_TIMER;

    (void)xTimerHandle;

    xQueueSend(xQueue, &ulValueToSend, 0U);
}
/*-----------------------------------------------------------*/

static void prvQueueReceiveTask(void *pvParameters)
{
    uint32_t ulReceivedValue;

    (void)pvParameters;

    for (;;)
    {
        xQueueReceive(xQueue, &ulReceivedValue, portMAX_DELAY);

        if (ulReceivedValue == mainVALUE_SENT_FROM_TASK)
        {
            printf("Message received from task\r\n");
        }
        else if (ulReceivedValue == mainVALUE_SENT_FROM_TIMER)
        {
            printf("Message received from software timer\r\n");
        }
        else
        {
            printf("Unexpected message\r\n");
        }
    }
}
/*-----------------------------------------------------------*/

static void prvUARTLoopbackTask(void *pvParameters)
{
    const uint8_t tx = 'A';
    uint8_t rx;

    (void)pvParameters;

    for (;;)
    {
        uart_clear_rbuf();
        uart_send(tx);
        rx = uart_recv();
        uart_clear_rbuf();

        if (rx == tx)
        {
            uart_send('O');
        }
        else
        {
            uart_send('E');
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
/*-----------------------------------------------------------*/
static void prvSDRAMTestTask(void *pvParameters)
{
    (void)pvParameters;

    volatile uint32_t *sdram = (volatile uint32_t *)SDRAM_BASE;

    const uint32_t test_val = 0xABCD1234;
    uint32_t read_val = 0;
    volatile int sdram_ok = 0;

    for (;;)
    {
        sdram[0] = test_val;

        read_val = sdram[0];

        if (read_val == test_val)
        {
            sdram_ok = 1;
        }
        else
        {
            sdram_ok = 0;
        }

        __asm__ volatile("nop");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
/*-----------------------------------------------------------*/
static void prvGPIOOutputTask(void *prvParameters)
{
    (void)prvParameters;

    GPIO_SetDir(GPIO_A, 0x00);
    GPIO_SetDir(GPIO_B, 0x00);
    GPIO_SetDir(GPIO_C, 0x00);
    GPIO_SetDir(GPIO_D, 0x00);
    uint8_t i = 0;
    for (;;)
    {
        GPIO_SetVal(GPIO_A, i);
        GPIO_SetVal(GPIO_B, i);
        GPIO_SetVal(GPIO_C, i);
        GPIO_SetVal(GPIO_D, i);
        i++;
    }
}

static void prvGPIOInputTask(void *prvParameters)
{
    (void)prvParameters;

    GPIO_SetDir(GPIO_A, 0xFF);
    GPIO_SetDir(GPIO_B, 0xFF);
    GPIO_SetDir(GPIO_C, 0xFF);
    GPIO_SetDir(GPIO_D, 0xFF);
    for (;;)
    {
    }
}

static void prvGPIOIntTask(void *prvParameters)
{
    (void)prvParameters;

    GPIO_SetIntType(0, GPIO_EDGE_SENSITIVE);
    GPIO_SetIntPolarity(0, GPIO_HIGH);
    GPIO_SetIntEn(0, GPIO_ENABLE);
    for (;;)
    {
    }
}
