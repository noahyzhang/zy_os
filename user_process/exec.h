/**
 * @file exec.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef USER_PROCESS_EXEC_H_
#define USER_PROCESS_EXEC_H_

#include "lib/stdint.h"

int32_t sys_execv(const char* path, const char* argv[]);

#endif  // USER_PROCESS_EXEC_H_
