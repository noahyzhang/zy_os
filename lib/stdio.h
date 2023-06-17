/**
 * @file stdio.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-12
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef LIB_STDIO_H_
#define LIB_STDIO_H_

#include "lib/stdint.h"

typedef char* va_list;

uint32_t printf(const char* str, ...);
uint32_t snprintf(char* buf, uint32_t size, const char* format, ...);
uint32_t vsprintf(char* str, const char* format, va_list ap);

#endif  // LIB_STDIO_H_
