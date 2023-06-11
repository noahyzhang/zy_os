/**
 * @file process.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-11
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef USER_PROCESS_PROCESS_H_
#define USER_PROCESS_PROCESS_H_

#include "thread/thread.h"
#include "lib/stdint.h"

#define DEFAULT_PRIO (31)
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)
#define USER_VADDR_START (0x8048000)

void process_execute(void* process_name, char* name);
void start_process(void* process_name);
void process_activate(struct task_struct* p_thread);
void page_dir_activate(struct task_struct* p_thread);
uint32_t* create_page_dir(void);
void create_user_vaddr_bitmap(struct task_struct* user_prog);

#endif  // USER_PROCESS_PROCESS_H_
