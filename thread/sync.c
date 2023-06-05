#include "kernel/interrupt.h"
#include "kernel/debug.h"
#include "thread/sync.h"

/**
 * @brief 初始化信号量
 * 
 * @param psema 
 * @param value 
 */
void sema_init(struct semaphore *psema, uint8_t value) {
    psema->value = value;
    list_init(&psema->waiters);
}

/**
 * @brief 初始化锁
 * 
 * @param plock 
 */
void lock_init(struct lock *plock) {
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    // 信号量初值设置为 1，也即二元信号量
    sema_init(&plock->sem, 1);
}

/**
 * @brief 信号量的 down 操作
 * 
 * @param psema 
 */
void sema_down(struct semaphore *psema) {
    // 关中断来保证原子操作
    enum intr_status old_status = intr_disable();
    // 如果信号量值为 0，表示已经被别人持有
    for (; psema->value == 0;) {
        ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));
        // 当前线程不应该已经在信号量的 waiters 队列中
        if (elem_find(&psema->waiters, &running_thread()->general_tag)) {
            PANIC("sema down: thread blocked has been in waiters_list\n");
        }
        // 若信号量的值等于 0，则当前线程把自己加入该锁的等待队列，然后阻塞自己
        list_append(&psema->waiters, &running_thread()->general_tag);
        // 阻塞当前线程，直到被唤醒
        thread_block(TASK_BLOCKED);
    }
    // 若 value 为 1 或被唤醒后，即获取到了锁
    psema->value--;
    ASSERT(psema->value == 0);
    // 恢复之前的中断状态
    intr_set_status(old_status);
}

/**
 * @brief 信号量的 up 操作
 * 
 * @param psema 
 */
void sema_up(struct semaphore *psema) {
    // 关中断，保证原子操作
    enum intr_status old_status = intr_disable();
    ASSERT(psema->value == 0);
    if (!list_empty(&psema->waiters)) {
        // 唤醒一个被阻塞的线程
        // 所谓的唤醒只不过是将阻塞中的线程加入到阻塞队列，将来可以参与调度
        // 而且当前是关中断的状态，所以调度器并不会被触发，所以 psema->value++ 是安全的
        struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
        thread_unblock(thread_blocked);
    }
    psema->value++;
    ASSERT(psema->value == 1);
    // 恢复之前的中断状态
    intr_set_status(old_status);
}

/**
 * @brief 获取锁
 * 
 * @param plock 
 */
void lock_acquire(struct lock* plock) {
    // 注意重复获取锁的情况
    if (plock->holder != running_thread()) {
        sema_down(&plock->sem);
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
    } else {
        plock->holder_repeat_nr++;
    }
}

void lock_release(struct lock* plock) {
    ASSERT(plock->holder == running_thread());
    if (plock->holder_repeat_nr > 1) {
        plock->holder_repeat_nr--;
        return;
    }
    ASSERT(plock->holder_repeat_nr == 1);
    // 必须放在 sema_up 操作之前
    // 这里是未关中断的状态，因此需要先给结构赋值，再进行 sema_up 操作
    // 如果先进行 sema_up 操作，然后被调度器换下 CPU，其他线程给 plock 结构赋值了
    // 此线程被唤醒后又更改了 plock 结构，就会导致错误
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_up(&plock->sem);
}

