#include "FreeRTOS.h"
#include "task.h"
#include "clint.h"

#define CLINT_TEST_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define TEST_PERIOD_MS pdMS_TO_TICKS(1000)

static void prvClintTestTask(void *pvParameters)
{
    (void)pvParameters;

    clint_init();

    /* 设定一次定时比较事件，随后循环做自检，模式与 timer 示例一致：等待条件 -> 重启 -> 延时 */
    const uint64_t delta_ticks = 50000ull;

    for (;;)
    {
        /* 1) mtime 连续读取应递增 */
        uint64_t t0 = clint_read_mtime64();
        uint64_t t1;
        do
        {
            t1 = clint_read_mtime64();
        } while (t1 == t0);

        /* 2) 读取高/低 32 位并回读验证组合一致 */
        uint32_t hi1 = clint_read_mtime_hi();
        uint32_t lo = clint_read_mtime_lo();
        uint32_t hi2 = clint_read_mtime_hi();
        (void)hi1;
        (void)lo;
        (void)hi2; /* 可在底层加断言或串口打印 */

        /* 3) 设置 mtimecmp=mtime+delta，采用 32 位安全写序列 */
        uint64_t now = clint_read_mtime64();
        clint_write_mtimecmp64(CLINT_HARTID, now + delta_ticks);

        /* 4) 轮询到达点（到达即视为 mtime 正常计数 + mtimecmp 写入可用） */
        while (clint_read_mtime64() < now + delta_ticks)
        {
            taskYIELD();
        }

        /* 5) 触发并清除一次软件中断，验证 MSIP 可写 */
        clint_msip_set(CLINT_HARTID);
        clint_msip_clear(CLINT_HARTID);

        /* 间隔一段时间再测，保持与定时器样例一致的节奏 */
        vTaskDelay(TEST_PERIOD_MS);
    }
}

void main_blinky(void)
{
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
