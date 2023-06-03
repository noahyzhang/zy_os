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

#define PAGE_SIZE (4096)

// 内存池标志，用于判断用那个内存池
enum pool_flags {
    // 内核内存池
    PF_KERNEL = 1,
    // 用户内存池
    PF_USER = 2
};

// 页表项或页目录项存在属性位
// 表示 P 位的值为 1，表示此页内存已存在
#define PG_P_1 1
// 表示 P 位的值为 0，表示此页内存不存在
#define PG_P_0 0

// R/W 属性位值，读/执行
// 表示 RW 位的值为 R，即 RW=0，表示此页内存允许读、执行
#define PG_RW_R 0
// 表示 RW 位的值为 W，即 RW=1，表示此页内存允许读、写、执行
#define PG_RW_W 2

// U/S 属性位值
// 表示 US 位的值为 S，即 US=0，表示只允许特权级别为 0、1、2 的程序访问此页内存，3 特权级程序不被允许
#define PG_US_S 0
// 表示 US 位的值为 U，即 US=1，表示允许所有特权级别的程序访问此页内存
#define PG_US_U 4

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

void mem_init(void);
void* get_kernel_pages(uint32_t pg_cnt);
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);

#endif  // KERNEL_MEMORY_H_
