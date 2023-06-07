#include "kernel/debug.h"
#include "device/io_queue.h"
#include "kernel/interrupt.h"

/**
 * @brief 初始化 IO 队列
 * 
 * @param ioq 
 */
void io_queue_init(struct io_queue_t* ioq) {
    lock_init(&ioq->lock);
    ioq->producer = ioq->consumer = NULL;
    ioq->head = ioq->tail = 0;
}

/**
 * @brief 返回 pos 在缓冲区中的下一个位置索引
 * 
 * @param pos 
 * @return int32_t 
 */
static int32_t next_pos(int32_t pos) {
    return (pos + 1) % BUF_SIZE;
}

/**
 * @brief 判断队列是否已满
 * 
 * @param ioq 
 * @return true 
 * @return false 
 */
bool ioq_full(struct io_queue_t* ioq) {
    // 保证此时中断是关闭状态
    ASSERT(intr_get_status() == INTR_OFF);
    // 队首的位置不能超过队尾
    return next_pos(ioq->head) == ioq->tail;
}

/**
 * @brief 判断队列是否为空
 * 
 * @param ioq 
 * @return true 
 * @return false 
 */
bool ioq_empty(struct io_queue_t* ioq) {
    ASSERT(intr_get_status() == INTR_OFF);
    // 队首位置和队尾位置相等，则说明队列为空
    return ioq->head == ioq->tail;
}

/**
 * @brief 使当前生产者或者当前消费者在此缓冲区上等待
 * 
 * @param waiter 
 */
static void ioq_wait(struct task_struct** waiter) {
    ASSERT(waiter != NULL && *waiter == NULL);
    *waiter = running_thread();
    thread_block(TASK_BLOCKED);
}

/**
 * @brief 唤醒被阻塞的线程
 * 
 * @param waiter 
 */
static void ioq_wakeup(struct task_struct** waiter) {
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);
    *waiter = NULL;
}

/**
 * @brief 消费者从队列中获取一个字符
 * 
 * @param ioq 
 * @return char 
 */
char ioq_getchar(struct io_queue_t* ioq) {
    // 保证中断是关闭状态，保证线程安全
    ASSERT(intr_get_status() == INTR_OFF);
    // 若缓冲区（队列）为空，把消费者（ioq->consumer）设置为当前线程
    // 等待生产者来唤醒
    for (; ioq_empty(ioq);) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->consumer);
        lock_release(&ioq->lock);
    }
    // 从缓冲区中获取一个字符
    char byte = ioq->buf[ioq->tail];
    // 移动队尾指针
    ioq->tail = next_pos(ioq->tail);
    // 如果生产者不为空，说明有生产者被阻塞，唤醒他
    if (ioq->producer != NULL) {
        ioq_wakeup(&ioq->producer);
    }
    return byte;
}

/**
 * @brief 生产者向队列中写入一个字符
 * 
 * @param ioq 
 * @param byte 
 */
void ioq_putchar(struct io_queue_t* ioq, char byte) {
    ASSERT(intr_get_status() == INTR_OFF);
    // 如果缓冲区满了，设置生产者 (ioq->producer) 为当前线程
    // 等待消费者来唤醒
    for (; ioq_full(ioq);) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->producer);
        lock_release(&ioq->lock);
    }
    // 将字节放入到缓冲区
    ioq->buf[ioq->head] = byte;
    // 移动队首指针
    ioq->head = next_pos(ioq->head);
    // 如果有消费者不为空，说明消费者有阻塞，唤醒他
    if (ioq->consumer != NULL) {
        ioq_wakeup(&ioq->consumer);
    }
}
