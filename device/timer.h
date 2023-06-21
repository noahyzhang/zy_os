/**
 * @file timer.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-06-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef DEVICE_TIME_H_
#define DEVICE_TIME_H_

#include "lib/stdint.h"

void timer_init(void);
void mtime_sleep(uint32_t m_seconds);

#endif  // DEVICE_TIME_H_
