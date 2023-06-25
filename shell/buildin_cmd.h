/**
 * @file buildin_cmd.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SHELL_BUILDIN_CMD_H_
#define SHELL_BUILDIN_CMD_H_

#include "lib/stdint.h"

void make_clear_abs_path(char* path, char* wash_buf);
void buildin_ls(uint32_t argc, char** argv);
char* buildin_cd(uint32_t argc, char** argv);
int32_t buildin_mkdir(uint32_t argc, char** argv);
int32_t buildin_rmdir(uint32_t argc, char** argv);
int32_t buildin_rm(uint32_t argc, char** argv);
void buildin_pwd(uint32_t argc, char** argv);
void buildin_ps(uint32_t argc, char** argv);
void buildin_clear(uint32_t argc, char** argv);

#endif  // SHELL_BUILDIN_CMD_H_
