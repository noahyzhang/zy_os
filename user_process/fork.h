/**
 * @file fork.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-24
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef USER_PROCESS_FORK_H_
#define USER_PROCESS_FORK_H_

#include "thread/thread.h"

// fork 子进程，只能由用户进程通过系统调用 fork 调用
// 内核线程不可直接调用，原因是要从 0 级栈中获取 esp3 等
pid_t sys_fork(void);

#endif  // USER_PROCESS_FORK_H_
