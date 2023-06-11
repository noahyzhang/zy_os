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
    SYS_GETPID
};

uint32_t getpid(void);

#endif  // LIB_USER_SYSCALL_H_
