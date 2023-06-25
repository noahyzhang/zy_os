/**
 * @file shell.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SHELL_SHELL_H_
#define SHELL_SHELL_H_

#include "fs/fs.h"

extern char final_path[MAX_PATH_LEN];

void print_prompt(void);
void my_shell(void);

#endif  // SHELL_SHELL_H_
