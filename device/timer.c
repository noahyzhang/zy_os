#include "device/timer.h"
#include "lib/kernel/io.h"
#include "lib/kernel/print.h"
#include "thread/thread.h"
#include "kernel/debug.h"
#include "kernel/interrupt.h"

// 时钟中断频率（每秒 100 次）
#define IRQ0_FREQUENCY     100
#define INPUT_FREQUENCY    1193180U
#define COUNTER0_VALUE     ((uint16_t)INPUT_FREQUENCY / (uint16_t)IRQ0_FREQUENCY)
#define CONTRER0_PORT      0x40
#define COUNTER0_NO        0
#define COUNTER_MODE       2
#define READ_WRITE_LATCH   3
#define PIT_CONTROL_PORT   0x43

// 每多少毫秒发生一次中断
#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)

// 内核自中断开启以来总共的滴答数
uint32_t ticks;

/**
 * @brief 时钟中断的处理函数
 * 
 */
static void intr_timer_handler(void) {
    struct task_struct* cur_thread = running_thread();
    ASSERT(cur_thread->stack_magic == TASK_STACK_MAGIC_VALUE);
    // 记录此线程占有的 CPU 时间
    cur_thread->elapsed_ticks++;
    // 从内核第一次处理时间中断后开始至今的滴答数，内核态和用户态总共的滴答数
    ticks++;
    if (cur_thread->ticks == 0) {
        // 如果任务的时间片使用完了，就开始调度到新的任务上 CPU
        schedule();
    } else {
        // 将当前任务的时间片减一
        cur_thread->ticks--;
    }
}

/**
 * @brief 以 tick 为单位的 sleep, 任何时间形式的 sleep 会转换此 ticks 形式
 * 
 * @param sleep_ticks 
 */
static void ticks_to_sleep(uint32_t sleep_ticks) {
    uint32_t start_tick = ticks;
    // 若间隔的ticks数不够便让出cpu
    while (ticks - start_tick < sleep_ticks) {
        thread_yield();
    }
}

/**
 * @brief 以毫秒为单位的 sleep
 * 
 * @param m_seconds 
 */
void mtime_sleep(uint32_t m_seconds) {
    uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
    ASSERT(sleep_ticks > 0);
    ticks_to_sleep(sleep_ticks);
}

/**
 * @brief 把操作的计数器counter_no、读写锁属性rwl、计数器模式counter_mode写入模式控制寄存器并赋予初始值counter_value
 * 
 */
static void frequency_set(uint8_t counter_port, \
    uint8_t counter_no, \
    uint8_t rwl, \
    uint8_t counter_mode, \
    uint16_t counter_value) {
    /* 往控制字寄存器端口0x43中写入控制字 */
    outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
    /* 先写入counter_value的低8位 */
    outb(counter_port, (uint8_t)counter_value);
    /* 再写入counter_value的高8位 */
    outb(counter_port, (uint8_t)counter_value >> 8);
}

/**
 * @brief 初始化PIT8253
 * 
 */
void timer_init() {
    put_str("timer_init start\n");
    // 设置8253的定时周期,也就是发中断的周期
    frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done\n");
}
