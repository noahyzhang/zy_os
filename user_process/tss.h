/**
 * @file tss.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef USER_PROCESS_TSS_H_
#define USER_PROCESS_TSS_H_

#include "thread/thread.h"

void update_tss_esp(struct task_struct* pthread);
void tss_init(void);

#endif  // USER_PROCESS_TSS_H_
