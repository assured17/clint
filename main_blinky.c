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

#define UART_TASK_STACK_SIZE 256
#define UART_LOOPBACK_PRIORITY (tskIDLE_PRIORITY + 1)
#define SDRAM_CTRL_BASE 0x100B0000
#define SDRAM_BASE 0x80000000

/* Priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define mainQUEUE_SEND_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

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

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

/*-----------------------------------------------------------*/

/*** SEE THE COMMENTS AT THE TOP OF THIS FILE ***/

void my_handle(void)
{
    uint32_t irq_num = PLIC_GetClaim();
    switch (irq_num)
    {
    case 8:
        GPIO_SetEOI(GPIO_NUM_0);
        PLIC_SetComplete(GPIO_INT_NUM_0);
        break;
    }
}

void main_blinky(void)
{
    const TickType_t xTimerPeriod = mainTIMER_SEND_FREQUENCY_MS;

    /*
    sdram_init_c();

          xTaskCreate(prvSDRAMTestTask,
             "SDRAMTest",
             configMINIMAL_STACK_SIZE,
             NULL,
             tskIDLE_PRIORITY + 1,
             NULL);

     */
    // GPIO test Task
    /*
    xTaskCreate(prvGPIOOutputTask,
        "GPIOOutput",
        configMINIMAL_STACK_SIZE,
        NULL,
        configMAX_PRIORITIES-1,
        NULL);
    */
    /*
    xTaskCreate(prvGPIOInputTask,
        "GPIOInput",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL);
    */
    // /*
    PLIC_SetEn(GPIO_INT_NUM_0);
    PLIC_SetPriority(GPIO_INT_NUM_0, 10);
    PLIC_SetPriorityThreshold(0);

    xTaskCreate(prvGPIOIntTask,
                "GPIOOInt",
                configMINIMAL_STACK_SIZE,
                NULL,
                configMAX_PRIORITIES - 1,
                NULL);
    // */
    // Start Scheduler

    vTaskStartScheduler();

    for (;;)
    {
    }
}
/*-----------------------------------------------------------*/

static void prvQueueSendTask(void *pvParameters)
{
    TickType_t xNextWakeTime;
    const TickType_t xBlockTime = mainTASK_SEND_FREQUENCY_MS;
    const uint32_t ulValueToSend = mainVALUE_SENT_FROM_TASK;

    /* Prevent the compiler warning about the unused parameter. */
    (void)pvParameters;

    /* Initialise xNextWakeTime - this only needs to be done once. */
    xNextWakeTime = xTaskGetTickCount();

    for (;;)
    {
        /* Place this task in the blocked state until it is time to run again.
         *  The block time is specified in ticks, pdMS_TO_TICKS() was used to
         *  convert a time specified in milliseconds into a time specified in ticks.
         *  While in the Blocked state this task will not consume any CPU time. */
        vTaskDelayUntil(&xNextWakeTime, xBlockTime);

        /* Send to the queue - causing the queue receive task to unblock and
         * write to the console.  0 is used as the block time so the send operation
         * will not block - it shouldn't need to block as the queue should always
         * have at least one space at this point in the code. */
        xQueueSend(xQueue, &ulValueToSend, 0U);
    }
}
/*-----------------------------------------------------------*/

static void prvQueueSendTimerCallback(TimerHandle_t xTimerHandle)
{
    const uint32_t ulValueToSend = mainVALUE_SENT_FROM_TIMER;

    /* This is the software timer callback function.  The software timer has a
     * period of two seconds and is reset each time a key is pressed.  This
     * callback function will execute if the timer expires, which will only happen
     * if a key is not pressed for two seconds. */

    /* Avoid compiler warnings resulting from the unused parameter. */
    (void)xTimerHandle;

    /* Send to the queue - causing the queue receive task to unblock and
     * write out a message.  This function is called from the timer/daemon task, so
     * must not block.  Hence the block time is set to 0. */
    xQueueSend(xQueue, &ulValueToSend, 0U);
}
/*-----------------------------------------------------------*/

static void prvQueueReceiveTask(void *pvParameters)
{
    uint32_t ulReceivedValue;

    /* Prevent the compiler warning about the unused parameter. */
    (void)pvParameters;

    for (;;)
    {
        /* Wait until something arrives in the queue - this task will block
         * indefinitely provided INCLUDE_vTaskSuspend is set to 1 in
         * FreeRTOSConfig.h.  It will not use any CPU time while it is in the
         * Blocked state. */
        xQueueReceive(xQueue, &ulReceivedValue, portMAX_DELAY);

        /*  To get here something must have been received from the queue, but
         * is it an expected value? */
        if (ulReceivedValue == mainVALUE_SENT_FROM_TASK)
        {
            /* It is normally not good to call printf() from an embedded system,
             * although it is ok in this simulated case. */
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
    GPIO_SetIntType(0, GPIO_EDGE_SENSITIVE);
    GPIO_SetIntPolarity(0, GPIO_HIGH);
    GPIO_SetIntEn(0, GPIO_ENABLE);
    for (;;)
    {
    }
}
