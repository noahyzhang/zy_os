/**
 * @file syscall.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef LIB_USER_SYSCALL_H_
#define LIB_USER_SYSCALL_H_

#include "lib/stdint.h"

enum SYSCALL_NR {
    SYS_GETPID,
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE
};

uint32_t getpid(void);
void* malloc(uint32_t size);
void free(void* ptr);
uint32_t write(uint32_t fd, const void* buf, uint32_t count);

#endif  // LIB_USER_SYSCALL_H_
