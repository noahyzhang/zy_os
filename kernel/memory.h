/**
 * @file memory.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-01
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef KERNEL_MEMORY_H_
#define KERNEL_MEMORY_H_

#include "lib/stdint.h"
#include "lib/kernel/bitmap.h"

/**
 * @brief 虚拟地址池，用于虚拟地址管理
 * 
 */
struct virtual_addr {
    // 虚拟地址用到的位图结构
    struct bitmap vaddr_bitmap;
    // 虚拟地址的起始地址
    uint32_t vaddr_start;
};

extern struct pool kernel_pool, user_pool;

void mem_init();

#endif  // KERNEL_MEMORY_H_
