/**
 * @file sync.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef THREAD_SYNC_H_
#define THREAD_SYNC_H_

#include "lib/kernel/list.h"
#include "lib/stdint.h"
#include "thread/thread.h"

// 信号量
struct semaphore {
    // 信号量值
    uint8_t value;
    // 保存在此信号量上阻塞的线程
    struct list waiters;
};

// 锁
struct lock {
    // 锁的持有者
    struct task_struct* holder;
    // 用二元信号量实现锁
    struct semaphore sem;
    // 锁的持有者重复申请锁的次数
    // 用于规避重复申请锁的情况
    uint32_t holder_repeat_nr;
};

void sema_init(struct semaphore* psema, uint8_t value);
void sema_down(struct semaphore* psema);
void sema_up(struct semaphore* psema);
void lock_init(struct lock* plock);
void lock_acquire(struct lock* plock);
void lock_release(struct lock* plock);

#endif  // THREAD_SYNC_H_
