/**
 * @file console.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-05
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef DEVICE_CONSOLE_H_
#define DEVICE_CONSOLE_H_

#include "lib/stdint.h"

void console_init(void);
void console_acquire(void);
void console_release(void);
void console_put_str(char* str);
void console_put_char(uint8_t char_asci);
void console_put_int(uint32_t num);

#endif  // DEVICE_CONSOLE_H_
