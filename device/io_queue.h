/**
 * @file io_queue.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef DEVICE_IO_QUEUE_H_
#define DEVICE_IO_QUEUE_H_

#include "lib/stdint.h"
#include "thread/thread.h"
#include "thread/sync.h"

#define BUF_SIZE (64)

/**
 * @brief 环形队列
 * 
 */
struct io_queue_t {
    // 锁
    struct lock lock;
    // 生产者线程，缓冲区不满时就存放数据
    // 否则就睡眠，此项记录那个生产者线程在缓冲区上睡眠
    struct task_struct* producer;
    // 消费者线程，缓存区不空时就获取数据
    // 否则就睡眠，此项记录那个消费者线程在缓冲区上睡眠
    struct task_struct* consumer;
    // 缓冲区
    char buf[BUF_SIZE];
    // 队首，数据往队首处写入
    int32_t head;
    // 队尾，数据从队尾读出
    int32_t tail;
};

void io_queue_init(struct io_queue_t* ioq);
bool ioq_full(struct io_queue_t* ioq);
char ioq_getchar(struct io_queue_t* ioq);
void ioq_putchar(struct io_queue_t* ioq, char byte);

#endif  // DEVICE_IO_QUEUE_H_
