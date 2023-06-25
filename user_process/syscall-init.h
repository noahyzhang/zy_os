/**
 * @file syscall-init.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef USER_PROCESS_SYSCALL_INIT_H_
#define USER_PROCESS_SYSCALL_INIT_H_

#include "lib/stdint.h"

void syscall_init(void);
uint32_t sys_getpid(void);

#endif  // USER_PROCESS_SYSCALL_INIT_H_
