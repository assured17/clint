#include "FreeRTOS.h"
#include "task.h"
#include "timer.h"

#define TIMER_TEST_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define TEST_PERIOD_MS pdMS_TO_TICKS(1000)

static void prvTimerTestTask(void *pvParameters)
{
    (void)pvParameters;

    timer1_init();

    timer2_init();

    timer1_start_periodic();
    timer2_start_oneshot();

    for (;;)
    {

        while (timer1_get_current() != 0)
        {
            taskYIELD();
        }

        while (timer2_get_current() != 0)
        {
            taskYIELD();
        }

        timer2_restart();

        vTaskDelay(TEST_PERIOD_MS);
    }
}

void main_blinky(void)
{
    xTaskCreate(prvTimerTestTask,
                "TimerTest",
                configMINIMAL_STACK_SIZE * 2,
                NULL,
                TIMER_TEST_TASK_PRIORITY,
                NULL);

    vTaskStartScheduler();

    for (;;)
    {
    }
}
